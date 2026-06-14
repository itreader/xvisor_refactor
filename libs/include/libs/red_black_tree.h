/**
 * Copyright (c) 2014 Anup Patel.
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * @file red_black_tree.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief Interface for Red Back Trees.
 *
 * The header has been largely adapted from:
 * linux-xxx/include/linux/red_black_tree.h
 *
 * Red Black Trees
 * (C) 1999  Andrea Arcangeli <andrea@suse.de>
 *
 * To use red_black_trees you'll have to implement your own insert and search cores.
 * This will avoid us to use callbacks and to drop drammatically performances.
 * I know it's not the cleaner way,  but in C (not in C++) to get
 * performances and genericity...
 *
 * See <linux_source>/Documentation/red_black_tree.txt for documentation and samples.
 *
 * The original code is licensed under the GPL.
 */

#ifndef __RBTREE_H
#define __RBTREE_H

#include <vmm_types.h>

typedef struct red_black_node {
    uint64_t               red_black_parent_color;
    struct red_black_node *rb_right;
    struct red_black_node *rb_left;
} __attribute__((aligned(sizeof(long)))) red_black_node_t;

/* The alignment might seem pointless, but allegedly CRIS needs it */

typedef struct red_black_root {
    red_black_node_t *red_black_node;
} red_black_root_t;

#define rb_parent(r) ((red_black_node_t *)((r)->red_black_parent_color & ~3))

#define RB_ROOT   (red_black_root_t) { NULL}

#define rb_entry(ptr, type, member) container_of(ptr, type, member)

#define RB_EMPTY_ROOT(root)         ((root)->red_black_node == NULL)

/* 'empty' nodes are nodes that are known not to be inserted in an rbree */
#define RB_EMPTY_NODE(node)         ((node)->red_black_parent_color == (uint64_t)(node))
#define RB_CLEAR_NODE(node)         ((node)->red_black_parent_color = (uint64_t)(node))

extern void rb_insert_color(red_black_node_t *, red_black_root_t *);
extern void rb_erase(red_black_node_t *, red_black_root_t *);

/* Find logical next and previous nodes in a tree */
extern red_black_node_t *rb_next(const red_black_node_t *);
extern red_black_node_t *rb_prev(const red_black_node_t *);
extern red_black_node_t *rb_first(const red_black_root_t *);
extern red_black_node_t *rb_last(const red_black_root_t *);

/* Postorder iteration - always visit the parent after its children */
extern red_black_node_t *rb_first_postorder(const red_black_root_t *);
extern red_black_node_t *rb_next_postorder(const red_black_node_t *);

/* Fast replacement of a single node without remove/rebalance/add/rebalance */
extern void rb_replace_node(red_black_node_t *victim, red_black_node_t *new, red_black_root_t *root);

static inline void rb_link_node(red_black_node_t *node, red_black_node_t *parent, red_black_node_t **rb_link)
{
    node->red_black_parent_color = (uint64_t)parent;
    node->rb_left = node->rb_right = NULL;

    *rb_link                       = node;
}

#define rb_entry_safe(ptr, type, member)                                                                                                             \
    ({                                                                                                                                               \
        typeof(ptr) ____ptr = (ptr);                                                                                                                 \
        ____ptr ? rb_entry(____ptr, type, member) : NULL;                                                                                            \
    })

/**
 * red_black_tree_postorder_for_each_entry_safe - iterate over red_black_root in post order of
 * given type safe against removal of red_black_node entry
 *
 * @pos:    the 'type *' to use as a loop cursor.
 * @n:      another 'type *' to use as temporary storage
 * @root:   'red_black_root *' of the red_black_tree.
 * @field:  the name of the red_black_node field within 'type'.
 */
#define red_black_tree_postorder_for_each_entry_safe(pos, n, root, field)                                                                            \
    for (pos = rb_entry_safe(rb_first_postorder(root), typeof(*pos), field);                                                                         \
         pos && ({                                                                                                                                   \
             n = rb_entry_safe(rb_next_postorder(&pos->field), typeof(*pos), field);                                                                 \
             1;                                                                                                                                      \
         });                                                                                                                                         \
         pos = n)

#endif /* __RBTREE_H */
