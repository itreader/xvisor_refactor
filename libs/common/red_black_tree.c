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
 * @file red_black_tree.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief Implementation of Red Black Trees.
 *
 * The source has been largely adapted from:
 * linux-xxx/lib/red_black_tree.c
 *
 * Red Black Trees
 * (C) 1999  Andrea Arcangeli <andrea@suse.de>
 * (C) 2002  David Woodhouse <dwmw2@infradead.org>
 * (C) 2012  Michel Lespinasse <walken@google.com>
 *
 * The original code is licensed under the GPL.
 */

#include <libs/red_black_tree_augmented.h>

/*
 * red-black trees properties:  http://en.wikipedia.org/wiki/Rbtree
 *
 *  1) A node is either red or black
 *  2) The root is black
 *  3) All leaves (NULL) are black
 *  4) Both children of every red node are black
 *  5) Every simple path from root to leaves contains the same number
 *     of black nodes.
 *
 *  4 and 5 give the O(log n) guarantee, since 4 implies you cannot have two
 *  consecutive red nodes in a path and every red node is therefore followed by
 *  a black. So if B is the number of black nodes on every simple path (as per
 *  5), then the longest possible path due to 4 is 2B.
 *
 *  We shall indicate color with case, where black nodes are uppercase and red
 *  nodes will be lowercase. Unknown color nodes shall be drawn as red within
 *  parentheses and have some accompanying text comment.
 */

static inline void rb_set_black(red_black_node_t *rb)
{
    rb->red_black_parent_color |= RB_BLACK;
}

static inline red_black_node_t *rb_red_parent(red_black_node_t *red)
{
    return (red_black_node_t *)red->red_black_parent_color;
}

/*
 * Helper function for rotations:
 * - old's parent and color get assigned to new
 * - old gets assigned new as a parent and 'color' as a color.
 */
static inline void __rb_rotate_set_parents(red_black_node_t *old, red_black_node_t *new, red_black_root_t *root, int color)
{
    red_black_node_t *parent = rb_parent(old);
    new->red_black_parent_color = old->red_black_parent_color;
    rb_set_parent_color(old, new, color);
    __rb_change_child(old, new, parent, root);
}

static inline void __rb_insert(
    red_black_node_t *node, red_black_root_t *root, void (*augment_rotate)(red_black_node_t *old, red_black_node_t *new))
{
    red_black_node_t *parent = rb_red_parent(node), *gparent, *tmp;

    while (true) {
        /*
         * Loop invariant: node is red
         *
         * If there is a black parent, we are done.
         * Otherwise, take some corrective action as we don't
         * want a red root or two consecutive red nodes.
         */
        if (!parent) {
            rb_set_parent_color(node, NULL, RB_BLACK);
            break;
        } else if (rb_is_black(parent)) {
            break;
        }

        gparent = rb_red_parent(parent);

        tmp     = gparent->rb_right;

        if (parent != tmp) { /* parent == gparent->rb_left */
            if (tmp && rb_is_red(tmp)) {
                /*
                 * Case 1 - color flips
                 *
                 *       G            g
                 *      / \          / \
                 *     p   u  -->   P   U
                 *    /            /
                 *   n            N
                 *
                 * However, since g's parent might be red, and
                 * 4) does not allow this, we need to recurse
                 * at g.
                 */
                rb_set_parent_color(tmp, gparent, RB_BLACK);
                rb_set_parent_color(parent, gparent, RB_BLACK);
                node   = gparent;
                parent = rb_parent(node);
                rb_set_parent_color(node, parent, RB_RED);
                continue;
            }

            tmp = parent->rb_right;

            if (node == tmp) {
                /*
                 * Case 2 - left rotate at parent
                 *
                 *      G             G
                 *     / \           / \
                 *    p   U  -->    n   U
                 *     \           /
                 *      n         p
                 *
                 * This still leaves us in violation of 4), the
                 * continuation into Case 3 will fix that.
                 */
                parent->rb_right = tmp = node->rb_left;
                node->rb_left          = parent;

                if (tmp) {
                    rb_set_parent_color(tmp, parent, RB_BLACK);
                }

                rb_set_parent_color(parent, node, RB_RED);
                augment_rotate(parent, node);
                parent = node;
                tmp    = node->rb_right;
            }

            /*
             * Case 3 - right rotate at gparent
             *
             *        G           P
             *       / \         / \
             *      p   U  -->  n   g
             *     /                 \
             *    n                   U
             */
            gparent->rb_left = tmp; /* == parent->rb_right */
            parent->rb_right = gparent;

            if (tmp) {
                rb_set_parent_color(tmp, gparent, RB_BLACK);
            }

            __rb_rotate_set_parents(gparent, parent, root, RB_RED);
            augment_rotate(gparent, parent);
            break;
        } else {
            tmp = gparent->rb_left;

            if (tmp && rb_is_red(tmp)) {
                /* Case 1 - color flips */
                rb_set_parent_color(tmp, gparent, RB_BLACK);
                rb_set_parent_color(parent, gparent, RB_BLACK);
                node   = gparent;
                parent = rb_parent(node);
                rb_set_parent_color(node, parent, RB_RED);
                continue;
            }

            tmp = parent->rb_left;

            if (node == tmp) {
                /* Case 2 - right rotate at parent */
                parent->rb_left = tmp = node->rb_right;
                node->rb_right        = parent;

                if (tmp) {
                    rb_set_parent_color(tmp, parent, RB_BLACK);
                }

                rb_set_parent_color(parent, node, RB_RED);
                augment_rotate(parent, node);
                parent = node;
                tmp    = node->rb_left;
            }

            /* Case 3 - left rotate at gparent */
            gparent->rb_right = tmp; /* == parent->rb_left */
            parent->rb_left   = gparent;

            if (tmp) {
                rb_set_parent_color(tmp, gparent, RB_BLACK);
            }

            __rb_rotate_set_parents(gparent, parent, root, RB_RED);
            augment_rotate(gparent, parent);
            break;
        }
    }
}

