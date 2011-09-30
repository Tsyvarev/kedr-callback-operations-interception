#define <$interceptor.operations_prefix$>_<$operation.name$>_pre(pre_handler) { \
    .operation_offset = offsetof(<$operations_type$>, <$operation.name$>), \
    .func = KEDR_COI_CALLBACK_CHECKED(pre_handler, <$interceptor.operations_prefix$>_<$operation.name$>_pre_handler_t) \
    }
#define <$interceptor.operations_prefix$>_<$operation.name$>_post(post_handler) { \
    .operation_offset = offsetof(<$operations_type$>, <$operation.name$>), \
    .func = KEDR_COI_CALLBACK_CHECKED(post_handler, <$interceptor.operations_prefix$>_<$operation.name$>_post_handler_t) \
}<$if operation.default$>
#define <$interceptor.operations_prefix$>_<$operation.name$>_pre_external(pre_handler) { \
    .operation_offset = offsetof(<$operations_type$>, <$operation.name$>), \
    .func = KEDR_COI_CALLBACK_CHECKED(pre_handler, <$interceptor.operations_prefix$>_<$operation.name$>_pre_handler_t), \
    .external = true \
    }
#define <$interceptor.operations_prefix$>_<$operation.name$>_post_external(post_handler) { \
    .operation_offset = offsetof(<$operations_type$>, <$operation.name$>), \
    .func = KEDR_COI_CALLBACK_CHECKED(post_handler, <$interceptor.operations_prefix$>_<$operation.name$>_post_handler_t), \
    .external = true \
}<$endif$>
