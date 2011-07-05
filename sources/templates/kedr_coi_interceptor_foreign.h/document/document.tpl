#ifndef KEDR_COI_INTERCEPTOR_<$interceptor.name$>
#define KEDR_COI_INTERCEPTOR_<$interceptor.name$>

#include <kedr-coi/operations_interception.h>

<$if concat(header)$><$header: join(\n)$>

<$endif$>/* Interceptor <$interceptor.name$> */
extern int <$interceptor.name$>_init(void);
extern void <$interceptor.name$>_destroy(void);

extern int <$interceptor.name$>_payload_register(struct kedr_coi_payload_foreign* payload);
extern int <$interceptor.name$>_payload_unregister(struct kedr_coi_payload_foreign* payload);

extern int <$interceptor.name$>_start(void);
extern int <$interceptor.name$>_stop(void);

extern int <$interceptor.name$>_watch(<$object.type$> *object);
extern int <$interceptor.name$>_forget(<$object.type$> *object);

extern int <$interceptor.name$>_forget_norestore(<$object.type$> *object);

#endif /* KEDR_COI_INTERCEPTOR_<$interceptor.name$> */