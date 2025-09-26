/* SPDX-License-Identifier: <Insert identifier here!!!>
 * Copyright(c) 2017-2020 Intel Corporation
 */

#ifndef __DLB2_OSDEP_LIST_H
#define __DLB2_OSDEP_LIST_H

/***********************/
/*** List operations ***/
/***********************/

struct dlb2_list_head {
};

struct dlb2_list_entry {
};

/**
 * dlb2_list_init_head() - initialize the head of a list
 * @head: list head
 */
static inline void dlb2_list_init_head(struct dlb2_list_head *head)
{
}

/**
 * dlb2_list_add() - add an entry to a list
 * @head: list head
 * @entry: new list entry
 */
static inline void
dlb2_list_add(struct dlb2_list_head *head, struct dlb2_list_entry *entry)
{
}

/**
 * dlb2_list_del() - delete an entry from a list
 * @entry: list entry
 * @head: list head
 */
static inline void dlb2_list_del(struct dlb2_list_head __always_unused *head,
				 struct dlb2_list_entry *entry)
{
}

/**
 * dlb2_list_empty() - check if a list is empty
 * @head: list head
 *
 * Return:
 * Returns 1 if empty, 0 if not.
 */
static inline int dlb2_list_empty(struct dlb2_list_head *head)
{
}

/**
 * DLB2_LIST_HEAD() - retrieve the head of the list
 * @head: list head
 * @type: type of the list variable
 * @name: name of the dlb2_list_entry field within the containing struct
 */
#define DLB2_LIST_HEAD(head, type, name)

/**
 * DLB2_LIST_FOR_EACH() - iterate over a list
 * @head: list head
 * @ptr: pointer to struct containing a struct dlb2_list_entry
 * @name: name of the dlb2_list_entry field within the containing struct
 * @iter: iterator variable
 */
#define DLB2_LIST_FOR_EACH(head, ptr, name, iter)

/**
 * DLB2_LIST_FOR_EACH_SAFE() - iterate over a list. This loop works even if
 * an element is removed from the list while processing it.
 * @ptr: pointer to struct containing a struct dlb2_list_entry
 * @ptr_tmp: pointer to struct containing a struct dlb2_list_entry (temporary)
 * @head: list head
 * @name: name of the dlb2_list_entry field within the containing struct
 * @iter: iterator variable
 * @iter_tmp: iterator variable (temporary)
 */
#define DLB2_LIST_FOR_EACH_SAFE(head, ptr, ptr_tmp, name, iter, iter_tmp)

#endif /*  __DLB2_OSDEP_LIST_H */
