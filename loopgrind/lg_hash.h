#ifndef __LG__HASH_H_
#define __LG__HASH_H_

#include "pub_tool_basics.h"
#include "pub_tool_hashtable.h"
#include "pub_tool_mallocfree.h"
#include "pub_tool_tooliface.h"
#include "pub_tool_libcassert.h"
#include "pub_tool_libcprint.h"
#include "pub_tool_debuginfo.h"
#include "pub_tool_libcbase.h"
#include "pub_tool_options.h"

/******************************** structs ************************************/


typedef struct _sb_record 
{
    struct _sb_record   *next;
    Addr                addr;

    char                fn_name[256];
    ULong               count;
    VgHashTable         jump_targets;
} 
sb_record;



typedef struct _shadow_record
{
    struct _shadow_record  *next;
    Addr                addr;

    IRType              type;
    Long                oldval;
    Long                newval;
}
shadow_record;

/**************************** Function prototypes ****************************/

shadow_record *add_shadow_record(VgHashTable, Addr);
shadow_record* get_shadow_record(VgHashTable, Addr);
void pp_shadow_record(shadow_record*);

sb_record* add_sb_record(VgHashTable, Addr);
sb_record* get_sb_record(VgHashTable, Addr);
void pp_sb_record(sb_record *r);


#endif
