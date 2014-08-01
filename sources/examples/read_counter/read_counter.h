#ifndef READ_COUNTER_H
#define READ_COUNTER_H

/*
 * Exported functions and annotations for use in module implemented
 * character device for count reads from this device.
 */

int read_counter_init(void);
void read_counter_destroy(void);

int cdev_file_operations_interceptor_watch(struct cdev* dev);
void cdev_file_operations_interceptor_forget(struct cdev* dev);

#define CDEV_ADD_ANNOTATION(cdev) cdev_file_operations_interceptor_watch(cdev)
#define CDEV_DEL_ANNOTATION(cdev) cdev_file_operations_interceptor_forget(cdev)

#endif /* READ_COUNTER_H */