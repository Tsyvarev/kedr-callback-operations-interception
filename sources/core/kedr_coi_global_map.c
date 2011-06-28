/*
 * Implementation of global hash map
 * used by instrumentors for avoid collisions.
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
 
#include "kedr_coi_global_map_internal.h"

#include <linux/list.h> /* hash table organization */
#include <linux/hash.h> /* hash function for pointers */
#include <linux/slab.h> /* kmalloc */
#include <linux/spinlock.h> /* spinlock */

// Initial value of bits in the map
#define BITS_DEFAULT 4

// Minimal value of bits in the map
#define BITS_MIN 1

#define SIZE_DEFAULT (1 << BITS_DEFAULT)

#define SIZE_MIN (1 << BITS_MIN)

static void init_hlist_heads(struct hlist_head* heads, unsigned int bits)
{
    int i;
    for(i = 0; i < (1 << bits); i++)
        INIT_HLIST_HEAD(&heads[i]);
}

struct global_map_elem
{
    struct hlist_node node;
    
    void* key;
    // additional data may be here
};

struct global_map
{
    struct hlist_head* heads;
    
    unsigned int bits;
    // Current number of elements
    size_t n_elems;

    spinlock_t lock;

};

// Should be executed with lock taken
static int global_map_realloc(
    struct global_map* map, unsigned int bits_new);

// Whether need to expand map before adding new element
static int is_need_expand(struct global_map* map);
// Whether need to narrow map (called after removing element)
static int is_need_narrow(struct global_map* map);

static struct global_map* global_map_create(void)
{
    struct global_map* map =
        kmalloc(sizeof(*map), GFP_KERNEL);
    
    if(map == NULL)
    {
        pr_err("Failed to allocate global map structure");
        
        return NULL;
    }
    
    map->bits = BITS_DEFAULT;
    
    map->heads = kmalloc(sizeof(*map->heads) * SIZE_DEFAULT, GFP_ATOMIC);
    
    if(map->heads == NULL)
    {
        pr_err("Failed to allocate head nodes for global map.");
        
        kfree(map);
        
        return NULL;
    }
    
    init_hlist_heads(map->heads, BITS_DEFAULT);

    spin_lock_init(&map->lock);
    
    map->n_elems = 0;
    
    return map;
}

/*
 * Return 0 on success and negative error code on fail.
 * 
 * If key already added, return -EXIST.
 */

static int
global_map_add_key(struct global_map* map, void* key)
{
    unsigned long flags;

    struct global_map_elem* elem_new;
    struct global_map_elem* elem;
    struct hlist_head* head;
    struct hlist_node* node_tmp;

    elem_new = kmalloc(sizeof(*elem_new), GFP_KERNEL);

    if(elem_new == NULL)
    {
        return -ENOMEM;
    }
    
    INIT_HLIST_NODE(&elem_new->node);
    elem_new->key = key;
    
    spin_lock_irqsave(&map->lock, flags);

    if(is_need_expand(map))
    {
        global_map_realloc(map, map->bits + 1);
    }
    
    head = &map->heads[hash_ptr(key, map->bits)];
    
    hlist_for_each_entry(elem, node_tmp, head, node)
    {
        if(elem->key == key)
        {
            spin_unlock_irqrestore(&map->lock, flags);
            kfree(elem_new);
            return -EEXIST;
        }
    }
    
    hlist_add_head(&elem_new->node, head);
    map->n_elems++;
    
    spin_unlock_irqrestore(&map->lock, flags);

    return 0;
}

static void global_map_remove_key(struct global_map* map,
    void* key)
{
    struct hlist_head* head;
    struct global_map_elem* elem;
    struct hlist_node* node_tmp;

    unsigned long flags;

    spin_lock_irqsave(&map->lock, flags);
    
    head = &map->heads[hash_ptr(key, map->bits)];
    
    hlist_for_each_entry(elem, node_tmp, head, node)
    {
        if(elem->key == key)
        {
            hlist_del(&elem->node);
            kfree(elem);
            map->n_elems--;
            if(is_need_narrow(map))
                global_map_realloc(map, map->bits - 1);
            
            spin_unlock_irqrestore(&map->lock, flags);
            
            return;
        }
    }
    
    spin_unlock_irqrestore(&map->lock, flags);
    BUG();
}

