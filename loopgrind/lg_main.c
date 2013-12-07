/*--------------------------------------------------------------------*/
/*--- Loopgrind: an event loop analyzer                  lg_main.c ---*/
/*--------------------------------------------------------------------*/

/* This file is a part of a submission for a course project in
 * CPSC 538W, Execution Mining, at UBC, Winter 2010.

 * Copyright (c) 2010 Nathan Taylor <tnathan@cs.ubc.ca>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "pub_tool_basics.h"
#include "pub_tool_hashtable.h"
#include "pub_tool_mallocfree.h"
#include "pub_tool_tooliface.h"
#include "pub_tool_libcassert.h"
#include "pub_tool_libcprint.h"
#include "pub_tool_debuginfo.h"
#include "pub_tool_libcbase.h"
#include "pub_tool_options.h"
#include "pub_tool_machine.h"     // VG_(fnptr_to_fnentry)

#include "lg_hash.h"



/****************************** globals **************************************/

#define TEXT_SEG_BEGIN 0x08048000
#define HEAP_SEG_END   0x40000000
#define KERN_SEG_BEGIN 0xc0000000


/* Global superblock hash table: maps Addrs -> sb_record */
VgHashTable global_bb_ht    = NULL;

/* Shadow memory table: maps Addrs -> shadow_record */
VgHashTable shadow_table    = NULL;




/************************** Command-line args ********************************/

/* Since the actual main loop event header is currently being figured out offline,
 * we can take the address as an argument in order to actually do stuff. */
static Addr clo_loop_addr     = 0;


/* Be even more verbose than usual. */
static Bool clo_debug_mode      = False;


/* We're not interested in analyzing gory libc startup/pulldown functions,
 * so only log after hitting the main SB and stop when we find a call to exit */
static Bool logging             = False;

/* We use the entry function's address to register when the target program jumps
 * into library calls, and the "values" for all jumps in the SB graph are weighed
 * exponentially less the farther away the destination is from the initial frame. */
static char* log_entry_fnname   = "main";
static Addr  log_entry_addr     = 0x0;
static Addr  log_entry_ebp    = 0x0;

static char* log_exit_fnname    = "exit";

static Addr curr_bb_addr        = 0x0;


/******************** Helpful utility functions ******************************/


/* Calculates the weight that should be assigned to an edge in the SB graph.  Because
 * one of our tenets states that the main loop shouldn't be far up the backtrace,
 * we penalize SBs that are far away from main on the stack.  So, we have an
 * exponential decay model function, arbitrarily chosen to be
 *
 * y = 1000 * exp(-(1/512)x)
 *
 * Also, because Valgrind doesn't have floating point support yet (!!!), 
 * we scale up everything by 1000.
 */
static ULong calculate_weight(Addr ebp)
{
    double x, y = 1.0;

    tl_assert(log_entry_ebp);
    
    if ((log_entry_ebp - ebp) > 0x750) return 0;


    x = -((double)(log_entry_ebp - ebp))/512.0;


    /* Taylor series unrolled for OMG MAXIMUM FASTNESS */
    y += x;
    y += (x * x) / 2.0;
    y += (x * x * x) / 6.0;
    y += (x * x * x * x) / 24.0;
    y += (x * x * x * x * x) / 120.0;
    y += (x * x * x * x *x * x) / 720.0;

    return (ULong)(y * 1000.0);  /* ouch! */
}

static void start_logging(void)
{
    if (clo_debug_mode)
    {
        VG_(printf)("*** instrumentation enabled ***\n");
    }
    logging = True;
}

static void stop_logging(void)
{
    if (clo_debug_mode)
    {
        VG_(printf)("*** instrumentation disabled ***\n");
    }
    logging = False;
}


/* Handle entering the program's entry point. */
static  void process_main_SB(IRSB *bb)
{
    IRStmt *first_stmt = bb->stmts[0];

    if(clo_debug_mode)
        VG_(printf)("* Found %s() at %08llx *\n", 
                log_entry_fnname, first_stmt->Ist.IMark.addr);

    /* We now care about what Valgrind is executing!! */
    start_logging();
    curr_bb_addr = log_entry_addr = first_stmt->Ist.IMark.addr;

    add_sb_record(global_bb_ht, curr_bb_addr);
}

