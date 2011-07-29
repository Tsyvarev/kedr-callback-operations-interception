#define intermediate_operation_<$operation.name$>    {\
        .operation_offset = OPERATION_OFFSET(<$operation.name$>),\
        .repl = OPERATION_CHECKED_TYPE(&intermediate_repl_<$operation.name$>, <$operation.name$>),\
    <$if operation.group_id$>    .group_id = <$operation.group_id$>,\
    <$endif$>}

<$if operation.default$>static <$if operation.returnType$><$operation.returnType$><$else$>void<$endif$> intermediate_operation_default_<$operation.name$>(<$argumentSpec$>)
{
    <$operation.default$>
}

<$endif$>static <$if operation.returnType$><$operation.returnType$><$else$>void<$endif$> intermediate_repl_<$operation.name$>(<$argumentSpec$>)
{
    struct kedr_coi_operation_call_info call_info;
    struct kedr_coi_intermediate_info intermediate_info;
    <$if operation.returnType$><$operation.returnType$> returnValue;

    <$endif$><$if operation.returnType$><$operation.returnType$><$else$>void<$endif$> (*orig)(<$argumentSpec$>);
            
    kedr_coi_interceptor_get_intermediate_info(
        interceptor,
        <$operation.object$>,
        OPERATION_OFFSET(<$operation.name$>),
        &intermediate_info);
    
    call_info.return_address = __builtin_return_address(0);
    call_info.op_orig = intermediate_info.op_orig;
    
    orig = (typeof(orig))intermediate_info.op_orig;
    
    if(intermediate_info.pre != NULL)
    {
        void (**pre_function)(<$argumentSpec_comma$>struct kedr_coi_operation_call_info* call_info);
        
        for(pre_function = (typeof(pre_function))intermediate_info.pre;
            *pre_function != NULL;
            pre_function++)
            (*pre_function)(<$argumentList_comma$>&call_info);
    }
    
    <$if operation.returnType$>returnValue = <$endif$><$if operation.default$>orig ? orig(<$argumentList$>)
        : intermediate_operation_default_<$operation.name$>(<$argumentList$>)<$else$>orig(<$argumentList$>)<$endif$>;

    if(intermediate_info.post != NULL)
    {
        void (**post_function)(<$argumentSpec_comma$><$if operation.returnType$><$operation.returnType$> returnValue, <$endif$>struct kedr_coi_operation_call_info* call_info);
        
        for(post_function = (typeof(post_function))intermediate_info.post;
            *post_function != NULL;
            post_function++)
            (*post_function)(<$argumentList_comma$><$if operation.returnType$>returnValue, <$endif$>&call_info);
    }

<$if operation.returnType$>    return returnValue;
<$endif$>}