static void global_map_destroy(struct global_map* map)
{
    int i;
    
    struct hlist_head* heads = map->heads;
    
    for(i = 0; i < (1 << map->bits); i++)
    {
        struct hlist_head* head = &heads[i];
        
        while(!hlist_empty(head))
        {
            struct global_map_elem* elem =
                hlist_entry(head->first, struct global_map_elem, node);
            hlist_del(&elem->node);
            pr_err("Undeleted global key: %p", elem->key);
            kfree(elem);
        }
    }
    
    kfree(heads);
    kfree(map);
}
// Public API
static struct global_map* map;

int kedr_coi_global_map_init(void)
{
    map = global_map_create();
    if(map == NULL)
    {
        return -ENOMEM;
    }
    
    return 0;
}

int kedr_coi_global_map_add_key(void* key)
{
    return global_map_add_key(map, key);
}

void kedr_coi_global_map_delete_key(void* key)
{
    return global_map_remove_key(map, key);
}

void kedr_coi_global_map_destroy(void)
{
    global_map_destroy(map);
}
// Implementation of auxiliary functions
static void global_map_fill_from(
    struct hlist_head* heads_new, unsigned int bits_new,
    struct hlist_head* heads_old, unsigned int bits_old)
{
    int i;
    
    for(i = 0; i < (1 << bits_old); i++)
    {
        struct hlist_head* head_old = &heads_old[i];
        while(!hlist_empty(head_old))
        {
            struct global_map_elem* elem =
                hlist_entry(head_old->first, struct global_map_elem, node);
            
            hlist_del(&elem->node);
            hlist_add_head(&elem->node, &heads_new[hash_ptr(elem->key, bits_new)]);
        }
        
    }
}

static int global_map_realloc_slowpath(
    struct global_map* map, unsigned int bits_new)
{
    // Narrow map to minimum size, free old heads and try to allocate new
    struct hlist_head* heads_min =    
        kmalloc(sizeof(*heads_min) * SIZE_MIN, GFP_ATOMIC);
    if(heads_min == NULL) return -ENOMEM;
    
    init_hlist_heads(heads_min, BITS_MIN);
    
    global_map_fill_from(heads_min, BITS_MIN, map->heads, map->bits);

    kfree(map->heads);
    map->heads = kmalloc(sizeof(*map->heads) * (1 << bits_new), GFP_ATOMIC);
    
    if(map->heads == NULL)
    {
        // Table become with min size
        map->heads = heads_min;
        map->bits = BITS_MIN;
        
        return -ENOMEM;
    }

    init_hlist_heads(map->heads, bits_new);
    
    global_map_fill_from(map->heads, bits_new, heads_min, BITS_MIN);
    
    map->bits = bits_new;
    
    kfree(heads_min);
    
    return 0;
}

int global_map_realloc(
    struct global_map* map, unsigned int bits_new)
{
    struct hlist_head* heads_new =
        kmalloc(sizeof(*heads_new) * (1 << bits_new), GFP_ATOMIC);
    
    if(heads_new == NULL)
    {
        if((map->bits > bits_new) && (bits_new != BITS_MIN))
        {
            // Slowpath for narrowing table
            return global_map_realloc_slowpath(map, bits_new);
        }
        // For expanding table there is no slowpath, table remains as is.
        return -ENOMEM;
    }
    
    init_hlist_heads(heads_new, bits_new);
    
    global_map_fill_from(heads_new, bits_new, map->heads, map->bits);

    kfree(map->heads);

    map->bits = bits_new;
    map->heads = heads_new;

    return 0;
}

int is_need_expand(struct global_map* map)
{
    //not implemented yet
    return 0;
}

int is_need_narrow(struct global_map* map)
{
    //not implemented yet
    return 0;
}