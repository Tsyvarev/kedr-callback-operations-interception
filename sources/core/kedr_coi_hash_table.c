/*
 * Implementation of hash table used by instrumentors.
 */

/* ========================================================================
 * Copyright (C) 2011, Andrey V. Tsyvarev  <tsyvarev@ispras.ru>
 *
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 ======================================================================== */
 
#include "kedr_coi_hash_table_internal.h"

#include <linux/list.h> /* hash table organization */
#include <linux/hash.h> /* hash function for pointers */
#include <linux/slab.h> /* kmalloc */
#include <linux/spinlock.h> /* spinlock */

// Initial value of bits in the table
#define BITS_DEFAULT 4

#define BITS_MIN 1

#define SIZE_DEFAULT (1 << BITS_DEFAULT)

#define SIZE_MIN (1 << BITS_MIN)

static inline unsigned long hash_function(const char* key, unsigned int bits)
{
    // really hash_ptr process first argument as unsigned long, so
    // its constantness has no sence
    return hash_ptr((void*)key, bits);
}

static void init_hlist_heads(struct hlist_head* heads, unsigned int bits)
{
    int i;
    for(i = 0; i < (1 << bits); i++)
        INIT_HLIST_HEAD(&heads[i]);
}

struct hash_table_elem
{
    struct hlist_node node;
    
    const void* key;
    void* data;
};

struct kedr_coi_hash_table
{
    struct hlist_head* heads;
    
    unsigned int bits;
    // Current number of elements
    size_t n_elems;

    spinlock_t lock;

};

// Should be executed with lock taken
static int kedr_coi_hash_table_realloc(
    struct kedr_coi_hash_table* table, unsigned int bits_new);

// Whether need to expand table before adding new element
static int is_need_expand(struct kedr_coi_hash_table* table);
// Whether need to narrow table (called after removing element)
static int is_need_narrow(struct kedr_coi_hash_table* table);

struct kedr_coi_hash_table* kedr_coi_hash_table_create(void)
{
    struct kedr_coi_hash_table* table =
        kmalloc(sizeof(*table), GFP_KERNEL);
    
    if(table == NULL)
    {
        pr_err("Failed to allocate hash table structure");
        
        return NULL;
    }
    
    table->bits = BITS_DEFAULT;
    
    table->heads = kmalloc(sizeof(*table->heads) * SIZE_DEFAULT, GFP_ATOMIC);
    
    if(table->heads == NULL)
    {
        pr_err("Failed to allocate head nodes for hash table.");
        
        kfree(table);
        
        return NULL;
    }
    
    init_hlist_heads(table->heads, BITS_DEFAULT);

    spin_lock_init(&table->lock);
    
    table->n_elems = 0;
    
    return table;
}

/*
 * Add element to the table.
 * 
 * Return NULL or old element or PTR_ERR.
 * 
 * Should be executed under lock.
 */

static struct hash_table_elem*
hash_table_add_elem(struct kedr_coi_hash_table* table, struct hash_table_elem* elem);

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
    void* data)
{
    unsigned long flags;

    struct hash_table_elem* elem_new;
    struct hash_table_elem* elem;
    
    elem_new = kmalloc(sizeof(*elem_new), GFP_KERNEL);

    if(elem_new == NULL)
    {
        return ERR_PTR(-ENOMEM);
    }
    
    INIT_HLIST_NODE(&elem_new->node);
    elem_new->key = key;
    elem_new->data = data;
    
    spin_lock_irqsave(&table->lock, flags);
    
    elem = hash_table_add_elem(table, elem_new);
    /*
     *  Element cannot be removing during addition
     * (it should be enforced by the caller),
     * so we may work with it outside critical section.
     */
    spin_unlock_irqrestore(&table->lock, flags);

    if(IS_ERR(elem))
    {
        kfree(elem_new);
        return ERR_PTR(PTR_ERR(elem));
    }
    else if(elem != NULL)
    {
        kfree(elem_new);
        return elem->data;
    }
    else
    {
        return NULL;
    }
}

static struct hash_table_elem*
hash_table_get_elem(struct kedr_coi_hash_table* table, const void* key);

/*
 * Return data, previousely added to the table.
 * 
 * Return ERR_PTR() on fail.
 * 
 * Return ERR_PTR(-ENOENT) if key was not registered.
 */
void*
kedr_coi_hash_table_get(struct kedr_coi_hash_table* table,
    const void* key)
{
    unsigned long flags;

    struct hash_table_elem* elem;
    
    spin_lock_irqsave(&table->lock, flags);

    elem = hash_table_get_elem(table, key);
    
    spin_unlock_irqrestore(&table->lock, flags);
    
    if(IS_ERR(elem))
    {
        return ERR_PTR(PTR_ERR(elem));
    }
    else
        return elem->data;
}

static void* hash_table_remove(struct kedr_coi_hash_table* table,
    const void* key);

