<$ extends 'interceptor_c' $>
<$ block prepare $>
#define OPERATION_OFFSET(operation_name) offsetof({{object.operations_type}}, operation_name)
#define OPERATION_TYPE(operation_name) typeof((({{object.operations_type}}*)0)->operation_name)
#define OPERATION_CHECKED_TYPE(op, operation_name) \
(BUILD_BUG_ON_ZERO(!__builtin_types_compatible_p(typeof(op), OPERATION_TYPE(operation_name))) + op)

static struct kedr_coi_factory_interceptor* interceptor;

/* Helper for use in intermediate functions, also check object type. */
static inline int bind_object(
        {{object.type}}* object,
        const <$ include 'tie_type' $>* tie,
        size_t operation_offset,
        struct kedr_coi_intermediate_info* info)
{
    return kedr_coi_factory_interceptor_bind_object(interceptor,
        object, tie, operation_offset, info);
}
<$ endblock prepare $>
<$ block fill_info $>
    bind_object(
        {{operation.object}},
        <$ include 'operation_tie' $>,
        OPERATION_OFFSET({{operation.name}}),
        &intermediate_info);
    
    chained = (typeof(chained))intermediate_info.op_chained;
    
    if(IS_ERR(chained))
    {
        /* Failed to determine chained operation */
        BUG();
    }
    
<$ endblock fill_info $>
<$ block api_implementation $>
<$ if factory.operations_field $>
int {{interceptor.name}}_init(
    struct kedr_coi_factory_interceptor* (*factory_interceptor_create)(
        const char* name,
        size_t factory_operations_field_offset,
        const struct kedr_coi_intermediate* intermediate_operations))
{
    interceptor = factory_interceptor_create(
        "{{interceptor.name}}",
        offsetof({{factory.type}}, {{factory.operations_field}}),
        intermediate_operations);
    if(interceptor == NULL)
    {
        pr_err("Failed create factory interceptor");
        return -EINVAL;
    }
    
    return 0;
}

int {{interceptor.name}}_watch({{factory.type}} *factory)
{
    return kedr_coi_factory_interceptor_watch(
        interceptor, factory);
}

int {{interceptor.name}}_forget({{factory.type}} *factory)
{
    return kedr_coi_factory_interceptor_forget(
        interceptor, factory);
}

int {{interceptor.name}}_forget_norestore({{factory.type}} *factory)
{
    return kedr_coi_factory_interceptor_forget_norestore(
        interceptor, factory);
}
<$ else $>
int {{interceptor.name}}_init(
    struct kedr_coi_factory_interceptor* (*factory_interceptor_create_generic)(
        const char* name,
        const struct kedr_coi_intermediate* intermediate_operations))
{
    interceptor = factory_interceptor_create_generic(
        "{{interceptor.name}}",
        intermediate_operations);
    if(interceptor == NULL)
    {
        pr_err("Failed to create generic factory interceptor");
        return -EINVAL;
    }
    
    return 0;
}

int {{interceptor.name}}_watch({{id.type}} id, {{tie.type}} tie, const {{object.operations_type}}** p_ops)
{
    return kedr_coi_factory_interceptor_watch_generic(
        interceptor, id, tie, (const void**)(p_ops));
}

int {{interceptor.name}}_forget({{id.type}} id, const {{object.operations_type}}** p_ops)
{
    return kedr_coi_factory_interceptor_forget_generic(
        interceptor, id, (const void**)(p_ops));
}
<$ endif $>

void {{interceptor.name}}_trace_unforgotten_object(void (*cb)(const <$ include 'id_type' $>* object))
{
    kedr_coi_factory_interceptor_trace_unforgotten_object(interceptor, (void (*)(const void*))cb);
}

int {{interceptor.name}}_payload_register(struct kedr_coi_payload* payload)
{
    return kedr_coi_factory_payload_register(interceptor, payload);
}

void {{interceptor.name}}_payload_unregister(struct kedr_coi_payload* payload)
{
    kedr_coi_factory_payload_unregister(interceptor, payload);
}


void {{interceptor.name}}_destroy(void)
{
    kedr_coi_factory_interceptor_destroy(interceptor);
}
<$ endblock api_implementation $>
