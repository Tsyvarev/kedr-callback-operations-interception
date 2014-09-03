/*
 * Implementation of instrumentors.
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

/* @ops shouldn't be NULL. */
static void* operation_at_offset(const void* ops, size_t operation_offset)
{
    return *((void**)((const char*)ops + operation_offset));
}

static struct kedr_coi_replacement empty_replacements[] = {
    {
        .operation_offset = -1
    }
};
//************* Normal instrumentor *****************************
// Auxiliary functions
/* 
 * Return watch data for given object.
 * If object is not watched, return NULL.
 */
static struct kedr_coi_instrumentor_watch_data* instrumentor_find_watch_data(
    struct kedr_coi_instrumentor* instrumentor, const void* object)
{
    struct kedr_coi_hash_elem* elem;
    struct kedr_coi_instrumentor_watch_data* watch_data;
    
    elem = kedr_coi_hash_table_find_elem(
        &instrumentor->objects_table, object);

    if(elem)
        watch_data = container_of(elem, typeof(*watch_data), object_elem);
    else
        watch_data = NULL;
    
    return watch_data;
}

static void instrumentor_destroy_watch_data(
    struct kedr_coi_instrumentor* instrumentor,
    struct kedr_coi_instrumentor_watch_data* watch_data)
{
    instrument_data_unref(instrumentor, watch_data->idata);
    
    kedr_coi_hash_table_remove_elem(&instrumentor->objects_table,
        &watch_data->object_elem);
    
    kfree(watch_data);
}

static void instrumentor_destroy_watch_data_norestore(
    struct kedr_coi_instrumentor* instrumentor,
    struct kedr_coi_instrumentor_watch_data* watch_data)
{
    instrument_data_unref_norestore(instrumentor, watch_data->idata);
    
    kedr_coi_hash_table_remove_elem(&instrumentor->objects_table,
        &watch_data->object_elem);
    
    kfree(watch_data);
}

/* 
 * Apply instrumentation, that is set operations to the replacement ones.
 * 
 * Do nothing for 'at_place' instrumentation mechanism.
 */
static void instrument_data_replace_ops(struct instrument_data* idata,
    const void** ops_p)
{
    void* ops_repl = instrument_data_get_repl_operations(idata);
    if(ops_repl != *ops_p)
        *ops_p = ops_repl;
}

/* 
 * Revert instrumentation, that is set operations to the original ones.
 * 
 * Do nothing for 'at_place' instrumentation mechanism.
 */
static void instrument_data_restore_ops(struct instrument_data* idata,
    const void** ops_p)
{
    void* ops_orig = instrument_data_get_orig_operations(idata);
    if(ops_orig != *ops_p)
        *ops_p = ops_orig;
}

struct instrument_data* instrumentor_find_data(
    struct kedr_coi_instrumentor* instrumentor,
    const void* ops)
{
    struct kedr_coi_hash_elem* elem;
    struct instrument_data_search* idata_search;
    struct instrument_data* idata;
    
    elem = kedr_coi_hash_table_find_elem(
        &instrumentor->idata_table, ops);

    if(elem)
    {
        idata_search = container_of(elem, typeof(*idata_search), ops_elem);
        idata = idata_search->idata;
    }
    else
    {
        idata = NULL;
    }

    return idata;
}

void* instrument_data_get_repl_operations(struct instrument_data* idata)
{
    return idata->i_ops->get_repl_operations(idata);
}

void* instrument_data_get_orig_operations(struct instrument_data* idata)
{
    return idata->i_ops->get_orig_operations(idata);
}

void* instrument_data_get_orig_operation(struct instrument_data* idata,
    size_t operation_offset)
{
    return idata->i_ops->get_orig_operation(idata, operation_offset);
}

bool instrument_data_my_operations(struct instrument_data* idata,
    const void* ops)
{
    return idata->i_ops->my_operations(idata, ops);
}


void instrument_data_ref(struct instrument_data* idata)
{
    idata->refs++;
}

void instrument_data_unref(
    struct kedr_coi_instrumentor* instrumentor,
    struct instrument_data* idata)
{
    if(--idata->refs == 0)
    {
        idata->i_ops->destroy_idata(instrumentor, idata);
    }
}

void instrument_data_unref_norestore(
    struct kedr_coi_instrumentor* instrumentor,
    struct instrument_data* idata)
{
    if(--idata->refs == 0)
    {
        idata->i_ops->destroy_idata_norestore(instrumentor, idata);
    }
}

