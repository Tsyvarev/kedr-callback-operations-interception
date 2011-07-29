<$if operation.default$>static <$if operation.returnType$><$operation.returnType$><$else$>void<$endif$> intermediate_operation_default_<$operation.name$>(<$argumentSpec$>)
{
    <$operation.default$>
}

<$endif$>static <$if operation.returnType$><$operation.returnType$><$else$>void<$endif$> intermediate_operation_<$operation.name$>(<$argumentSpec$>)
{
    <$if operation.returnType$><$operation.returnType$><$else$>void<$endif$> (*orig)(<$argumentSpec$>);
    struct kedr_coi_foreign_intermediate_info intermediate_info;
    foreign_object_t *foreign_object = <$operation.foreign_object$>;
    
    kedr_coi_foreign_interceptor_restore_copy(interceptor,
        <$operation.object$>, foreign_object, &intermediate_info);
    
    if(intermediate_info.on_create_handlers != NULL)
    {
        const kedr_coi_foreign_handler_t* on_create_handler;
        
        for(on_create_handler = intermediate_info.on_create_handlers;
            *on_create_handler != NULL;
            on_create_handler++)
            (*on_create_handler)(foreign_object);
    }
    
    orig = get_foreign_operations(foreign_object)-><$operation.name$>;
    <$if operation.returnType$>return <$endif$><$if operation.default$>orig? orig(<$argumentList$>)
        : intermediate_operation_default_<$operation.name$>(<$argumentList$>)<$else$>orig(<$argumentList$>)<$endif$>;
}