/*
 * Operations interceptor <$interceptor.name$>.
 */

#include <kedr-coi/operations_interception.h>

<$if concat(header)$><$header: join(\n)$>

<$endif$><$if concat(implementation_header)$><$implementation_header: join(\n)$>

<$endif$>static struct kedr_coi_foreign_interceptor* interceptor = NULL;

typedef <$foreign_object.type$> foreign_object_t;
static const <$operations.type$>* get_foreign_operations(<$foreign_object.type$>* foreign_object)
{
    return foreign_object-><$foreign_object.operations_field$>;
}

<$block: join(\n)$>


static struct kedr_coi_foreign_intermediate intermediate_operations[] =
{
<$intermediate_operation: join(\n)$>
    {
        .operation_offset = -1,
    }
};

int <$interceptor.name$>_init(void)
{
    interceptor = kedr_coi_foreign_interceptor_create("<$interceptor.name$>",
       offsetof(<$object.type$>, <$object.operations_field$>),
       sizeof(<$operations.type$>),
       offsetof(<$foreign_object.type$>, <$foreign_object.operations_field$>),
       intermediate_operations);
    
    if(interceptor == NULL)
        return -EINVAL;//TODO: how to report concrete error?
    
    return 0;
}

void <$interceptor.name$>_destroy(void)
{
    kedr_coi_foreign_interceptor_destroy(interceptor);
}

int <$interceptor.name$>_payload_register(struct kedr_coi_foreign_payload* payload)
{
    return kedr_coi_foreign_payload_register(interceptor, payload);
}

void <$interceptor.name$>_payload_unregister(struct kedr_coi_foreign_payload* payload)
{
    kedr_coi_foreign_payload_unregister(interceptor, payload);
}


int <$interceptor.name$>_start(void)
{
    return kedr_coi_foreign_interceptor_start(interceptor);
}

void <$interceptor.name$>_stop(void (*trace_unforgotten_object)(<$object.type$>* object))
{
    kedr_coi_foreign_interceptor_stop(interceptor, (void (*)(void* object))trace_unforgotten_object);
}

int <$interceptor.name$>_watch(<$object.type$> *object)
{
    return kedr_coi_foreign_interceptor_watch(
        interceptor, object);
}

int <$interceptor.name$>_forget(<$object.type$> *object)
{
    return kedr_coi_foreign_interceptor_forget(
        interceptor, object);
}

int <$interceptor.name$>_forget_norestore(<$object.type$> *object)
{
    return kedr_coi_foreign_interceptor_forget_norestore(
        interceptor, object);
}