/*
 * Called by kedr_coi_instrumentor_get_orig_operation when no
 * instrument data are found.
 * 
 * Return original operation in that case(which may be NULL).
 * 
 * Return PTR_ERR() in case of error.
 */
static void* instrumentor_get_orig_operation_nodata(
    struct kedr_coi_instrumentor* instrumentor,
    const void* ops,
    size_t operation_offset)
{
    /* 
     * Options are not instrumented by us, or, more likely,
     * our instrumentation is just removed without restoring.
     * 
     * Verify operation at given offset. If it is not
     * coincide with any of our replacements, return it.
     * 
     * Otherwise return error. It is better than infinite recursion, which
     * would occure if we return operation itself, and it will be called
     * again by the caller of kedr_coi_instrumentor_get_orig_operation().
     */
    const struct kedr_coi_replacement* replacement;
    void* op = operation_at_offset(ops, operation_offset);

    for_each_replacement(replacement, instrumentor->replacements)
    {
        if(replacement->repl == op)
        {
            pr_err("Original for replacement operation %pF has lost.", op);
            return ERR_PTR(-EINVAL);
        }
    }
    return op;
}

/* Called under lock. */
static int instrumentor_watch_internal(
    struct kedr_coi_instrumentor* instrumentor,
    void* object,
    const void** ops_p)
{
    int err;
    struct instrument_data* idata;
    struct kedr_coi_instrumentor_watch_data* watch_data;
    
    watch_data = instrumentor_find_watch_data(instrumentor, object);

    if(watch_data)
    {
        // Update replacement
        idata = watch_data->idata;
        if(!instrument_data_my_operations(idata, *ops_p))
        {
            //Need to change instrument data
            idata = instrumentor_get_data(instrumentor, *ops_p);
            if(IS_ERR(idata))
            {
                /*
                 * Forget watch in case of unsuccessfull update.
                 * Watch has no sence if we fail to change operations pointer.
                 */
                instrument_data_restore_ops(watch_data->idata, ops_p);
                instrumentor_destroy_watch_data(instrumentor, watch_data);

                return PTR_ERR(idata);
            }
            instrument_data_unref(instrumentor, watch_data->idata);
            watch_data->idata = idata;
        }
        
        instrument_data_replace_ops(idata, ops_p);
        return 1;
    }
    // Create new watch
    idata = instrumentor_get_data(instrumentor, *ops_p);
    if(IS_ERR(idata))
    {
        return PTR_ERR(idata);
    }
    
    err = -ENOMEM;
    watch_data = kmalloc(sizeof(*watch_data), GFP_ATOMIC);
    if(watch_data == NULL) goto fail_alloc_watch_data;

    watch_data->idata = idata;
    kedr_coi_hash_elem_init(&watch_data->object_elem, object);
    err = kedr_coi_hash_table_add_elem(
        &instrumentor->objects_table, &watch_data->object_elem);
    if(err) goto fail_add_object_elem;

    instrument_data_replace_ops(watch_data->idata, ops_p);
    return 0;

fail_add_object_elem:
    kfree(watch_data);

fail_alloc_watch_data:
    instrument_data_unref(instrumentor, idata);
    return err;
}
//*************API for normal instrumentor*************************
struct kedr_coi_instrumentor* kedr_coi_instrumentor_create(
    size_t operations_struct_size,
    const struct kedr_coi_replacement* replacements,
    bool (*replace_at_place)(const void* ops))
{
    int err;
    struct kedr_coi_instrumentor* instrumentor = kmalloc(sizeof(*instrumentor),
        GFP_KERNEL);
    if(instrumentor == NULL) return NULL;
    
    err = kedr_coi_hash_table_init(&instrumentor->objects_table);
    if(err) goto fail_objects_table_init;

    err = kedr_coi_hash_table_init(&instrumentor->idata_table);
    if(err) goto fail_idata_table_init;
    
    err = kedr_coi_hash_table_init(&instrumentor->foreign_ops_p_table);
    if(err) goto fail_foreign_ops_p_table_init;
    
    instrumentor->operations_struct_size = operations_struct_size;
    instrumentor->replacements = replacements
        ? replacements
        : empty_replacements;
    instrumentor->replace_at_place = replace_at_place;
    
    spin_lock_init(&instrumentor->lock);

    return instrumentor;

fail_foreign_ops_p_table_init:
    kedr_coi_hash_table_destroy(&instrumentor->idata_table, NULL, NULL);
fail_idata_table_init:
    kedr_coi_hash_table_destroy(&instrumentor->objects_table, NULL, NULL);

fail_objects_table_init:
    kfree(instrumentor);
    return NULL;
}

