/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017-2018 Intel Corporation
 */

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <getopt.h>
#include <fcntl.h>
#include <sys/mman.h>
#include "dlb.h"
#include "dump_dlb_regs.h"

#define CSR_BAR_SIZE  (4ULL * 1024ULL * 1024ULL * 1024ULL)
#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

static void dump_regs(uint8_t *base)
{
    int i;

    for (i = 0; i < ARRAY_SIZE(dlb2_regs); i++) {
        printf("%s 0x%08x 0x%08x\n",
               dlb2_regs[i].name,
               dlb2_regs[i].offset,
               *(uint32_t *)(base + dlb2_regs[i].offset));
    }
}

static struct option long_options[] = {
	{"device_id", optional_argument, 0, 'd'},
	{0, 0, 0, 0}
};

static void
usage(void)
{
	const char *usage_str =
		"  Usage: dump_dlb_regs [options]\n"
		"  Options:\n"
		"  -d, --device_id=N   Device ID (default: 0)\n"
		"\n";
	fprintf(stderr, "%s", usage_str);
	exit(1);
}

int main(int argc, char **argv)
{
    char path[] = "/sys/class/dlb2/dlb0!dlb/device/resource2";
    int option_index, c;
    int device_id = 0;
    uint8_t *base;
    int fd;

    for (;;) {
        c = getopt_long(argc, argv, "d:", long_options, &option_index);
        if (c == -1)
            break;

        switch (c) {
        case 'd':
            device_id = atoi(optarg);
            break;
        default:
            usage();
        }
    }

    snprintf(path,
             sizeof(path),
             "/sys/class/dlb2/dlb%d/device/resource2",
             device_id);

    fd = open(path, O_RDWR | O_SYNC);
    if (fd < 0) {
        perror("open");
        exit(-1);
    }

    base = mmap(0, CSR_BAR_SIZE, PROT_READ, MAP_SHARED, fd, 0);
    if (base == (void *) -1) {
        perror("mmap");
        exit(-1);
    }

    dump_regs(base);

    close(fd);

    return 0;
}
