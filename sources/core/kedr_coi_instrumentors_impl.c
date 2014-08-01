/*
 * Implementation of instrumentation mechanisms.
 */

/* ========================================================================
 * Copyright (C) 2014, Andrey V. Tsyvarev  <tsyvarev@ispras.ru>
 *
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 ======================================================================== */
 
#include "kedr_coi_instrumentor_internal.h"
#include "kedr_coi_hash_table.h"


#include <linux/slab.h> /* memory allocations */
#include <linux/spinlock.h> /* spinlocks */

/*
 * Global table of all used operations.
 * 
 * Elements are instrument_data->ops_elem_global.
 * 
 * Used for prevent instrumentation of already instrumented data.
 */
static struct kedr_coi_hash_table ops_table_global;
/* Protect ops_table_global from concurrent accesses. */
static spinlock_t ops_table_global_lock;

/*
// For debug
#ifdef spin_lock_irqsave
#undef spin_lock_irqsave
#endif

#define spin_lock_irqsave(lock, flags) do {flags = 1;} while(0);

#ifdef spin_unlock_irqrestore
#undef spin_unlock_irqrestore
#endif

#define spin_unlock_irqrestore(lock, flags) do {} while(0);
*/


/* Initialize common idata structure. Descructor not needed. */
static void instrument_data_init(struct instrument_data* idata,
    const struct instrument_data_operations* i_ops)
{
    idata->refs = 1;
    idata->i_ops = i_ops;
}

/* Initialize search data structure. */
static void instrument_data_search_init(
    struct instrument_data_search* data_search,
    struct instrument_data* idata,
    const void* ops)
{
    data_search->idata = idata;
    kedr_coi_hash_elem_init(&data_search->ops_elem, ops);
    kedr_coi_hash_elem_init(&data_search->ops_elem_global, ops);
}

/* 
 * Return operations pointer from search data structure.
 * 
 * Result is converted to void* type.
 */
static void* instrument_data_search_get_ops(
    struct instrument_data_search* data_search)
{
    return (void*)data_search->ops_elem.key;
}

/* Add idata search structure to hash table of the instrumentor. */
static int instrumentor_add_data_search(
    struct kedr_coi_instrumentor* instrumentor,
    struct instrument_data_search* data_search)
{
    unsigned long flags;
    int err = -EBUSY;
    void* ops = instrument_data_search_get_ops(data_search);
    
    spin_lock_irqsave(&ops_table_global_lock, flags);
    
    if(kedr_coi_hash_table_find_elem(&ops_table_global, ops))
    {
        pr_err("Cannot intercept operations %p, which are already intercepted by other interceptor.", ops);
        goto out;
    }
    
    err = kedr_coi_hash_table_add_elem(&ops_table_global,
        &data_search->ops_elem_global);
    if(err) goto out;
    
    err = kedr_coi_hash_table_add_elem(&instrumentor->idata_table,
        &data_search->ops_elem);
    
    if(err)
    {
        kedr_coi_hash_table_remove_elem(&ops_table_global,
            &data_search->ops_elem_global);
        goto out;
    }

out:        
    spin_unlock_irqrestore(&ops_table_global_lock, flags);

    return err;
}

/* Remove idata search structure to hash table of the instrumentor. */
static void instrumentor_remove_data_search(
    struct kedr_coi_instrumentor* instrumentor,
    struct instrument_data_search* data_search)
{
    unsigned long flags;
    spin_lock_irqsave(&ops_table_global_lock, flags);
    
    kedr_coi_hash_table_remove_elem(&instrumentor->idata_table,
        &data_search->ops_elem);
    kedr_coi_hash_table_remove_elem(&ops_table_global,
        &data_search->ops_elem_global);
    
    spin_unlock_irqrestore(&ops_table_global_lock, flags);
}

/* 
 * Initialize common foreign idata structure.
 * 
 * NB: Descruction is required to unref binded idata.
 */