struct instrumentor_destroy_data
{
    struct kedr_coi_instrumentor* instrumentor;
    trace_unforgotten_watch_t trace_unforgotten_watch;
    void* user_data;
};

static void instrumentor_destroy_watch_data_callback(
    struct kedr_coi_hash_elem* elem,
    void* user_data)
{
    const void* object = elem->key;
    struct kedr_coi_instrumentor_watch_data* watch_data = 
        container_of(elem, typeof(*watch_data), object_elem);
    struct instrumentor_destroy_data* destroy_data = user_data;
    
    instrument_data_unref(destroy_data->instrumentor, watch_data->idata);
    
    kfree(watch_data);
    
    if(destroy_data->trace_unforgotten_watch)
        destroy_data->trace_unforgotten_watch(object, destroy_data->user_data);
}

void kedr_coi_instrumentor_destroy(struct kedr_coi_instrumentor* instrumentor,
    void (*trace_unforgotten_watch)(const void* object, void* user_data),
    void* user_data)
{
    struct instrumentor_destroy_data destroy_data =
    {
        .instrumentor = instrumentor,
        .trace_unforgotten_watch = trace_unforgotten_watch,
        .user_data = user_data
    };
    
    kedr_coi_hash_table_destroy(&instrumentor->objects_table,
        &instrumentor_destroy_watch_data_callback, &destroy_data);

    kedr_coi_hash_table_destroy(&instrumentor->foreign_ops_p_table,
        NULL, NULL);

    kedr_coi_hash_table_destroy(&instrumentor->idata_table, NULL, NULL);
    
    kfree(instrumentor);
}

int kedr_coi_instrumentor_watch(
    struct kedr_coi_instrumentor* instrumentor,
    void* object,
    const void** ops_p)
{
    unsigned long flags;
    int err;
    
    spin_lock_irqsave(&instrumentor->lock, flags);
    err = instrumentor_watch_internal(instrumentor, object, ops_p);
    spin_unlock_irqrestore(&instrumentor->lock, flags);

    return err;
}


int kedr_coi_instrumentor_forget(
    struct kedr_coi_instrumentor* instrumentor,
    void* object,
    const void** ops_p)
{
    unsigned long flags;
    int err = 0;
    struct kedr_coi_instrumentor_watch_data* watch_data;
    struct instrument_data* idata;
    
    spin_lock_irqsave(&instrumentor->lock, flags);
    
    watch_data = instrumentor_find_watch_data(instrumentor, object);
    if(watch_data)
    {
        idata = watch_data->idata;
        
        if(ops_p && instrument_data_my_operations(idata, *ops_p))
            instrument_data_restore_ops(idata, ops_p);
        
        instrumentor_destroy_watch_data(instrumentor, watch_data);
    }
    else
    {
        err = 1; //Not watched
    }
    
    spin_unlock_irqrestore(&instrumentor->lock, flags);
    
    return err;
}

int kedr_coi_instrumentor_get_orig_operation(
    struct kedr_coi_instrumentor* instrumentor,
    const void* object,
    const void* ops,
    size_t operation_offset,
    void** op_orig)
{
    unsigned long flags;
    int err = 0;
    struct kedr_coi_instrumentor_watch_data* watch_data;
    struct instrument_data* idata;

    spin_lock_irqsave(&instrumentor->lock, flags);
    
    watch_data = instrumentor_find_watch_data(instrumentor, object);
    if(watch_data)
    {
        idata = watch_data->idata;
        *op_orig = idata->i_ops->get_orig_operation(idata, operation_offset);
    }
    else
    {
        err = 1; //Not watched
        idata = instrumentor_find_data(instrumentor, ops);
        if(idata == NULL)
        {
            *op_orig = instrumentor_get_orig_operation_nodata(
                instrumentor, ops, operation_offset);
            if(IS_ERR(*op_orig))
                err = PTR_ERR(*op_orig);
        }
        else
        {
            *op_orig = instrument_data_get_orig_operation(
                idata, operation_offset);
        }
    }
    
    spin_unlock_irqrestore(&instrumentor->lock, flags);
    
    return err;
}
/* 
 * Similar methods, but for directly watched object, which is also a
 * container of operations.
 */
int kedr_coi_instrumentor_watch_direct(
    struct kedr_coi_instrumentor* instrumentor,
    void* object)
{
    //TODO: Ignore seatch idata by ops.
    return kedr_coi_instrumentor_watch(instrumentor, object, (const void**)&object);
}

