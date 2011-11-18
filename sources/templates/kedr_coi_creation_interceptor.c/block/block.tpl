<$if operation.default$>static <$if operation.returnType$><$operation.returnType$><$else$>void<$endif$> intermediate_operation_default_<$operation.name$>(<$argumentSpec$>)
{
    <$operation.default$>
}

<$endif$>static <$if operation.returnType$><$operation.returnType$><$else$>void<$endif$> intermediate_operation_<$operation.name$>(<$argumentSpec$>)
{
    <$if operation.returnType$><$operation.returnType$><$else$>void<$endif$> (*op_chained)(<$argumentSpec$>);

    kedr_coi_bind_object(interceptor,
        <$operation.object$>,
        <$operation.tie$>,
        OPERATION_OFFSET(<$operation.name$>),
        (void**)&op_chained);
    
    <$if operation.returnType$>return <$endif$><$if operation.default$>op_chained? op_chained(<$argumentList$>)
        : intermediate_operation_default_<$operation.name$>(<$argumentList$>)<$else$>op_chained(<$argumentList$>)<$endif$>;
}