static void instrument_data_foreign_init(
    struct instrument_data_foreign* idata_foreign,
    struct instrument_data* idata_binded,
    const struct instrument_data_foreign_operations* fi_ops)
{
    idata_foreign->refs = 1;
    idata_foreign->idata_binded = idata_binded;
    idata_foreign->fi_ops = fi_ops;
}
/****************** Helpers for simplify access to operation **********/

/*
 * Get operation at given offset in the operations structure.
 */
static inline void*
operation_at_offset(const void* ops, size_t operation_offset)
{
    if(ops == NULL) return NULL;
    return *((void**)((const char*)ops + operation_offset));
}

/*
 * Get operation at given offset in the operations structure.
 * Operations pointer cannot be NULL.
 * 
 */
static inline void*
operation_at_offset_not_null(const void* ops, size_t operation_offset)
{
    BUG_ON(ops != NULL);
    return *((void**)((const char*)ops + operation_offset));
}


/*
 * Get reference to operation at given offset in the operations
 * structure.
 */

static inline void**
operation_at_offset_p(void* ops, size_t operation_offset)
{
    return ((void**)((char*)ops + operation_offset));
}

/*
 * Replace operation according to its initial value and @replacement.
 * This is a generic variant, where given operation may be already
 * replaced by someone else (note, that replacement_mode bases on original
 * value of the operation).
 */
static void replace_operation_generic(void** op_p, void* op_orig,
    const struct kedr_coi_replacement* replacement)
{
    if(replacement->mode == replace_all)
    {
        *op_p = replacement->repl;
    }
    else if(op_orig)
    {
        if(replacement->mode == replace_not_null) *op_p = replacement->repl;
    }
    else
    {
        if(replacement->mode == replace_null) *op_p = replacement->repl;
    }
}

/*
 * Replace operation according to @replacement.
 * Exepects that operation has not be replaced before, and contains
 * original value before function's call.
 */
static void replace_operation(void** op_p,
    const struct kedr_coi_replacement* replacement)
{
    replace_operation_generic(op_p, *op_p, replacement);
}


/*************** Foreign 'at_place' instrumentation *******************/
/* 
 * Both 'at_place' and 'use_copy' normal instrumentations has
 * replaced operations structure suitable for futher 'at_place' foreign
 * instrumentation.
 */

/*
 * Common instrument_data structure, which are are usable by
 * 'at_place' foreign instrumentation.
 */
struct apf_instrument_data
{
    struct instrument_data idata_base;
    /* List of foreign instrumentation data which uses this object. */
    struct list_head foreign_data_list;
};

static void apf_instrument_data_init(struct apf_instrument_data* apf_idata,
    const struct instrument_data_operations* i_ops)
{
    instrument_data_init(&apf_idata->idata_base, i_ops);
    INIT_LIST_HEAD(&apf_idata->foreign_data_list);
}

/* Common foreign instrumentation data structure itself. */
struct apf_instrument_data_foreign
{
    struct instrument_data_foreign idata_foreign_base;
    
    struct list_head foreign_data_elem;

    /* 
     * Used for search foreign data for normal ones.
     * 
     * Also, when need to uninstument, we reconstruct operations replacement
     * using replacements in every foreign instrumentation in a chain,
     * and replacements in the normal instrumentation structure.
     */
    struct kedr_coi_foreign_instrumentor* instrumentor;
};


/* Return binded data for these foreign ones. */
static struct apf_instrument_data* apf_idata_foreign_get_binded(
    struct apf_instrument_data_foreign* apf_idata_foreign)
{
    struct apf_instrument_data* apf_idata;
    struct instrument_data* idata;
    
    idata = apf_idata_foreign->idata_foreign_base.idata_binded;
    apf_idata = container_of(idata, typeof(*apf_idata), idata_base);
    
    return apf_idata;
}

static void* apf_idata_foreign_get_repl_ops(
    struct apf_instrument_data_foreign* apf_idata_foreign)
{
    return instrument_data_get_repl_operations(apf_idata_foreign->idata_foreign_base.idata_binded);
}