int kedr_coi_instrumentor_forget_direct(
    struct kedr_coi_instrumentor* instrumentor,
    void* object,
    bool norestore)
{
    /* 
     * Difference from indirect instrumentor's method is that
     * in case of norestore watch data are destroyed in 'norestore' mode
     * also.
     */
    unsigned long flags;
    int err = 0;
    struct kedr_coi_instrumentor_watch_data* watch_data;
    struct instrument_data* idata;
    
    spin_lock_irqsave(&instrumentor->lock, flags);
    
    watch_data = instrumentor_find_watch_data(instrumentor, object);
    if(watch_data)
    {
        idata = watch_data->idata;
        
        if(!norestore) 
        {
            // On direct instrumentation operations cannot be changed outside.
            instrument_data_restore_ops(idata, (const void**)&object);
            instrumentor_destroy_watch_data(instrumentor, watch_data);
        }
        else
        {
            instrumentor_destroy_watch_data_norestore(instrumentor, watch_data);
        }
    }
    else
    {
        err = 1; //Not watched
    }
    
    spin_unlock_irqrestore(&instrumentor->lock, flags);
    
    return err;

}


//**************Foreign instrumentor************************//
void* instrument_data_foreign_get_repl_operations(
    struct instrument_data_foreign* idata_foreign)
{
    return idata_foreign->fi_ops->get_repl_operations(
        idata_foreign);
}

void instrument_data_foreign_ref(
    struct instrument_data_foreign* idata_foreign)
{
    idata_foreign->refs++;
}

void instrument_data_foreign_unref(
    struct kedr_coi_foreign_instrumentor* instrumentor,
    struct instrument_data_foreign* idata_foreign)
{
    if(--idata_foreign->refs == 0)
    {
        idata_foreign->fi_ops->destroy_idata_foreign(
            instrumentor, idata_foreign);
    }
}

void instrument_data_foreign_unref_norestore(
    struct kedr_coi_foreign_instrumentor* instrumentor,
    struct instrument_data_foreign* idata_foreign)
{
    if(--idata_foreign->refs == 0)
    {
        idata_foreign->fi_ops->destroy_idata_foreign_norestore(
            instrumentor, idata_foreign);
    }
}

struct instrument_data_foreign* instrumentor_foreign_find_data(
    struct kedr_coi_foreign_instrumentor* instrumentor,
    const void* ops)
{
    struct instrument_data_foreign* idata_foreign = NULL;
    struct instrument_data* idata;

    struct kedr_coi_instrumentor* instrumentor_binded =
        instrumentor->instrumentor_binded;
    
    idata = instrumentor_find_data(instrumentor_binded, ops);
    if(idata)
    {
        idata_foreign = idata->i_ops->find_foreign_data(idata, instrumentor);
    }

    return idata_foreign;
}

struct instrument_data_foreign* instrumentor_foreign_get_data(
    struct kedr_coi_foreign_instrumentor* instrumentor,
    const void* ops)
{
    struct instrument_data* idata;
    struct kedr_coi_instrumentor* instrumentor_binded;
    struct instrument_data_foreign* idata_foreign;
    
    idata_foreign = instrumentor_foreign_find_data(instrumentor, ops);
    if(idata_foreign)
    {
        instrument_data_foreign_ref(idata_foreign);
        return idata_foreign;
    }
    
    instrumentor_binded = instrumentor->instrumentor_binded;
    idata = instrumentor_get_data(instrumentor_binded, ops);
    if(IS_ERR(idata)) return (void*)idata;
    
    idata_foreign = idata->i_ops->create_foreign_data(idata, instrumentor);
    if(IS_ERR(idata_foreign))
    {
        instrument_data_unref(instrumentor_binded, idata);
    }
    
    return idata_foreign;
}

/* 
 * Return watch data for given id.
 * If object is not watched, return NULL.
 */
static struct kedr_coi_foreign_instrumentor_watch_data*
instrumentor_foreign_find_watch_data_by_id(
    struct kedr_coi_foreign_instrumentor* instrumentor,
    const void* id)
{
    struct kedr_coi_foreign_instrumentor_watch_data* watch_data;
    struct kedr_coi_hash_elem* elem;
    
    elem = kedr_coi_hash_table_find_elem(&instrumentor->ids_table, id);
    if(elem)
        watch_data = container_of(elem, typeof(*watch_data), id_elem);
    else
        watch_data = NULL;
    
    return watch_data;
}

/* 
 * Return watch data for given tie.
 * If object is not watched, return NULL.
 */
