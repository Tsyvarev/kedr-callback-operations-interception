#define <$interceptor.operations_prefix$>_<$operation.name$>_pre(pre_handler) { \
    .operation_offset = offsetof(<$if interceptor.is_direct$><$object.type$><$else$><$operations.type$><$endif$>, <$operation.name$>), \
    .func = (void*)&pre_handler \
    }
#define <$interceptor.operations_prefix$>_<$operation.name$>_post(post_handler) { \
    .operation_offset = offsetof(<$if interceptor.is_direct$><$object.type$><$else$><$operations.type$><$endif$>, <$operation.name$>), \
    .func = (void*)&post_handler \
}