/*
 * Inline version for rb_erase() use - we want to be able to inline
 * and eliminate the dummy_rotate callback there
 */
static inline void ____rb_erase_color(
    red_black_node_t *parent, red_black_root_t *root, void (*augment_rotate)(red_black_node_t *old, red_black_node_t *new))
{
    red_black_node_t *node = NULL, *sibling, *tmp1, *tmp2;

    while (true) {
        /*
         * Loop invariants:
         * - node is black (or NULL on first iteration)
         * - node is not the root (parent is not NULL)
         * - All leaf paths going through parent and node have a
         *   black node count that is 1 lower than other leaf paths.
         */
        sibling = parent->rb_right;

        if (node != sibling) { /* node == parent->rb_left */
            if (rb_is_red(sibling)) {
                /*
                 * Case 1 - left rotate at parent
                 *
                 *     P               S
                 *    / \             / \
                 *   N   s    -->    p   Sr
                 *      / \         / \
                 *     Sl  Sr      N   Sl
                 */
                parent->rb_right = tmp1 = sibling->rb_left;
                sibling->rb_left        = parent;
                rb_set_parent_color(tmp1, parent, RB_BLACK);
                __rb_rotate_set_parents(parent, sibling, root, RB_RED);
                augment_rotate(parent, sibling);
                sibling = tmp1;
            }

            tmp1 = sibling->rb_right;

            if (!tmp1 || rb_is_black(tmp1)) {
                tmp2 = sibling->rb_left;

                if (!tmp2 || rb_is_black(tmp2)) {
                    /*
                     * Case 2 - sibling color flip
                     * (p could be either color here)
                     *
                     *    (p)           (p)
                     *    / \           / \
                     *   N   S    -->  N   s
                     *      / \           / \
                     *     Sl  Sr        Sl  Sr
                     *
                     * This leaves us violating 5) which
                     * can be fixed by flipping p to black
                     * if it was red, or by recursing at p.
                     * p is red when coming from Case 1.
                     */
                    rb_set_parent_color(sibling, parent, RB_RED);

                    if (rb_is_red(parent)) {
                        rb_set_black(parent);
                    } else {
                        node   = parent;
                        parent = rb_parent(node);

                        if (parent) {
                            continue;
                        }
                    }

                    break;
                }

                /*
                 * Case 3 - right rotate at sibling
                 * (p could be either color here)
                 *
                 *   (p)           (p)
                 *   / \           / \
                 *  N   S    -->  N   Sl
                 *     / \             \
                 *    sl  Sr            s
                 *                       \
                 *                        Sr
                 */
                sibling->rb_left = tmp1 = tmp2->rb_right;
                tmp2->rb_right          = sibling;
                parent->rb_right        = tmp2;

                if (tmp1) {
                    rb_set_parent_color(tmp1, sibling, RB_BLACK);
                }

                augment_rotate(sibling, tmp2);
                tmp1    = sibling;
                sibling = tmp2;
            }

            /*
             * Case 4 - left rotate at parent + color flips
             * (p and sl could be either color here.
             *  After rotation, p becomes black, s acquires
             *  p's color, and sl keeps its color)
             *
             *      (p)             (s)
             *      / \             / \
             *     N   S     -->   P   Sr
             *        / \         / \
             *      (sl) sr      N  (sl)
             */
            parent->rb_right = tmp2 = sibling->rb_left;
            sibling->rb_left        = parent;
            rb_set_parent_color(tmp1, sibling, RB_BLACK);

            if (tmp2) {
                rb_set_parent(tmp2, parent);
            }

            __rb_rotate_set_parents(parent, sibling, root, RB_BLACK);
            augment_rotate(parent, sibling);
            break;
        } else {
            sibling = parent->rb_left;

            if (rb_is_red(sibling)) {
                /* Case 1 - right rotate at parent */
                parent->rb_left = tmp1 = sibling->rb_right;
                sibling->rb_right      = parent;
                rb_set_parent_color(tmp1, parent, RB_BLACK);
                __rb_rotate_set_parents(parent, sibling, root, RB_RED);
                augment_rotate(parent, sibling);
                sibling = tmp1;
            }

            tmp1 = sibling->rb_left;

            if (!tmp1 || rb_is_black(tmp1)) {
                tmp2 = sibling->rb_right;

                if (!tmp2 || rb_is_black(tmp2)) {
                    /* Case 2 - sibling color flip */
                    rb_set_parent_color(sibling, parent, RB_RED);

                    if (rb_is_red(parent)) {
                        rb_set_black(parent);
                    } else {
                        node   = parent;
                        parent = rb_parent(node);

                        if (parent) {
                            continue;
                        }
                    }

                    break;
                }

                /* Case 3 - right rotate at sibling */
                sibling->rb_right = tmp1 = tmp2->rb_left;
                tmp2->rb_left            = sibling;
                parent->rb_left          = tmp2;

                if (tmp1) {
                    rb_set_parent_color(tmp1, sibling, RB_BLACK);
                }

                augment_rotate(sibling, tmp2);
                tmp1    = sibling;
                sibling = tmp2;
            }

            /* Case 4 - left rotate at parent + color flips */
            parent->rb_left = tmp2 = sibling->rb_right;
            sibling->rb_right      = parent;
            rb_set_parent_color(tmp1, sibling, RB_BLACK);

            if (tmp2) {
                rb_set_parent(tmp2, parent);
            }

            __rb_rotate_set_parents(parent, sibling, root, RB_BLACK);
            augment_rotate(parent, sibling);
            break;
        }
    }
}