static const struct instrument_data_foreign_operations apf_idata_foreign_operations;
/************** Foreign 'at_place' instrumentation callbacks **********/
static struct instrument_data_foreign* apf_idata_ops_create_foreign_data(
    struct instrument_data* idata,
    struct kedr_coi_foreign_instrumentor* instrumentor_foreign)
{
    struct apf_instrument_data_foreign* apf_idata_foreign;
    struct apf_instrument_data* apf_idata;

    const struct kedr_coi_replacement* replacement;
    void* ops;

    apf_idata_foreign = kmalloc(sizeof(*apf_idata_foreign), GFP_ATOMIC);
    if(!apf_idata_foreign) return ERR_PTR(-ENOMEM);
    
    apf_idata = container_of(idata, typeof(*apf_idata), idata_base);
    ops = instrument_data_get_repl_operations(idata);
    
    apf_idata_foreign->instrumentor = instrumentor_foreign;
    
    instrument_data_foreign_init(&apf_idata_foreign->idata_foreign_base,
        idata, &apf_idata_foreign_operations);

    list_add_tail(&apf_idata_foreign->foreign_data_elem,
        &apf_idata->foreign_data_list);
    
    for_each_replacement(replacement, instrumentor_foreign->replacements)
    {
        size_t operation_offset = replacement->operation_offset;
        void** op_p = operation_at_offset_p(ops, operation_offset);
        void* op_orig = instrument_data_get_orig_operation(
            idata, operation_offset);
        replace_operation_generic(op_p, op_orig, replacement);
    }

    return &apf_idata_foreign->idata_foreign_base;
}

static struct instrument_data_foreign* apf_idata_ops_find_foreign_data(
    struct instrument_data* idata,
    struct kedr_coi_foreign_instrumentor* instrumentor_foreign)
{
    struct apf_instrument_data_foreign* apf_idata_foreign;
    struct apf_instrument_data* apf_idata;
    
    apf_idata = container_of(idata, typeof(*apf_idata), idata_base);
    
    list_for_each_entry(apf_idata_foreign, &apf_idata->foreign_data_list,
        foreign_data_elem)
    {
        if(apf_idata_foreign->instrumentor == instrumentor_foreign)
            return &apf_idata_foreign->idata_foreign_base;
    }

    return NULL;
}

static void* apf_idata_foreign_ops_get_repl_operations(
    struct instrument_data_foreign* idata_foreign)
{
    struct apf_instrument_data_foreign* apf_idata_foreign;
    
    apf_idata_foreign = container_of(idata_foreign,
        typeof(*apf_idata_foreign), idata_foreign_base);
    
    return apf_idata_foreign_get_repl_ops(apf_idata_foreign);
}

static void* apf_idata_foreign_ops_get_normal_operation(
    struct kedr_coi_foreign_instrumentor* instrumentor,
    struct instrument_data_foreign* idata_foreign,
    size_t operation_offset)
{
    struct apf_instrument_data* apf_idata;
    struct apf_instrument_data_foreign* apf_idata_foreign;
    struct kedr_coi_instrumentor* instrumentor_binded;
    const struct kedr_coi_replacement* replacement;
    void* op;
    
    apf_idata_foreign = container_of(idata_foreign,
        typeof(*apf_idata_foreign), idata_foreign_base);
    apf_idata = apf_idata_foreign_get_binded(apf_idata_foreign);
    instrumentor_binded = instrumentor->instrumentor_binded;
    
    // Take original operation...
    op = instrument_data_get_orig_operation(&apf_idata->idata_base,
        operation_offset);

    /*
     * TODO: Make fast path for the case "1 replacement for 1 foreign in chain".
     * This iteration is too expensive for regular execution.
     */
    for_each_replacement(replacement, instrumentor_binded->replacements)
    {
        if(replacement->operation_offset == operation_offset)
        {
            // And reconstruct normal replacement for it.
            replace_operation(&op, replacement);
            break;
        }
    }
    
    return op;
}

