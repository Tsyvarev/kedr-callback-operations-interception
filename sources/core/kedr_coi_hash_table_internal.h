#ifndef KEDR_COI_HASH_TABLE_INTERNAL_H
#define KEDR_COI_HASH_TABLE_INTERNAL_H

/*
 * Definition of hash table for using by instrumentors.
 */

#include <linux/types.h>

struct kedr_coi_hash_table;


struct kedr_coi_hash_table* kedr_coi_hash_table_create(void);

/*
 * Add data for the key to the table.
 * 
 * Return NULL if data was added successfully.
 * 
 * If key was already added, return pointer to the data, which already associated
 * with the key.
 * 
 * Return ERR_PTR() on error.
 */
void*
kedr_coi_hash_table_add(struct kedr_coi_hash_table* table,
    const void* key,
    void* data);

/*
 * Return data, previousely added to the table.
 * 
 * Return ERR_PTR() on fail.
 * 
 * Return ERR_PTR(-ENOENT) if key was not registered.
 */
void*
kedr_coi_hash_table_get(struct kedr_coi_hash_table* table,
    const void* key);

/*
 * Remove key from the hash table and return data associated with it.
 * 
 * Return ERR_PTR(-ENOENT) if key was not registered.
 */
void* kedr_coi_hash_table_remove(struct kedr_coi_hash_table* table,
    const void* key);

/*
 * Destroy table.
 * 
 * Call 'free_data' for every data in this table.
 * 
 * Note, that normally table is empty at this stage,
 * so free_data is allowed to print warnings.
 */

void kedr_coi_hash_table_destroy(struct kedr_coi_hash_table* table,
    void (*free_data)(void* data, const void* key, void *user_data),
    void* user_data);

#endif /* KEDR_COI_HASH_TABLE_INTERNAL_H */