static struct kedr_coi_foreign_instrumentor_watch_data*
instrumentor_foreign_find_watch_data_by_tie(
    struct kedr_coi_foreign_instrumentor* instrumentor,
    const void* tie)
{
    struct kedr_coi_foreign_instrumentor_watch_data* watch_data;
    struct kedr_coi_hash_elem* elem;
    
    elem = kedr_coi_hash_table_find_elem(&instrumentor->ties_table, tie);
    if(elem)
        watch_data = container_of(elem, typeof(*watch_data), tie_elem);
    else
        watch_data = NULL;
    
    return watch_data;
}


static void instrumentor_foreign_destroy_watch_data(
    struct kedr_coi_foreign_instrumentor* instrumentor,
    struct kedr_coi_foreign_instrumentor_watch_data* watch_data)
{
    instrument_data_foreign_unref(instrumentor,
        watch_data->idata_foreign);
    
    kedr_coi_hash_table_remove_elem(&instrumentor->ids_table,
        &watch_data->id_elem);
    
    kedr_coi_hash_table_remove_elem(&instrumentor->ties_table,
        &watch_data->tie_elem);
    
    kedr_coi_hash_table_remove_elem(
        &instrumentor->instrumentor_binded->foreign_ops_p_table,
        &watch_data->ops_p_elem);
    
    kfree(watch_data);
}

void instrument_data_foreign_replace_ops(
    struct instrument_data_foreign* idata_foreign,
    const void** ops_p)
{
    const void* ops_repl =
        instrument_data_foreign_get_repl_operations(idata_foreign);
    if(ops_repl != *ops_p) *ops_p = ops_repl;
}


/*
 * Return normal operation at given offset.
 * 
 * It may be a replacement of the normal instrumentor, or original operation
 * iself (which, in turn, may be NULL).
 * 
 * This value is used for @op_chained, if object becomes watched
 * or is watched already.
 */
void* instrument_data_foreign_normal_operation(
    struct kedr_coi_foreign_instrumentor* instrumentor,
    struct instrument_data_foreign* idata_foreign,
    size_t operation_offset)
{
    return idata_foreign->fi_ops->get_normal_operation(
        instrumentor,
        idata_foreign,
        operation_offset);
}

/*
 * Chain operation at given offset.
 * 
 * It may be a replacement of the normal instrumentor, or original operation
 * iself (which, in turn, may be NULL).
 * 
 * This value is used for @op_chained when 'bind_with_factory' path shouldn't
 * bind object.
 */
void* instrument_data_foreign_chain_operation(
    struct kedr_coi_foreign_instrumentor* instrumentor,
    struct instrument_data_foreign* idata_foreign,
    size_t operation_offset)
{
    return idata_foreign->fi_ops->get_chain_operation(
        instrumentor,
        idata_foreign,
        operation_offset);
}

/*
 * Called by kedr_coi_foreign_instrumentor_bind when no foreign
 * instrument data are found.
 * 
 * Return original operation in that case(which may be NULL).
 * 
 * Return PTR_ERR() in case of error.
 */
static void* instrumentor_foreign_get_orig_operation_nodata(
    struct kedr_coi_foreign_instrumentor* instrumentor,
    const void* ops,
    size_t operation_offset)
{
    /* 
     * Options are not instrumented by us, or, more likely,
     * our instrumentation is just removed without restoring.
     * 
     * Verify operation at given offset. If it is not
     * coincide with any of our replacements, return it.
     * 
     * Otherwise return error. It is better than infinite recursion, which
     * would occure if we return operation itself, and it will be called
     * again by the caller of kedr_coi_instrumentor_get_orig_operation().
     */
    const struct kedr_coi_replacement* replacement;
    void* op = operation_at_offset(ops, operation_offset);

    for_each_replacement(replacement, instrumentor->replacements)
    {
        if(replacement->repl == op)
        {
            pr_err("Original for replacement operation %pF has lost.", op);
            return ERR_PTR(-EINVAL);
        }
    }
    return op;
}

