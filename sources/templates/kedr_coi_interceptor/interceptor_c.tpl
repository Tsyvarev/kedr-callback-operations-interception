/*
 * Operations interceptor {{interceptor.name}}.
 */

#include <kedr-coi/operations_interception.h>

<$if header$>
{{ header }}
<$endif$>
<$if implementation_header$>
{{ implementation_header }}
<$endif$>

<$for operation in operations if operation.header$>
{{ operation.header }}
<$endfor$>
<$for operation in operations if operation.implementation_header$>
{{ operation.implementation_header }}
<$endfor$>

<$ block prepare scoped$>
#define OPERATION_OFFSET(operation_name) offsetof(<$include 'operations_type'$>, operation_name)
#define OPERATION_TYPE(operation_name) typeof(((<$include 'operations_type'$>*)0)->operation_name)
#define OPERATION_CHECKED_TYPE(op, operation_name) \
(BUILD_BUG_ON_ZERO(!__builtin_types_compatible_p(typeof(op), OPERATION_TYPE(operation_name))) + op)

static struct kedr_coi_interceptor* interceptor = NULL;

/* Helper for use in intermediate functions, also check object type. */
static inline void get_intermediate_info(
    const {{object.type}}* object,
    size_t operation_offset,
    struct kedr_coi_intermediate_info* intermediate_info)
{
    kedr_coi_interceptor_get_intermediate_info(
        interceptor,
        object,
        operation_offset,
        intermediate_info);
}
<$ endblock prepare $>

<$for operation in operations$>
/* Interception of operation '{{ operation.name }}' */
<$if operation.default$>
static <$if operation.returnType$>{{ operation.returnType }}<$else$>void<$endif$>
 intermediate_operation_default_{{ operation.name }}(<$ include 'argumentSpec'$>)
{
    {{ operation.default }}
}
<$endif$>
static <$if operation.returnType$>{{operation.returnType}}<$else$>void<$endif$> intermediate_repl_{{operation.name}}(<$include 'argumentSpec'$>)
{
    struct kedr_coi_operation_call_info call_info;
    struct kedr_coi_intermediate_info intermediate_info;
<$if operation.returnType$>
    {{operation.returnType}} returnValue;
<$endif$>
    <$if operation.returnType$>{{operation.returnType}}<$else$>void<$endif$> (*chained)(<$include 'argumentSpec'$>);
            
<$ block fill_info scoped$>
    get_intermediate_info({{operation.object}},
        OPERATION_OFFSET({{operation.name}}), &intermediate_info);
    
<# For normal interceptor op_chained is always same as op_orig. #>
    chained = (typeof(chained))intermediate_info.op_orig;

    if(IS_ERR(chained))
    {
        /* Failed to determine chained operation */
        BUG();
    }
<$ endblock fill_info$>
    call_info.return_address = __builtin_return_address(0);
    call_info.op_orig = intermediate_info.op_orig;
    
    if(intermediate_info.pre != NULL)
    {
        void (**pre_function)(<$include 'argumentSpec_comma'$>struct kedr_coi_operation_call_info* call_info);
        
        for(pre_function = (typeof(pre_function))intermediate_info.pre;
            *pre_function != NULL;
            pre_function++)
            (*pre_function)(<$include 'argumentList_comma'$>&call_info);
    }
    
<$if not operation.default$>
    BUG_ON(chained == NULL);
<$endif$>
    <$if operation.returnType$>returnValue = <$endif$><$if operation.default$>chained ? chained(<$include 'argumentList'$>)
        : intermediate_operation_default_{{operation.name}}(<$include 'argumentList'$>)<$else$>chained(<$include 'argumentList'$>)<$endif$>;

    if(intermediate_info.post != NULL)
    {
        void (**post_function)(<$include 'argumentSpec_comma'$><$if operation.returnType$>{{operation.returnType}} returnValue, <$endif$>struct kedr_coi_operation_call_info* call_info);
        
        for(post_function = (typeof(post_function))intermediate_info.post;
            *post_function != NULL;
            post_function++)
            (*post_function)(<$include 'argumentList_comma'$><$if operation.returnType$>returnValue, <$endif$>&call_info);
    }

<$if operation.returnType$>
    return returnValue;
<$endif$>
}

<$endfor$>

static struct kedr_coi_intermediate intermediate_operations[] =
{
<$for operation in operations$>
    {
        .operation_offset = OPERATION_OFFSET({{operation.name}}),
        .repl = OPERATION_CHECKED_TYPE(&intermediate_repl_{{operation.name}}, {{operation.name}}),
<$if operation.group_id$>
        .group_id = {{operation.group_id}},
<$endif$>
<$if not operation.default$>
        .internal_only = true,
<$endif$>
    },
<$endfor$>
    {
        .operation_offset = -1,
    }
};

<$ block api_implementation scoped $>
int {{interceptor.name}}_init(void)
{
    interceptor = <$if interceptor.is_direct$>kedr_coi_interceptor_create_direct("{{interceptor.name}}",
       sizeof({{object.type}})<$else$>kedr_coi_interceptor_create("{{interceptor.name}}",
       offsetof({{object.type}}, {{object.operations_field}}),
       sizeof({{object.operations_type}})<$endif$>,
       intermediate_operations);

    if(interceptor == NULL)
        return -EINVAL;//TODO: how to report concrete error?
    
    return 0;
}

void {{interceptor.name}}_trace_unforgotten_object(void (*cb)(const {{object.type}}* object))
{
    kedr_coi_interceptor_trace_unforgotten_object(interceptor, (void (*)(const void*))cb);
}

void {{interceptor.name}}_destroy(void)
{
    kedr_coi_interceptor_destroy(interceptor);
}

int {{interceptor.name}}_payload_register(struct kedr_coi_payload* payload)
{
    return kedr_coi_payload_register(interceptor, payload);
}

void {{interceptor.name}}_payload_unregister(struct kedr_coi_payload* payload)
{
    kedr_coi_payload_unregister(interceptor, payload);
}

int {{interceptor.name}}_start(void)
{
    return kedr_coi_interceptor_start(interceptor);
}

void {{interceptor.name}}_stop(void)
{
    kedr_coi_interceptor_stop(interceptor);
}

int {{interceptor.name}}_watch({{object.type}} *object)
{
    return kedr_coi_interceptor_watch(interceptor, object);
}

int {{interceptor.name}}_forget({{object.type}} *object)
{
    return kedr_coi_interceptor_forget(interceptor, object);
}

int {{interceptor.name}}_forget_norestore({{object.type}} *object)
{
    return kedr_coi_interceptor_forget_norestore(interceptor, object);
}

<$if not interceptor.is_direct$>
void {{interceptor.name}}_mechanism_selector(
    bool (*replace_at_place)(const {{object.operations_type}}* ops))
{
    kedr_coi_interceptor_mechanism_selector(interceptor, (bool (*)(const void*))(replace_at_place));
}
// For create factory interceptors
struct kedr_coi_factory_interceptor*
{{interceptor.name}}_factory_interceptor_create(
    const char* name,
    size_t factory_operations_field_offset,
    const struct kedr_coi_intermediate* _intermediate_operations)
{
    return kedr_coi_factory_interceptor_create(
        interceptor,
        name,
        factory_operations_field_offset,
        _intermediate_operations);
}

struct kedr_coi_factory_interceptor*
{{interceptor.name}}_factory_interceptor_create_generic(
    const char* name,
    const struct kedr_coi_intermediate* _intermediate_operations)
{
    return kedr_coi_factory_interceptor_create_generic(
        interceptor,
        name,
        _intermediate_operations);
}
<$endif$>

<$ endblock api_implementation $>
