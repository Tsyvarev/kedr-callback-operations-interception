#define {{operation.name}}_PRE_HANDLER_T(type) void (*type)({{operation.args | join(d=", ", attribute="type")}}, struct kedr_coi_operation_call_info* call_info)
#define {{operation.name}}_POST_HANDLER_T(type) void (*type)({{operation.args | join(d=", ", attribute="type")}}<$if operation.returnType$>, {{operation.returnType}}<$endif$>, struct kedr_coi_operation_call_info* call_info)