/* Executed under lock. */
static int foreign_instrumentor_watch(
    struct kedr_coi_foreign_instrumentor* instrumentor,
    const void* id,
    const void* tie,
    const void** ops_p)
{
    int err;
    struct instrument_data_foreign* idata_foreign;
    struct kedr_coi_foreign_instrumentor_watch_data* watch_data;
    struct kedr_coi_instrumentor* instrumentor_binded;
    
    watch_data = instrumentor_foreign_find_watch_data_by_id(instrumentor, id);

    if(watch_data)
    {
        // Update replacement
        idata_foreign = watch_data->idata_foreign;
        
        if(!instrument_data_my_operations(idata_foreign->idata_binded, *ops_p))
        {
            idata_foreign = instrumentor_foreign_get_data(instrumentor, *ops_p);
            if(IS_ERR(idata_foreign))
            {
                instrument_data_restore_ops(idata_foreign->idata_binded, ops_p);
                instrumentor_foreign_destroy_watch_data(instrumentor, watch_data);
                return PTR_ERR(idata_foreign);
            }
            instrument_data_foreign_unref(instrumentor,
                watch_data->idata_foreign);
            watch_data->idata_foreign = idata_foreign;
        }
        
        instrument_data_foreign_replace_ops(idata_foreign, ops_p);
        return 1;
    }
    
    instrumentor_binded = instrumentor->instrumentor_binded;
    
    if(kedr_coi_hash_table_find_elem(
        &instrumentor_binded->foreign_ops_p_table,
        ops_p))
    {
        pr_err("Operations field %p is already watched by other foreign interceptor. "
            "There is no sense to watch that field again.", ops_p);
        return -EBUSY;
    }
    
    // Create new watch
    idata_foreign = instrumentor_foreign_get_data(instrumentor, *ops_p);
    if(IS_ERR(idata_foreign))
    {
        return PTR_ERR(idata_foreign);
    }
    
    err = -ENOMEM;
    watch_data = kmalloc(sizeof(*watch_data), GFP_ATOMIC);
    if(watch_data == NULL) goto fail_alloc_watch_data;

    watch_data->idata_foreign = idata_foreign;
    
    kedr_coi_hash_elem_init(&watch_data->id_elem, id);
    err = kedr_coi_hash_table_add_elem(
        &instrumentor->ids_table, &watch_data->id_elem);
    if(err) goto fail_add_id_elem;

    kedr_coi_hash_elem_init(&watch_data->tie_elem, tie);
    err = kedr_coi_hash_table_add_elem(
        &instrumentor->ties_table, &watch_data->tie_elem);
    if(err) goto fail_add_tie_elem;

    kedr_coi_hash_elem_init(&watch_data->ops_p_elem, ops_p);
    err = kedr_coi_hash_table_add_elem(
        &instrumentor_binded->foreign_ops_p_table,
        &watch_data->ops_p_elem);
    if(err) goto fail_add_ops_p_elem;


    instrument_data_foreign_replace_ops(watch_data->idata_foreign, ops_p);
    return 0;

fail_add_ops_p_elem:
    kedr_coi_hash_table_remove_elem(&instrumentor_binded->foreign_ops_p_table,
        &watch_data->ops_p_elem);
fail_add_tie_elem:
    kedr_coi_hash_table_remove_elem(&instrumentor->ids_table,
        &watch_data->id_elem);
fail_add_id_elem:
    kfree(watch_data);
fail_alloc_watch_data:
    instrument_data_foreign_unref(instrumentor, idata_foreign);
    return err;
}

