/*
 * Operations interceptor <$interceptor.name$>.
 */

#include <kedr-coi/operations_interception.h>

<$if concat(header)$><$header: join(\n)$>

<$endif$>static struct kedr_coi_interceptor* interceptor = NULL;

static struct kedr_coi_intermediate_foreign_info intermediate_info;

typedef <$foreign_object.type$> foreign_object_t;
static const <$operations.type$>* get_foreign_operations(<$foreign_object.type$>* foreign_object)
{
    return foreign_object-><$foreign_object.operations_field$>;
}

<$block: join(\n)$>


static struct kedr_coi_intermediate_foreign intermediate_operations[] =
{
<$intermediate_operation: join(\n)$>
    {
        .operation_offset = -1,
    }
};

int <$interceptor.name$>_init(void)
{
    interceptor = kedr_coi_interceptor_create_foreign("<$interceptor.name$>",
       offsetof(<$object.type$>, <$object.operations_field$>),
       sizeof(<$operations.type$>),
       offsetof(<$foreign_object.type$>, <$foreign_object.operations_field$>),
       intermediate_operations,
       &intermediate_info);
    
    if(interceptor == NULL)
        return -EINVAL;//TODO: how to report concrete error?
    
    return 0;
}

void <$interceptor.name$>_destroy(void)
{
    kedr_coi_interceptor_destroy(interceptor);
}

int <$interceptor.name$>_payload_register(struct kedr_coi_payload_foreign* payload)
{
    return kedr_coi_payload_foreign_register(interceptor, payload);
}

void <$interceptor.name$>_payload_unregister(struct kedr_coi_payload_foreign* payload)
{
    kedr_coi_payload_foreign_unregister(interceptor, payload);
}


int <$interceptor.name$>_start(void)
{
    return kedr_coi_interceptor_start(interceptor);
}

void <$interceptor.name$>_stop(void)
{
    kedr_coi_interceptor_stop(interceptor);
}

int <$interceptor.name$>_watch(<$object.type$> *object)
{
    return kedr_coi_interceptor_watch(
        interceptor, object);
}

int <$interceptor.name$>_forget(<$object.type$> *object)
{
    return kedr_coi_interceptor_forget(
        interceptor, object);
}

int <$interceptor.name$>_forget_norestore(<$object.type$> *object)
{
    return kedr_coi_interceptor_forget_norestore(
        interceptor, object);
}
