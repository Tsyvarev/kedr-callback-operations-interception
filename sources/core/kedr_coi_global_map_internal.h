#ifndef KEDR_COI_GLOBAL_MAP_INTERNAL_H
#define KEDR_COI_GLOBAL_MAP_INTERNAL_H

/*
 * Definition of global map
 * for detecting collisions between interceptors.
 */

#include <linux/types.h>

int kedr_coi_global_map_init(void);

/*
 * Add key to the map.
 * 
 * Return 0 on success and negative error code on fail.
 * 
 * If key already added, return -EBUSY.
 */
int kedr_coi_global_map_add_key(const void* key);

/*
 * Remove key from the map.
 * 
 * It is an error to delete unexistent key.
 */
void kedr_coi_global_map_delete_key(const void* key);

void kedr_coi_global_map_destroy(void);

#endif /* KEDR_COI_GLOBAL_MAP_INTERNAL_H */