/* Executed under lock. */
int foreign_instrumentor_bind(
    struct kedr_coi_foreign_instrumentor* instrumentor,
    void* object,
    const void* foreign_tie,
    const void** ops_p,
    size_t operation_offset,
    void** op_chained,
    void** op_orig)
{
    struct kedr_coi_instrumentor* instrumentor_binded
        = instrumentor->instrumentor_binded;
    
    struct kedr_coi_foreign_instrumentor_watch_data* watch_data_foreign;
    struct kedr_coi_instrumentor_watch_data* watch_data;
    
    struct instrument_data_foreign* idata_foreign;
    struct instrument_data* idata;
    
    watch_data_foreign = instrumentor_foreign_find_watch_data_by_tie(
        instrumentor, foreign_tie);
    
    if(watch_data_foreign)
    {
        /* Regular path. */
        idata_foreign = watch_data_foreign->idata_foreign;
        idata = idata_foreign->idata_binded;
        
        *op_chained = instrument_data_foreign_normal_operation(
            instrumentor,
            idata_foreign,
            operation_offset);
        *op_orig = instrument_data_get_orig_operation(idata, operation_offset);
        
        // If object is already watched, 1 will be returned.
        return instrumentor_watch_internal(instrumentor_binded, (void*)object,
            ops_p);
    }
    // Foreign tie is not watched.

    watch_data = instrumentor_find_watch_data(instrumentor_binded, object);
    if(watch_data)
    {
        /* 
         * .. but object is watched.
         * 
         * Need to chain to normal replacement or original operation.
         * 
         * By the way, update normal instrumentation.
         */
        idata = watch_data->idata;
        
        instrument_data_replace_ops(idata, ops_p);

        idata_foreign = instrumentor_foreign_find_data(instrumentor, *ops_p);
        if(idata_foreign)
        {
            /* 
             * Note, that chaining to other foreign replacements is
             * useless, because object is already watched.
             */
            *op_chained = instrument_data_foreign_normal_operation(
                instrumentor,
                idata_foreign,
                operation_offset);
        }
        else
        {
            /*
             *  Just check that operations, replaced by the normal
             * instrumentor, do not contain garbage from foreign
             * instrumentation. Here it would be our error.
             */
            *op_chained = instrumentor_foreign_get_orig_operation_nodata(
                instrumentor, *ops_p, operation_offset);
            BUG_ON(IS_ERR(*op_chained));
        }
        *op_orig = instrument_data_get_orig_operation(idata, operation_offset);
        return 1;
    }
    
    idata = instrumentor_find_data(instrumentor_binded, *ops_p);
    if(idata)
    {
        /* 
         * Both object and foriegn tie are not watched,
         * but operations are instrumented.
         * 
         * Need to chain other foreign instrumentation or to original operation.
         */
        idata_foreign = instrumentor_foreign_find_data(instrumentor, *ops_p);
        if(idata_foreign)
        {
            *op_chained = instrument_data_foreign_chain_operation(
                instrumentor,
                idata_foreign,
                operation_offset);
        }
        else
        {
            /*
             * There is no foreign instrumentation. Restore operations.
             * 
             * Also check that operations, replaced by the normal
             * instrumentor, do not contain garbage from foreign
             * instrumentation. Here it would be our error.
             */
            instrument_data_restore_ops(idata, ops_p);
            
            *op_chained = instrumentor_foreign_get_orig_operation_nodata(
                instrumentor, *ops_p, operation_offset);
            BUG_ON(IS_ERR(*op_chained));
        }

        *op_orig = instrument_data_get_orig_operation(idata, operation_offset);
        
        return 1;
    }
    //debug
    pr_info("foreign_instrumentor_bind: cannot find instrumentation data.");

    // Operations are not instrumented.
    *op_chained = instrumentor_foreign_get_orig_operation_nodata(
        instrumentor,
        *ops_p,
        operation_offset);

    *op_orig = *op_chained;
    
    return IS_ERR(*op_chained) ? PTR_ERR(*op_chained) : 1;
}

//***********  API for foreign instrumentor************************
struct kedr_coi_foreign_instrumentor* kedr_coi_foreign_instrumentor_create(
    struct kedr_coi_instrumentor* instrumentor_binded,
    const struct kedr_coi_replacement* replacements)
{
    int err;
    struct kedr_coi_foreign_instrumentor* instrumentor = kmalloc(
        sizeof(*instrumentor), GFP_KERNEL);
    
    if(!instrumentor) return NULL;
    
    err = kedr_coi_hash_table_init(&instrumentor->ids_table);
    if(err) goto fail_ids_table;

    err = kedr_coi_hash_table_init(&instrumentor->ties_table);
    if(err) goto fail_ties_table;

    instrumentor->instrumentor_binded = instrumentor_binded;
    instrumentor->replacements = replacements
        ? replacements
        : empty_replacements;

    return instrumentor;

fail_ties_table:
    kedr_coi_hash_table_destroy(&instrumentor->ids_table,
        NULL, NULL);
fail_ids_table:
    kfree(instrumentor);
    return NULL;
}

struct instrumentor_foreign_destroy_data
{
    struct kedr_coi_foreign_instrumentor* instrumentor;
    trace_unforgotten_watch_t trace_unforgotten_watch;
    void* user_data;
};

static void instrumentor_foreign_destroy_watch_data_callback(
    struct kedr_coi_hash_elem* elem,
    void* user_data)
{
    const void* object = elem->key;
    struct kedr_coi_foreign_instrumentor_watch_data* watch_data = 
        container_of(elem, typeof(*watch_data), id_elem);
    struct instrumentor_foreign_destroy_data* destroy_data = user_data;
    struct kedr_coi_foreign_instrumentor* instrumentor = destroy_data->instrumentor;
    
    instrument_data_foreign_unref(instrumentor,
        watch_data->idata_foreign);
    
    kedr_coi_hash_table_remove_elem(&instrumentor->ties_table,
        &watch_data->tie_elem);
    
    kedr_coi_hash_table_remove_elem(
        &instrumentor->instrumentor_binded->foreign_ops_p_table,
        &watch_data->ops_p_elem);
    
    kfree(watch_data);
    
    if(destroy_data->trace_unforgotten_watch)
        destroy_data->trace_unforgotten_watch(object, destroy_data->user_data);
}


