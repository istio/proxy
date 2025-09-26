/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Intel Corporation
 */

#include <stdlib.h>
#include <error.h>
#include <signal.h>
#include <unistd.h>
#include "dlb.h"

dlb_hdl_t dlb;

void handler(int arg) {
    if (dlb)
        dlb_close(dlb);
}

/* Open a DLB device file until the process receives a signal. The DLB will
 * remain powered on for (at least) the duration of this process.
 */
int main(int argc, char **argv)
{
    if (dlb_open(0, &dlb) == -1)
        error(1, errno, "dlb_open");

    if (signal(SIGINT, handler) == SIG_ERR)
        error(1, errno, "signal");

    /* Yield until the process receives a signal */
    pause();

    return 0;
}