/* Handle leaving the program. */
static  void process_exit_SB(IRSB *bb)
{
    IRStmt *first_stmt = bb->stmts[0];

    if(clo_debug_mode)
        VG_(printf)("* Found %s() at %08llx *\n", 
                log_exit_fnname, first_stmt->Ist.IMark.addr);

    /* Stop logging so we don't track process pulldown stuff. */
    stop_logging();
    log_entry_addr = 0x0;
}



/* U-widen 8/16/32 bit int expr to 32.  Sort of stolen from Chronicle's
 * add_trace_store_flatten() */
static IRTemp widen_expr_to_32_bits (IRSB *bb, IRExpr* e )
{
    IRExpr *wide_e;
    IRTemp temp = newIRTemp(bb->tyenv, Ity_I32); /*This will break on x64 */

    /* Widen e into a 64-bit (tree IR) value */
    switch (typeOfIRExpr(bb->tyenv,e)) 
    {
        case Ity_F32:
        case Ity_I32: wide_e = deepCopyIRExpr(e); break;
        case Ity_I16: wide_e = IRExpr_Unop(Iop_16Uto32,e); break;
        case Ity_I8:  wide_e = IRExpr_Unop(Iop_8Uto32,e); break;
        default:  ppIRType(typeOfIRExpr(bb->tyenv,e)); 
                  tl_assert2(0, "widen_expr_to_32_bits: unimplemented IRType conversion"); 
    }


    addStmtToIRSB(bb, IRStmt_WrTmp(temp, wide_e));

    return temp;
} 

/************** SB graph generation callback functions ************************/


/* Callback when instrumented execution jumps to a new superblock */
static void trace_superblock(Addr ebp, Addr key)
{
    sb_record *node;
    VgHashTable old_jump_targets; 


    /* Little trick: since we know the entry point of main() is the first
     * one we'll see, if log_entry_ebp is unset, set it now. */
    if (log_entry_ebp == 0)
    {
        log_entry_ebp = ebp;
    }


    tl_assert(key != 0);
    tl_assert(ebp <= log_entry_ebp); /* stack down, heap up! */


    /* Increment global superblock counter */
    node = get_sb_record(global_bb_ht, key);

    if (!node)
    {
        node = add_sb_record(global_bb_ht, key);
    }

    node->count += calculate_weight(ebp);

    if (clo_debug_mode)
    {
        VG_(printf)("EBP %p\n", log_entry_ebp - ebp);
        VG_(printf)("SB %08lx: (%lu)\n", node->addr, node->count);
    }


    /* Increment jump target count in current superblock */
    node = get_sb_record(global_bb_ht, curr_bb_addr);

    /* If we don't have the current sb hashed, there's something fishy */
    tl_assert(node);

    old_jump_targets = node->jump_targets;
    node = get_sb_record(old_jump_targets, key);
    if (!node)
    {
        node = add_sb_record(old_jump_targets, key);
    }

    node->count += calculate_weight(ebp);

    if (clo_debug_mode)
        VG_(printf)("JP %08lx -> %08lx (%lu)\n\n", 
                curr_bb_addr,
                node->addr,
                node->count);

    curr_bb_addr = key;

}




/************************ Shadow memory functions ****************************/





static void print_and_reset_shadow_mem(void)
{
    shadow_record *r;

        VG_(printf)(" *** Memory diff since last entry into %p ***\n", clo_loop_addr);


        VG_(HT_ResetIter)(shadow_table);

        while ((r = VG_(HT_Next)(shadow_table)) != NULL)
        {
            pp_shadow_record(r);
        }

        VG_(printf)(" ***\n");



        /* Free up all memory in existing table */
        VG_(HT_destruct)(shadow_table);



    shadow_table = VG_(HT_construct)("shadow_table");

    tl_assert(shadow_table); 

}



static void log_shadow_write(Addr addr, IRType type, Long oldval, Long newval) 
{
    shadow_record *r;

    tl_assert(addr);
    tl_assert(type != Ity_INVALID);


    r = get_shadow_record(shadow_table, addr);

    if (!r)
    {
        r = add_shadow_record(shadow_table, addr);
        r->type = type;
        r->oldval = oldval;

        /* Mask out high order bits if the size of the write is < 32 bits */
        switch (r->type)
        {
            case Ity_I1:
                r->oldval &= 0x1;
                break;
            case Ity_I8:
                r->oldval &= 0xFF000000FF; /* wtffffff*/
                break;
            case Ity_I16:
                r->oldval &= 0xFFFF;
                break;
            case Ity_I32:
            case Ity_I64:
            case Ity_I128:
            case Ity_F32:
            case Ity_F64:
            case Ity_V128:
            case Ity_INVALID:
                break;
        }
    }

    r->newval = newval;
}



