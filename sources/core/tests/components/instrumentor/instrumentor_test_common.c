#include "instrumentor_test_common.h"

static void* get_operation(const void* operations_struct,
    size_t offset)
{
    return *(void* const*)((const char*)operations_struct + offset);
}

static int check_object_instrumented_common(const void* object,
    struct kedr_coi_instrumentor* instrumentor,
    const void* operations_struct,
    const struct kedr_coi_test_replaced_operation* replaced_operations,
    const struct kedr_coi_test_unaffected_operation* unaffected_operations,
    int is_foreign)
{
    if(replaced_operations)
    {
        const struct kedr_coi_test_replaced_operation* replaced_operation;
        for(replaced_operation = replaced_operations;
            replaced_operation->offset != -1;
            replaced_operation++)
        {
            void* operation = get_operation(operations_struct,
                replaced_operation->offset);
            if(operation != replaced_operation->repl)
            {
                pr_err("For object %p operation at offset %zu should be replaced with %p, "
                    "but it is %p(original operation is %p).",
                    object, replaced_operation->offset, replaced_operation->repl,
                    operation, replaced_operation->orig);
                return -1;
            }

            if(!is_foreign)
            {
                void* orig_operation = kedr_coi_instrumentor_get_orig_operation(instrumentor, object,
                    replaced_operation->offset);
                
                if(orig_operation != replaced_operation->orig)
                {
                    pr_err("For object %p operation at offset %zu originally was %p, "
                        "but 'get_orig_operation' returns %p(operation is replaced with %p).",
                        object, replaced_operation->offset, replaced_operation->orig,
                        orig_operation, replaced_operation->repl);
                    return -1;
                }
            }
        }
    }
    if(unaffected_operations)
    {
        const struct kedr_coi_test_unaffected_operation* unaffected_operation;
        for(unaffected_operation = unaffected_operations;
            unaffected_operation->offset != -1;
            unaffected_operation++)
        {
            void* operation = get_operation(operations_struct,
                unaffected_operation->offset);
            if(operation != unaffected_operation->orig)
            {
                pr_err("For object %p operation at offset %zu should be %p "
                    "but it is replaced with %p.",
                    object, unaffected_operation->offset, unaffected_operation->orig,
                    operation);
                return -1;
            }
        }
    }
    
    return 0;
}

int check_object_instrumented(const void* object,
    struct kedr_coi_instrumentor* instrumentor,
    const void* operations_struct,
    const struct kedr_coi_test_replaced_operation* replaced_operations,
    const struct kedr_coi_test_unaffected_operation* unaffected_operations)
{
    return check_object_instrumented_common(object, instrumentor,
        operations_struct, replaced_operations, unaffected_operations, 0);
}

int check_object_instrumented_foreign(const void* object,
    struct kedr_coi_instrumentor* instrumentor,
    const void* operations_struct,
    const struct kedr_coi_test_replaced_operation* replaced_operations,
    const struct kedr_coi_test_unaffected_operation* unaffected_operations)
{
    return check_object_instrumented_common(object, instrumentor,
        operations_struct, replaced_operations, unaffected_operations, 1);
}


int check_object_uninstrumented(const void* object,
    const void* operations_struct,
    const struct kedr_coi_test_replaced_operation* replaced_operations,
    const struct kedr_coi_test_unaffected_operation* unaffected_operations)
{
    if(replaced_operations)
    {
        const struct kedr_coi_test_replaced_operation* replaced_operation;
        for(replaced_operation = replaced_operations;
            replaced_operation->offset != -1;
            replaced_operation++)
        {
            void* operation = get_operation(operations_struct,
                replaced_operation->offset);
            if(operation != replaced_operation->orig)
            {
                pr_err("For object %p operation at offset %zu should be %p, "
                    "but it is %p(operation was replaced with %p some times ago).",
                    object, replaced_operation->offset, replaced_operation->orig,
                    operation, replaced_operation->repl);
                return -1;
            }
        }
    }
    if(unaffected_operations)
    {
        const struct kedr_coi_test_unaffected_operation* unaffected_operation;
        for(unaffected_operation = unaffected_operations;
            unaffected_operation->offset != -1;
            unaffected_operation++)
        {
            void* operation = get_operation(operations_struct,
                unaffected_operation->offset);
            if(operation != unaffected_operation->orig)
            {
                pr_err("For object %p operation at offset %zu should be %p "
                    "but it is replaced with %p.",
                    object, unaffected_operation->offset, unaffected_operation->orig,
                    operation);
                return -1;
            }
        }
    }
    
    return 0;
}

int check_object_foreign_restored(const void* foreign_object,
    const void* foreign_operations_struct,
    const struct kedr_coi_test_replaced_operation* replaced_operations,
    const struct kedr_coi_test_unaffected_operation* unaffected_operations)
{
    if(replaced_operations)
    {
        const struct kedr_coi_test_replaced_operation* replaced_operation;
        for(replaced_operation = replaced_operations;
            replaced_operation->offset != -1;
            replaced_operation++)
        {
            void* operation = get_operation(foreign_operations_struct,
                replaced_operation->offset);
            if(operation != replaced_operation->orig)
            {
                pr_err("For foreign object %p operation at offset %zu should be %p, "
                    "but it is %p(operation was replaced with %p some times ago).",
                    foreign_object, replaced_operation->offset, replaced_operation->orig,
                    operation, replaced_operation->repl);
                return -1;
            }
        }
    }
    if(unaffected_operations)
    {
        const struct kedr_coi_test_unaffected_operation* unaffected_operation;
        for(unaffected_operation = unaffected_operations;
            unaffected_operation->offset != -1;
            unaffected_operation++)
        {
            void* operation = get_operation(foreign_operations_struct,
                unaffected_operation->offset);
            if(operation != unaffected_operation->orig)
            {
                pr_err("For foreign object %p operation at offset %zu should be %p "
                    "but it is replaced with %p.",
                    foreign_object, unaffected_operation->offset, unaffected_operation->orig,
                    operation);
                return -1;
            }
        }
    }
    
    return 0;
   
}
