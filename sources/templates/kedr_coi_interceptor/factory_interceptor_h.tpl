<$ extends 'interceptor_h' $>
<$ block api_declaration $>
<$ if factory.operations_field $>
extern int {{interceptor.name}}_init(
    struct kedr_coi_factory_interceptor* (*factory_interceptor_create)(
        const char* name,
        size_t factory_operations_field_offset,
        const struct kedr_coi_intermediate* intermediate_operations));

extern int {{interceptor.name}}_watch({{factory.type}} *factory);
extern int {{interceptor.name}}_forget({{factory.type}} *factory);
extern int {{interceptor.name}}_forget_norestore({{factory.type}} *factory);
<$ else $>
extern int {{interceptor.name}}_init(
    struct kedr_coi_factory_interceptor* (*factory_interceptor_create_generic)(
        const char* name,
        const struct kedr_coi_intermediate* intermediate_operations));

extern int {{interceptor.name}}_watch({{id.type}} id, {{tie.type}} tie, const {{object.operations_type}}** ops_p);
extern int {{interceptor.name}}_forget({{id.type}} id, const {{object.operations_type}}** ops_p);
<$ endif $>

int {{interceptor.name}}_payload_register(struct kedr_coi_payload* payload);
int {{interceptor.name}}_payload_unregister(struct kedr_coi_payload* payload);

void {{interceptor.name}}_trace_unforgotten_object(void (*cb)(const <$ include 'id_type' $>* object));

extern void {{interceptor.name}}_destroy(void);

/* Types of handlers for payloads may be taken from the header for binded interceptor. */
<$ endblock api_declaration $>
