/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017-2018 Intel Corporation
 */

#ifndef __EXAMPLE_BUFFER_H__
#define __EXAMPLE_BUFFER_H__

#ifdef __cplusplus
extern "C" {
#endif

#define METADATA_SIZE 27

typedef struct {
    uint8_t metadata[METADATA_SIZE];
} example_buffer_hdr_t;

typedef struct {
    example_buffer_hdr_t hdr;
    uint8_t data;
} example_buffer_t;

#define DATA_TO_HDR(addr) \
    ((void *) (addr - sizeof(example_buffer_hdr_t)))

#ifdef __cplusplus
}
#endif

#endif /* __EXAMPLE_BUFFER_H__ */