/********************* Valgrind callback functions ***************************/




/* Where the magic happens. */
static IRSB* lg_instrument ( VgCallbackClosure* closure,
        IRSB* sbIn,
        VexGuestLayout* layout, 
        VexGuestExtents* vge,
        IRType gWordTy, IRType hWordTy )
{
    int i = 0;
    char fnname[128];
    IRSB *sbOut;

    /* Set up SB reamble */
    sbOut = deepCopyIRSBExceptStmts(sbIn);
    while (i < sbIn->stmts_used &&
            sbIn->stmts[i]->tag != Ist_IMark)
    {
        addStmtToIRSB(sbOut, sbIn->stmts[i]);
        i++;
    }

    IRStmt *first_stmt = sbIn->stmts[i];

    /* The first statement should be an IMark */
    tl_assert(i < sbIn->stmts_used && first_stmt->tag == Ist_IMark);




    /* Start logging again if we've coming up from being under main */
    if (log_entry_addr && 
            !logging && first_stmt->Ist.IMark.addr >= TEXT_SEG_BEGIN)
    {
        start_logging();
    }
    /* If we've jumped to some loaded library, assume that we're
     * in libc somewhere and stop instrumenting until we get back out */
    if (logging && first_stmt->Ist.IMark.addr < TEXT_SEG_BEGIN)
    {
        stop_logging();
    }



    /*******
     * Shadow memory stuff
     ******/
//    VG_(printf)("first_stmt->Ist.IMark.addr = %p clo_loop_addr = %p\n", 
//            (Addr)first_stmt->Ist.IMark.addr, clo_loop_addr);

    if ((Addr)first_stmt->Ist.IMark.addr == clo_loop_addr)
    {
        IRDirty *di = unsafeIRDirty_0_N(
                0, "print_and_reset_shadow_mem",
                VG_(fnptr_to_fnentry)( &print_and_reset_shadow_mem ),
                mkIRExprVec_0());
        addStmtToIRSB(sbOut, IRStmt_Dirty(di));
    }




    /*******
     * SB graph generation stuff
     ******/

    /* Instrument this block! */
    if (logging)
    {
        /* Construct a temporary to extract out the frame pointer */
        IRTemp temp = newIRTemp(sbOut->tyenv, gWordTy);
        addStmtToIRSB(
                sbOut,
                IRStmt_WrTmp(
                    temp,
                    IRExpr_Get(layout->offset_FP, gWordTy)));

        IRDirty *di = unsafeIRDirty_0_N(
                0, "trace_superblock",
                VG_(fnptr_to_fnentry)( &trace_superblock ),
                mkIRExprVec_2(  IRExpr_RdTmp(temp),
                    mkIRExpr_HWord( vge->base[0] )) );

        /* Set annotations indicating that we want the frame pointer */
        di->nFxState = 1;
        di->fxState[0].fx       = Ifx_Read;
        di->fxState[0].offset   = layout->offset_FP;
        di->fxState[0].size     = layout->sizeof_FP;

        addStmtToIRSB(sbOut, IRStmt_Dirty(di));
    }




    /********
     * Instrument subsequent VEX instructions
     *******/

    for(/* use current i */; i < sbIn->stmts_used; ++i) 
    { 
        IRStmt *curr_stmt = sbIn->stmts[i];

        switch (curr_stmt->tag)
        {
            case Ist_IMark:    
                /* We are interested in enabling logging if we've found main;
                 * conversely, disable logging if we've exited out of main(). */
                if (VG_(get_fnname_if_entry)(
                            curr_stmt->Ist.IMark.addr,
                            fnname,
                            sizeof(fnname))) 
                {

                    if (VG_(strcmp)(fnname, log_entry_fnname) == 0) 
                    {
                        process_main_SB(sbIn);
                    }
                    else if (VG_(strcmp)(fnname, log_exit_fnname) == 0)
                    {
                        process_exit_SB(sbIn);
                    } 

                }//if
                addStmtToIRSB(sbOut, curr_stmt);
                break; //IMark

            case Ist_Store:
                if (logging && clo_loop_addr)
                {
                    IRExpr **argv;
                    IRDirty *di;
                    IRTemp oldval, newval;

                    /* Get the current value at the address we're overwriting */
                    oldval = newIRTemp(sbOut->tyenv, gWordTy);
                    addStmtToIRSB(sbOut, 
                            IRStmt_WrTmp(   oldval, 
                                IRExpr_Load(False,
                                    Iend_LE,
                                    Ity_I32,
                                    curr_stmt->Ist.Store.addr)));


                    /* Get new value that we will write */
                    newval = widen_expr_to_32_bits(sbOut, curr_stmt->Ist.Store.data);

                    argv = mkIRExprVec_4(
                            curr_stmt->Ist.Store.addr, 
                            mkIRExpr_HWord(typeOfIRExpr(sbIn->tyenv,curr_stmt->Ist.Store.data)),
                            IRExpr_RdTmp(oldval),
                            IRExpr_RdTmp(newval));

                    di = unsafeIRDirty_0_N(0 /* regparm */,
                            "log_shadow_write",
                            VG_(fnptr_to_fnentry)(log_shadow_write),
                            argv);

                    addStmtToIRSB(sbOut, IRStmt_Dirty(di));

                }
                addStmtToIRSB(sbOut, curr_stmt);
                break; //Store


            case Ist_NoOp:
            case Ist_AbiHint:
            case Ist_Put:
            case Ist_PutI:
            case Ist_MBE:
            case Ist_WrTmp:
            case Ist_Dirty:
            case Ist_CAS:
            case Ist_Exit:
                addStmtToIRSB(sbOut, curr_stmt);
                break;


            default:
                tl_assert2(0, "Invalid IRStmt tag!"); /* nooooooooooooo */
        }


    }//i

    /* The last statement should be an IRStmt_Exit containing the
     * branch instruction. */

    return sbOut;
}




