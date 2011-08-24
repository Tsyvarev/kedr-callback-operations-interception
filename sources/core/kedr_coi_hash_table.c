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
 
#include "kedr_coi_hash_table.h"

#include <linux/list.h> /* hash table organization */
#include <linux/hash.h> /* hash function for pointers */
#include <linux/slab.h> /* kmalloc */
#include <linux/spinlock.h> /* spinlock */

// Initial value of bits in the table
#define BITS_DEFAULT 4

#define BITS_MIN 1

#define SIZE_DEFAULT (1 << BITS_DEFAULT)

#define SIZE_MIN (1 << BITS_MIN)

static inline unsigned long hash_function(const void* key, unsigned int bits)
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

/*
 * Reallocate array of heads for hash table.
 * 
 * May be executed in atomic context.
 * 
 * Return 0 on success and negative error code on fail.
 */

static int kedr_coi_hash_table_realloc(
    struct kedr_coi_hash_table* table, unsigned int bits_new);

// Whether need to expand table before adding new element
static int is_need_expand(struct kedr_coi_hash_table* table);
// Whether need to narrow table (called after removing element)
static int is_need_narrow(struct kedr_coi_hash_table* table);

int kedr_coi_hash_table_init(struct kedr_coi_hash_table* table)
{
    table->heads = kmalloc(sizeof(*table->heads) * SIZE_DEFAULT, GFP_ATOMIC);
    
    if(table->heads == NULL)
    {
        pr_err("Failed to allocate head nodes for hash table.");
        return -ENOMEM;
    }
    
    init_hlist_heads(table->heads, BITS_DEFAULT);

    table->bits = BITS_DEFAULT;
    table->n_elems = 0;
    
    return 0;
}

int kedr_coi_hash_table_add_elem(struct kedr_coi_hash_table* table,
    struct kedr_coi_hash_elem* elem)
{
    struct hlist_head* head;
    
    BUG_ON(table == NULL);
    BUG_ON(elem == NULL);
    
    if(is_need_expand(table))
    {
        kedr_coi_hash_table_realloc(table, table->bits + 1);
    }
    head = &table->heads[hash_function(elem->key, table->bits)];
    
    hlist_add_head(&elem->node, head);
    table->n_elems++;
    
    return 0;
}

struct kedr_coi_hash_elem*
kedr_coi_hash_table_find_elem(struct kedr_coi_hash_table* table,
    const void* key)
{
    struct kedr_coi_hash_elem* elem;
    struct hlist_head* head;
    struct hlist_node* node_tmp;
    
    head = &table->heads[hash_function(key, table->bits)];
    
    hlist_for_each_entry(elem, node_tmp, head, node)
    {
        if(elem->key == key) return elem;
    }
    
    return NULL;
}

void kedr_coi_hash_table_remove_elem(struct kedr_coi_hash_table* table,
    struct kedr_coi_hash_elem* elem)
{
    hlist_del(&elem->node);
    table->n_elems--;
    if(is_need_narrow(table))
        kedr_coi_hash_table_realloc(table, table->bits - 1);
}

void kedr_coi_hash_table_destroy(struct kedr_coi_hash_table* table,
    void (*free_elem)(struct kedr_coi_hash_elem* elem,
                        struct kedr_coi_hash_table* table))
{
    struct hlist_head* head_end = table->heads + (1 << table->bits);
    struct hlist_head* head;
    // Look for first non-deleted element
    for(head = table->heads; head < head_end; head++)
    {
        if(!hlist_empty(head))
        {
            if(free_elem == NULL)
            {
                pr_warning("Hash table %p wasn't freed before deleting.",
                    table);
                // go to the end of the cycle
                head = head_end;
                break;
            }
            else
            {
                break;
            }
        }
    }
    // Remove all non-deleted elements with function supplied.
    for(; head < head_end; head++)
    {
        while(!hlist_empty(head))
        {
            struct kedr_coi_hash_elem* elem =
                hlist_entry(head->first, struct kedr_coi_hash_elem, node);
            hlist_del(&elem->node);
            free_elem(elem, table);
        }
    }    
    kfree(table->heads);
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
            struct kedr_coi_hash_elem* elem =
                hlist_entry(head_old->first, struct kedr_coi_hash_elem, node);
            
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