void* kedr_coi_hash_table_remove(struct kedr_coi_hash_table* table,
    const void* key)
{
    unsigned long flags;

    void* data;
    
    spin_lock_irqsave(&table->lock, flags);

    data = hash_table_remove(table, key);
    
    spin_unlock_irqrestore(&table->lock, flags);
    
    return data;
}

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
    void* user_data)
{
    int i;
    
    struct hlist_head* heads = table->heads;
    
    for(i = 0; i < (1 << table->bits); i++)
    {
        struct hlist_head* head = &heads[i];
        
        while(!hlist_empty(head))
        {
            void* data;
            const void* key;
            struct hash_table_elem* elem =
                hlist_entry(head->first, struct hash_table_elem, node);
            data = elem->data;
            key = elem->key;
            hlist_del(&elem->node);
            kfree(elem);
            if(free_data)
                free_data(data, key, user_data);
        }
    }
    
    kfree(heads);
    kfree(table);
}

/* Implementation of auxiliary functions */

static void hash_table_fill_from(
    struct hlist_head* heads_new, unsigned int bits_new,
    struct hlist_head* heads_old, unsigned int bits_old)
{
    int i;
    
    for(i = 0; i < (1 << bits_old); i++)
    {
        struct hlist_head* head_old = &heads_old[i];
        while(!hlist_empty(head_old))
        {
            struct hash_table_elem* elem =
                hlist_entry(head_old->first, struct hash_table_elem, node);
            
            hlist_del(&elem->node);
            hlist_add_head(&elem->node, &heads_new[hash_function(elem->key, bits_new)]);
        }
        
    }
}

static int kedr_coi_hash_table_realloc_slowpath(
    struct kedr_coi_hash_table* table, unsigned int bits_new)
{
    // Narrow table to minimum size, free old heads and try to allocate new
    struct hlist_head* heads_min =    
        kmalloc(sizeof(*heads_min) * SIZE_MIN, GFP_ATOMIC);
    if(heads_min == NULL) return -ENOMEM;
    
    init_hlist_heads(heads_min, BITS_MIN);
    
    hash_table_fill_from(heads_min, BITS_MIN, table->heads, table->bits);

    kfree(table->heads);
    table->heads = kmalloc(sizeof(*table->heads) * (1 << bits_new), GFP_ATOMIC);
    
    if(table->heads == NULL)
    {
        // Table become with min size
        table->heads = heads_min;
        table->bits = BITS_MIN;
        
        return -ENOMEM;
    }

    init_hlist_heads(table->heads, bits_new);
    
    hash_table_fill_from(table->heads, bits_new, heads_min, BITS_MIN);
    
    table->bits = bits_new;
    
    kfree(heads_min);
    
    return 0;
}

int kedr_coi_hash_table_realloc(
    struct kedr_coi_hash_table* table, unsigned int bits_new)
{
    struct hlist_head* heads_new =
        kmalloc(sizeof(*heads_new) * (1 << bits_new), GFP_ATOMIC);
    
    if(heads_new == NULL)
    {
        if((table->bits > bits_new) && (bits_new != BITS_MIN))
        {
            // Slowpath for narrowing table
            return kedr_coi_hash_table_realloc_slowpath(table, bits_new);
        }
        // For expanding table there is no slowpath, table remains as is.
        return -ENOMEM;
    }
    
    init_hlist_heads(heads_new, bits_new);
    
    hash_table_fill_from(heads_new, bits_new, table->heads, table->bits);

    kfree(table->heads);

    table->bits = bits_new;
    table->heads = heads_new;

    return 0;
}

int is_need_expand(struct kedr_coi_hash_table* table)
{
    //not implemented yet
    return 0;
}

int is_need_narrow(struct kedr_coi_hash_table* table)
{
    //not implemented yet
    return 0;
}

struct hash_table_elem*
hash_table_add_elem(struct kedr_coi_hash_table* table, struct hash_table_elem* elem_new)
{
    struct hlist_head* head;
    struct hash_table_elem* elem;
    struct hlist_node* node_tmp;
    
    BUG_ON(table == NULL);
    BUG_ON(elem_new == NULL);
    
    head = &table->heads[hash_function(elem_new->key, table->bits)];
    
    hlist_for_each_entry(elem, node_tmp, head, node)
    {
        if(elem->key == elem_new->key) return elem;
    }

    if(is_need_expand(table))
    {
        kedr_coi_hash_table_realloc(table, table->bits + 1);
        head = &table->heads[hash_function(elem_new->key, table->bits)];
    }
    
    hlist_add_head(&elem_new->node, head);
    table->n_elems++;
    
    return NULL;
}

struct hash_table_elem*
hash_table_get_elem(struct kedr_coi_hash_table* table, const void* key)
{
    struct hlist_head* head;
    struct hash_table_elem* elem;
    struct hlist_node* node_tmp;
    
    head = &table->heads[hash_function(key, table->bits)];
    
    hlist_for_each_entry(elem, node_tmp, head, node)
    {
        if(elem->key == key) return elem;
    }
    
    return ERR_PTR(-ENOENT);
}

void* hash_table_remove(struct kedr_coi_hash_table* table,
    const void* key)
{
    struct hlist_head* head;
    struct hash_table_elem* elem;
    struct hlist_node* node_tmp;
    
    head = &table->heads[hash_function(key, table->bits)];
    
    hlist_for_each_entry(elem, node_tmp, head, node)
    {
        if(elem->key == key)
        {
            void* data = elem->data;
            hlist_del(&elem->node);
            kfree(elem);
            table->n_elems--;
            if(is_need_narrow(table))
                kedr_coi_hash_table_realloc(table, table->bits - 1);
            
            return data;
        }
    }
    
    return ERR_PTR(-ENOENT);
}