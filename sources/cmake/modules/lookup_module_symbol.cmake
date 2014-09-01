#  lookup_module_symbol(RESULT_VAR symbol_name)
#
# Check, whether symbol(usually, function) with given name is accessible
# for the kernel modules.
function(lookup_module_symbol RESULT_VAR symbol_name)
    execute_process(
        COMMAND grep -E "[[:xdigit:]]+[[:space:]]+${symbol_name}[[:space:]]+vmlinux[[:space:]]+EXPORT"
            "${Kbuild_BUILD_DIR}/Module.symvers"
        RESULT_VARIABLE res
        OUTPUT_QUIET
    )
    if(res EQUAL 0)
        set(${RESULT_VAR} "TRUE" PARENT_SCOPE)
    elseif(res EQUAL 1)
        set(${RESULT_VAR} "FALSE" PARENT_SCOPE)
    else(res EQUAL 0)
        message(FATAL_ERROR "Failed to search module symbols")
    endif(res EQUAL 0)
endfunction(lookup_module_symbol RESULT_VAR symbol_name)

