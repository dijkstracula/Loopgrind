/*--------------------------------------------------------------------*/
/*--- Loopgrind: an event loop analyzer                  lg_hash.c ---*/
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

#include "lg_hash.h"



/************************** Shadow table stuff *******************************/

shadow_record* add_shadow_record(VgHashTable ht, Addr addr)
{
    shadow_record *r;
   
    tl_assert(addr);

    r = VG_(malloc)("shadow_record", sizeof(shadow_record));
    r->addr = addr;
    r->type = Ity_INVALID;
    r->oldval = 0;
    r->newval = 0;


    VG_(HT_add_node)(ht, (VgHashNode *)r);

    return r;
}



shadow_record* get_shadow_record(VgHashTable ht, Addr key)
{
    tl_assert(ht);
    return (shadow_record *)VG_(HT_lookup)(ht, key);
}



void pp_shadow_record(shadow_record* r) {
    switch (r->type)
    {
        case Ity_I1:
            VG_(printf)("W %p : %d => %d\n", r->addr, 
                                             (r->oldval ? 1 : 0), 
                                             (r->newval ? 1 : 0) );
            break;
        case Ity_I8:
            VG_(printf)("W %p : 0x------%02lx => 0x------%02lx\n", 
                    r->addr, r->oldval, r->newval);
            break;
        case Ity_I16:
            VG_(printf)("W %p : 0x----%04lx => 0x----%04lx\n", 
                    r->addr, r->oldval, r->newval);
            break;
        case Ity_I32:
            VG_(printf)("W %p : 0x%08lx => 0x%08lx\n", 
                    r->addr, r->oldval, r->newval);
            break;
        case Ity_F32:
            VG_(printf)("W %p : 0x%08f => 0x%08f\n", 
                    r->addr, r->oldval, r->newval);
            break;
        case Ity_F64:
            VG_(printf)("W %p : 0x%Lf => 0x %Lf\n",
                    r->addr, r->oldval, r->newval);
            break;
        default:
            tl_assert2(0, "log_shadow_write: IRType not implemented");
    }

}


/************************* Superblock record stuff ***************************/
sb_record* add_sb_record(VgHashTable ht, Addr key) 
{
    sb_record *r = VG_(malloc)("sb_record", sizeof(sb_record));
    r->addr = key;
    VG_(memset)(r->fn_name, 0, sizeof(r->fn_name));
    r->count = 0;
    r->jump_targets = VG_(HT_construct)("jump_targets");

    VG_(get_fnname_if_entry)(
            key,
            r->fn_name,
            sizeof(r->fn_name));

    VG_(HT_add_node)(ht, (VgHashNode*)r);

    return r;
}

sb_record* get_sb_record(VgHashTable ht, Addr key)
{
    return (sb_record *)VG_(HT_lookup)(ht, key);
}


void pp_sb_record(sb_record *r)
{
    static int depth = 0;
    static char space_buffer[1024];
    sb_record *child;

    VG_(memset)(space_buffer, ' ', (depth < 1024) ? depth : 1024);
    space_buffer[depth] = 0;

    VG_(printf)("NODE 0x%08lx (%lu)\n", r->addr, r->count);


    if (r->fn_name[0])
    {
        VG_(printf)("FNNAME 0x%08lx %s\n", r->addr, r->fn_name);
    }


    VG_(HT_ResetIter)(r->jump_targets);

    while ((child = VG_(HT_Next)(r->jump_targets)) != NULL)
    {
        VG_(printf)("EDGE 0x%08lx => 0x%08lx (%lu)\n",
                r->addr, child->addr, child->count);

        depth++;
        pp_sb_record(child);
        depth--;
    }

}

