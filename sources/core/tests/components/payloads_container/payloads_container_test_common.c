#include "payloads_container_test_common.h"

/*
 * Verify that NULL-terminated array 'handlers' contains same elements
 * as array 'handlers_expected'.
 */
static int check_function_array(void* const* funcs, void* const* funcs_expected)
{
    void* const* func;
    int funcs_n = 0;
    
    void* const* func_expected;
    int funcs_expected_n = 0;
    
    // Count functions, taking into account that arrays may be NULL.
    if(funcs)
    {
        for(func = funcs; *func != NULL; func++)
            funcs_n++;
    }
    
    if(funcs_expected)
    {
        for(func_expected = funcs_expected;
            *func_expected != NULL;
            func_expected++)
        {
            funcs_expected_n++;
        }
    }
    
    if(funcs_n != funcs_expected_n)
    {
        pr_err("Array of functions should contain %d elements, but it contains %d.",
            funcs_expected_n, funcs_n);
        return -1;
    }
    
    if(funcs_expected_n == 0) return 0;
    
    // Now both arrays are not NULL.
    for(func_expected = funcs_expected;
        *func_expected != NULL;
        func_expected++)
    {
        for(func = funcs; *func != NULL; func++)
        {
            if(*func == *func_expected) break;
        }
        if(*func == NULL)
        {
            pr_err("Array of functions should contain function %p, but it doesn't.",
                *func_expected);
            return -1;
        }
    }
    
    return 0;
}

/*
 * Verify that handlers is correctly set for the intermediate,
 * pointed in 'replacement_expected'.
 */
static int check_replacement_fill(
    const struct kedr_coi_test_replacement* replacement_expected)
{
    int result;
    
    result = check_function_array(
        replacement_expected->intermediate->info->pre,
        replacement_expected->pre);
    
    if(result)
    {
        pr_err("Incorrect array of pre-functions.");
        return result;
    }

    result = check_function_array(
        replacement_expected->intermediate->info->post,
        replacement_expected->post);
    
    if(result)
    {
        pr_err("Incorrect array of post-functions.");
        return result;
    }

    return 0;
}

int check_replacements(const struct kedr_coi_instrumentor_replacement* replacements,
    const struct kedr_coi_test_replacement* replacements_expected)
{
    int result;

    const struct kedr_coi_instrumentor_replacement* replacement;
    const struct kedr_coi_test_replacement* replacement_expected;
    
    int replacements_n = 0;
    int replacements_expected_n = 0;
    
    // Count elements in both arrays
    for(replacement = replacements;
        replacement->operation_offset != -1;
        replacement++)
    {
        replacements_n ++;
    }

    for(replacement_expected = replacements_expected;
        replacement_expected->intermediate != NULL;
        replacement_expected++)
    {
        replacements_expected_n ++;
    }
    
    if(replacements_n != replacements_expected_n)
    {
        pr_err("Array of replacements containes %d elemens, but should %d.",
            replacements_n, replacements_expected_n);
        return -1;
    }
    /*
     *  Check that all expected replacements are contained
     * in the replacements array and filled as expected
     */
    for(replacement_expected = replacements_expected;
        replacement_expected->intermediate != NULL;
        replacement_expected++)
    {
        size_t operation_offset = replacement_expected->intermediate->operation_offset;
        void* repl = replacement_expected->intermediate->repl;
        
        for(replacement = replacements;
            replacement->operation_offset != -1;
            replacement++)
        {
            if(replacement->operation_offset != operation_offset) continue;
            if(replacement->repl != repl)
            {
                pr_err("Replacement operation at offset %zu is %p, "
                    "but should be %p", operation_offset,
                    replacement->repl, repl);
                return -1;
            }
            result = check_replacement_fill(replacement_expected);
            if(result)
            {
                pr_err("Intermediate information for operation at offset %zu "
                    "is incorrectly filled.", operation_offset);
                return result;
            }
            break;
        }
        if(replacement->operation_offset == -1)
        {
            pr_err("Replacements array should contain replacement "
                "for operation at offset %zu, but it doesn't.",
                operation_offset);
            return -1;
        }
        
    }
    return 0;
}