/* Non-inline version for rb_erase_augmented() use */
void __rb_erase_color(
    red_black_node_t *parent, red_black_root_t *root, void (*augment_rotate)(red_black_node_t *old, red_black_node_t *new))
{
    ____rb_erase_color(parent, root, augment_rotate);
}

/*
 * Non-augmented red_black_tree manipulation functions.
 *
 * We use dummy augmented callbacks here, and have the compiler optimize them
 * out of the rb_insert_color() and rb_erase() function definitions.
 */

static inline void dummy_propagate(red_black_node_t *node, red_black_node_t *stop) {}

static inline void dummy_copy(red_black_node_t *old, red_black_node_t *new) {}

static inline void dummy_rotate(red_black_node_t *old, red_black_node_t *new) {}

static const struct rb_augment_callbacks dummy_callbacks = {dummy_propagate, dummy_copy, dummy_rotate};

void rb_insert_color(red_black_node_t *node, red_black_root_t *root)
{
    __rb_insert(node, root, dummy_rotate);
}

void rb_erase(red_black_node_t *node, red_black_root_t *root)
{
    red_black_node_t *rebalance;
    rebalance = __rb_erase_augmented(node, root, &dummy_callbacks);

    if (rebalance) {
        ____rb_erase_color(rebalance, root, dummy_rotate);
    }
}