static Bool lg_process_cmd_line_option(Char *arg)
{
    if      VG_BOOL_CLO(arg, "--debug",         clo_debug_mode) {}
    else if VG_BHEX_CLO(arg, "--loop-addr", clo_loop_addr, TEXT_SEG_BEGIN, HEAP_SEG_END) {}

    else return False;

    return True;
}

static void lg_print_usage(void)
{
    VG_(printf)("\t--debug=no|yes             Verbose mode\n"
            "\t--header-addr=<addr>       Specify a priori header start for analysis\n");
}

static void lg_print_debug_usage(void)
{
    VG_(printf)("none\n");
}


static void lg_post_clo_init(void)
{
    /* Make the VEX optimizer stupid. */
    VG_(clo_vex_control).iropt_level = 0;
    VG_(clo_vex_control).iropt_unroll_thresh = 0;
    //    VG_(clo_vex_control).guest_chase_thresh = 0;
}


static void lg_fini(Int exitcode)
{
    sb_record *r;

    VG_(HT_ResetIter)(global_bb_ht);

    while ((r = VG_(HT_Next)(global_bb_ht)) != NULL)
    {
        //    VG_(umsg)("%08lx\t%lu\n",
        //                r->addr,
        //                r->count);
        pp_sb_record(r);
    }
}

static void lg_pre_clo_init(void)
{
    /* Valgrind banner crap, etc. */
    VG_(details_name)            ("Loopgrind");
    VG_(details_version)         (NULL);
    VG_(details_description)     ("an event loop analyzer");
    VG_(details_copyright_author)(
            "Copyright (C) 2010, by Nathan Taylor <tnathan@cs.ubc.ca>");
    VG_(details_bug_reports_to)  (VG_BUGS_TO);

    VG_(basic_tool_funcs)        (lg_post_clo_init,
            lg_instrument,
            lg_fini);

    VG_(needs_command_line_options)(lg_process_cmd_line_option,
            lg_print_usage,
            lg_print_debug_usage);


    global_bb_ht = VG_(HT_construct)("global_bb_ht");
    shadow_table = VG_(HT_construct)("shadow_table");

}

VG_DETERMINE_INTERFACE_VERSION(lg_pre_clo_init)

    /*--------------------------------------------------------------------*/
    /*--- end                                                          ---*/
    /*--------------------------------------------------------------------*/
