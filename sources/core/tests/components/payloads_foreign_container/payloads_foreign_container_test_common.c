#include "payloads_foreign_container_test_common.h"

static int check_foreign_intermediate_info(
    const struct kedr_coi_intermediate_foreign_info* info,
    const kedr_coi_handler_foreign_t* handlers_expected)
{
    const kedr_coi_handler_foreign_t* handlers = info->on_create_handlers;
    
    const kedr_coi_handler_foreign_t* handler;
    int handlers_n = 0;
    
    const kedr_coi_handler_foreign_t* handler_expected;
    int handlers_expected_n = 0;
    
    // Count handlers, taking into account that arrays may be NULL.
    if(handlers)
    {
        for(handler = handlers; *handler != NULL; handler++)
            handlers_n++;
    }
    
    if(handlers_expected)
    {
        for(handler_expected = handlers_expected;
            *handler_expected != NULL;
            handler_expected++)
        {
            handlers_expected_n++;
        }
    }
    
    if(handlers_n != handlers_expected_n)
    {
        pr_err("Array of handlers should contain %d elements, but it contains %d.",
            handlers_expected_n, handlers_n);
        return -1;
    }
    
    if(handlers_expected_n == 0) return 0;
    
    // Now both arrays are not NULL.
    for(handler_expected = handlers_expected;
        *handler_expected != NULL;
        handler_expected++)
    {
        for(handler = handlers; *handler != NULL; handler++)
        {
            if(*handler == *handler_expected) break;
        }
        if(*handler == NULL)
        {
            pr_err("Array of handlers should contain handler %p, but it doesn't.",
                *handler_expected);
            return -1;
        }
    }
    
    return 0;
}


static int check_foreign_replacements_array(const struct kedr_coi_instrumentor_replacement* replacements,
    const struct kedr_coi_intermediate_foreign* intermediates)
{
    const struct kedr_coi_instrumentor_replacement* replacement;
    int replacements_n = 0;
    
    const struct kedr_coi_intermediate_foreign* intermediate;
    int intermediates_n = 0;
    
    // Count elements in arrays, taking into account that arrays may be NULL.
    for(replacement = replacements;
        replacement->operation_offset != -1;
        replacement++)
    {
        replacements_n++;
    }
    for(intermediate = intermediates;
        intermediate->operation_offset != -1;
        intermediate++)
    {
        intermediates_n++;
    }
    
    if(replacements_n != intermediates_n)
    {
        pr_err("Array of replacements should contain %d elements, but it contains %d.",
            intermediates_n, replacements_n);
        return -1;
    }
    
    for(intermediate = intermediates;
        intermediate->operation_offset != -1;
        intermediate++)
    {
        size_t operation_offset = intermediate->operation_offset;
        void* repl = intermediate->repl;

        for(replacement = replacements;
            replacement->operation_offset != -1;
            replacement++)
        {
            if(replacement->operation_offset != operation_offset) continue;
            if(replacement->repl != repl)
            {
                pr_err("Replacement operation of offset %zu should be %p, "
                    "but it is %p.", operation_offset, repl, replacement->repl);
                return -1;
            }
            break;
        }
        if(replacement->operation_offset == -1)
        {
            pr_err("Array of replacements should contain operation with offset %zu, but it doesn't.",
                operation_offset);
            return -1;
        }
    }
    
    return 0;
}

int check_foreign_replacements(const struct kedr_coi_instrumentor_replacement* replacements,
    const struct kedr_coi_intermediate_foreign_info* info,
    const struct kedr_coi_intermediate_foreign* intermediates,
    const kedr_coi_handler_foreign_t* handlers_expected)
{
    int result = check_foreign_replacements_array(replacements, intermediates);
    if(result) return result;
    
    return check_foreign_intermediate_info(info, handlers_expected);
}