/*
 * Augmented red_black_tree manipulation functions.
 *
 * This instantiates the same __always_inline functions as in the non-augmented
 * case, but this time with user-defined callbacks.
 */

void __rb_insert_augmented(
    red_black_node_t *node, red_black_root_t *root, void (*augment_rotate)(red_black_node_t *old, red_black_node_t *new))
{
    __rb_insert(node, root, augment_rotate);
}

/*
 * This function returns the first node (in sort order) of the tree.
 */
red_black_node_t *rb_first(const red_black_root_t *root)
{
    red_black_node_t *n;

    n = root->red_black_node;

    if (!n) {
        return NULL;
    }

    while (n->rb_left) {
        n = n->rb_left;
    }

    return n;
}

red_black_node_t *rb_last(const red_black_root_t *root)
{
    red_black_node_t *n;

    n = root->red_black_node;

    if (!n) {
        return NULL;
    }

    while (n->rb_right) {
        n = n->rb_right;
    }

    return n;
}

red_black_node_t *rb_next(const red_black_node_t *node)
{
    red_black_node_t *parent;

    if (RB_EMPTY_NODE(node)) {
        return NULL;
    }

    /*
     * If we have a right-hand child, go down and then left as far
     * as we can.
     */
    if (node->rb_right) {
        node = node->rb_right;

        while (node->rb_left) {
            node = node->rb_left;
        }

        return (red_black_node_t *)node;
    }

    /*
     * No right-hand children. Everything down and left is smaller than us,
     * so any 'next' node must be in the general direction of our parent.
     * Go up the tree; any time the ancestor is a right-hand child of its
     * parent, keep going up. First time it's a left-hand child of its
     * parent, said parent is our 'next' node.
     */
    while ((parent = rb_parent(node)) && node == parent->rb_right) {
        node = parent;
    }

    return parent;
}

red_black_node_t *rb_prev(const red_black_node_t *node)
{
    red_black_node_t *parent;

    if (RB_EMPTY_NODE(node)) {
        return NULL;
    }

    /*
     * If we have a left-hand child, go down and then right as far
     * as we can.
     */
    if (node->rb_left) {
        node = node->rb_left;

        while (node->rb_right) {
            node = node->rb_right;
        }

        return (red_black_node_t *)node;
    }

    /*
     * No left-hand children. Go up till we find an ancestor which
     * is a right-hand child of its parent.
     */
    while ((parent = rb_parent(node)) && node == parent->rb_left) {
        node = parent;
    }

    return parent;
}

void rb_replace_node(red_black_node_t *victim, red_black_node_t *new, red_black_root_t *root)
{
    red_black_node_t *parent = rb_parent(victim);

    /* Set the surrounding nodes to point to the replacement */
    __rb_change_child(victim, new, parent, root);

    if (victim->rb_left) {
        rb_set_parent(victim->rb_left, new);
    }

    if (victim->rb_right) {
        rb_set_parent(victim->rb_right, new);
    }

    /* Copy the pointers/colour from the victim to the replacement */
    *new = *victim;
}

static red_black_node_t *rb_left_deepest_node(const red_black_node_t *node)
{
    for (;;) {
        if (node->rb_left) {
            node = node->rb_left;
        } else if (node->rb_right) {
            node = node->rb_right;
        } else {
            return (red_black_node_t *)node;
        }
    }
}

red_black_node_t *rb_next_postorder(const red_black_node_t *node)
{
    const red_black_node_t *parent;

    if (!node) {
        return NULL;
    }

    parent = rb_parent(node);

    /* If we're sitting on node, we've already seen our children */
    if (parent && node == parent->rb_left && parent->rb_right) {
        /* If we are the parent's left node, go to the parent's right
         * node then all the way down to the left */
        return rb_left_deepest_node(parent->rb_right);
    } else {
        /* Otherwise we are the parent's right node, and the parent
         * should be next */
        return (red_black_node_t *)parent;
    }
}

red_black_node_t *rb_first_postorder(const red_black_root_t *root)
{
    if (!root->red_black_node) {
        return NULL;
    }

    return rb_left_deepest_node(root->red_black_node);
}