static void* apf_idata_foreign_ops_get_chain_operation(
    struct kedr_coi_foreign_instrumentor* instrumentor,
    struct instrument_data_foreign* idata_foreign,
    size_t operation_offset)
{
    struct apf_instrument_data* apf_idata;
    struct apf_instrument_data_foreign* apf_idata_foreign;
    struct kedr_coi_instrumentor* instrumentor_binded;
    const struct kedr_coi_replacement* replacement;
    void* op;
    void* op_orig;

    /* These variable refers other foreign data in chain. */
    struct apf_instrument_data_foreign* apf_idata_foreign_other;

    
    apf_idata_foreign = container_of(idata_foreign,
        typeof(*apf_idata_foreign), idata_foreign_base);
    apf_idata = apf_idata_foreign_get_binded(apf_idata_foreign);
    instrumentor_binded = instrumentor->instrumentor_binded;
    
    op_orig = instrument_data_get_orig_operation(&apf_idata->idata_base,
        operation_offset);
    // Take original operation...
    op = op_orig;
    
    apf_idata_foreign_other = apf_idata_foreign;
    list_for_each_entry_continue_reverse(apf_idata_foreign_other,
        &apf_idata->foreign_data_list, foreign_data_elem)
    {
        for_each_replacement(replacement, apf_idata_foreign_other->instrumentor->replacements)
        {
            if(replacement->operation_offset == operation_offset)
            {
                // .. and replace it according to repacement for previous foreign.
                replace_operation(&op, replacement);
                // Changed? Should chain to that foreign.
                if(op != op_orig) return op;
            }
        }

    }
    
    // No foreign change operation? Chain to original.
    return op_orig;
}

/* Common implementation for .destroy* methods*/
static void apf_idata_foreign_ops_destroy_common(
    struct kedr_coi_foreign_instrumentor* instrumentor,
    struct instrument_data_foreign* idata_foreign,
    bool norestore)
{
    struct apf_instrument_data_foreign* apf_idata_foreign;
    struct apf_instrument_data* apf_idata;
    struct kedr_coi_instrumentor* instrumentor_binded = 
        instrumentor->instrumentor_binded;
    
    void* ops;
    void* ops_orig;
    const struct kedr_coi_replacement* replacement;

    apf_idata_foreign = container_of(idata_foreign,
        typeof(*apf_idata_foreign), idata_foreign_base);

    apf_idata = apf_idata_foreign_get_binded(apf_idata_foreign);
    ops = apf_idata_foreign_get_repl_ops(apf_idata_foreign);
    ops_orig = instrument_data_get_orig_operations(&apf_idata->idata_base);
    
    /* 
     * 'norestore' flag prevents from restoring operations struct only
     * when if it is original one.
     */
    if(norestore && (ops_orig == ops)) goto out;
    
    for_each_replacement(replacement, instrumentor->replacements)
    {
        /* These variables refers other foreign data in chain. */
        struct apf_instrument_data_foreign* apf_idata_foreign_other;
        const struct kedr_coi_replacement* replacement_other;
        
        void* op_orig;
        
        size_t operation_offset = replacement->operation_offset;

        void** op_p = operation_at_offset_p(ops, operation_offset);
        
        /* 
         * Do not touch operations which are not replaced by us.
         */
        if(*op_p != replacement->repl) continue;
        
        /* 
         * Otherwise search in foreign data chain, which operation
         * we replaced in init.
         */
        apf_idata_foreign_other = apf_idata_foreign;
        op_orig = instrument_data_get_orig_operation(&apf_idata->idata_base, operation_offset);

        list_for_each_entry_continue_reverse(apf_idata_foreign_other,
            &apf_idata->foreign_data_list, foreign_data_elem)
        {
            for_each_replacement(replacement_other, apf_idata_foreign_other->instrumentor->replacements)
            {
                if(replacement_other->operation_offset == operation_offset)
                {
                    // Repeat replacement...
                    replace_operation_generic(op_p, op_orig,
                        replacement_other);
                    // (Normally, there is only one foreign replacement. Do not check here.)
                }
            }
            // ..and check, whether operation has changed.
            if(*op_p != replacement->repl) break;
        }
        if(*op_p != replacement->repl) continue;
        
        /*
         * Do the same for normal interception data.
         */
        for_each_replacement(replacement_other, instrumentor_binded->replacements)
        {
            if(replacement_other->operation_offset == operation_offset)
            {
                // Repeat replacement...
                replace_operation_generic(op_p, op_orig,
                    replacement_other);
                // ..and check, whether operation has changed.
                if(*op_p != replacement->repl) break;
            }
        }
        
        if(*op_p == replacement->repl)
        {
            // Finally set operation to original.
            *op_p = op_orig;
        }
    }
out:    
    list_del(&apf_idata_foreign->foreign_data_elem);
    if(!norestore)
        instrument_data_unref(instrumentor_binded,
            &apf_idata->idata_base);
    else
        instrument_data_unref_norestore(instrumentor_binded,
            &apf_idata->idata_base);
    
    kfree(apf_idata_foreign);
}

