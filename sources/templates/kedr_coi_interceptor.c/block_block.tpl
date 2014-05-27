/* Interception of operation '{{ operation.name }}' */
<$if operation.default$>
static <$if operation.returnType$>{{ operation.returnType }}<$else$>void<$endif$>
 intermediate_operation_default_{{ operation.name }}(<$ include 'block_argumentSpec'$>)
{
    {{ operation.default }}
}
<$endif$>
static <$if operation.returnType$>{{operation.returnType}}<$else$>void<$endif$> intermediate_repl_{{operation.name}}(<$include 'block_argumentSpec'$>)
{
    struct kedr_coi_operation_call_info call_info;
    struct kedr_coi_intermediate_info intermediate_info;
<$if operation.returnType$>
    {{operation.returnType}} returnValue;
<$endif$>
    <$if operation.returnType$>{{operation.returnType}}<$else$>void<$endif$> (*orig)(<$include 'block_argumentSpec'$>);
            
    get_intermediate_info({{operation.object}},
        OPERATION_OFFSET({{operation.name}}), &intermediate_info);
    
    call_info.return_address = __builtin_return_address(0);
    call_info.op_orig = intermediate_info.op_orig;
    
    orig = (typeof(orig))intermediate_info.op_orig;
    
    if(intermediate_info.pre != NULL)
    {
        void (**pre_function)(<$include 'block_argumentSpec_comma'$>struct kedr_coi_operation_call_info* call_info);
        
        for(pre_function = (typeof(pre_function))intermediate_info.pre;
            *pre_function != NULL;
            pre_function++)
            (*pre_function)(<$include 'block_argumentList_comma'$>&call_info);
    }
    
<$if not operation.default$>
    BUG_ON(orig == NULL);
<$endif$>
    <$if operation.returnType$>returnValue = <$endif$><$if operation.default$>orig ? orig(<$include 'block_argumentList'$>)
        : intermediate_operation_default_{{operation.name}}(<$include 'block_argumentList'$>)<$else$>orig(<$include 'block_argumentList'$>)<$endif$>;

    if(intermediate_info.post != NULL)
    {
        void (**post_function)(<$include 'block_argumentSpec_comma'$><$if operation.returnType$>{{operation.returnType}} returnValue, <$endif$>struct kedr_coi_operation_call_info* call_info);
        
        for(post_function = (typeof(post_function))intermediate_info.post;
            *post_function != NULL;
            post_function++)
            (*post_function)(<$include 'block_argumentList_comma'$><$if operation.returnType$>returnValue, <$endif$>&call_info);
    }

<$if operation.returnType$>
    return returnValue;
<$endif$>
}

