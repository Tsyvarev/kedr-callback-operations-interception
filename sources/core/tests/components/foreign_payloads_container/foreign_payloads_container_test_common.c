#include "foreign_payloads_container_test_common.h"

static int check_foreign_handlers(
    const kedr_coi_foreign_handler_t* handlers,
    const kedr_coi_foreign_handler_t* handlers_expected)
{
    const kedr_coi_foreign_handler_t* handler;
    int handlers_n = 0;
    
    const kedr_coi_foreign_handler_t* handler_expected;
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


static int check_foreign_replacements(const struct kedr_coi_replacement* replacements,
    const struct kedr_coi_foreign_intermediate* intermediates)
{
    const struct kedr_coi_replacement* replacement;
    int replacements_n = 0;
    
    const struct kedr_coi_foreign_intermediate* intermediate;
    int intermediates_n = 0;
    
    // Count elements in arrays, taking into account that arrays may be NULL.
    if(replacements != NULL)
    {
        for(replacement = replacements;
            replacement->operation_offset != -1;
            replacement++)
        {
            replacements_n++;
        }
    }
    if(intermediates != NULL)
    {
        for(intermediate = intermediates;
            intermediate->operation_offset != -1;
            intermediate++)
        {
            intermediates_n++;
        }
    }
    if(replacements_n != intermediates_n)
    {
        pr_err("Array of replacements should contain %d elements, but it contains %d.",
            intermediates_n, replacements_n);
        return -1;
    }

    if((replacements == NULL) || (intermediates == NULL))
        return 0;//empty arrays
    
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

int check_foreign_payloads_container(
    struct kedr_coi_foreign_payloads_container* container,
    const struct kedr_coi_foreign_intermediate* intermediates,
    const kedr_coi_foreign_handler_t* handlers_expected)
{
    int result;
    const struct kedr_coi_replacement* replacements;
    const kedr_coi_foreign_handler_t* handlers;
    
    replacements = kedr_coi_foreign_payloads_container_get_replacements(
        container);
     
    result = check_foreign_replacements(replacements, intermediates);
    if(result)
    {
        pr_err("Incorrect replacements array returned by container.");
        return result;
    }

    result = kedr_coi_foreign_payloads_container_get_handlers(
        container, &handlers);
    if(result)
    {
        pr_err("Failed to get handlers from container.");
        return result;
    }
    
    result = check_foreign_handlers(handlers, handlers_expected);
    if(result)
    {
        pr_err("Incorrect handlers returned by container.");
        return result;
    }

    return 0;
}

