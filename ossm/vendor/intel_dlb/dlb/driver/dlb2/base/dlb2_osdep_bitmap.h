/* SPDX-License-Identifier: GPL-2.0-only
 * Copyright(c) 2017-2020 Intel Corporation
 */

#ifndef __DLB2_OSDEP_BITMAP_H
#define __DLB2_OSDEP_BITMAP_H

#include <linux/bitmap.h>
#include <linux/slab.h>

#include "../dlb2_main.h"

/*************************/
/*** Bitmap operations ***/
/*************************/
struct dlb2_bitmap {
	unsigned long *map;
	unsigned int len;
};

/**
 * dlb2_bitmap_alloc() - alloc a bitmap data structure
 * @bitmap: pointer to dlb2_bitmap structure pointer.
 * @len: number of entries in the bitmap.
 *
 * This function allocates a bitmap and initializes it with length @len. All
 * entries are initially zero.
 *
 * Return:
 * Returns 0 upon success, < 0 otherwise.
 *
 * Errors:
 * EINVAL - bitmap is NULL or len is 0.
 * ENOMEM - could not allocate memory for the bitmap data structure.
 */
static inline int dlb2_bitmap_alloc(struct dlb2_bitmap **bitmap,
				    unsigned int len)
{
	struct dlb2_bitmap *bm;

	if (!bitmap || len == 0)
		return -EINVAL;

	bm = kzalloc(sizeof(*bm), GFP_KERNEL);
	if (!bm)
		return -ENOMEM;

	bm->map = kcalloc(BITS_TO_LONGS(len), sizeof(*bm->map), GFP_KERNEL);
	if (!bm->map) {
		kfree(bm);
		return -ENOMEM;
	}

	bm->len = len;

	*bitmap = bm;

	return 0;
}

/**
 * dlb2_bitmap_free() - free a previously allocated bitmap data structure
 * @bitmap: pointer to dlb2_bitmap structure.
 *
 * This function frees a bitmap that was allocated with dlb2_bitmap_alloc().
 */
static inline void dlb2_bitmap_free(struct dlb2_bitmap *bitmap)
{
	if (!bitmap)
		return;

	kfree(bitmap->map);

	kfree(bitmap);
}

/**
 * dlb2_bitmap_fill() - fill a bitmap with all 1s
 * @bitmap: pointer to dlb2_bitmap structure.
 *
 * This function sets all bitmap values to 1.
 *
 * Return:
 * Returns 0 upon success, < 0 otherwise.
 *
 * Errors:
 * EINVAL - bitmap is NULL or is uninitialized.
 */
static inline int dlb2_bitmap_fill(struct dlb2_bitmap *bitmap)
{
	if (!bitmap || !bitmap->map)
		return -EINVAL;

	bitmap_fill(bitmap->map, bitmap->len);

	return 0;
}

/**
 * dlb2_bitmap_fill() - fill a bitmap with all 0s
 * @bitmap: pointer to dlb2_bitmap structure.
 *
 * This function sets all bitmap values to 0.
 *
 * Return:
 * Returns 0 upon success, < 0 otherwise.
 *
 * Errors:
 * EINVAL - bitmap is NULL or is uninitialized.
 */
static inline int dlb2_bitmap_zero(struct dlb2_bitmap *bitmap)
{
	if (!bitmap || !bitmap->map)
		return -EINVAL;

	bitmap_zero(bitmap->map, bitmap->len);

	return 0;
}

/**
 * dlb2_bitmap_set_range() - set a range of bitmap entries
 * @bitmap: pointer to dlb2_bitmap structure.
 * @bit: starting bit index.
 * @len: length of the range.
 *
 * Return:
 * Returns 0 upon success, < 0 otherwise.
 *
 * Errors:
 * EINVAL - bitmap is NULL or is uninitialized, or the range exceeds the bitmap
 *	    length.
 */
static inline int dlb2_bitmap_set_range(struct dlb2_bitmap *bitmap,
					unsigned int bit,
					unsigned int len)
{
	if (!bitmap || !bitmap->map)
		return -EINVAL;

	if (bitmap->len <= bit)
		return -EINVAL;

	bitmap_set(bitmap->map, bit, len);

	return 0;
}

/**
 * dlb2_bitmap_clear_range() - clear a range of bitmap entries
 * @bitmap: pointer to dlb2_bitmap structure.
 * @bit: starting bit index.
 * @len: length of the range.
 *
 * Return:
 * Returns 0 upon success, < 0 otherwise.
 *
 * Errors:
 * EINVAL - bitmap is NULL or is uninitialized, or the range exceeds the bitmap
 *	    length.
 */
static inline int dlb2_bitmap_clear_range(struct dlb2_bitmap *bitmap,
					  unsigned int bit,
					  unsigned int len)
{
	if (!bitmap || !bitmap->map)
		return -EINVAL;

	if (bitmap->len <= bit)
		return -EINVAL;

	bitmap_clear(bitmap->map, bit, len);

	return 0;
}

/**
 * dlb2_bitmap_find_set_bit_range() - find an range of set bits
 * @bitmap: pointer to dlb2_bitmap structure.
 * @len: length of the range.
 *
 * This function looks for a range of set bits of length @len.
 *
 * Return:
 * Returns the base bit index upon success, < 0 otherwise.
 *
 * Errors:
 * ENOENT - unable to find a length *len* range of set bits.
 * EINVAL - bitmap is NULL or is uninitialized, or len is invalid.
 */
