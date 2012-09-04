<$if operation.default$>static <$if operation.returnType$><$operation.returnType$><$else$>void<$endif$> intermediate_operation_default_<$operation.name$>(<$argumentSpec$>)
{
    <$operation.default$>
}

<$endif$>static <$if operation.returnType$><$operation.returnType$><$else$>void<$endif$> intermediate_operation_<$operation.name$>(<$argumentSpec$>)
{
    struct kedr_coi_operation_call_info call_info;
    struct kedr_coi_creation_intermediate_info intermediate_info;
    <$if operation.returnType$><$operation.returnType$> returnValue;
    
    <$endif$><$if operation.returnType$><$operation.returnType$><$else$>void<$endif$> (*op_chained)(<$argumentSpec$>);

    int result = bind_object(
        <$operation.object$>,
        <$operation.tie$>,
        OPERATION_OFFSET(<$operation.name$>),
        &intermediate_info);
    
    if(IS_ERR(intermediate_info.op_chained))
    {
        /* Failed to determine chained operation */
        BUG();
    }
    
    op_chained = intermediate_info.op_chained
        ? (typeof(op_chained))intermediate_info.op_chained
        : &intermediate_operation_default_<$operation.name$>;
    
    if(result)
    {
        /* Binding failed. Simply call chained operation. */
        <$if operation.returnType$>return <$endif$>op_chained(<$argumentList$>);
<$if operation.returnType$><$else$>        return;
<$endif$>    }
    
    call_info.op_orig = intermediate_info.op_orig;
    
    if(intermediate_info.pre)
    {
        void (**pre_function)(<$argumentSpec_comma$>struct kedr_coi_operation_call_info* call_info);
        
        for(pre_function = (typeof(pre_function))intermediate_info.pre;
            *pre_function != NULL;
            pre_function++)
            (*pre_function)(<$argumentList_comma$>&call_info);
    }
    
    <$if operation.returnType$>returnValue = <$endif$>op_chained(<$argumentList$>);
    
    if(intermediate_info.post != NULL)
    {
        void (**post_function)(<$argumentSpec_comma$><$if operation.returnType$><$operation.returnType$> returnValue, <$endif$>struct kedr_coi_operation_call_info* call_info);
        
        for(post_function = (typeof(post_function))intermediate_info.post;
            *post_function != NULL;
            post_function++)
            (*post_function)(<$argumentList_comma$><$if operation.returnType$>returnValue, <$endif$>&call_info);
    }
    
    return<$if operation.returnType$> returnValue<$endif$>;
}