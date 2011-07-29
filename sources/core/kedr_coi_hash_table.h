#ifndef KEDR_COI_HASH_TABLE_H
#define KEDR_COI_HASH_TABLE_H
/*
 * General hash table, which is used for different purposes in KEDR COI.
 * 
 * Main features:
 * 
 * 0) Keys are pointers, hash function is hash_ptr()
 * 1) Dinamically change size (when number of elements became too big
 *     for fast search or too small and waste memory)
 * 2) Adding/removing/searching elements in the table may be performed in the atomic context.
 * 3) Not sync.(synchronization should be done by users)
 */
 
#include <linux/list.h> /* hash table organization */

/* Element of the hash table */
struct kedr_coi_hash_elem
{
    struct hlist_node node;
    // Readonly for outer usage
    const void* key;
};

/* Initialize hash table element and set key for it */
static inline void kedr_coi_hash_elem_init(struct kedr_coi_hash_elem* elem,
    const void* key)
{
    elem->key = key;
    INIT_HLIST_NODE(&elem->node);
}

/* Hash table itself */
struct kedr_coi_hash_table
{
    struct hlist_head* heads;
    // determine size of the table(1 << bits)
    unsigned int bits;
    // Current number of elements
    size_t n_elems;
};


/* Initialize hash table structure. */
int kedr_coi_hash_table_init(struct kedr_coi_hash_table* table);

/* Destroy hash table structure.
 * 
 * 'free_elem' will be called for each element in the table.
 * 
 * It is allowed to pass NULL pointer as free_elem in case when table is
 * definitely empty(e.g. table deleted before any element was added to it).
 * If 'free_elem' is NULL but table is not empty, warning will be printed.
 */
void kedr_coi_hash_table_destroy(struct kedr_coi_hash_table* table,
    void (*free_elem)(  struct kedr_coi_hash_elem* elem,
                        struct kedr_coi_hash_table* table));

/* Add element into table. Table will be extended if needed.
 * 
 * NOTE: Element will be added even element with same key already exists in the table.
 * 
 * Return 0 on success, negative error code on fail.
 */
int
kedr_coi_hash_table_add_elem(struct kedr_coi_hash_table* table,
    struct kedr_coi_hash_elem* elem);

/*
 * Remove element from the table. Table will be narrowed if needed.
 */
void
kedr_coi_hash_table_remove_elem(struct kedr_coi_hash_table* table,
    struct kedr_coi_hash_elem* elem);

/*
 * Search element in the table.
 * 
 * Return element with given key if it exists.
 * 
 * Return NULL if element doesn't exist.
 */
struct kedr_coi_hash_elem*
kedr_coi_hash_table_find_elem(struct kedr_coi_hash_table* table,
    const void* key);

#endif /* KEDR_COI_HASH_TABLE_H */