static void apf_idata_foreign_ops_destroy(
    struct kedr_coi_foreign_instrumentor* instrumentor,
    struct instrument_data_foreign* idata_foreign)
{
    apf_idata_foreign_ops_destroy_common(instrumentor, idata_foreign, 0);
}

static void apf_idata_foreign_ops_destroy_norestore(
    struct kedr_coi_foreign_instrumentor* instrumentor,
    struct instrument_data_foreign* idata_foreign)
{
    apf_idata_foreign_ops_destroy_common(instrumentor, idata_foreign, 1);
}

/************** Foreign 'at_place' instrumentation(operations structs) ********/

static const struct instrument_data_foreign_operations apf_idata_foreign_operations =
{
    .get_repl_operations = apf_idata_foreign_ops_get_repl_operations,
    .get_normal_operation = apf_idata_foreign_ops_get_normal_operation,
    .get_chain_operation = apf_idata_foreign_ops_get_chain_operation,
    .destroy_idata_foreign = apf_idata_foreign_ops_destroy,
    .destroy_idata_foreign_norestore = apf_idata_foreign_ops_destroy_norestore
};

/****************** 'at_place' instrumentation ************************/
struct ap_instrument_data
{
    struct apf_instrument_data apf_idata_base;
    
    /* Search data by (original) operations. */
    struct instrument_data_search ops_elem;
    
    /* Copy of original operations. */
    void* ops_orig;
};

static const struct instrument_data_operations ap_idata_operations;


/*
 * Restore one operation if it was replaced.
 */
static void restore_operation(void** op_p,
    const struct kedr_coi_replacement* replacement,
    void* op_orig)
{
#ifdef KEDR_COI_DEBUG
    if((replacement->mode == replace_not_null) && !op_orig);
    else if((replacement->mode == replace_null) && op_orig);
    else if(replacement->repl != *op_p)
    {
        pr_info("Operation was changed externally from %pF to %pF.",
            replacement->repl, *op_p);
    }
#endif /* KEDR_COI_DEBUG */
    if(replacement->repl == *op_p) *op_p = op_orig;
}

static inline struct instrument_data*
ap_idata_to_idata(struct ap_instrument_data* ap_idata)
{
    return &ap_idata->apf_idata_base.idata_base;
}

static inline struct ap_instrument_data*
ap_idata_from_idata(struct instrument_data* idata)
{
    struct ap_instrument_data* ap_idata;
    struct apf_instrument_data* apf_idata;
    
    apf_idata = container_of(idata, typeof(*apf_idata), idata_base);
    ap_idata = container_of(apf_idata, typeof(*ap_idata), apf_idata_base);

    return ap_idata;
}

// Return both operations, replacement and original ones.
static void* ap_idata_get_operations(struct ap_instrument_data* ap_idata)
{
    return instrument_data_search_get_ops(&ap_idata->ops_elem);
}

