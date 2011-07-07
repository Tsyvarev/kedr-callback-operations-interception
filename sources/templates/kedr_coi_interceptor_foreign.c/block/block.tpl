<$if operation.default$>static <$if operation.returnType$><$operation.returnType$><$else$>void<$endif$> intermediate_operation_default_<$operation.name$>(<$argumentSpec$>)
{
    <$operation.default$>
}

<$endif$>static <$if operation.returnType$><$operation.returnType$><$else$>void<$endif$> intermediate_operation_<$operation.name$>(<$argumentSpec$>)
{
    <$if operation.returnType$><$operation.returnType$><$else$>void<$endif$> (*orig)(<$argumentSpec$>);
    foreign_object_t *foreign_object = <$operation.foreign_object$>;
    
    kedr_coi_interceptor_foreign_restore_copy(interceptor,
        <$operation.object$>, foreign_object);
    
    if(intermediate_info.handlers != NULL)
    {
        void (**on_create_handler)(foreign_object_t *foreign_object);
        
        for(on_create_handler = (typeof(on_create_handler))intermediate_info.handlers;
            *on_create_handler != NULL;
            on_create_handler++)
            (*on_create_handler)(foreign_object);
    }
    
    orig = get_foreign_operations(foreign_object)-><$operation.name$>;
    <$if operation.returnType$>return <$endif$><$if operation.default$>orig? orig(<$argumentList$>)
        : intermediate_operation_default_<$operation.name$>(<$argumentList$>)<$else$>orig(<$argumentList$>)<$endif$>;
}