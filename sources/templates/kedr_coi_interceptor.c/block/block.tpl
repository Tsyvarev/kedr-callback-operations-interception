static struct kedr_coi_intermediate_info
intermediate_info_<$operation.name$>;

static <$if operation.returnType$><$operation.returnType$><$else$>void<$endif$> intermediate_operation_default_<$operation.name$>(<$argumentSpec$>)
{
    <$if operation.default$><$operation.default$><$else$>BUG();<$endif$>
}

static <$if operation.returnType$><$operation.returnType$><$else$><$void$><$endif$> intermediate_operation_<$operation.name$>(<$argumentSpec$>)
{
    struct kedr_coi_operation_call_info call_info;
    <$if operation.returnType$><$operation.returnType$> returnValue;

    <$endif$><$if operation.returnType$><$operation.returnType$><$else$>void<$endif$> (*orig)(<$argumentSpec$>) =
        kedr_coi_interceptor_get_orig_operation(interceptor, <$operation.object$>,
            OPERATION_OFFSET(<$operation.name$>));
    
    call_info.return_address = __builtin_return_address(0);
    
    if(intermediate_info_<$operation.name$>.pre != NULL)
    {
        void (**pre_function)(<$argumentSpec_comma$>struct kedr_coi_operation_call_info* call_info);
        
        for(pre_function = (typeof(pre_function))intermediate_info_<$operation.name$>.pre;
            *pre_function != NULL;
            pre_function++)
            (*pre_function)(<$argumentList_comma$>&call_info);
    }
    
    <$if operation.returnType$>returnValue = <$endif$>orig ? orig(<$argumentList$>)
        : intermediate_operation_default_<$operation.name$>(<$argumentList$>);

    if(intermediate_info_<$operation.name$>.post != NULL)
    {
        void (**post_function)(<$argumentSpec_comma$><$if operation.returnType$><$operation.returnType$> returnValue, <$endif$>struct kedr_coi_operation_call_info* call_info);
        
        for(post_function = (typeof(post_function))intermediate_info_<$operation.name$>.post;
            *post_function != NULL;
            post_function++)
            (*post_function)(<$argumentList_comma$><$if operation.returnType$>returnValue, <$endif$>&call_info);
    }

<$if operation.returnType$>    return returnValue;
<$endif$>}