static struct instrument_data* ap_idata_create(
    struct kedr_coi_instrumentor* instrumentor, const void* ops)
{
    //TODO: process case when ops is NULL.
    int err = -ENOMEM;
    struct ap_instrument_data* ap_idata;
    const struct kedr_coi_replacement* replacement;
    
    ap_idata = kmalloc(sizeof(*ap_idata), GFP_ATOMIC);
    if(!ap_idata) goto fail;

    ap_idata->ops_orig = kmalloc(instrumentor->operations_struct_size,
        GFP_ATOMIC);
    if(!ap_idata->ops_orig) goto fail_alloc_orig;

    instrument_data_search_init(&ap_idata->ops_elem, ap_idata_to_idata(ap_idata), ops);
    err = instrumentor_add_data_search(instrumentor, &ap_idata->ops_elem);
    if(err) goto fail_add_ops;

    apf_instrument_data_init(&ap_idata->apf_idata_base, &ap_idata_operations);

    memcpy(ap_idata->ops_orig, ops, instrumentor->operations_struct_size);

    for_each_replacement(replacement, instrumentor->replacements)
    {
        void** op_p;
        op_p = operation_at_offset_p((void**)ops, replacement->operation_offset);
        replace_operation(op_p, replacement);
    }

    
    return ap_idata_to_idata(ap_idata);

fail_add_ops:
    kfree(ap_idata->ops_orig);
fail_alloc_orig:
    kfree(ap_idata);
fail:
    return ERR_PTR(err);
}

static void ap_idata_destroy_norestore(
    struct kedr_coi_instrumentor* instrumentor,
    struct ap_instrument_data* ap_idata)
{
    instrumentor_remove_data_search(instrumentor, &ap_idata->ops_elem);
    
    kfree(ap_idata->ops_orig);
    kfree(ap_idata);
}

static void ap_idata_destroy(
    struct kedr_coi_instrumentor* instrumentor,
    struct ap_instrument_data* ap_idata)
{
    const struct kedr_coi_replacement* replacement;
    void* ops = ap_idata_get_operations(ap_idata);

    for_each_replacement(replacement, instrumentor->replacements)
    {
        void* op_orig;
        void** op_p;
        
        op_p = operation_at_offset_p(ops, replacement->operation_offset);
        op_orig = operation_at_offset(ap_idata->ops_orig,
            replacement->operation_offset);
        
        restore_operation(op_p, replacement, op_orig);
    }

    ap_idata_destroy_norestore(instrumentor, ap_idata);
}

/************** 'at_place' instrumentation callbacks(normal) **********/
static void ap_idata_ops_destroy(
    struct kedr_coi_instrumentor* instrumentor,
    struct instrument_data* idata)
{
    struct ap_instrument_data* ap_idata = ap_idata_from_idata(idata);
    ap_idata_destroy(instrumentor, ap_idata);
}

static void ap_idata_ops_destroy_norestore(
    struct kedr_coi_instrumentor* instrumentor,
    struct instrument_data* idata)
{
    struct ap_instrument_data* ap_idata = ap_idata_from_idata(idata);
    ap_idata_destroy_norestore(instrumentor, ap_idata);
}

// Return both operations, replacement and original ones.
static void* ap_idata_ops_get_operations(struct instrument_data* idata)
{
    return ap_idata_get_operations(ap_idata_from_idata(idata));
}


static void* ap_idata_ops_get_orig_operation(struct instrument_data* idata,
    size_t operation_offset)
{
    struct ap_instrument_data* ap_idata = ap_idata_from_idata(idata);
    
    return operation_at_offset(ap_idata->ops_orig, operation_offset);
}

static bool ap_idata_ops_my_operations(struct instrument_data* idata,
    const void* ops)
{
    struct ap_instrument_data* ap_idata = ap_idata_from_idata(idata);
    
    return ap_idata_get_operations(ap_idata) == ops;
}

