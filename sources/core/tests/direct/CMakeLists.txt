kedr_coi_test_add_prefix("direct")

# Common way for add test in that category.
# Automatically assign target name for module built.
function(add_test_interceptor_direct test_name source)
    add_test_interceptor(${test_name}
        "test_interceptor_direct_${test_name}"
        ${source}
        ${ARGN}
    )
endfunction(add_test_interceptor_direct test_name source)


add_subdirectory(base)
add_subdirectory(no_payloads)
add_subdirectory(conflicted_interceptors)
add_subdirectory(update)
add_subdirectory(internal_interception)
add_subdirectory(external_interception)
add_subdirectory(external_interception_forbidden)
