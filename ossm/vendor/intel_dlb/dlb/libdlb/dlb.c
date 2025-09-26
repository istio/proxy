/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017-2018 Intel Corporation
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/fcntl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <errno.h>
#include <assert.h>
#include <emmintrin.h>
#include <pthread.h>
#include <sched.h>
#include <sys/eventfd.h>
#include <sys/time.h>

#include "dlb.h"
#include "dlb_priv.h"
#include "dlb2_ioctl.h"
#include "dlb_ops.h"

/*
 * To avoid deadlock, ports holding to credits will release them after these
 * many consecutive zero dequeues
 */
#define DLB_ZERO_DEQ_CREDIT_RETURN_THRES 1024

/*
 * To avoid deadlock, ports holding to credits will release them after these
 * many consecutive enqueue failures
 */
#define DLB_ENQ_FAIL_CREDIT_RETURN_THRES 100

/*
 * To avoid deadlock, ports holding to credits will release them after
 * accumulated credits exceed BATCH_SIZE these  many consecutive times
 */
#define DLB_BATCH_SZ_EXCEED_CREDIT_RETURN_THRES 1024

#define LOW_POOL_CREDITS(port_hdl, thres, is_2_5) \
    (__sync_fetch_and_add(port_hdl->credit_pool[LDB], 0) < thres || \
     (!is_2_5 && __sync_fetch_and_add(port_hdl->credit_pool[DIR], 0) < thres))

static dlb_credit_return_t credit_return[NUM_CREDIT_RET_TYPES] = {
    {0, 0, DLB_ZERO_DEQ_CREDIT_RETURN_THRES},
    {0, 0, DLB_ENQ_FAIL_CREDIT_RETURN_THRES},
    {DLB_SW_CREDIT_BATCH_SZ, DLB_SW_CREDIT_BATCH_SZ, DLB_BATCH_SZ_EXCEED_CREDIT_RETURN_THRES},
    {2 * DLB_SW_CREDIT_BATCH_SZ, DLB_SW_CREDIT_BATCH_SZ, 0},
    {0, 0, 0},
};


/***************************/
/* Shared Memory Functions */
/***************************/

/**
 * dlb_shm_filename() - create a string containing the domain's shm filename
 * @path: pointer to char array
 * @device_id: device ID.
 * @domain_id: domain ID.
 *
 * Return:
 * Returns the snprintf return value (< 0 if an error occurs)
 */
static inline int dlb_shm_filename(
    char *path,
    int device_id,
    int domain_id)
{
    const char *fmt = "dlb%d_%d";

    return snprintf(path, DLB_MAX_PATH_LEN, fmt, device_id, domain_id);
}

/**
 * dlb_shm_create() - create the device's shm file
 * @path: path to the shm file.
 *
 * This function creates a shm file sized to contain a dlb_shared_domain_t
 * structure.
 *
 * Return:
 * Returns 0 if successful, < 0 if an error occurs.
 */
static int dlb_shm_create(
    int device_id,
    int domain_id)
{
    char shm_path[DLB_MAX_PATH_LEN];
    int fd, ret;

    if (dlb_shm_filename(shm_path, device_id, domain_id) < 0)
        return -1;

    fd = shm_open(shm_path, O_RDWR | O_CREAT | O_EXCL, 0600);

    /* libdlb returns EPERM for any shm error, so print the true errno */
    if (fd < 0 && errno == EEXIST) {
        /* A previous process must not have exited cleanly. We're guaranteed
         * that the existing shm file isn't valid, because the kernel driver
         * wouldn't allow libdlb to create the domain if it was still in use.
         * Unlink the file and try again.
         */
        if (shm_unlink(shm_path)) {
            perror("(create) shm_unlink()");
            return -1;
        }

        fd = shm_open(shm_path, O_RDWR | O_CREAT | O_EXCL, 0600);
    }

    if (fd < 0) {
        perror("(create) shm_open()");
        return fd;
    }

    ret = ftruncate(fd, sizeof(dlb_shared_domain_t));
    if (ret < 0) {
        perror("(create) ftruncate()");
        shm_unlink(shm_path);
        close(fd);
        return ret;
    }

    return fd;
}

/**
 * dlb_shm_resize() - resize the device's shm file
 * @path: path to the shm file.
 *
 * This function resizes a shm file to the full DLB_SHM_SIZE.
 *
 * Return:
 * Returns 0 if successful, < 0 if an error occurs.
 */
static int dlb_shm_resize(
    int fd)
{
    int ret = ftruncate(fd, DLB_SHM_SIZE);
    if (ret < 0)
        perror("(resize) ftruncate()");

    return ret;
}

/**
 * dlb_shm_open() - open the device's shm file
 * @device_id: device ID.
 * @domain_id: domain ID.
 *
 * Return:
 * Returns 0/1 if successful (1 if the file existed, 0 otherwise), or < 0 if an
 * error occurs.
 */
static int dlb_shm_open(
    int device_id,
    int domain_id)
{
    char path[DLB_MAX_PATH_LEN];
    int fd;

    if (dlb_shm_filename(path, device_id, domain_id) < 0)
        return -1;

    fd = shm_open(path, O_RDWR, 0600);

    /* libdlb returns EPERM for any shm error, so print the true errno */
    if (fd < 0)
        perror("(open) shm_open()");

    return fd;
}

/**
 * dlb_shm_get_size() - return the shm file size
 * @fd: file descriptor.
 *
 * Return:
 * Returns the file size (>= 0) if successful, else < 0.
 */
static int dlb_shm_get_size(
    int fd)
{
    struct stat stat;

    if (fstat(fd, &stat) < 0) {
        perror("fstat()");
        return -1;
    }

    return stat.st_size;
}

/**
 * dlb_shm_map() - map the device's shm file
 * @fd: file descriptor.
 * @sz: file size.
 *
 * Return:
 * Returns 0 if successful, or < 0 if an error occurs.
 */