static inline int dlb2_bitmap_find_set_bit_range(struct dlb2_bitmap *bitmap,
						 unsigned int len)
{
	struct dlb2_bitmap *complement_mask = NULL;
	int ret;

	if (!bitmap || !bitmap->map || len == 0)
		return -EINVAL;

	if (bitmap->len < len)
		return -ENOENT;

	ret = dlb2_bitmap_alloc(&complement_mask, bitmap->len);
	if (ret)
		return ret;

	dlb2_bitmap_zero(complement_mask);

	bitmap_complement(complement_mask->map, bitmap->map, bitmap->len);

	ret = bitmap_find_next_zero_area(complement_mask->map,
					 complement_mask->len,
					 0,
					 len,
					 0);

	dlb2_bitmap_free(complement_mask);

	/* No set bit range of length len? */
	return (ret >= (int)bitmap->len) ? -ENOENT : ret;
}

/**
 * dlb2_bitmap_find_set_bit() - find an range of set bits
 * @bitmap: pointer to dlb2_bitmap structure.
 *
 * This function looks for a single set bit.
 *
 * Return:
 * Returns the base bit index upon success, -1 if not found, <-1 otherwise.
 *
 * Errors:
 * EINVAL - bitmap is NULL or is uninitialized, or len is invalid.
 */
static inline int dlb2_bitmap_find_set_bit(struct dlb2_bitmap *bitmap)
{
	return dlb2_bitmap_find_set_bit_range(bitmap, 1);
}

/**
 * dlb2_bitmap_count() - returns the number of set bits
 * @bitmap: pointer to dlb2_bitmap structure.
 *
 * This function looks for a single set bit.
 *
 * Return:
 * Returns the number of set bits upon success, <0 otherwise.
 *
 * Errors:
 * EINVAL - bitmap is NULL or is uninitialized.
 */
static inline int dlb2_bitmap_count(struct dlb2_bitmap *bitmap)
{
	if (!bitmap || !bitmap->map)
		return -EINVAL;

	return bitmap_weight(bitmap->map, bitmap->len);
}

/**
 * dlb2_bitmap_longest_set_range() - returns longest contiguous range of set
 *				      bits
 * @bitmap: pointer to dlb2_bitmap structure.
 *
 * Return:
 * Returns the bitmap's longest contiguous range of of set bits upon success,
 * <0 otherwise.
 *
 * Errors:
 * EINVAL - bitmap is NULL or is uninitialized.
 */
static inline int dlb2_bitmap_longest_set_range(struct dlb2_bitmap *bitmap)
{
	unsigned int bits_per_long;
	unsigned int i, j;
	int max_len, len;

	if (!bitmap || !bitmap->map)
		return -EINVAL;

	if (dlb2_bitmap_count(bitmap) == 0)
		return 0;

	max_len = 0;
	len = 0;
	bits_per_long = sizeof(unsigned long) * BITS_PER_BYTE;

	for (i = 0; i < BITS_TO_LONGS(bitmap->len); i++) {
		for (j = 0; j < bits_per_long; j++) {
			if ((i * bits_per_long + j) >= bitmap->len)
				break;

			len = (test_bit(j, &bitmap->map[i])) ? len + 1 : 0;

			if (len > max_len)
				max_len = len;
		}
	}

	return max_len;
}

/**
 * dlb2_bitmap_or() - store the logical 'or' of two bitmaps into a third
 * @dest: pointer to dlb2_bitmap structure, which will contain the results of
 *	  the 'or' of src1 and src2.
 * @src1: pointer to dlb2_bitmap structure, will be 'or'ed with src2.
 * @src2: pointer to dlb2_bitmap structure, will be 'or'ed with src1.
 *
 * This function 'or's two bitmaps together and stores the result in a third
 * bitmap. The source and destination bitmaps can be the same.
 *
 * Return:
 * Returns the number of set bits upon success, <0 otherwise.
 *
 * Errors:
 * EINVAL - One of the bitmaps is NULL or is uninitialized.
 */
static inline int dlb2_bitmap_or(struct dlb2_bitmap *dest,
				 struct dlb2_bitmap *src1,
				 struct dlb2_bitmap *src2)
{
	unsigned int min;

	if (!dest || !dest->map ||
	    !src1 || !src1->map ||
	    !src2 || !src2->map)
		return -EINVAL;

	min = dest->len;
	min = (min > src1->len) ? src1->len : min;
	min = (min > src2->len) ? src2->len : min;

	bitmap_or(dest->map, src1->map, src2->map, min);

	return 0;
}

/**
 * dlb2_bitmap_find_nth_set_bit() - find the last set bit upto n
 * @bitmap: pointer to dlb2_bitmap structure.
 *
 * This function looks for the nth set bit or the last set bit < n.
 * n = 0 => 1st set bit
 *
 * Return:
 * Returns the base bit index upon success, < 0 otherwise.
 *
 * Errors:
 * EINVAL - bitmap is NULL or is uninitialized, or len is invalid.
 */
static inline int dlb2_bitmap_find_nth_set_bit(struct dlb2_bitmap *bitmap, int n)
{
	int cnt = 0, idx = -1;

	if (!bitmap || !bitmap->map)
		return -EINVAL;

	if (bitmap->len <= n || bitmap_empty(bitmap->map, bitmap->len))
		return -ENOENT;

	if (bitmap_weight(bitmap->map, bitmap->len) <= n + 1)
		return find_last_bit(bitmap->map, bitmap->len);

	while (cnt++ <= n)
		idx = find_next_bit(bitmap->map, bitmap->len, idx + 1);

	return idx;
}
#endif /*  __DLB2_OSDEP_BITMAP_H */
