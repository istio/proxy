/* SPDX-License-Identifier: <Insert identifier here!!!>
 * Copyright(c) 2017-2020 Intel Corporation
 */

#ifndef __DLB2_OSDEP_BITMAP_H
#define __DLB2_OSDEP_BITMAP_H

/*************************/
/*** Bitmap operations ***/
/*************************/
struct dlb2_bitmap {
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
static inline int dlb2_bitmap_alloc(struct dlb2_hw *hw,
				    struct dlb2_bitmap **bitmap,
				    unsigned int len)
{
}

/**
 * dlb2_bitmap_free() - free a previously allocated bitmap data structure
 * @bitmap: pointer to dlb2_bitmap structure.
 *
 * This function frees a bitmap that was allocated with dlb2_bitmap_alloc().
 */
static inline void dlb2_bitmap_free(struct dlb2_bitmap *bitmap)
{
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
}

#endif /*  __DLB2_OSDEP_BITMAP_H */