void kedr_coi_foreign_instrumentor_destroy(
    struct kedr_coi_foreign_instrumentor* instrumentor,
    void (*trace_unforgotten_watch)(const void* object, void* user_data),
    void* user_data)
{
    unsigned long flags;
    struct kedr_coi_instrumentor* instrumentor_binded =
        instrumentor->instrumentor_binded;
    struct instrumentor_foreign_destroy_data destroy_data =
    {
        .instrumentor = instrumentor,
        .trace_unforgotten_watch = trace_unforgotten_watch,
        .user_data = user_data
    };

    /* 
     * Removing  of unforgotten watches may touch hash tables
     * in normal instrumentor, so do that under lock.
     */
    spin_lock_irqsave(&instrumentor_binded->lock, flags);
    kedr_coi_hash_table_destroy(&instrumentor->ids_table,
        &instrumentor_foreign_destroy_watch_data_callback, &destroy_data);
    spin_unlock_irqrestore(&instrumentor_binded->lock, flags);
    
    /*
     * Other hash tables are destroyed without callbacks.
     * Locking is not needed here.
     */
    kedr_coi_hash_table_destroy(&instrumentor->ties_table,
        NULL, NULL);

    kfree(instrumentor);
}


int kedr_coi_foreign_instrumentor_watch(
    struct kedr_coi_foreign_instrumentor* instrumentor,
    const void* id,
    const void* tie,
    const void** ops_p)
{
    unsigned long flags;
    int err;
    struct kedr_coi_instrumentor* instrumentor_binded =
        instrumentor->instrumentor_binded;

    spin_lock_irqsave(&instrumentor_binded->lock, flags);
    err = foreign_instrumentor_watch(instrumentor, id, tie, ops_p);
    spin_unlock_irqrestore(&instrumentor_binded->lock, flags);
    
    return err;
}

/* 
 * Cancel watching for given object.
 * 
 * If ops_p is not NULL, operations replacement should be rollbacked.
 * 
 * Return 0 on success, 1 if object wasn't watched.
 */
int kedr_coi_foreign_instrumentor_forget(
    struct kedr_coi_foreign_instrumentor* instrumentor,
    const void* id,
    const void** ops_p)
{
    unsigned long flags;
    int err = 0;
    struct kedr_coi_instrumentor* instrumentor_binded =
        instrumentor->instrumentor_binded;
    
    struct kedr_coi_foreign_instrumentor_watch_data* watch_data;
    struct instrument_data_foreign* idata_foreign;
    
    spin_lock_irqsave(&instrumentor_binded->lock, flags);
    
    watch_data = instrumentor_foreign_find_watch_data_by_id(instrumentor, id);
    if(watch_data)
    {
        idata_foreign = watch_data->idata_foreign;
    
        if(ops_p)
        {
            /*
             * It is possible, that normal instrumentor watch for
             * the same object('self-foreign watch').
             * 
             * Normally, object forgotten by both instrumentors, normal
             * and foreign ones. So we restore operations for both.
             * 
             * If normal instrumentation is needed, kedr_coi_instrumentor_watch()
             * may be called for same object after.
             */
            instrument_data_restore_ops(idata_foreign->idata_binded, ops_p);
            
        }
        
        instrumentor_foreign_destroy_watch_data(instrumentor, watch_data);
    }
    else
    {
        err = 1; //Not watched
    }
    
    spin_unlock_irqrestore(&instrumentor_binded->lock, flags);
    
    return err;
}

int kedr_coi_foreign_instrumentor_bind(
    struct kedr_coi_foreign_instrumentor* instrumentor,
    void* object,
    const void* foreign_tie,
    const void** ops_p,
    size_t operation_offset,
    void** op_chained,
    void** op_orig)
{
    int err;
    unsigned long flags;
    struct kedr_coi_instrumentor* instrumentor_binded
        = instrumentor->instrumentor_binded;
    
    spin_lock_irqsave(&instrumentor_binded->lock, flags);
    err = foreign_instrumentor_bind(instrumentor,
        object, foreign_tie, ops_p, operation_offset, op_chained, op_orig);
    spin_unlock_irqrestore(&instrumentor_binded->lock, flags);

    return err;
}