/************** 'at_place' instrumentation(operations structs) ********/
static const struct instrument_data_operations ap_idata_operations =
{
    .get_repl_operations = &ap_idata_ops_get_operations,
    .get_orig_operations = &ap_idata_ops_get_operations,
    .get_orig_operation = &ap_idata_ops_get_orig_operation,
    .my_operations = &ap_idata_ops_my_operations,
    .destroy_idata = &ap_idata_ops_destroy,
    .destroy_idata_norestore = &ap_idata_ops_destroy_norestore,
    .create_foreign_data = &apf_idata_ops_create_foreign_data,
    .find_foreign_data = &apf_idata_ops_find_foreign_data,
};

/************** 'use_copy' instrumentation ****************************/
struct uc_instrument_data
{
    struct apf_instrument_data apf_idata_base;
    
    /* Search data by original operations. */
    struct instrument_data_search ops_orig_elem;
    
    /* Search data by replaced operations. */
    struct instrument_data_search ops_repl_elem;
};

static const struct instrument_data_operations uc_idata_operations;

static inline struct instrument_data*
uc_idata_to_idata(struct uc_instrument_data* uc_idata)
{
    return &uc_idata->apf_idata_base.idata_base;
}

static inline struct uc_instrument_data*
uc_idata_from_idata(struct instrument_data* idata)
{
    struct uc_instrument_data* uc_idata;
    struct apf_instrument_data* apf_idata;
    
    apf_idata = container_of(idata, typeof(*apf_idata), idata_base);
    uc_idata = container_of(apf_idata, typeof(*uc_idata), apf_idata_base);

    return uc_idata;
}

static struct instrument_data* uc_idata_create(
    struct kedr_coi_instrumentor* instrumentor,
    const void* ops)
{
    int err = -ENOMEM;
    void* ops_repl;
    const struct kedr_coi_replacement* replacement;
    struct uc_instrument_data* uc_idata;
    
    uc_idata = kmalloc(sizeof(*uc_idata), GFP_ATOMIC);
    if(!uc_idata) goto fail;
    
    ops_repl = kmalloc(instrumentor->operations_struct_size,
        GFP_ATOMIC);
    if(!ops_repl) goto fail_alloc_ops;
    
    if(ops)
        memcpy(ops_repl, ops, instrumentor->operations_struct_size);
    else
        memset(ops_repl, 0, instrumentor->operations_struct_size);
    
    instrument_data_search_init(&uc_idata->ops_orig_elem,
        uc_idata_to_idata(uc_idata), ops);
    err = instrumentor_add_data_search(instrumentor,
        &uc_idata->ops_orig_elem);
    if(err) goto fail_add_ops_orig;

    instrument_data_search_init(&uc_idata->ops_repl_elem,
        uc_idata_to_idata(uc_idata), ops_repl);
    err = instrumentor_add_data_search(instrumentor,
        &uc_idata->ops_repl_elem);
    if(err) goto fail_add_ops_repl;

    apf_instrument_data_init(&uc_idata->apf_idata_base, &uc_idata_operations);
    
    for_each_replacement(replacement, instrumentor->replacements)
    {
        void** op_p = operation_at_offset_p(ops_repl, replacement->operation_offset);
        replace_operation(op_p, replacement);
    }
    
    return uc_idata_to_idata(uc_idata);

fail_add_ops_repl:
    instrumentor_remove_data_search(instrumentor, &uc_idata->ops_orig_elem);
fail_add_ops_orig:
    kfree(ops_repl);
fail_alloc_ops:
    kfree(uc_idata);
fail:    
    return ERR_PTR(err);
}

// Both normal and 'norestore' variants.
static void uc_idata_destroy(
    struct kedr_coi_instrumentor* instrumentor,
    struct uc_instrument_data* uc_idata)
{
    void* ops_repl = instrument_data_search_get_ops(&uc_idata->ops_repl_elem);