static void *dlb_shm_map(
    int fd,
    size_t sz)
{
    void *addr;

    addr = mmap(0, sz, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

    /* libdlb returns EPERM for any shm error, so print the true errno */
    if (addr == MAP_FAILED)
        perror("(shm file) mmap()");

    return addr;
}

/**
 * dlb_shm_unlink() - unlink the device's shm file
 * @device_id: device ID.
 * @domain_id: domain ID.
 *
 * Return:
 * Returns 0 if successful, < 0 if an error occurs.
 */
static int dlb_shm_unlink(
    int device_id,
    int domain_id)
{
    char shm_path[DLB_MAX_PATH_LEN];

    dlb_shm_filename(shm_path, device_id, domain_id);

    return shm_unlink(shm_path);
}

int
dlb_get_xstats(dlb_hdl_t hdl, uint32_t type, uint32_t id, uint64_t *val)
{
    dlb_t *dlb = (dlb_t *)hdl;
    int ret = -1;

    VALIDATE_DLB_HANDLE(hdl);

    ASSERT(val != NULL, EINVAL);

    ret = dlb2_ioctl_get_xtats(dlb->fd, type, id, val);

cleanup:
    return ret;
}


/********************/
/* Socket Functions */
/********************/

/**
 * dlb_socket_filename() - Create a string for the domain's socket name
 * @sockaddr: pointer to socket address that will hold the name
 * @domain_id: domain ID.
 *
 * Return:
 * Returns the snprintf return value (< 0 if an error occurs)
 */
static inline int dlb_socket_filename(
    struct sockaddr_un *sockaddr,
    int device_id,
    int domain_id)
{
    /* Assert that DLB_SOCKET_PREFIX plus the UID, device ID, domain ID, two
     * underscores, and a NULL terminator will fit within sockaddr's sun_path.
     * UID, device ID, domain ID and the underscores should only need 17 bytes,
     * but round up to 32 for safety.
     *
     * Without this check, multiple domains could re-use a socket name.
     */
    BUG_ON(sizeof(DLB_SOCKET_PREFIX) + 32 > sizeof(sockaddr->sun_path) - 1);

    /* Include UID in the name so if the application exits uncleanly and
     * another user then attempts to use the scheduling domain, it doesn't fail
     * to unlink the previous process's socket.
     */
    return snprintf(sockaddr->sun_path,
                    sizeof(sockaddr->sun_path),
                    "%s_%u_%u_%u",
                    DLB_SOCKET_PREFIX, getuid(), device_id, domain_id);
}

#ifndef DLB_NOT_USE_DOMAIN_SERVER
/**
 * dlb_create_domain_socket()
 * @domain_id: domain ID
 *
 * This function creates a unix domain socket (with owner-only permissions)
 * through which the process can share the domain file.
 */
static int dlb_create_domain_socket(
    int device_id,
    int domain_id)
{
    struct sockaddr_un sockaddr;
    int ret, err, sock_fd;

    ret = -1;

    sock_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    ASSERT(sock_fd != -1, errno);

    memset(&sockaddr, 0, sizeof(sockaddr));
    sockaddr.sun_family = AF_UNIX;

    err = dlb_socket_filename(&sockaddr, device_id, domain_id);
    ASSERT(err != sizeof(sockaddr.sun_path) - 1, err);

    /* If a previous libdlb application didn't exit cleanly, we need to unlink
     * its socket before continuing. Ignore the return value -- we don't know
     * whether the socket exists or not and therefore whether this should
     * succeed or fail.
     */
    unlink(sockaddr.sun_path);

    /* Set owner permissions before binding to avoid a race condition */
    err = fchmod(sock_fd, S_IRUSR | S_IWUSR | S_IXUSR);
    ASSERT(err != -1, errno);

    err = bind(sock_fd, (struct sockaddr *)&sockaddr, sizeof(sockaddr));
    ASSERT(err != -1, errno);

    /* Listen with a backlog of up to 128 connections (should be plenty) */
    err = listen(sock_fd, 128);
    ASSERT(err != -1, err);

    ret = sock_fd;

cleanup:
    if (ret == -1 && sock_fd != -1)
        close(sock_fd);

    return ret;
}

/**
 * dlb_domain_server() - pthread function for the domain server thread.
 * @arg: pointer to dlb_domain_t structure
 *
 * This thread listens on a unix domain socket and shares the domain file
 * with all other threads or processes that connect to the socket via
 * dlb_attach_sched_domain().
 *
 * This thread accepts connections continuously until the domain is reset.
 */
static void *dlb_domain_server(void *arg)
{
    dlb_domain_hdl_internal_t *domain_hdl = arg;
    struct sockaddr_un sockaddr;
    dlb_shared_domain_t *shared;
    dlb_local_domain_t *local;
    int dom_fd, err;
    dlb_t *dlb;

    shared = domain_hdl->domain.shared;
    local = domain_hdl->domain.local;
    dlb = domain_hdl->dlb;

    /* domain_hdl is allocated in dlb_launch_domain_server_thread() */
    free(domain_hdl);

    dom_fd = local->creator_fd;

    err = dlb_socket_filename(&sockaddr, dlb->id, shared->id);
    ASSERT(err != sizeof(sockaddr.sun_path) - 1, err);

    /* This loop exits when dlb_reset_sched_domain() calls shutdown on the
     * socket fd.
     */
    while (1) {
        /* Pass the domain fd in a control message. This piggybacks on a
         * message (must be non-empty, hence iov_len of 1) whose contents are
         * ignored by the recipient.
         */
        union {
            char buf[CMSG_SPACE(sizeof(dom_fd))];
            struct cmsghdr align;
        } u;
        struct cmsghdr *cmsg;
        struct msghdr msg;
        struct iovec iov;
        int client;

        client = accept(local->socket_fd, NULL, NULL);
        ASSERT(client != -1, errno);

        iov.iov_base = " ";
        iov.iov_len = 1;

        msg.msg_iov = &iov;
        msg.msg_iovlen = 1;
        msg.msg_control = u.buf;
        msg.msg_controllen = sizeof(u.buf);

        cmsg = CMSG_FIRSTHDR(&msg);
        cmsg->cmsg_level = SOL_SOCKET;
        cmsg->cmsg_type = SCM_RIGHTS;
        cmsg->cmsg_len = CMSG_LEN(sizeof(dom_fd));

        memcpy(CMSG_DATA(cmsg), &dom_fd, sizeof(dom_fd));

        err = sendmsg(client, &msg, 0);

        /* Close client fd whether or not sendmsg succeeds. close's return
         * value can be safely ignored.
         */
        close(client);

        if (err == -1)
            printf("[%s()] Error: failed to share domain fd\n", __func__);

        ASSERT(err != -1, errno);
    }

cleanup:
    if (local->socket_fd != -1)
        close(local->socket_fd);

    local->socket_fd = -1;

    /* Nothing to do if this fails, so ignore the return value */
    unlink(sockaddr.sun_path);

    return NULL;
}

static int dlb_launch_domain_server_thread(
    dlb_t *dlb,
    dlb_local_domain_t *local_domain,
    dlb_shared_domain_t *shared_domain)
{
    dlb_domain_hdl_internal_t *domain_hdl = NULL;
    bool attr_init = false;
    pthread_attr_t attr;
    int err, ret = -1;
    int sock_fd = -1;

    sock_fd = dlb_create_domain_socket(dlb->id, shared_domain->id);
    ASSERT(sock_fd != -1, errno);

    local_domain->socket_fd = sock_fd;

    /* Run the server thread as detached so the thread state memory is freed
     * when it terminates.
     */
    err = pthread_attr_init(&attr);
    ASSERT(err == 0, err);

    attr_init = true;

    /* Freed by the child thread */
    domain_hdl = calloc(1, sizeof(*domain_hdl));
    ASSERT(domain_hdl, ENOMEM);

    domain_hdl->domain.shared = shared_domain;
    domain_hdl->domain.local = local_domain;
    domain_hdl->dlb = dlb;

    err = pthread_create(&local_domain->socket_thread,
                         &attr,
                         dlb_domain_server,
                         domain_hdl);
    ASSERT(err == 0, err);

    ret = 0;

cleanup:

    if (attr_init)
        pthread_attr_destroy(&attr);
    if (ret && domain_hdl)
        free(domain_hdl);
    if (ret && sock_fd != -1)
        close(sock_fd);

    return ret;
}

static int dlb_get_sched_domain_fd(
    dlb_t *dlb,
    dlb_shared_domain_t *domain)
{
    /* (See dlb_domain_server for details on msg and cmsg sizing) */
    char m_buf[1], c_buf[CMSG_SPACE(sizeof(int))];
    struct sockaddr_un sockaddr;
    int sock_fd, dom_fd, err;
    struct msghdr msg;
    struct iovec io;

    dom_fd = -1;

    sock_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    ASSERT(sock_fd != -1, errno);

    memset(&sockaddr, 0, sizeof(sockaddr));
    sockaddr.sun_family = AF_UNIX;

    err = dlb_socket_filename(&sockaddr, dlb->id, domain->id);
    ASSERT(err != sizeof(sockaddr.sun_path) - 1, err);

    err = connect(sock_fd, (struct sockaddr*)&sockaddr, sizeof(sockaddr));
    ASSERT(err != -1, errno);

    io.iov_base = m_buf;
    io.iov_len = sizeof(m_buf);

    memset(&msg, 0, sizeof(msg));
    msg.msg_iov = &io;
    msg.msg_iovlen = 1;

    msg.msg_control = c_buf;
    msg.msg_controllen = sizeof(c_buf);

    err = recvmsg(sock_fd, &msg, 0);
    ASSERT(err >= 0, errno);

    dom_fd = *((int *)CMSG_DATA(CMSG_FIRSTHDR(&msg)));

cleanup:
    if (sock_fd != -1)
        close(sock_fd);

    return dom_fd;
}
#endif

/***************************/
/* DLB Device Capabilities */
/***************************/
static dlb_dev_cap_t dlb2_caps = {
    .ldb_deq_event_fid = 1,
    .port_cos = 1,
    .queue_dt = 1,
    .lock_id_comp = 1,
};

static dlb_dev_cap_t dlb2_5_caps = {
    .ldb_deq_event_fid = 1,
    .port_cos = 1,
    .queue_dt = 1,
    .lock_id_comp = 1,
    .combined_credits = 1,
    .qe_weight = 1,
    .op_frag = 1,
};

int dlb_get_dev_capabilities(
    dlb_hdl_t hdl,
    dlb_dev_cap_t *cap)
{
    dlb_t *dlb = (dlb_t *) hdl;
    int ret = -1;

    VALIDATE_DLB_HANDLE(hdl);

    ASSERT(cap != NULL, EINVAL);

    switch (dlb->device_version.ver) {
        case 2:
            *cap = dlb2_caps;
            break;
        case 3:
            *cap = dlb2_5_caps;
            break;
        default:
            goto cleanup;
    }

    ret = 0;

cleanup:
    return ret;
}

/*****************/
/* DLB Functions */
/*****************/
static inline int dlb_check_driver_version(
    int fd)
{
    /* Driver version has not been implemented for dlb2.0 yet.*/
    return 0;
}

static inline int check_driver_version(
    dlb_t *dlb)
{
    return dlb_check_driver_version(dlb->fd);
}

static inline void dlb_get_dev_version(dlb_t *dlb)
{
    uint8_t version, revision;

    dlb2_ioctl_get_device_version(dlb->fd, &version, &revision);

    dlb->device_version.ver = version;
    dlb->device_version.rev = revision;
}

int dlb_open(
    int device_id,
    dlb_hdl_t *hdl)
{
    char path[DLB_MAX_PATH_LEN];
    int ret = -1, err;
    dlb_t *dlb;

    dlb = calloc(1, sizeof(dlb_t));
    ASSERT(dlb != NULL, ENOMEM);

    dlb->fd = -1;

    BUG_ON(sizeof(dlb_enqueue_qe_t) != 16);
    BUG_ON(sizeof(dlb_dequeue_qe_t) != 16);
    BUG_ON(sizeof(dlb_enqueue_qe_t) != sizeof(dlb_send_t));
    BUG_ON(sizeof(dlb_enqueue_qe_t) != sizeof(dlb_adv_send_t));
    BUG_ON(sizeof(dlb_dequeue_qe_t) != sizeof(dlb_recv_t));

#ifndef DEV_NAME
    snprintf(path, sizeof(path), "/dev/dlb%d", device_id);
#else
    snprintf(path, sizeof(path), "/dev/" xstr(DEV_NAME) "%d/" xstr(DEV_NAME),
             device_id);
#endif

    dlb->fd = open(path, O_RDWR);
    ASSERT(dlb->fd != -1, errno);

    dlb_get_dev_version(dlb);

    ASSERT(check_driver_version(dlb) == 0, EINVAL);

    dlb->magic_num = DLB_MAGIC_NUM;
    dlb->id = device_id;

    ASSERT(dlb_get_dev_capabilities(dlb, &dlb->cap) == 0, EINVAL);

    err = pthread_mutex_init(&dlb->resource_mutex, NULL);
    ASSERT(err == 0, err);

    *hdl = (dlb_hdl_t) dlb;

    ret = 0;

cleanup:

    if (ret && dlb) {
        if (dlb->fd != -1)
            close(dlb->fd);
        free(dlb);
    }

    return ret;
}

int dlb_close(
    dlb_hdl_t hdl)
{
    dlb_t *dlb = (dlb_t *) hdl;
    int i, ret = -1;

    VALIDATE_DLB_HANDLE(hdl);

    /* Check if there are any remaining attached domain handles */
    for (i = 0; i < MAX_NUM_SCHED_DOMAINS; i++)
        ASSERT(!dlb->local_domains[i].handles, EEXIST);

    /* If any of the cleanup procedures (close(), etc.) unexpectedly fail,
     * print a message and continue.
     */
    if (close(dlb->fd))
        perror("close()");

    for (i = 0; i < MAX_NUM_SCHED_DOMAINS; i++) {
        if (dlb->shared_domains[i]) {
            if (dlb_reset_sched_domain(dlb, i))
                printf("[%s()] Failed to reset sched domain %d", __func__, i);
        }
    }

    if (pthread_mutex_destroy(&dlb->resource_mutex))
        printf("[%s()] Failed to destroy pthread mutex", __func__);

    memset(dlb, 0, sizeof(dlb_t));
    free(dlb);

    ret = 0;

cleanup:

    return ret;
}

int dlb_get_num_resources(
    dlb_hdl_t hdl,
    dlb_resources_t *rsrcs)
{
    dlb_t *dlb = (dlb_t *) hdl;
    dlb_device_version ver;
    int ret = -1;

    VALIDATE_DLB_HANDLE(hdl);

    ASSERT(rsrcs != NULL, EINVAL);

    ver = dlb->device_version.ver;
    ret = dlb2_ioctl_get_num_resources(dlb->fd,
                                       rsrcs,
                                       ver == VER_DLB2_5);

cleanup:
    return ret;
}

int
dlb_set_ldb_sequence_number_allocation(
    dlb_hdl_t hdl,
    unsigned int group,
    unsigned int num)
{
    dlb_t *dlb = (dlb_t *) hdl;
    int ret = -1;

    VALIDATE_DLB_HANDLE(hdl);

    ret = dlb2_ioctl_set_sn_allocation(dlb->fd, group, num);

cleanup:
    return ret;
}

int
dlb_get_ldb_sequence_number_allocation(
    dlb_hdl_t hdl,
    unsigned int group,
    unsigned int *num)
{
    dlb_t *dlb = (dlb_t *) hdl;
    int ret = -1;

    VALIDATE_DLB_HANDLE(hdl);
    ASSERT(num, EINVAL);

    ret = dlb2_ioctl_get_sn_allocation(dlb->fd, group, num);

cleanup:
    return ret;
}

int
dlb_get_ldb_sequence_number_occupancy(
    dlb_hdl_t hdl,
    unsigned int group,
    unsigned int *num)
{
    dlb_t *dlb = (dlb_t *) hdl;
    int ret = -1;

    VALIDATE_DLB_HANDLE(hdl);
    ASSERT(num, EINVAL);

    ret = dlb2_ioctl_get_sn_occupancy(dlb->fd, group, num);

cleanup:
    return ret;
}

int
dlb_get_num_ldb_sequence_number_groups(
    dlb_hdl_t hdl)
{
    int ret = -1;

    VALIDATE_DLB_HANDLE(hdl);

    ret = NUM_V2_LDB_SN_GROUPS;

cleanup:
    return ret;
}

int
dlb_get_min_ldb_sequence_number_allocation(
    dlb_hdl_t hdl)
{
    int ret = -1;

    VALIDATE_DLB_HANDLE(hdl);

    ret = NUM_V2_MIN_LDB_SN_ALLOC;

cleanup:
    return ret;
}

/*********************************************/
/* Scheduling domain configuration Functions */
/*********************************************/
int dlb_adv_create_sched_domain(
    dlb_hdl_t hdl,
    dlb_create_sched_domain_t *args,
    dlb_adv_create_sched_domain_t *adv_args)
{
    dlb_shared_domain_t *shared_domain = NULL;
    bool unlock = false, attr_init = false;
    dlb_local_domain_t *local_domain;
    int id, ret, fd, err, dom_fd;
    dlb_t *dlb = (dlb_t *) hdl;
    pthread_mutexattr_t attr;
    dlb_device_version ver;

    ret = -1;
    fd = -1;
    dom_fd = -1;

    VALIDATE_DLB_HANDLE(hdl);
    ASSERT(args != NULL && adv_args != NULL, EINVAL);

    err = pthread_mutex_lock(&dlb->resource_mutex);
    ASSERT(err == 0, err);

    unlock = true;

    ver = dlb->device_version.ver;

    if (ver == VER_DLB2) {
    } else if (ver == VER_DLB2_5) {
    }

    if (ver == VER_DLB2) {
        ASSERT(args->num_ldb_credit_pools <= MAX_NUM_LDB_CREDIT_POOLS, EINVAL);
        ASSERT(args->num_dir_credit_pools <= MAX_NUM_DIR_CREDIT_POOLS, EINVAL);

        id = dlb2_ioctl_create_sched_domain(dlb->fd, args, adv_args,
                                            &dom_fd, false);
    } else {
        ASSERT(args->num_credit_pools <= MAX_NUM_LDB_CREDIT_POOLS, EINVAL);

        id = dlb2_ioctl_create_sched_domain(dlb->fd, args, adv_args,
                                            &dom_fd, true);
    }

    ASSERT(id != -1, errno);

    local_domain = &dlb->local_domains[id];

    /* At creation time, the shm file is not fully sized. This is used as a
     * signal to other processes attempting to attach to the domain that the
     * file is not yet ready.
     */
    fd = dlb_shm_create(dlb->id, id);

    ASSERT(fd >= 0, EPERM);

    shared_domain = dlb_shm_map(fd, sizeof(dlb_shared_domain_t));
    ASSERT(shared_domain != MAP_FAILED, ENOMEM);

    shared_domain->id = id;

    /* The creator process opens the domain FD here and keeps it alive until it
     * either successfully calls dlb_reset_sched_domain() or the process dies.
     * This ensures the domain exists for the lifetime of the application.
     */
    local_domain->creator_fd = dom_fd;

    local_domain->creator = true;

    err = pthread_mutexattr_init(&attr);
    ASSERT(err == 0, err);

    attr_init = true;

    err = pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
    ASSERT(err == 0, err);

    err = pthread_mutex_init(&shared_domain->resource_mutex, &attr);
    ASSERT(err == 0, err);

    shared_domain->num_ldb_queues = args->num_ldb_queues;
    shared_domain->num_dir_queues = args->num_dir_ports;

    if (dlb->device_version.ver < VER_DLB2_5) {
        shared_domain->sw_credits.total_credits[LDB] = args->num_ldb_credits;
        shared_domain->sw_credits.total_credits[DIR] = args->num_dir_credits;

        shared_domain->sw_credits.avail_credits[LDB] = args->num_ldb_credits;
        shared_domain->sw_credits.avail_credits[DIR] = args->num_dir_credits;
    } else {
        shared_domain->sw_credits.total_credits[LDB] = args->num_credits;
        shared_domain->sw_credits.avail_credits[LDB] = args->num_credits;
    }

    shared_domain->use_rsvd_token_scheme = (dlb->device_version.ver == VER_DLB);
    shared_domain->configured = true;

    dlb->shared_domains[id] = shared_domain;

    /* Resize the shm file, signalling that other processes can attach. (This
     * process is blocked by its resource_mutex.)
     */
    err = dlb_shm_resize(fd);
    ASSERT(err == 0, err);

    close(fd);
    fd = -1;

#ifndef DLB_NOT_USE_DOMAIN_SERVER
    /* Launch the thread that shares the domain file with attaching processes */
    err = dlb_launch_domain_server_thread(dlb, local_domain, shared_domain);
    ASSERT(err == 0, err);
#endif
    /* shared_domain is unmapped when the domain is reset, in
     * dlb_reset_sched_domain().
     */

    ret = id;

cleanup:

    if (unlock) {
        if (pthread_mutex_unlock(&dlb->resource_mutex))
            printf("[%s()] Internal error: mutex unlock failed\n", __func__);
    }

    if (ret < 0) {
        if (shared_domain)
            munmap(shared_domain, sizeof(dlb_shared_domain_t));
        if (fd != -1) {
            dlb_shm_unlink(dlb->id, id);
            close(fd);
        }
        if (dom_fd != -1)
            close(dom_fd);
    }

    if (attr_init)
        pthread_mutexattr_destroy(&attr);

    return ret;
}

int dlb_create_sched_domain(
    dlb_hdl_t hdl,
    dlb_create_sched_domain_t *args)
{
    dlb_adv_create_sched_domain_t adv_args;

    adv_args.num_cos_ldb_ports[0] = 0;
    adv_args.num_cos_ldb_ports[1] = 0;
    adv_args.num_cos_ldb_ports[2] = 0;
    adv_args.num_cos_ldb_ports[3] = 0;

    return dlb_adv_create_sched_domain(hdl, args, &adv_args);
}

dlb_domain_hdl_t dlb_attach_sched_domain(
    dlb_hdl_t hdl,
    int domain_id)
{
    dlb_domain_hdl_internal_t *domain_hdl = NULL;
    dlb_shared_domain_t *shared_domain = NULL;
    dlb_local_domain_t *local_domain;
    dlb_t *dlb = (dlb_t *) hdl;
    bool unlock_domain = false;
    bool unlock_dlb = false;
    int ret = -1;
    int fd = -1;
    int sz;

    VALIDATE_DLB_HANDLE(hdl);

    ASSERT(domain_id >= 0 && domain_id < MAX_NUM_SCHED_DOMAINS, EINVAL);

    pthread_mutex_lock(&dlb->resource_mutex);
    unlock_dlb = true;

    fd = dlb_shm_open(dlb->id, domain_id);
    ASSERT(fd >= 0, EPERM);

    sz = dlb_shm_get_size(fd);
    ASSERT(sz >= 0, EPERM);

    /* If the file exists but it's not fully sized yet, another process is
     * creating the domain. This is an error, since the caller shouldn't
     * attempt to attach until the domain is created (how could it know which
     * domain ID to attach to?).
     */
    ASSERT(sz == DLB_SHM_SIZE, EINVAL);

    shared_domain = dlb_shm_map(fd, DLB_SHM_SIZE);
    ASSERT(shared_domain != MAP_FAILED, ENOMEM);

    close(fd);
    fd = -1;

    ASSERT(shared_domain->configured, EINVAL);

    local_domain = &dlb->local_domains[domain_id];

    local_domain->shared_base = shared_domain;

    pthread_mutex_lock(&shared_domain->resource_mutex);
    unlock_domain = true;

    domain_hdl = calloc(1, sizeof(*domain_hdl));
    ASSERT(domain_hdl != NULL, ENOMEM);

    /* The sched domain creator creates a socket (with owner-only permissions)
     * and listener thread. When it accepts a connection, it sends the fd. The
     * attacher connects to the socket, does a blocking receive, then closes
     * the connection.
     */
#ifndef DLB_NOT_USE_DOMAIN_SERVER
    domain_hdl->fd = dlb_get_sched_domain_fd(dlb, shared_domain);
#else
    domain_hdl->fd = dlb->local_domains[domain_id].creator_fd;
#endif
    ASSERT(domain_hdl->fd >= 0, errno);

    domain_hdl->magic_num = DOMAIN_MAGIC_NUM;
    domain_hdl->domain.device_version = dlb->device_version;
    domain_hdl->domain.shared = shared_domain;
    domain_hdl->domain.local = local_domain;
    domain_hdl->shared_base = local_domain->shared_base;
    domain_hdl->cap = dlb->cap;
    domain_hdl->dlb = dlb;

    /* Add the new handle to the domain's linked list of handles */
    LIST_ADD(local_domain->handles, domain_hdl);

    shared_domain->refcnt++;

    ret = 0;

cleanup:

    if (unlock_domain)
        pthread_mutex_unlock(&shared_domain->resource_mutex);
    if (unlock_dlb)
        pthread_mutex_unlock(&dlb->resource_mutex);

    if (ret) {
        if (fd != -1)
            close(fd);
        if (shared_domain)
            munmap(shared_domain, DLB_SHM_SIZE);

        if (domain_hdl)
            free(domain_hdl);

        domain_hdl = NULL;
    }

    return (dlb_domain_hdl_t) domain_hdl;
}

int dlb_detach_sched_domain(
    dlb_domain_hdl_t hdl)
{
    dlb_domain_hdl_internal_t *domain_hdl = hdl;
    dlb_shared_domain_t *shared_domain;
    dlb_local_domain_t *local_domain;
    bool unlock = false;
    int i, ret = -1;
    bool found;

    VALIDATE_DOMAIN_HANDLE(hdl);

    local_domain = domain_hdl->domain.local;
    shared_domain = domain_hdl->domain.shared;

    pthread_mutex_lock(&shared_domain->resource_mutex);
    unlock = true;

    /* All port handles must be detached before the domain handle */
    for (i = 0; i < MAX_NUM_LDB_PORTS; i++)
        ASSERT(!local_domain->ldb_ports[i].handles, EEXIST);
    for (i = 0; i < MAX_NUM_DIR_PORTS; i++)
        ASSERT(!local_domain->dir_ports[i].handles, EEXIST);

    /* Remove the handle from the domain's handles list */
    LIST_DEL(local_domain->handles, hdl, found);

    if (!found) {
        printf("[%s()] Internal error: couldn't find domain handle\n",
               __func__);
        ASSERT(0, EINVAL);
    }

#ifndef DLB_NOT_USE_DOMAIN_SERVER
    close(domain_hdl->fd);
#endif

    memset(hdl, 0, sizeof(dlb_domain_hdl_internal_t));
    free(hdl);

    shared_domain->refcnt--;

    pthread_mutex_unlock(&shared_domain->resource_mutex);
    unlock = false;

    munmap(shared_domain, DLB_SHM_SIZE);

    ret = 0;

cleanup:

    if (unlock)
        pthread_mutex_unlock(&shared_domain->resource_mutex);

    return ret;
}

int dlb2_create_ldb_credit_pool(
    dlb_domain_hdl_internal_t *hdl,
    int num_credits)
{
    dlb_shared_domain_t *domain;
    int i, ret = -1;

    VALIDATE_DOMAIN_HANDLE(hdl);

    domain = hdl->domain.shared;

    ASSERT(num_credits <= domain->sw_credits.avail_credits[LDB], EINVAL);

    for (i = 0; i < MAX_NUM_LDB_CREDIT_POOLS; i++) {
        if (!domain->sw_credits.ldb_pools[i].configured)
            break;
    }

    ASSERT(i < MAX_NUM_LDB_CREDIT_POOLS, EINVAL);

    domain->sw_credits.ldb_pools[i].avail_credits = num_credits;
    domain->sw_credits.ldb_pools[i].configured = true;

    domain->sw_credits.avail_credits[LDB] -= num_credits;

    ret = i;

cleanup:

    return ret;
}

int dlb_create_ldb_credit_pool(
    dlb_domain_hdl_t hdl,
    int num_credits)
{
    dlb_domain_hdl_internal_t *domain_hdl = hdl;
    dlb_domain_t *domain;
    bool unlock = false;
    int ret = -1;

    VALIDATE_DOMAIN_HANDLE(hdl);

    domain = &domain_hdl->domain;

    pthread_mutex_lock(&domain->shared->resource_mutex);
    unlock = true;

    ASSERT(domain_hdl->dlb->device_version.ver <= VER_DLB2, EINVAL);

    ret = dlb2_create_ldb_credit_pool(domain_hdl, num_credits);

cleanup:

    if (unlock)
        pthread_mutex_unlock(&domain->shared->resource_mutex);

    return ret;
}

int dlb2_create_dir_credit_pool(
    dlb_domain_hdl_internal_t *hdl,
    int num_credits)
{
    dlb_shared_domain_t *domain;
    int i, ret = -1;

    VALIDATE_DOMAIN_HANDLE(hdl);

    domain = hdl->domain.shared;

    ASSERT(num_credits <= domain->sw_credits.avail_credits[DIR], EINVAL);

    for (i = 0; i < MAX_NUM_DIR_CREDIT_POOLS; i++) {
        if (!domain->sw_credits.dir_pools[i].configured)
            break;
    }

    ASSERT(i < MAX_NUM_DIR_CREDIT_POOLS, EINVAL);

    domain->sw_credits.dir_pools[i].avail_credits = num_credits;
    domain->sw_credits.dir_pools[i].configured = true;

    domain->sw_credits.avail_credits[DIR] -= num_credits;

    ret = i;

cleanup:

    return ret;
}

int dlb_create_dir_credit_pool(
    dlb_domain_hdl_t hdl,
    int num_credits)
{
    dlb_domain_hdl_internal_t *domain_hdl = hdl;
    dlb_domain_t *domain;
    bool unlock = false;
    int ret = -1;

    VALIDATE_DOMAIN_HANDLE(hdl);

    domain = &domain_hdl->domain;

    pthread_mutex_lock(&domain->shared->resource_mutex);
    unlock = true;

    ASSERT(domain_hdl->dlb->device_version.ver <= VER_DLB2, EINVAL);

    ret = dlb2_create_dir_credit_pool(domain_hdl, num_credits);

cleanup:

    if (unlock)
        pthread_mutex_unlock(&domain->shared->resource_mutex);

    return ret;
}

int dlb2_5_create_credit_pool(
    dlb_domain_hdl_internal_t *hdl,
    int num_credits)
{
    dlb_shared_domain_t *domain;
    int i, ret = -1;

    VALIDATE_DOMAIN_HANDLE(hdl);

    domain = hdl->domain.shared;

    ASSERT(num_credits <= domain->sw_credits.avail_credits[LDB], EINVAL);

    for (i = 0; i < MAX_NUM_LDB_CREDIT_POOLS; i++) {
        if (!domain->sw_credits.ldb_pools[i].configured)
            break;
    }

    ASSERT(i < MAX_NUM_LDB_CREDIT_POOLS, EINVAL);

    domain->sw_credits.ldb_pools[i].avail_credits = num_credits;
    domain->sw_credits.ldb_pools[i].configured = true;

    domain->sw_credits.avail_credits[LDB] -= num_credits;

    ret = i;

cleanup:

    return ret;
}

int dlb_create_credit_pool(
    dlb_domain_hdl_t hdl,
    int num_credits)
{
    dlb_domain_hdl_internal_t *domain_hdl = hdl;
    dlb_domain_t *domain;
    bool unlock = false;
    int ret = -1;

    VALIDATE_DOMAIN_HANDLE(hdl);

    domain = &domain_hdl->domain;

    pthread_mutex_lock(&domain->shared->resource_mutex);
    unlock = true;

    ASSERT(domain_hdl->dlb->device_version.ver >= VER_DLB2_5, EINVAL);

    ret = dlb2_5_create_credit_pool(domain_hdl, num_credits);

cleanup:

    if (unlock)
        pthread_mutex_unlock(&domain->shared->resource_mutex);

    return ret;
}

int dlb_create_ldb_queue(
    dlb_domain_hdl_t hdl,
    dlb_create_ldb_queue_t *args)
{
    dlb_domain_hdl_internal_t *domain_hdl = hdl;
    dlb_domain_t *domain;
    bool unlock = false;
    int ret = -1;

    VALIDATE_DOMAIN_HANDLE(hdl);
    ASSERT(args != NULL, EINVAL);

    domain = &domain_hdl->domain;

    pthread_mutex_lock(&domain->shared->resource_mutex);
    unlock = true;

    /* Set the threshold to 2/3 of the total available credits, which will
    * result in the four levels specified in dlb_queue_depth_levels_t.
    */
    int threshold = domain->shared->sw_credits.total_credits[LDB] * 2 / 3;

    ret = dlb2_ioctl_create_ldb_queue(domain_hdl->fd, args, threshold);

    if (ret >= 0)
        domain->shared->queue_type[LDB][ret] = QUEUE_TYPE_REGULAR;

cleanup:

    if (unlock)
        pthread_mutex_unlock(&domain->shared->resource_mutex);

    return ret;
}

int dlb_create_dir_queue(
    dlb_domain_hdl_t hdl,
    int port_id)
{
    dlb_domain_hdl_internal_t *domain_hdl = hdl;
    dlb_domain_t *domain;
    bool unlock = false;
    int ret = -1;
    dlb_device_version ver = VER_DLB2;
    int threshold;

    VALIDATE_DOMAIN_HANDLE(hdl);

    domain = &domain_hdl->domain;
    ver = domain_hdl->dlb->device_version.ver;

    pthread_mutex_lock(&domain->shared->resource_mutex);
    unlock = true;

    /* Set the threshold to 2/3 of the total available credits, which will
    * result in the four levels specified in dlb_queue_depth_levels_t.
    */

    if (ver < VER_DLB2_5) {
	    threshold = domain->shared->sw_credits.total_credits[DIR] * 2 / 3;
    } else {
	    threshold = domain->shared->sw_credits.total_credits[LDB] * 2 / 3;
    }

    ret = dlb2_ioctl_create_dir_queue(domain_hdl->fd, port_id, threshold);

    if (ret >= 0)
        domain->shared->queue_type[DIR][ret] = QUEUE_TYPE_REGULAR;

cleanup:

    if (unlock)
        pthread_mutex_unlock(&domain->shared->resource_mutex);

    return ret;
}

static int dlb_create_ldb_port_adv(
    dlb_domain_hdl_t hdl,
    dlb_create_port_t *args,
    uint16_t rsvd_tokens);

int dlb_create_ldb_port(
    dlb_domain_hdl_t hdl,
    dlb_create_port_t *args)
{
    dlb_domain_hdl_internal_t *domain_hdl = hdl;
    dlb_shared_domain_t *domain;
    dlb_create_port_t __args;
    uint16_t rsvd_tokens;
    bool unlock = false;
    int ret = -1;

    VALIDATE_DOMAIN_HANDLE(hdl);

    ASSERT(args != NULL, EINVAL);

    /* Create a local copy to allow for modifications */
    __args = *args;

    domain = domain_hdl->domain.shared;

    pthread_mutex_lock(&domain->resource_mutex);
    unlock = true;

    /* The reserved token interrupt arming scheme requires that one or more CQ
     * tokens be reserved by the library. This limits the amount of CQ space
     * usable by the DLB, so in order to give an *effective* CQ depth equal to
     * the user-requested value, we double CQ depth and reserve half of its
     * tokens. If the user requests the max CQ depth (1024) then we cannot
     * double it, so we reserve one token and give an effective depth of 1023
     * entries.
     */
    rsvd_tokens = 1;

    if (domain->use_rsvd_token_scheme && __args.cq_depth < 1024) {
        rsvd_tokens = __args.cq_depth;
        __args.cq_depth *= 2;
    }

    /* Create the load-balanced port */
    ret = dlb_create_ldb_port_adv(hdl, &__args, rsvd_tokens);

cleanup:

    if (unlock)
        pthread_mutex_unlock(&domain->resource_mutex);

    return ret;
}

static int dlb_create_dir_port_adv(
    dlb_domain_hdl_t hdl,
    dlb_create_port_t *args,
    int queue_id,
    uint16_t rsvd_tokens);

int dlb_create_dir_port(
    dlb_domain_hdl_t hdl,
    dlb_create_port_t *args,
    int queue_id)
{
    dlb_domain_hdl_internal_t *domain_hdl = hdl;
    dlb_shared_domain_t *domain;
    dlb_create_port_t __args;
    uint16_t rsvd_tokens;
    bool unlock = false;
    int ret = -1;

    ASSERT(args != NULL, EINVAL);

    /* Create a local copy to allow for modifications */
    __args = *args;

    VALIDATE_DOMAIN_HANDLE(hdl);

    domain = domain_hdl->domain.shared;

    /* The reserved token interrupt arming scheme requires that one or more CQ
     * tokens be reserved by the library. This limits the amount of CQ space
     * usable by the DLB, so in order to give an *effective* CQ depth equal to
     * the user-requested value, we double CQ depth and reserve half of its
     * tokens. If the user requests the max CQ depth (1024) then we cannot
     * double it, so we reserve one token and give an effective depth of 1023
     * entries.
     */
    rsvd_tokens = 1;

    if (domain->use_rsvd_token_scheme && __args.cq_depth < 1024) {
        rsvd_tokens = __args.cq_depth;
        __args.cq_depth *= 2;
    }

    pthread_mutex_lock(&domain->resource_mutex);
    unlock = true;

    /* Create the directed port */
    ret = dlb_create_dir_port_adv(hdl,
                                  &__args,
                                  queue_id,
                                  rsvd_tokens);

cleanup:

    if (unlock)
        pthread_mutex_unlock(&domain->resource_mutex);

    return ret;
}

static int map_consumer_queue(
    dlb_port_hdl_internal_t *port_hdl)
{
    dlb_port_type_t type;
    int id, fd;

    id = port_hdl->port.shared->id;
    type = port_hdl->port.shared->type;

    if (type == LDB)
        fd = dlb2_ioctl_get_ldb_port_cq_fd(port_hdl->domain_hdl->fd, id);
    else
        fd = dlb2_ioctl_get_dir_port_cq_fd(port_hdl->domain_hdl->fd, id);

    if (fd < 0)
        return fd;

    port_hdl->cq_base = mmap(NULL, DLB2_CQ_SIZE, PROT_WRITE, MAP_SHARED, fd, 0);

    close(fd);

    return ((void *)port_hdl->cq_base == (uint64_t *)-1) ? -1 : 0;
}

static int map_producer_port(
    dlb_port_hdl_internal_t *port_hdl)
{
    dlb_port_type_t type;
    int id, fd;

    id = port_hdl->port.shared->id;
    type = port_hdl->port.shared->type;

    if (type == LDB)
        fd = dlb2_ioctl_get_ldb_port_pp_fd(port_hdl->domain_hdl->fd, id);
    else
        fd = dlb2_ioctl_get_dir_port_pp_fd(port_hdl->domain_hdl->fd, id);

    if (fd < 0)
        return fd;

    port_hdl->pp_addr = mmap(NULL, DLB2_PP_SIZE, PROT_WRITE, MAP_SHARED, fd, 0);

    close(fd);

    return (port_hdl->pp_addr == (uint64_t *)-1) ? -1 : 0;
}

static int dlb_block_on_cq_interrupt(
    dlb_port_hdl_internal_t *hdl,
    dlb_shared_port_t *port);

static int dlb_block_on_umwait(
    dlb_port_hdl_internal_t *hdl,
    dlb_shared_port_t *port);

static int dlb_hard_poll_cq(
    dlb_port_hdl_internal_t *hdl,
    dlb_shared_port_t *port);

static int dlb_sleep_poll_cq(
    dlb_port_hdl_internal_t *hdl,
    dlb_shared_port_t *port);

dlb_port_hdl_t dlb_attach_ldb_port(
    dlb_domain_hdl_t hdl,
    int port_id)
{
    dlb_domain_hdl_internal_t *domain_hdl = hdl;
    dlb_port_hdl_internal_t *port_hdl = NULL;
    dlb_shared_domain_t *shared_domain;
    dlb_local_domain_t *local_domain;
    dlb_sw_credit_pool_t *ldb_pool;
    dlb_sw_credit_pool_t *dir_pool;
    dlb_shared_port_t *shared_port;
    dlb_local_port_t *local_port;
    bool unlock_domain = false;
    bool unlock_port = false;
    int ret = -1, err;

    VALIDATE_DOMAIN_HANDLE(hdl);

    local_domain = domain_hdl->domain.local;
    shared_domain = domain_hdl->domain.shared;

    ASSERT(port_id >= 0 && port_id < MAX_NUM_LDB_PORTS, EINVAL);

    pthread_mutex_lock(&shared_domain->resource_mutex);
    unlock_domain = true;

    ASSERT(shared_domain->ldb_ports[port_id].configured, EINVAL);

    shared_port = &shared_domain->ldb_ports[port_id];
    local_port = &local_domain->ldb_ports[port_id];

    pthread_mutex_lock(&shared_port->resource_mutex);
    unlock_port = true;

    port_hdl = calloc(1, sizeof(dlb_port_hdl_internal_t));
    ASSERT(port_hdl != NULL, ENOMEM);

    /* Allocate cache-line-aligned memory for sending QEs */
    err = posix_memalign((void **) &port_hdl->qe,
                         CACHE_LINE_SIZE,
                         CACHE_LINE_SIZE);
    ASSERT(err == 0, err);

    port_hdl->magic_num = PORT_MAGIC_NUM;
    port_hdl->wait_profile.type = DLB_WAIT_INTR;
    port_hdl->wait_func = dlb_block_on_cq_interrupt;
    port_hdl->port.shared = shared_port;
    port_hdl->port.local = local_port;
    port_hdl->domain_hdl = domain_hdl;
    port_hdl->device_version = domain_hdl->dlb->device_version;
    port_hdl->cap = domain_hdl->cap;
    port_hdl->shared_base = domain_hdl->shared_base;

    ldb_pool = &shared_domain->sw_credits.ldb_pools[shared_port->ldb_pool_id];
    dir_pool = &shared_domain->sw_credits.dir_pools[shared_port->dir_pool_id];

    port_hdl->credit_pool[LDB] = &ldb_pool->avail_credits;
    port_hdl->credit_pool[DIR] = &dir_pool->avail_credits;

    err = map_consumer_queue(port_hdl);
    ASSERT(err == 0, errno);

    err = map_producer_port(port_hdl);
    ASSERT(err == 0, errno);

    if (movdir64b_supported())
        port_hdl->enqueue_four = dlb_movdir64b;
    else
        port_hdl->enqueue_four = dlb_movntdq;

    /* Add the newly created handle to the port's linked list of handles */
    LIST_ADD(local_port->handles, port_hdl);

    ret = 0;

cleanup:

    if (ret) {
        if (port_hdl && port_hdl->pp_addr)
            munmap(port_hdl->pp_addr, DLB2_PP_SIZE);
        if (port_hdl && port_hdl->cq_base)
            munmap((void *)port_hdl->cq_base, DLB2_CQ_SIZE);
        if (port_hdl && port_hdl->qe)
            free(port_hdl->qe);
        if (port_hdl)
            free(port_hdl);
        port_hdl = NULL;
    }

    if (unlock_port)
        pthread_mutex_unlock(&shared_port->resource_mutex);
    if (unlock_domain)
        pthread_mutex_unlock(&shared_domain->resource_mutex);

    return (dlb_port_hdl_t) port_hdl;
}

dlb_port_hdl_t dlb_attach_dir_port(
    dlb_domain_hdl_t hdl,
    int port_id)
{
    dlb_domain_hdl_internal_t *domain_hdl = hdl;
    dlb_port_hdl_internal_t *port_hdl = NULL;
    dlb_shared_domain_t *shared_domain;
    dlb_local_domain_t *local_domain;
    dlb_sw_credit_pool_t *ldb_pool;
    dlb_sw_credit_pool_t *dir_pool;
    dlb_shared_port_t *shared_port;
    dlb_local_port_t *local_port;
    bool unlock_domain = false;
    bool unlock_port = false;
    int ret = -1, err;

    VALIDATE_DOMAIN_HANDLE(hdl);

    local_domain = domain_hdl->domain.local;
    shared_domain = domain_hdl->domain.shared;

    ASSERT(port_id >= 0 && port_id < MAX_NUM_DIR_PORTS, EINVAL);

    pthread_mutex_lock(&shared_domain->resource_mutex);
    unlock_domain = true;

    ASSERT(shared_domain->dir_ports[port_id].configured, EINVAL);

    shared_port = &shared_domain->dir_ports[port_id];
    local_port = &local_domain->dir_ports[port_id];

    pthread_mutex_lock(&shared_port->resource_mutex);
    unlock_port = true;

    port_hdl = calloc(1, sizeof(dlb_port_hdl_internal_t));
    ASSERT(port_hdl != NULL, ENOMEM);

    /* Allocate cache-line-aligned memory for sending QEs */
    err = posix_memalign((void **) &port_hdl->qe,
                         CACHE_LINE_SIZE,
                         CACHE_LINE_SIZE);
    ASSERT(err == 0, err);

    port_hdl->magic_num = PORT_MAGIC_NUM;
    port_hdl->wait_profile.type = DLB_WAIT_INTR;
    port_hdl->wait_func = dlb_block_on_cq_interrupt;
    port_hdl->port.shared = shared_port;
    port_hdl->port.local = local_port;
    port_hdl->domain_hdl = domain_hdl;
    port_hdl->device_version = domain_hdl->dlb->device_version;
    port_hdl->cap = domain_hdl->cap;
    port_hdl->shared_base = domain_hdl->shared_base;

    ldb_pool = &shared_domain->sw_credits.ldb_pools[shared_port->ldb_pool_id];
    dir_pool = &shared_domain->sw_credits.dir_pools[shared_port->dir_pool_id];

    port_hdl->credit_pool[LDB] = &ldb_pool->avail_credits;
    port_hdl->credit_pool[DIR] = &dir_pool->avail_credits;

    err = map_consumer_queue(port_hdl);
    ASSERT(err == 0, errno);

    err = map_producer_port(port_hdl);
    ASSERT(err == 0, errno);

    if (movdir64b_supported())
        port_hdl->enqueue_four = dlb_movdir64b;
    else
        port_hdl->enqueue_four = dlb_movntdq;

    /* Add the new handle to the port's linked list of handles */
    LIST_ADD(local_port->handles, port_hdl);

    ret = 0;

cleanup:

    if (ret) {
        if (port_hdl && port_hdl->pp_addr)
            munmap(port_hdl->pp_addr, DLB2_PP_SIZE);
        if (port_hdl && port_hdl->cq_base)
            munmap((void *)port_hdl->cq_base, DLB2_CQ_SIZE);
        if (port_hdl && port_hdl->qe)
            free(port_hdl->qe);
        if (port_hdl)
            free(port_hdl);
        port_hdl = NULL;
    }

    if (unlock_port)
        pthread_mutex_unlock(&shared_port->resource_mutex);
    if (unlock_domain)
        pthread_mutex_unlock(&shared_domain->resource_mutex);

    return (dlb_port_hdl_t) port_hdl;
}

int dlb_detach_port(
    dlb_port_hdl_t hdl)
{
    dlb_port_hdl_internal_t *port_hdl = hdl;
    dlb_shared_port_t *shared_port;
    dlb_local_port_t *local_port;
    bool unlock = false;
    int ret = -1;
    bool found;

    VALIDATE_PORT_HANDLE(hdl);

    shared_port = port_hdl->port.shared;
    local_port = port_hdl->port.local;

    pthread_mutex_lock(&shared_port->resource_mutex);
    unlock = true;

    /* Remove the handle from the port's handles list */
    LIST_DEL(local_port->handles, hdl, found);

    if (!found) {
        printf("[%s()] Internal error: couldn't delete the port handle\n",
               __func__);
        ASSERT(0, EINVAL);
    }

    munmap(port_hdl->pp_addr, DLB2_PP_SIZE);
    munmap((void *)port_hdl->cq_base, DLB2_CQ_SIZE);

    free(port_hdl->qe);

    memset(hdl, 0, sizeof(dlb_port_hdl_internal_t));
    free(hdl);

    ret = 0;

cleanup:

    if (unlock)
        pthread_mutex_unlock(&shared_port->resource_mutex);

    return ret;
}

static inline uint64_t
get_tsc_freq(void)
{
    if (cpuid_max() >= 0x15) {
        return cpuid_tsc_freq();
    } else {
        printf("[%s()] Internal error:CPUID leaf 0x15 not supported\n",
               __func__);
        return 0;
    }
}

int dlb_set_wait_profile(
    dlb_port_hdl_t hdl,
    dlb_api_class_t class,
    dlb_wait_profile_t profile)
{
    dlb_port_hdl_internal_t *port_hdl = hdl;
    dlb_shared_port_t *shared_port;
    bool unlock = false;
    int ret = -1;

    VALIDATE_PORT_HANDLE(hdl);

    ASSERT(class < DLB_NUM_API_CLASSES, EINVAL);
    ASSERT(profile.type < DLB_NUM_WAIT_TYPES, EINVAL);

    if (profile.type == DLB_WAIT_INTR_LOW_POWER) {
        ASSERT(umwait_supported(), EINVAL);
        ASSERT(get_tsc_freq() > 0, EINVAL);
    }

    shared_port = port_hdl->port.shared;

    pthread_mutex_lock(&shared_port->resource_mutex);
    unlock = true;

    /* Since there's only one API class, we only need 1 wait profile */
    port_hdl->wait_profile = profile;

    if (profile.type == DLB_WAIT_INTR)
        port_hdl->wait_func = dlb_block_on_cq_interrupt;
    else if (profile.type == DLB_WAIT_INTR_LOW_POWER)
        port_hdl->wait_func = dlb_block_on_umwait;
    else if (profile.type == DLB_WAIT_TIMEOUT_HARD_POLL)
        port_hdl->wait_func = dlb_hard_poll_cq;
    else if (profile.type == DLB_WAIT_TIMEOUT_SLEEP_POLL)
        port_hdl->wait_func = dlb_sleep_poll_cq;

    if (profile.type == DLB_WAIT_INTR_LOW_POWER) {
        port_hdl->umwait_ticks = get_tsc_freq() * profile.timeout_value_ns;
        port_hdl->umwait_ticks /= NS_PER_S;
    }

    ret = 0;

cleanup:

    if (unlock)
        pthread_mutex_unlock(&shared_port->resource_mutex);

    return ret;
}

int dlb_get_wait_profile(
    dlb_port_hdl_t hdl,
    dlb_api_class_t class,
    dlb_wait_profile_t *profile)
{
    dlb_port_hdl_internal_t *port_hdl = hdl;
    dlb_shared_port_t *shared_port;
    bool unlock = false;
    int ret = -1;

    VALIDATE_PORT_HANDLE(hdl);

    ASSERT(class < DLB_NUM_API_CLASSES, EINVAL);
    ASSERT(profile, EINVAL);

    shared_port = port_hdl->port.shared;

    pthread_mutex_lock(&shared_port->resource_mutex);
    unlock = true;

    *profile = port_hdl->wait_profile;

    ret = 0;

cleanup:

    if (unlock)
        pthread_mutex_unlock(&shared_port->resource_mutex);

    return ret;
}

int dlb_enable_cq_weight(
    dlb_port_hdl_t hdl)
{
    dlb_port_hdl_internal_t *port_hdl = hdl;
    dlb_shared_port_t *shared_port;
    bool unlock = false;
    int ret = -1;

    VALIDATE_PORT_HANDLE(hdl);

    ASSERT(port_hdl->device_version.ver == VER_DLB2_5, EINVAL);

    shared_port = port_hdl->port.shared;

    pthread_mutex_lock(&shared_port->resource_mutex);
    unlock = true;

    ret = dlb2_ioctl_enable_cq_weight(port_hdl->domain_hdl->fd,
                                      shared_port->id,
                                      shared_port->cq_depth - 1);

cleanup:

    if (unlock)
        pthread_mutex_unlock(&shared_port->resource_mutex);

    return ret;
}

int dlb_link_queue(
    dlb_port_hdl_t hdl,
    int qid,
    int priority)
{
    dlb_port_hdl_internal_t *port_hdl = hdl;
    dlb_domain_hdl_internal_t *domain_hdl;
    bool unlock = false;
    dlb_port_t *port;
    int fd, ret = -1;

    ASSERT(priority >= 0 && priority <= 7, EINVAL);
    ASSERT(qid < MAX_NUM_LDB_QUEUES, EINVAL);

    VALIDATE_PORT_HANDLE(hdl);

    port = &port_hdl->port;

    pthread_mutex_lock(&port->shared->resource_mutex);
    unlock = true;

    domain_hdl = port_hdl->domain_hdl;
    fd = domain_hdl->fd;

    ret = dlb2_ioctl_link_qid(fd, port->shared->id, qid, priority);

    ASSERT(ret == 0, errno);

cleanup:

    if (unlock)
        pthread_mutex_unlock(&port->shared->resource_mutex);

    return ret;
}

int dlb_unlink_queue(
    dlb_port_hdl_t hdl,
    int qid)
{
    dlb_port_hdl_internal_t *port_hdl = hdl;
    dlb_domain_hdl_internal_t *domain_hdl;
    bool unlock = false;
    dlb_port_t *port;
    int ret = -1;

    VALIDATE_PORT_HANDLE(hdl);
    ASSERT(qid < MAX_NUM_LDB_QUEUES, EINVAL);

    port = &port_hdl->port;

    pthread_mutex_lock(&port->shared->resource_mutex);
    unlock = true;

    domain_hdl = port_hdl->domain_hdl;

    ret = dlb2_ioctl_unlink_qid(domain_hdl->fd, port->shared->id, qid);

    ASSERT(ret == 0, errno);

cleanup:

    if (unlock)
        pthread_mutex_unlock(&port->shared->resource_mutex);

    return ret;
}

static int __dlb_enable_port(
    dlb_port_hdl_t hdl,
    bool sched_only)
{
    dlb_port_hdl_internal_t *port_hdl = hdl;
    dlb_domain_hdl_internal_t *domain_hdl;
    bool unlock = false;
    dlb_port_t *port;
    int ret = -1;

    VALIDATE_PORT_HANDLE(hdl);

    port = &port_hdl->port;

    pthread_mutex_lock(&port->shared->resource_mutex);
    unlock = true;

    domain_hdl = port_hdl->domain_hdl;

    if (port->shared->type == LDB)
        ret = dlb2_ioctl_enable_ldb_port(domain_hdl->fd, port->shared->id);
    else
        ret = dlb2_ioctl_enable_dir_port(domain_hdl->fd, port->shared->id);

    if (!sched_only)
        port->shared->enabled = true;

cleanup:

    if (unlock)
        pthread_mutex_unlock(&port->shared->resource_mutex);

    return ret;
}

int dlb_enable_port(
    dlb_port_hdl_t hdl)
{
    return __dlb_enable_port(hdl, false);
}

int dlb_enable_port_sched(
    dlb_port_hdl_t hdl)
{
    return __dlb_enable_port(hdl, true);
}

static inline void dlb2_check_and_release_credits(
    dlb_port_hdl_internal_t *port_hdl,
    int type,
    bool cond)
{
    bool is_2_5 = port_hdl->device_version.ver == VER_DLB2_5;
    int credit_threshold = credit_return[type].credit_thres;
    int cnt_threshold = credit_return[type].cnt_thres;
    int rem = credit_return[type].credit_rem;
    dlb_port_type_t port_type;
    bool cnt_reset = true;

    for (port_type = 0; port_type < NUM_PORT_TYPES; ++port_type) {
        if (is_2_5 && port_type == DIR)
            continue;
        if (cond && port_hdl->port.shared->credits[port_type].num > credit_threshold) {
            if (port_hdl->port.shared->credit_return_count[type] >= cnt_threshold) {
                int val = port_hdl->port.shared->credits[port_type].num - rem;

                if (val <= 0)
                    continue;
                __sync_fetch_and_add(port_hdl->credit_pool[port_type], val);
                port_hdl->port.shared->credits[port_type].num -= val;
            } else {
                cnt_reset = false;
            }
        }
    }
    if (cnt_threshold) {
        if (cnt_reset)
            port_hdl->port.shared->credit_return_count[type] = 0;
        else
            port_hdl->port.shared->credit_return_count[type]++;
    }
}

static int __dlb_disable_port(
    dlb_port_hdl_t hdl,
    bool sched_only)
{
    dlb_port_hdl_internal_t *port_hdl = hdl;
    dlb_domain_hdl_internal_t *domain_hdl;
    bool unlock = false;
    dlb_port_t *port;
    int fd, ret = -1;

    VALIDATE_PORT_HANDLE(hdl);

    port = &port_hdl->port;

    pthread_mutex_lock(&port->shared->resource_mutex);
    unlock = true;

    dlb2_check_and_release_credits(port_hdl, RETURN_ALL, true);
    domain_hdl = port_hdl->domain_hdl;
    fd = domain_hdl->fd;

    if (port->shared->type == LDB)
        ret = dlb2_ioctl_disable_ldb_port(fd, port->shared->id);
    else
        ret = dlb2_ioctl_disable_dir_port(fd, port->shared->id);

    if (!sched_only)
        port->shared->enabled = false;

cleanup:

    if (unlock)
        pthread_mutex_unlock(&port->shared->resource_mutex);

    return ret;
}

int dlb_disable_port(
    dlb_port_hdl_t hdl)
{
    return __dlb_disable_port(hdl, false);
}

int dlb_disable_port_sched(
    dlb_port_hdl_t hdl)
{
    return __dlb_disable_port(hdl, true);
}

int dlb_start_sched_domain(
    dlb_domain_hdl_t hdl)
{
    dlb_domain_hdl_internal_t *domain_hdl = hdl;
    dlb_domain_t *domain;
    bool unlock = false;
    int ret = -1;

    VALIDATE_DOMAIN_HANDLE(hdl);

    domain = &domain_hdl->domain;

    pthread_mutex_lock(&domain->shared->resource_mutex);
    unlock = true;

    ASSERT(domain->shared->alert_thread_started, ESRCH);

    dlb2_ioctl_start_domain(domain_hdl->fd);

    domain->shared->started = true;

    ret = 0;

cleanup:

    if (unlock)
        pthread_mutex_unlock(&domain->shared->resource_mutex);

    return ret;
}

int dlb_reset_sched_domain(
    dlb_hdl_t hdl,
    int domain_id)
{
    dlb_shared_domain_t *shared_domain;
    dlb_local_domain_t *local_domain;
    dlb_t *dlb = (dlb_t *) hdl;
    bool unlock = false;
    int ret = -1;

    VALIDATE_DLB_HANDLE(hdl);
    ASSERT(domain_id < MAX_NUM_SCHED_DOMAINS, EINVAL);

    local_domain = &dlb->local_domains[domain_id];
    shared_domain = dlb->shared_domains[domain_id];

    /* Only the process that created the domain can reset it. */
    ASSERT(local_domain->creator, EPERM);

    ASSERT(shared_domain, EINVAL);

    pthread_mutex_lock(&shared_domain->resource_mutex);
    unlock = true;

    ASSERT(shared_domain->configured, EINVAL);

    /* A domain handle can't be detached if there are any remaining port
     * handles.
     */
    ASSERT(!shared_domain->refcnt, EEXIST);

    /* Unlink the shm file so no new domain attach operations will succeed,
     * then unlock and unmap it. This, combined with the domain's 0 refcnt,
     * means that once the domain alert thread returns then the domain's
     * resource mutex is no longer needed.
     */
    ASSERT(!dlb_shm_unlink(dlb->id, domain_id), EPERM);

    /* A domain will reset automatically when all the domain fds are closed and
     * mmaps are freed. This must be done *before* freeing the CQ memory,
     * because closing these causes the driver to reset the domain, and the
     * reset ensures that no more QEs will be scheduled to the CQ. If the CQ
     * memory is freed first, subsequent QEs could corrupt that freed memory.
     */

    /* Wake this domain's alert thread and prevent further reads */
    dlb2_ioctl_enqueue_domain_alert(local_domain->creator_fd,
                                    DLB_DOMAIN_USER_ALERT_RESET);

    while (1) {
        bool started;

        started = shared_domain->alert_thread_started;

        pthread_mutex_unlock(&shared_domain->resource_mutex);
        unlock = true;

        if (!started)
            break;

        sched_yield();

        pthread_mutex_lock(&shared_domain->resource_mutex);
        unlock = false;
    }

    /* Wake the socket thread and wait for it to exit. shutdown() causes accept
     * to unblock and return EINVAL.
     */
#ifndef DLB_NOT_USE_DOMAIN_SERVER
    shutdown(local_domain->socket_fd, SHUT_RDWR);
    pthread_join(local_domain->socket_thread, NULL);
#endif

    /* Close the last fd connected to the domain device file, which causes the
     * driver to reset the domain. The shared memory munmap() must follow this
     * call, to ensure DLB can safely write to the CQ and popcount during the
     * reset.
     */
    close(local_domain->creator_fd);

    shared_domain->configured = false;

    munmap(shared_domain, sizeof(dlb_shared_domain_t));

    dlb->shared_domains[domain_id] = NULL;

    ret = 0;

cleanup:

    if (ret && unlock)
        pthread_mutex_unlock(&shared_domain->resource_mutex);

    return ret;
}

static void dlb_disable_ports(
    dlb_shared_domain_t *domain)
{
    int i;

    for (i = 0; i < MAX_NUM_LDB_PORTS; i++) {
        dlb_shared_port_t *port = &domain->ldb_ports[i];

        pthread_mutex_lock(&port->resource_mutex);
        port->enabled = false;
        pthread_mutex_unlock(&port->resource_mutex);
    }

    for (i = 0; i < MAX_NUM_DIR_PORTS; i++) {
        dlb_shared_port_t *port = &domain->dir_ports[i];

        pthread_mutex_lock(&port->resource_mutex);
        port->enabled = false;
        pthread_mutex_unlock(&port->resource_mutex);
    }
}

static int dlb2_read_domain_device_file(
    dlb_shared_domain_t *domain,
    int fd,
    dlb_alert_t *alert)
{
    struct dlb2_domain_alert kernel_alert;

    int ret = read(fd, (void *)&kernel_alert, sizeof(kernel_alert));

    if (ret == 0) {
        ret = -1;
        ASSERT(0, ENOENT);
    } else if (ret < 0) {
        ASSERT(0, errno);
    }

    ret = 0;
    alert->data = kernel_alert.aux_alert_data;

    switch (kernel_alert.alert_id) {
    case DLB2_DOMAIN_ALERT_DEVICE_RESET:
        dlb_disable_ports(domain);
        alert->id = DLB_ALERT_DEVICE_RESET;
        break;

    case DLB2_DOMAIN_ALERT_USER:
        if (kernel_alert.aux_alert_data == DLB_DOMAIN_USER_ALERT_RESET)
            alert->id = DLB_ALERT_DOMAIN_RESET;
        break;

    case DLB2_DOMAIN_ALERT_CQ_WATCHDOG_TIMEOUT:
        alert->id = DLB_ALERT_CQ_WATCHDOG_TIMEOUT;
        alert->data = kernel_alert.aux_alert_data;
        break;

    default:
        if (kernel_alert.alert_id < NUM_DLB2_DOMAIN_ALERTS)
            printf("[%s()] Internal error: received kernel alert %s\n",
                   __func__,
                   dlb2_domain_alert_strings[kernel_alert.alert_id]);
        else
            printf("[%s()] Internal error: received invalid alert id %llu\n",
                   __func__, kernel_alert.alert_id);
        ASSERT(0, EINVAL);
        break;
    }

cleanup:
    return ret;
}

static void *__alert_fn(void *__args)
{
    dlb_domain_t *domain = __args;

    while (1) {
        dlb_alert_t alert;
        int ret;

        ret = (dlb2_read_domain_device_file(domain->shared,
                                            domain->local->creator_fd,
                                            &alert));

        if (ret)
            break;

        if (domain->local->thread.fn)
            domain->local->thread.fn(&alert,
                                     domain->shared->id,
                                     domain->local->thread.arg);

        if (alert.id == DLB_ALERT_DOMAIN_RESET ||
            alert.id == DLB_ALERT_DEVICE_RESET)
            break;
    }

    pthread_mutex_lock(&domain->shared->resource_mutex);

    domain->shared->alert_thread_started = false;

    pthread_mutex_unlock(&domain->shared->resource_mutex);

    /* domain is allocated in dlb_launch_domain_alert_thread() */
    free(domain);

    return NULL;
}

int dlb_launch_domain_alert_thread(
    dlb_domain_hdl_t hdl,
    domain_alert_callback_t callback,
    void *callback_arg)
{
    dlb_domain_hdl_internal_t *domain_hdl = hdl;
    dlb_domain_t *domain = NULL;
    bool attr_init = false;
    pthread_t alert_thread;
    pthread_attr_t attr;
    bool unlock = false;
    int domain_id, err;
    int ret = -1;

    /* Run the alert thread as detached so the thread state memory is freed when
     * it terminates.
     */
    err = pthread_attr_init(&attr);
    ASSERT(err == 0, err);

    attr_init = true;

    err = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    ASSERT(err == 0, err);

    VALIDATE_DOMAIN_HANDLE(hdl);

    domain_id = domain_hdl->domain.shared->id;

    /* Freed by the child thread */
    domain = calloc(1, sizeof(dlb_domain_t));
    ASSERT(domain, ENOMEM);

    /* Use the dlb handle's shared_domain pointer, since it won't be unmapped
     * until dlb_reset_sched_domain().
     */
    *domain = domain_hdl->domain;
    domain->shared = domain_hdl->dlb->shared_domains[domain_id];

    pthread_mutex_lock(&domain_hdl->domain.shared->resource_mutex);
    unlock = true;

    /* Only the process that created the domain can launch the alert thread. */
    ASSERT(domain->local->creator, EPERM);

    /* Only one thread per domain allowed */
    ASSERT(!domain_hdl->domain.shared->alert_thread_started, EEXIST);

    domain->local->thread.fn = callback;
    domain->local->thread.arg = callback_arg;

    err = pthread_create(&alert_thread, &attr, __alert_fn, domain);
    ASSERT(err == 0, err);

    domain_hdl->domain.shared->alert_thread_started = true;

    ret = 0;

cleanup:
    if (unlock)
        pthread_mutex_unlock(&domain_hdl->domain.shared->resource_mutex);

    if (ret) {
        if (domain)
            free(domain);
    }

    if (attr_init)
        pthread_attr_destroy(&attr);

    return ret;
}

/****************************************/
/* Scheduling domain datapath functions */
/****************************************/
static const bool credits_required[NUM_EVENT_CMD_TYPES] = {
    false, /* NOOP */
    false, /* BAT_T */
    false, /* REL */
    false, /* REL_t */
    false, /* (unused) */
    false, /* (unused) */
    false, /* (unused) */
    false, /* (unused) */
    true,  /* NEW */
    true,  /* NEW_T */
    true,  /* FWD */
    true,  /* FWD_T */
    true,  /* FRAG */
    true,  /* FRAG_T */
};

static bool cmd_releases_hist_list_entry(
    enum dlb_event_cmd_t cmd)
{
       return (cmd == REL || cmd == REL_T || cmd == FWD || cmd == FWD_T);
}

static inline bool is_enq_hcw(
    dlb_event_t *event)
{
    enum dlb_event_cmd_t cmd = event->adv_send.cmd;

    return cmd == NEW || cmd == NEW_T || cmd == FWD || cmd == FWD_T;
}

static inline bool validate_send_events(
    dlb_shared_domain_t *domain,
    dlb_shared_port_t *port,
    dlb_event_t *evts,
    uint32_t num)
{
    dlb_queue_type_t queue_type;
    dlb_port_type_t sched_type;
    int i;

    for (i = 0; i < num; i++) {
        sched_type = (evts[i].adv_send.sched_type == SCHED_DIRECTED);
        queue_type = domain->queue_type[sched_type][evts[i].adv_send.queue_id];

        if (!is_enq_hcw(&evts[i]))
            return false;

        CHECK(queue_type != QUEUE_TYPE_INVALID, EINVAL);
    }

    return false;

cleanup:
    return true;
}

static inline void dec_port_owed_releases(
    dlb_shared_port_t *port,
    dlb_enqueue_qe_t *enqueue_qe)
{
    enum dlb_event_cmd_t cmd = enqueue_qe->cmd_info.qe_cmd;

    port->owed_releases -= cmd_releases_hist_list_entry(cmd);
}

static inline void inc_port_owed_releases(
    dlb_shared_port_t *port,
    int cnt)
{
    port->owed_releases += cnt;
}

static inline void dec_port_owed_tokens(
    dlb_shared_port_t *port,
    dlb_enqueue_qe_t *enqueue_qe)
{
    enum dlb_event_cmd_t cmd = enqueue_qe->cmd_info.qe_cmd;

    /* All token return commands set bit 0. BAT_T is a special case. */
    if (cmd & 0x1) {
        port->owed_tokens--;
        if (cmd == BAT_T)
            port->owed_tokens -= enqueue_qe->flow_id;
    }
}

static inline void inc_port_owed_tokens(
    dlb_shared_port_t *port,
    int cnt)
{
    if (port->use_rsvd_token_scheme) {
        if (cnt < port->cq_rsvd_token_deficit) {
            port->cq_rsvd_token_deficit -= cnt;
        } else {
            port->owed_tokens += cnt - port->cq_rsvd_token_deficit;
            port->cq_rsvd_token_deficit = 0;
        }
    } else {
        port->owed_tokens += cnt;
    }
}

static inline void dlb2_release_port_credits(
    dlb_port_hdl_internal_t *port_hdl,
    uint32_t count,
    bool is_2_5)
{
    /* When a port's local credit cache reaches a threshold, release them back
     * to the domain's pool only keeping batch_size credits.
     */
    dlb2_check_and_release_credits(port_hdl, BATCH_2SZ_EXCEED, true);
    dlb2_check_and_release_credits(port_hdl, ENQ_FAIL, !count);
}

static inline void dlb2_refresh_port_credits(
    dlb_port_hdl_internal_t *port_hdl,
    dlb_shared_port_t *port,
    dlb_port_type_t type)
{
    uint32_t credits = *port_hdl->credit_pool[type];
    uint32_t batch_size = DLB_SW_CREDIT_BATCH_SZ;

    batch_size = (credits < batch_size) ? credits : batch_size;

    if (credits && __sync_bool_compare_and_swap(port_hdl->credit_pool[type],
                                                credits,
                                                credits - batch_size))
        port->credits[type].num += batch_size;
}

static inline void dlb2_inc_port_credits(
    dlb_shared_port_t *port,
    int num,
    bool is_2_5)
{
    if (is_2_5)
        port->credits[LDB].num += num;
    else
        port->credits[port->type].num += num;
}

static inline int __attribute__((always_inline)) __dlb2_adv_send_no_credits(
    dlb_port_hdl_t hdl,
    uint32_t num,
    dlb_event_t *evts,
    bool is_bat_t,
    bool is_2_5)
{
    dlb_port_hdl_internal_t *port_hdl = hdl;
    dlb_enqueue_qe_t *enqueue_qe;
    dlb_shared_port_t *port;
    bool is_dir_port;
    int i, j, count;

    count = -1;

    enqueue_qe = port_hdl->qe;
    port = port_hdl->port.shared;

    if (!is_bat_t)
        ASSERT(port->enabled, EACCES);
    CHECK(port_hdl->domain_hdl->domain.shared->started, EPERM);

    is_dir_port = port->type == DIR;

    count = 0;

    /* Process the send events. DLB accepts 4 QEs (one cache line's worth) at a
     * time, so process in chunks of four.
     */
    for (i = 0; i < num; i += 4) {

        /* Use a store fence to ensure that only one write-combining
         * operation is present from this core on the system bus at a time.
         */
        if (!is_bat_t)
            _mm_sfence();

        /* Initialize the four commands to NOOP and zero int_arm and rsvd */
        enqueue_qe[0].cmd_byte = NOOP;
        enqueue_qe[0].misc_byte = 0;
        enqueue_qe[1].cmd_byte = NOOP;
        enqueue_qe[1].misc_byte = 0;
        enqueue_qe[2].cmd_byte = NOOP;
        enqueue_qe[2].misc_byte = 0;
        enqueue_qe[3].cmd_byte = NOOP;
        enqueue_qe[3].misc_byte = 0;

        for (j = 0; j < 4 && (i + j) < num; j++, count++) {
            dlb_adv_send_t *adv_send;

            adv_send = &evts[i+j].adv_send;

            /* Copy the 16B QE */
            memcpy(&enqueue_qe[j], adv_send, BYTES_PER_QE);

            /* Zero out meas_lat, no_dec, cmp_id, int_arm, error, and rsvd */
            enqueue_qe[j].misc_byte = 0;
            enqueue_qe[j].cmd_byte &= QE_CMD_MASK;

            dec_port_owed_tokens(port, &enqueue_qe[j]);
            dec_port_owed_releases(port, &enqueue_qe[j]);

            /* Clear the qe_comp bit if the sender is a directed port */
            if (is_dir_port)
                enqueue_qe[j].cmd_byte &= ~(1 << QE_COMP_SHIFT);
        }

        if (j != 0)
            port_hdl->enqueue_four(enqueue_qe, port_hdl->pp_addr);

        if (j != 4)
            break;
    }

cleanup:

    dlb2_release_port_credits(port_hdl, count, is_2_5);

    return count;
}

static inline int __attribute__((always_inline)) __dlb2_adv_send(
    dlb_port_hdl_t hdl,
    uint32_t num,
    dlb_event_t *evts,
    bool issue_store_fence,
    bool credits_required_for_all_cmds)
{
    dlb_port_hdl_internal_t *port_hdl = hdl;
    int used_credits[NUM_PORT_TYPES];
    dlb_enqueue_qe_t *enqueue_qe;
    dlb_shared_domain_t *domain;
    dlb_shared_port_t *port;
    int i, j, count;

    count = -1;

    enqueue_qe = port_hdl->qe;
    port = port_hdl->port.shared;
    domain = port_hdl->domain_hdl->domain.shared;

    ASSERT(port->enabled, EACCES);
    CHECK(domain->started, EPERM);

    if (validate_send_events(domain, port, evts, num))
        goto cleanup;

    used_credits[DIR] = 0;
    used_credits[LDB] = 0;

    count = 0;

    /* Process the send events. DLB accepts 4 QEs (one cache line's worth) at a
     * time, so process in chunks of four.
     */
    for (i = 0; i < num; i += 4) {

        /* Use a store fence to ensure that writes to the pointed-to data have
         * completed before enqueueing the HCW, and that only one
         * write-combining operation is present from this core on the system
         * bus at a time.
         */
        if (issue_store_fence)
            _mm_sfence();

        /* Initialize the four commands to NOOP and zero int_arm and rsvd */
        enqueue_qe[0].cmd_byte = NOOP;
        enqueue_qe[0].misc_byte = 0;
        enqueue_qe[1].cmd_byte = NOOP;
        enqueue_qe[1].misc_byte = 0;
        enqueue_qe[2].cmd_byte = NOOP;
        enqueue_qe[2].misc_byte = 0;
        enqueue_qe[3].cmd_byte = NOOP;
        enqueue_qe[3].misc_byte = 0;

        for (j = 0; j < 4 && (i + j) < num; j++, count++) {
            dlb_adv_send_t *adv_send;
            dlb_port_type_t type;

            adv_send = &evts[i+j].adv_send;

            type = (adv_send->sched_type == SCHED_DIRECTED);

            /* Copy the 16B QE */
            memcpy(&enqueue_qe[j], adv_send, BYTES_PER_QE);

            /* Zero out meas_lat, no_dec, cmp_id, int_arm, error, and rsvd */
            enqueue_qe[j].misc_byte = 0;
            enqueue_qe[j].cmd_byte &= QE_CMD_MASK;

            if (!credits_required_for_all_cmds &&
                !credits_required[adv_send->cmd]) {
                dec_port_owed_tokens(port, &enqueue_qe[j]);
                dec_port_owed_releases(port, &enqueue_qe[j]);
                continue;
            }

            /* Check credit availability */
            if (port->credits[type].num == used_credits[type]) {

                dlb2_refresh_port_credits(port_hdl, port, type);

                if (port->credits[type].num == used_credits[type]) {
                    /* Undo the 16B QE copy by setting cmd to NOOP */
                    enqueue_qe[j].cmd_byte = 0;
                    break;
                }
            }

            /* Clear the qe_comp bit if the sender is a directed port */
            enqueue_qe[j].cmd_byte &= ~((port->type == DIR) << 1);

            dec_port_owed_tokens(port, &enqueue_qe[j]);
            dec_port_owed_releases(port, &enqueue_qe[j]);

            used_credits[type]++;
        }

        if (j != 0)
            port_hdl->enqueue_four(enqueue_qe, port_hdl->pp_addr);

        if (j != 4)
            break;
    }

    port->credits[LDB].num -= used_credits[LDB];
    port->credits[DIR].num -= used_credits[DIR];

cleanup:

    dlb2_release_port_credits(port_hdl, count, false);

    return count;
}

static inline int __attribute__((always_inline)) __dlb2_5_adv_send(
    dlb_port_hdl_t hdl,
    uint32_t num,
    dlb_event_t *evts,
    bool issue_store_fence,
    bool credits_required_for_all_cmds)
{
    dlb_port_hdl_internal_t *port_hdl = hdl;
    int used_credits;
    dlb_enqueue_qe_t *enqueue_qe;
    dlb_shared_domain_t *domain;
    dlb_shared_port_t *port;
    int i, j, count;

    count = -1;

    enqueue_qe = port_hdl->qe;
    port = port_hdl->port.shared;
    domain = port_hdl->domain_hdl->domain.shared;

    ASSERT(port->enabled, EACCES);
    CHECK(domain->started, EPERM);

    if (validate_send_events(domain, port, evts, num))
        goto cleanup;

    used_credits = 0;

    count = 0;

    /* Process the send events. DLB accepts 4 QEs (one cache line's worth) at a
     * time, so process in chunks of four.
     */
    for (i = 0; i < num; i += 4) {

        /* Use a store fence to ensure that writes to the pointed-to data have
         * completed before enqueueing the HCW, and that only one
         * write-combining operation is present from this core on the system
         * bus at a time.
         */
        if (issue_store_fence)
            _mm_sfence();

        /* Initialize the four commands to NOOP and zero int_arm and rsvd */
        enqueue_qe[0].cmd_byte = NOOP;
        enqueue_qe[0].misc_byte = 0;
        enqueue_qe[1].cmd_byte = NOOP;
        enqueue_qe[1].misc_byte = 0;
        enqueue_qe[2].cmd_byte = NOOP;
        enqueue_qe[2].misc_byte = 0;
        enqueue_qe[3].cmd_byte = NOOP;
        enqueue_qe[3].misc_byte = 0;

        for (j = 0; j < 4 && (i + j) < num; j++, count++) {
            dlb_adv_send_t *adv_send;

            adv_send = &evts[i+j].adv_send;

            /* Copy the 16B QE */
            memcpy(&enqueue_qe[j], adv_send, BYTES_PER_QE);

            /* Zero out meas_lat, no_dec, cmp_id, int_arm, error, and rsvd */
            enqueue_qe[j].misc_byte &= QE_WEIGHT_MASK;
            enqueue_qe[j].cmd_byte &= QE_CMD_MASK;

            if (!credits_required_for_all_cmds &&
                !credits_required[adv_send->cmd]) {
                dec_port_owed_tokens(port, &enqueue_qe[j]);
                dec_port_owed_releases(port, &enqueue_qe[j]);
                continue;
            }

            /* Check credit availability */
            if (port->credits[LDB].num == used_credits) {

                dlb2_refresh_port_credits(port_hdl, port, LDB);

                if (port->credits[LDB].num == used_credits) {
                    /* Undo the 16B QE copy by setting cmd to NOOP */
                    enqueue_qe[j].cmd_byte = 0;
                    break;
                }
            }

            /* Clear the qe_comp bit if the sender is a directed port */
            enqueue_qe[j].cmd_byte &= ~((port->type == DIR) << 1);

            dec_port_owed_tokens(port, &enqueue_qe[j]);
            dec_port_owed_releases(port, &enqueue_qe[j]);

            used_credits++;
        }

        if (j != 0)
            port_hdl->enqueue_four(enqueue_qe, port_hdl->pp_addr);

        if (j != 4)
            break;
    }

    port->credits[LDB].num -= used_credits;

cleanup:
    dlb2_release_port_credits(port_hdl, count, true);

    return count;
}

static inline int dlb_adv_send_wrapper(
    dlb_port_hdl_t hdl,
    uint32_t num,
    dlb_event_t *send,
    enum dlb_event_cmd_t cmd)
{
    dlb_port_hdl_internal_t *port_hdl = hdl;
    int i, ret = -1;
    bool is_bat_t;

    CHECK_PORT_HANDLE(hdl);

    CHECK(send != NULL, EINVAL);
    CHECK(port_hdl->domain_hdl->domain.shared->started, EPERM);

    for (i = 0; i < num; i++)
        send[i].adv_send.cmd = cmd;

    is_bat_t = cmd;

    /* Since we're sending the same command for all events, we can use
     * specialized send functions according to whether or not credits
     * are required.
     *
     * A store fence isn't required if this is a BAT_T command, which is safe
     * to reorder and doesn't point to any data.
     */
    if (port_hdl->device_version.ver == VER_DLB2) {
        if (credits_required[cmd])
            ret = __dlb2_adv_send(hdl, num, send, true, true);
        else
            ret = __dlb2_adv_send_no_credits(hdl, num, send, is_bat_t, false);
    } else {
        if (credits_required[cmd])
            ret = __dlb2_5_adv_send(hdl, num, send, true, true);
        else
            ret = __dlb2_adv_send_no_credits(hdl, num, send, is_bat_t, true);
    }

cleanup:
    return ret;
}

int dlb_send(
    dlb_port_hdl_t hdl,
    uint32_t num,
    dlb_event_t *event)
{
    return dlb_adv_send_wrapper(hdl, num, event, NEW);
}

int dlb_release(
    dlb_port_hdl_t hdl,
    uint32_t num)
{
    /* This variable intentionally left blank */
    dlb_event_t send[num];
    dlb_port_t *port;
    int ret = -1;

    CHECK_PORT_HANDLE(hdl);

    port = &((dlb_port_hdl_internal_t *)hdl)->port;

    CHECK(port->shared->type == LDB, EINVAL);

    /* Prevent the user from releasing more events than are owed. */
    if (num > port->shared->owed_releases)
        num = port->shared->owed_releases;

    ret = dlb_adv_send_wrapper(hdl, num, send, REL);

cleanup:
    return ret;
}

int dlb_forward(
    dlb_port_hdl_t hdl,
    uint32_t num,
    dlb_event_t *event)
{
    return dlb_adv_send_wrapper(hdl, num, event, FWD);
}

int dlb_pop_cq(
    dlb_port_hdl_t hdl,
    uint32_t num)
{
    dlb_event_t send = {0};
    dlb_port_t *port;
    int ret = -1;

    CHECK_PORT_HANDLE(hdl);

    port = &((dlb_port_hdl_internal_t *)hdl)->port;

    /* Prevent the user from popping more tokens than are owed. This is
     * required when using dlb_recv_no_pop() and CQ interrupts (see
     * __dlb_block_on_cq_interrupt() for more details), and prevents user
     * errors when using dlb_recv().
     */
    send.adv_send.num_tokens_minus_one = (num < port->shared->owed_tokens) ?
                                          num : port->shared->owed_tokens;
    if (send.adv_send.num_tokens_minus_one == 0)
        return 0;

    /* The BAT_T count is zero-based so decrement num_tokens_minus_one */
    send.adv_send.num_tokens_minus_one--;

    ret = dlb_adv_send_wrapper(hdl, 1, &send, BAT_T);
    if (ret == 1)
        ret = send.adv_send.num_tokens_minus_one + 1;

cleanup:
    return ret;
}

static inline void __attribute__((always_inline)) __dlb_send_rsvd_token_int_arm(
    dlb_port_hdl_internal_t *hdl,
    dlb_shared_port_t *port)
{
    dlb_enqueue_qe_t *enqueue_qe = hdl->qe;

    enqueue_qe[0].cmd_byte = BAT_T;
    enqueue_qe[0].cmd_info.int_arm = true;
    enqueue_qe[0].num_tokens_minus_one = 0;

    /* Initialize the other commands to NOOP and zero int_arm and rsvd */
    enqueue_qe[1].cmd_byte = NOOP;
    enqueue_qe[1].misc_byte = 0;
    enqueue_qe[2].cmd_byte = NOOP;
    enqueue_qe[2].misc_byte = 0;
    enqueue_qe[3].cmd_byte = NOOP;
    enqueue_qe[3].misc_byte = 0;

    hdl->enqueue_four(enqueue_qe, hdl->pp_addr);

    /* Don't call dec_port_owed_tokens(), since this token is accounted for in
     * the reserved token deficit.
     */
    port->cq_rsvd_token_deficit = 1;
}

static inline void __attribute__((always_inline)) __dlb_send_int_arm(
    dlb_port_hdl_internal_t *hdl)
{
    dlb_enqueue_qe_t *enqueue_qe = hdl->qe;

    memset(enqueue_qe, 0, sizeof(dlb_enqueue_qe_t)*4);

    enqueue_qe[0].cmd_byte = DLB2_CMD_ARM;
    /* Initialize the other commands to NOOP */
    enqueue_qe[1].cmd_byte = NOOP;
    enqueue_qe[1].misc_byte = 0;
    enqueue_qe[2].cmd_byte = NOOP;
    enqueue_qe[2].misc_byte = 0;
    enqueue_qe[3].cmd_byte = NOOP;
    enqueue_qe[3].misc_byte = 0;

    hdl->enqueue_four(enqueue_qe, hdl->pp_addr);
}

static inline void __dlb_issue_int_arm_hcw(
    dlb_port_hdl_internal_t *hdl,
    dlb_shared_port_t *port)
{
    if (port->use_rsvd_token_scheme)
        __dlb_send_rsvd_token_int_arm(hdl, port);
    else
        __dlb_send_int_arm(hdl);

    port->int_armed = true;
}

static int dlb_block_on_cq_interrupt(
    dlb_port_hdl_internal_t *hdl,
    dlb_shared_port_t *port)
{
    int ret;

    /* If the interrupt is not armed, either sleep-poll (see comment below) or
     * arm the interrupt.
     */
    if (!port->int_armed) {
        /* The reserved token scheme requires setting the interrupt depth
         * threshold equal to the number of reserved tokens. Until the port
         * receives its set of reserved tokens it cannot block on the
         * interrupt, since it will not fire until the CQ contains at least
         * num-reserved-tokens QEs. In lieu of that, we sleep-poll the CQ,
         * which gives a similar behavior (other threads can run) albeit with a
         * potentially slower response time.
         */
        if (port->use_rsvd_token_scheme && port->cq_rsvd_token_deficit) {
            while (PORT_CQ_IS_EMPTY(hdl, port) && port->enabled)
                sched_yield();

            return 0;
        }

        __dlb_issue_int_arm_hcw(hdl, port);
    }

    ret = dlb2_ioctl_block_on_cq_interrupt(hdl->domain_hdl->fd,
                                           port->id,
                                           port->type == LDB,
                                           &hdl->cq_base[port->cq_idx],
                                           port->cq_gen,
                                           false);

    /* If the CQ block ioctl was unsuccessful, the interrupt is still armed */
    port->int_armed = (ret != 0);

    return ret;
}

/* Perform umwait in a loop until it succeeds, the timeout expires, or the port
 * is disabled.
 */
static int dlb_block_on_umwait(
    dlb_port_hdl_internal_t *hdl,
    dlb_shared_port_t *port)
{
    uint64_t tmo_ns, diff, start;
    struct timespec tv;

    /* If clock_gettime() fails, timeout immediately */
    if (clock_gettime(CLOCK_MONOTONIC, &tv))
        return 0;
    start = tv.tv_sec * NS_PER_S + tv.tv_nsec;

    tmo_ns = hdl->wait_profile.timeout_value_ns;

    dlb_umonitor(&hdl->cq_base[port->cq_idx]);

    diff = 0;

    while (PORT_CQ_IS_EMPTY(hdl, port) && port->enabled && diff < tmo_ns) {

        dlb_umwait(DLB_UMWAIT_CTRL_STATE_CO1, hdl->umwait_ticks);

        if (clock_gettime(CLOCK_MONOTONIC, &tv))
            return 0;

        diff = (tv.tv_sec * NS_PER_S + tv.tv_nsec) - start;
    }

    return 0;
}

/* Repeatedly poll the CQ until either it's non-empty, the timeout expires,
 * or the port is disabled.
 */
static int dlb_hard_poll_cq(
    dlb_port_hdl_internal_t *hdl,
    dlb_shared_port_t *port)
{
    uint64_t tmo_ns, diff, start, poll_interval;
    struct timespec tv;

    /* If clock_gettime() fails, timeout immediately */
    if (clock_gettime(CLOCK_MONOTONIC, &tv))
        return 0;
    start = tv.tv_sec * NS_PER_S + tv.tv_nsec;

    tmo_ns = hdl->wait_profile.timeout_value_ns;

    poll_interval = (tmo_ns < POLL_INTERVAL_NS)?
                     tmo_ns : POLL_INTERVAL_NS;

    diff = 0;

    while (PORT_CQ_IS_EMPTY(hdl, port) && port->enabled && diff < tmo_ns) {
        if (clock_gettime(CLOCK_MONOTONIC, &tv))
            break;

        delay_ns_block(start, poll_interval);

        diff = (tv.tv_sec * NS_PER_S + tv.tv_nsec) - start;
    }

    return 0;
}

/* Repeatedly poll the CQ until either it's non-empty, the timeout expires,
 * or the port is disabled, sleeping between polls.
 */
static int dlb_sleep_poll_cq(
    dlb_port_hdl_internal_t *hdl,
    dlb_shared_port_t *port)
{
    struct timespec tv, sleep_tv;
    uint64_t tmo_ns, diff, start;

    /* If clock_gettime() fails, timeout immediately */
    if (clock_gettime(CLOCK_MONOTONIC, &tv))
        return 0;
    start = tv.tv_sec * NS_PER_S + tv.tv_nsec;

    tmo_ns = hdl->wait_profile.timeout_value_ns;

    sleep_tv.tv_sec = hdl->wait_profile.sleep_duration_ns / NS_PER_S;
    sleep_tv.tv_nsec = hdl->wait_profile.sleep_duration_ns % NS_PER_S;

    diff = 0;

    while (PORT_CQ_IS_EMPTY(hdl, port) && port->enabled && diff < tmo_ns) {
        nanosleep(&sleep_tv, NULL);

        if (clock_gettime(CLOCK_MONOTONIC, &tv))
            break;
        diff = (tv.tv_sec * NS_PER_S + tv.tv_nsec) - start;
    }

    return 0;
}

int dlb_enable_cq_epoll(
	dlb_port_hdl_t hdl,
	bool is_ldb,
	int eventfd)
{
    dlb_port_hdl_internal_t *port_hdl = hdl;
    dlb_shared_port_t *shared_port;
    bool unlock = false;
    int pid, err;
    int ret = -1;

    VALIDATE_PORT_HANDLE(hdl);

    pid = getpid();
    port_hdl->event_fd = eventfd;
    shared_port = port_hdl->port.shared;

    err = pthread_mutex_lock(&shared_port->resource_mutex);
    ASSERT(err == 0, err);
    unlock = true;

    ret = dlb2_ioctl_enable_cq_epoll(port_hdl->domain_hdl->fd,
                                      shared_port->id,
				      is_ldb,
                                      pid,
				      eventfd);

    /* Enable CQ interrupt */
    __dlb_issue_int_arm_hcw(port_hdl, shared_port);

cleanup:

    if (unlock) {
        if (pthread_mutex_unlock(&shared_port->resource_mutex))
		printf("[%s()] Internal error: mutex unlock failed\n", __func__);
    }

    return ret;
}

static inline int __dlb_recv(
    dlb_port_hdl_t hdl,
    uint32_t max,
    bool wait,
    bool pop,
    dlb_recv_t *event)
{
    dlb_port_hdl_internal_t *port_hdl = hdl;
    int batch_size = DLB_SW_CREDIT_BATCH_SZ;
    dlb_shared_domain_t *domain;
    dlb_shared_port_t *port;
    uint64_t reset_ctr;
    bool is_dir_port;
    bool is_2_5;
    int i = -1;

    CHECK_PORT_HANDLE(hdl);

    CHECK(event != NULL, EINVAL);

    port = port_hdl->port.shared;
    domain = port_hdl->domain_hdl->domain.shared;

    CHECK(domain->started, EPERM);

    /* If the port is disabled and its CQ is empty, notify the application */
    ASSERT((port->enabled || !PORT_CQ_IS_EMPTY(port_hdl, port)), EACCES);

    is_2_5 = port_hdl->device_version.ver == VER_DLB2_5;
	dlb2_check_and_release_credits(port_hdl, BATCH_SZ_EXCEED, true);

    if (wait && PORT_CQ_IS_EMPTY(port_hdl, port)) {
        dlb2_check_and_release_credits(port_hdl, RETURN_ALL,
                                       LOW_POOL_CREDITS(port_hdl, batch_size, is_2_5));
        if (port_hdl->wait_func(port_hdl, port))
            return -1;
        /* Return if the port is disabled and its CQ is empty */
        ASSERT((port->enabled || !PORT_CQ_IS_EMPTY(port_hdl, port)), EACCES);
    }

    is_dir_port = port->type == DIR;

    for (i = 0; i < max; i++) {
        struct dlb_dequeue_qe *qe;
        int idx = port->cq_idx;
        int level;

        /* TODO: optimize cq_base and other port-> structures */
        if (PORT_CQ_IS_EMPTY(port_hdl, port))
            break;

        qe = (struct dlb_dequeue_qe *)&port_hdl->cq_base[idx];

        /* Copy the 16B QE into the user's event structure */
        memcpy(&event[i], (void *)qe, BYTES_PER_QE);

        level = qe->qid_depth;

        if (is_dir_port)
            port->queue_levels[port->id].count[level]++;
        else
            port->queue_levels[qe->qid].count[level]++;

        port->cq_idx += port->qe_stride;

        if (unlikely(port->cq_idx == port->cq_limit)) {
            port->cq_gen ^= 1;
            port->cq_idx = 0;
        }
    }

    /* If epoll mode is enabled, when CQ is empty, reset eventfd counter and enable interrupts */
    if (port_hdl->event_fd && PORT_CQ_IS_EMPTY(port_hdl, port)) {
        dlb2_check_and_release_credits(port_hdl, RETURN_ALL,
                                       LOW_POOL_CREDITS(port_hdl, batch_size, is_2_5));
	    if (read(port_hdl->event_fd, &reset_ctr, sizeof(reset_ctr)) < 0)
                printf("[%s()] Error: epoll read\n", __func__);
            else
	        __dlb_issue_int_arm_hcw(port_hdl, port);
    }

    inc_port_owed_tokens(port, i);
    inc_port_owed_releases(port, i);

    dlb2_inc_port_credits(port, i, is_2_5);

    dlb2_check_and_release_credits(port_hdl, ZERO_DEQ, !i);

    if (pop && i > 0)
        dlb_pop_cq(hdl, i);

cleanup:
    return i;
}

int dlb_recv(
    dlb_port_hdl_t hdl,
    uint32_t max,
    bool wait,
    dlb_event_t *event)
{
    /* __dlb_recv() performs the event NULL pointer check (since event is a
     * union, &event->recv == event).
     */
    return __dlb_recv(hdl, max, wait, true, &event->recv);
}

int dlb_recv_no_pop(
    dlb_port_hdl_t hdl,
    uint32_t max,
    bool wait,
    dlb_event_t *event)
{
    /* __dlb_recv() performs the event NULL pointer check (since event is a
     * union, &event->recv == event).
     */
    return __dlb_recv(hdl, max, wait, false, &event->recv);
}

/************************************/
/* Advanced Configuration Functions */
/************************************/
static int
dlb_query_cq_poll_mode(
    dlb_hdl_t hdl,
    enum dlb2_cq_poll_modes *mode)
{
    dlb_t *dlb = (dlb_t *) hdl;
    int ret = -1;

    VALIDATE_DLB_HANDLE(hdl);

    ret = dlb2_ioctl_query_cq_poll_mode(dlb->fd, (void *)mode);

cleanup:
    return ret;
}

static int dlb_create_ldb_port_adv(
    dlb_domain_hdl_t hdl,
    dlb_create_port_t *args,
    uint16_t rsvd_tokens)
{
    dlb_domain_hdl_internal_t *domain_hdl = hdl;
    dlb_shared_domain_t *shared_domain;
    dlb_shared_port_t *port = NULL;
    enum dlb2_cq_poll_modes mode;
    pthread_mutexattr_t attr;
    bool attr_init = false;
    int ret = -1, err, id;

    VALIDATE_DOMAIN_HANDLE(hdl);

    shared_domain = domain_hdl->domain.shared;

    err = dlb_query_cq_poll_mode(domain_hdl->dlb, &mode);
    ASSERT(err >= 0, errno);

    if (domain_hdl->dlb->device_version.ver == VER_DLB2) {
        dlb_sw_credit_pool_t *pool;
        uint32_t id;

        if (shared_domain->num_ldb_queues > 0) {
            id = args->ldb_credit_pool_id;
            ASSERT(id <= MAX_NUM_LDB_CREDIT_POOLS, EINVAL);
            pool = &shared_domain->sw_credits.ldb_pools[id];
            ASSERT(pool->configured, EINVAL);
        }

        if (shared_domain->num_dir_queues > 0) {
            id = args->dir_credit_pool_id;
            ASSERT(id <= MAX_NUM_DIR_CREDIT_POOLS, EINVAL);
            pool = &shared_domain->sw_credits.dir_pools[id];
            ASSERT(pool->configured, EINVAL);
        }
    } else if (domain_hdl->dlb->device_version.ver == VER_DLB2_5) {
        dlb_sw_credit_pool_t *pool;
        uint32_t id;

        id = args->credit_pool_id;
        ASSERT(id <= MAX_NUM_LDB_CREDIT_POOLS, EINVAL);
        pool = &shared_domain->sw_credits.ldb_pools[id];
        ASSERT(pool->configured, EINVAL);
    }

    id = dlb2_ioctl_create_ldb_port(domain_hdl->fd,
                                    args,
                                    rsvd_tokens);

    ASSERT(id >= 0, errno);

    port = &shared_domain->ldb_ports[id];

    port->id = id;
    port->type = LDB;

    err = pthread_mutexattr_init(&attr);
    ASSERT(err == 0, err);

    attr_init = true;

    err = pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
    ASSERT(err == 0, err);

    err = pthread_mutex_init(&port->resource_mutex, &attr);
    ASSERT(err == 0, err);

    port->ldb_pool_id = args->ldb_credit_pool_id;
    port->dir_pool_id = args->dir_credit_pool_id;

    memset(port->queue_levels, 0, sizeof(port->queue_levels));

    /* CQ depths less than 8 use an 8-entry queue but withhold credits */
    port->cq_depth = args->cq_depth <= 8 ? 8 : args->cq_depth;
    port->cq_limit = port->cq_depth;
    port->qe_stride = 1;
    port->cq_idx = 0;
    port->cq_gen = 1;

    /* In sparse CQ mode, DLB writes one QE per cache line. */
    if (mode == DLB2_CQ_POLL_MODE_STD)
        port->qe_stride = 1;
    else
        port->qe_stride = 4;

    port->cq_limit = port->cq_depth * port->qe_stride;

    port->cq_rsvd_token_deficit = rsvd_tokens;
    port->use_rsvd_token_scheme = shared_domain->use_rsvd_token_scheme;
    port->int_armed = false;

    port->enabled = true;
    port->configured = true;

    ret = port->id;

cleanup:
    if (attr_init)
        pthread_mutexattr_destroy(&attr);

    return ret;
}

static int dlb_create_dir_port_adv(
    dlb_domain_hdl_t hdl,
    dlb_create_port_t *args,
    int queue_id,
    uint16_t rsvd_tokens)
{
    dlb_domain_hdl_internal_t *domain_hdl = hdl;
    dlb_shared_domain_t *shared_domain;
    dlb_shared_port_t *port = NULL;
    enum dlb2_cq_poll_modes mode;
    pthread_mutexattr_t attr;
    bool attr_init = false;
    int ret = -1, err, id;

    shared_domain = domain_hdl->domain.shared;

    err = dlb_query_cq_poll_mode(domain_hdl->dlb, &mode);
    ASSERT(err >= 0, errno);

    if (domain_hdl->dlb->device_version.ver == VER_DLB2) {
        dlb_sw_credit_pool_t *pool;
        uint32_t id;

        if (shared_domain->num_ldb_queues > 0) {
            id = args->ldb_credit_pool_id;
            ASSERT(id <= MAX_NUM_LDB_CREDIT_POOLS, EINVAL);
            pool = &shared_domain->sw_credits.ldb_pools[id];
            ASSERT(pool->configured, EINVAL);
        }

        id = args->dir_credit_pool_id;
        ASSERT(id <= MAX_NUM_DIR_CREDIT_POOLS, EINVAL);
        pool = &shared_domain->sw_credits.dir_pools[id];
        ASSERT(pool->configured, EINVAL);
    } else if (domain_hdl->dlb->device_version.ver == VER_DLB2_5) {
        dlb_sw_credit_pool_t *pool;
        uint32_t id;

        id = args->credit_pool_id;
        ASSERT(id <= MAX_NUM_LDB_CREDIT_POOLS, EINVAL);
        pool = &shared_domain->sw_credits.ldb_pools[id];
        ASSERT(pool->configured, EINVAL);
    }

    id = dlb2_ioctl_create_dir_port(domain_hdl->fd,
                                    args,
                                    queue_id,
                                    rsvd_tokens);
    ASSERT(id >= 0, errno);

    port = &shared_domain->dir_ports[id];

    port->id = id;
    port->type = DIR;

    err = pthread_mutexattr_init(&attr);
    ASSERT(err == 0, err);

    attr_init = true;

    err = pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
    ASSERT(err == 0, err);

    err = pthread_mutex_init(&port->resource_mutex, &attr);
    ASSERT(err == 0, err);

    port->ldb_pool_id = args->ldb_credit_pool_id;
    port->dir_pool_id = args->dir_credit_pool_id;

    memset(port->queue_levels, 0, sizeof(port->queue_levels));

    /* CQ depths less than 8 use an 8-entry queue but withhold credits */
    port->cq_depth = args->cq_depth <= 8 ? 8 : args->cq_depth;
    port->cq_idx = 0;
    port->cq_gen = 1;

    /* In sparse CQ mode, DLB writes one QE per cache line. */
    if (mode == DLB2_CQ_POLL_MODE_STD)
        port->qe_stride = 1;
    else
        port->qe_stride = 4;

    port->cq_limit = port->cq_depth * port->qe_stride;

    port->cq_rsvd_token_deficit = rsvd_tokens;
    port->use_rsvd_token_scheme = shared_domain->use_rsvd_token_scheme;
    port->int_armed = false;

    port->enabled = true;
    port->configured = true;

    ret = port->id;

cleanup:

    if (attr_init)
        pthread_mutexattr_destroy(&attr);

    return ret;
}

/*******************************/
/* Advanced Datapath Functions */
/*******************************/
int dlb_adv_send(
    dlb_port_hdl_t hdl,
    uint32_t num,
    dlb_event_t *evts)
{
    uint16_t num_tokens, num_releases;
    dlb_port_t *port;
    int ret = -1;
    int i;

    CHECK_PORT_HANDLE(hdl);

    port = &((dlb_port_hdl_internal_t *)hdl)->port;

    CHECK(evts != NULL, EINVAL);

    /* Check whether the user is attempting to release more events or pop more
     * tokens than the port owes.
     */
    num_tokens = 0;
    num_releases = 0;
    for (i = 0; i < num; i++) {
        enum dlb_event_cmd_t cmd = evts[i].adv_send.cmd;

        /* All token return commands set bit 0. BAT_T is a special case. */
        num_tokens += (cmd & 0x1);
        num_tokens += evts[i].adv_send.num_tokens_minus_one * (cmd == BAT_T);

        num_releases += cmd_releases_hist_list_entry(cmd);
    }
    ASSERT(num_tokens <= port->shared->owed_tokens, EINVAL);
    ASSERT(num_releases <= port->shared->owed_releases, EINVAL);

    /* Setting credits_required_for_all_cmds to false means that some events
     * *may* need credits, so __dlb_adv_send() needs to check every event's
     * cmd.
     */
    ret = __dlb2_adv_send(hdl, num, evts, true, false);

cleanup:
    return ret;
}

static int64_t
__dlb_adv_read_ldb_qd_counter(
    dlb_shared_domain_t *domain,
    int queue_id,
    dlb_queue_depth_levels_t level)
{
    int64_t count = 0;
    int i;

    for (i = 0; i < MAX_NUM_LDB_PORTS; i++) {
        int64_t port_count, port_reset;

        port_count = domain->ldb_ports[i].queue_levels[queue_id].count[level];
        port_reset = domain->ldb_ports[i].queue_levels[queue_id].reset[level];

        count += port_count - port_reset;
    }

    return count;
}

static int64_t
__dlb_adv_read_dir_qd_counter(
    dlb_shared_domain_t *domain,
    int queue_id,
    dlb_queue_depth_levels_t level)
{
    int64_t count = 0;
    int i;

    for (i = 0; i < MAX_NUM_DIR_PORTS; i++) {
        int64_t port_count, port_reset;

        port_count = domain->dir_ports[i].queue_levels[queue_id].count[level];
        port_reset = domain->dir_ports[i].queue_levels[queue_id].reset[level];

        count += port_count - port_reset;
    }

    return count;
}

int64_t
dlb_adv_read_queue_depth_counter(
    dlb_domain_hdl_t hdl,
    int queue_id,
    bool is_dir,
    dlb_queue_depth_levels_t level)
{
    int max_queues = is_dir ? MAX_NUM_DIR_QUEUES : MAX_NUM_LDB_QUEUES;
    dlb_domain_hdl_internal_t *domain_hdl = hdl;
    dlb_shared_domain_t *domain;
    int64_t ret = -1;

    VALIDATE_DOMAIN_HANDLE(hdl);
    ASSERT(queue_id < max_queues, EINVAL);

    domain = domain_hdl->domain.shared;

    ASSERT(domain_hdl->domain.device_version.ver == VER_DLB2 ||
           domain_hdl->domain.device_version.ver == VER_DLB2_5, EINVAL);
    ASSERT(domain->queue_type[is_dir][queue_id] != QUEUE_TYPE_INVALID, EINVAL);

    if (is_dir)
        ret = __dlb_adv_read_dir_qd_counter(domain, queue_id, level);
    else
        ret = __dlb_adv_read_ldb_qd_counter(domain, queue_id, level);

cleanup:
    return ret;
}

static void
__dlb_adv_reset_ldb_qd_counter(
    dlb_shared_domain_t *domain,
    int queue_id,
    dlb_queue_depth_levels_t level)
{
    int i;

    for (i = 0; i < MAX_NUM_LDB_PORTS; i++)
        domain->ldb_ports[i].queue_levels[queue_id].reset[level] =
            domain->ldb_ports[i].queue_levels[queue_id].count[level];
}

static void
__dlb_adv_reset_dir_qd_counter(
    dlb_shared_domain_t *domain,
    int queue_id,
    dlb_queue_depth_levels_t level)
{
    int i;

    for (i = 0; i < MAX_NUM_DIR_PORTS; i++)
        domain->dir_ports[i].queue_levels[queue_id].reset[level] =
            domain->dir_ports[i].queue_levels[queue_id].count[level];
}

int
dlb_adv_reset_queue_depth_counter(
    dlb_domain_hdl_t hdl,
    int queue_id,
    bool is_dir,
    dlb_queue_depth_levels_t level)
{
    int max_queues = is_dir ? MAX_NUM_DIR_QUEUES : MAX_NUM_LDB_QUEUES;
    dlb_domain_hdl_internal_t *domain_hdl = hdl;
    dlb_shared_domain_t *domain;
    int64_t ret = -1;

    VALIDATE_DOMAIN_HANDLE(hdl);
    ASSERT(queue_id < max_queues, EINVAL);

    domain = domain_hdl->domain.shared;

    ASSERT(domain_hdl->domain.device_version.ver == VER_DLB2 ||
           domain_hdl->domain.device_version.ver == VER_DLB2_5, EINVAL);
    ASSERT(domain->queue_type[is_dir][queue_id] != QUEUE_TYPE_INVALID, EINVAL);

    if (is_dir)
        __dlb_adv_reset_dir_qd_counter(domain, queue_id, level);
    else
        __dlb_adv_reset_ldb_qd_counter(domain, queue_id, level);

    ret = 0;

cleanup:
    return ret;
}