    instrumentor_remove_data_search(instrumentor, &uc_idata->ops_repl_elem);
    instrumentor_remove_data_search(instrumentor, &uc_idata->ops_orig_elem);
    kfree(ops_repl);
    kfree(uc_idata);
}

/************** 'use_copy' instrumentation callbacks(normal) **********/
// Both normal and 'norestore' variants.
static void uc_idata_ops_destroy(
    struct kedr_coi_instrumentor* instrumentor,
    struct instrument_data* idata)
{
    struct uc_instrument_data* uc_idata = uc_idata_from_idata(idata);
    uc_idata_destroy(instrumentor, uc_idata);
}

static void* uc_idata_ops_get_orig_operations(struct instrument_data* idata)
{
    struct uc_instrument_data* uc_idata = uc_idata_from_idata(idata);
    return instrument_data_search_get_ops(&uc_idata->ops_orig_elem);
}

static void* uc_idata_ops_get_repl_operations(struct instrument_data* idata)
{
    struct uc_instrument_data* uc_idata = uc_idata_from_idata(idata);
    return instrument_data_search_get_ops(&uc_idata->ops_repl_elem);
}


static void* uc_idata_ops_get_orig_operation(struct instrument_data* idata,
    size_t operation_offset)
{
    void* ops_orig;
    struct uc_instrument_data* uc_idata = uc_idata_from_idata(idata);

    ops_orig = instrument_data_search_get_ops(&uc_idata->ops_orig_elem);

    return ops_orig
        ? operation_at_offset(ops_orig, operation_offset)
        : NULL;
}

static bool uc_idata_ops_my_operations(struct instrument_data* idata,
    const void* ops)
{
    void* ops_my;
    struct uc_instrument_data* uc_idata = uc_idata_from_idata(idata);

    ops_my = instrument_data_search_get_ops(&uc_idata->ops_repl_elem);
    if(ops_my == ops) return 1;

    ops_my = instrument_data_search_get_ops(&uc_idata->ops_orig_elem);
    if(ops_my == ops) return 1;

    return 0;
}

/************** 'use_copy' instrumentation(operations structs) ********/
static const struct instrument_data_operations uc_idata_operations =
{
    .get_repl_operations = &uc_idata_ops_get_repl_operations,
    .get_orig_operations = &uc_idata_ops_get_orig_operations,
    .get_orig_operation = &uc_idata_ops_get_orig_operation,
    .my_operations = &uc_idata_ops_my_operations,
    .destroy_idata = &uc_idata_ops_destroy,
    .destroy_idata_norestore = &uc_idata_ops_destroy,
    .create_foreign_data = &apf_idata_ops_create_foreign_data,
    .find_foreign_data = &apf_idata_ops_find_foreign_data
};

/********************* instrumentor_get_data **************************/
/* 
 * Return referenced 'instrument_data' object for given ops.
 * 
 * 'instrument_data' object will be created if needed.
 * 
 * On error, return ERR_PTR().
 */
struct instrument_data* instrumentor_get_data(
    struct kedr_coi_instrumentor* instrumentor,
    const void* ops)
{
    struct instrument_data* idata;
    
    idata = instrumentor_find_data(instrumentor, ops);
    if(idata)
    {
        instrument_data_ref(idata);
        return idata;
    }
    
    if(ops)
    {
        if(instrumentor->replace_at_place(ops))
            idata = ap_idata_create(instrumentor, ops);
        else
            idata = uc_idata_create(instrumentor, ops);
    }
    else
    {
        idata = uc_idata_create(instrumentor, ops);
    }
    
    return idata;
}

/********************** Global functions ******************************/
int kedr_coi_instrumentors_init(void)
{
    int err = kedr_coi_hash_table_init(&ops_table_global);
    if(err) return err;
    
    spin_lock_init(&ops_table_global_lock);
    
    return 0;
}

/*
 * This function should be called when instrumentors are not needed.
 */
void kedr_coi_instrumentors_destroy(void)
{
    kedr_coi_hash_table_destroy(&ops_table_global, NULL, NULL);
}
