/* SPDX-License-Identifier: MIT */
/*
 * Simple ping/pong client which can use the io_uring NAPI support.
 *
 * Needs to be run as root because it sets SCHED_FIFO scheduling class,
 * but will work without that.
 *
 * Example:
 *
 * sudo examples/napi-busy-poll-client -a 192.168.2.2 -n100000 -p4444 \
 *	-b -t10 -u
 *
 * send and receive 100k packets, using NAPI.
 */
#include <ctype.h>
#include <errno.h>
#include <float.h>
#include <getopt.h>
#include <liburing.h>
#include <math.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>

#define MAXBUFLEN 100
#define PORTNOLEN 10
#define ADDRLEN   80
#define RINGSIZE  1024

#define printable(ch) (isprint((unsigned char)ch) ? ch : '#')

enum {
	IOURING_RECV,
	IOURING_SEND,
	IOURING_RECVMSG,
	IOURING_SENDMSG
};

struct ctx
{
	struct io_uring ring;
	union {
		struct sockaddr_in6 saddr6;
		struct sockaddr_in saddr;
	};

	int sockfd;
	int buffer_len;
	int num_pings;
	bool napi_check;

	union {
		char buffer[MAXBUFLEN];
		struct timespec ts;
	};

	int rtt_index;
	double *rtt;
};

struct options
{
	int  num_pings;
	__u32  timeout;

	bool sq_poll;
	bool defer_tw;
	bool busy_loop;
	bool prefer_busy_poll;
	bool ipv6;

	char port[PORTNOLEN];
	char addr[ADDRLEN];
};

static struct option longopts[] =
{
	{"address"  , 1, NULL, 'a'},
	{"busy"     , 0, NULL, 'b'},
	{"help"     , 0, NULL, 'h'},
	{"num_pings", 1, NULL, 'n'},
	{"port"     , 1, NULL, 'p'},
	{"prefer"   , 1, NULL, 'u'},
	{"sqpoll"   , 0, NULL, 's'},
	{"timeout"  , 1, NULL, 't'},
	{NULL       , 0, NULL,  0 }
};

static void printUsage(const char *name)
{
	fprintf(stderr,
	"Usage: %s [-l|--listen] [-a|--address ip_address] [-p|--port port-no] [-s|--sqpoll]"
	" [-b|--busy] [-n|--num pings] [-t|--timeout busy-poll-timeout] [-u||--prefer] [-6] [-h|--help]\n"
	"--address\n"
	"-a        : remote or local ipv6 address\n"
	"--busy\n"
	"-b        : busy poll io_uring instead of blocking.\n"
	"--num_pings\n"
	"-n        : number of pings\n"
	"--port\n"
	"-p        : port\n"
	"--sqpoll\n"
	"-s        : Configure io_uring to use SQPOLL thread\n"
	"--timeout\n"
	"-t        : Configure NAPI busy poll timeout"
	"--prefer\n"
	"-u        : prefer NAPI busy poll\n"
	"-6        : use IPV6\n"
	"--help\n"
	"-h        : Display this usage message\n\n",
	name);
}

static void printError(const char *msg, int opt)
{
	if (msg && opt)
		fprintf(stderr, "%s (-%c)\n", msg, printable(opt));
}

static void setProcessScheduler(void)
{
	struct sched_param param;

	param.sched_priority = sched_get_priority_max(SCHED_FIFO);
	if (sched_setscheduler(0, SCHED_FIFO, &param) < 0)
		fprintf(stderr, "sched_setscheduler() failed: (%d) %s\n",
			errno, strerror(errno));
}

static double diffTimespec(const struct timespec *time1, const struct timespec *time0)
{
	return (time1->tv_sec - time0->tv_sec)
		+ (time1->tv_nsec - time0->tv_nsec) / 1000000000.0;
}

static uint64_t encodeUserData(char type, int fd)
{
	return (uint32_t)fd | ((uint64_t)type << 56);
}

static void decodeUserData(uint64_t data, char *type, int *fd)
{
	*type = data >> 56;
	*fd   = data & 0xffffffffU;
}

static const char *opTypeToStr(char type)
{
	const char *res;

	switch (type) {
	case IOURING_RECV:
		res = "IOURING_RECV";
		break;
	case IOURING_SEND:
		res = "IOURING_SEND";
		break;
	case IOURING_RECVMSG:
		res = "IOURING_RECVMSG";
		break;
	case IOURING_SENDMSG:
		res = "IOURING_SENDMSG";
		break;
	default:
		res = "Unknown";
	}

	return res;
}

static void reportNapi(struct ctx *ctx)
{
	unsigned int napi_id = 0;
	socklen_t len = sizeof(napi_id);

	getsockopt(ctx->sockfd, SOL_SOCKET, SO_INCOMING_NAPI_ID, &napi_id, &len);
	if (napi_id)
		printf(" napi id: %d\n", napi_id);
	else
		printf(" unassigned napi id\n");

	ctx->napi_check = true;
}

static void sendPing(struct ctx *ctx)
{
	struct io_uring_sqe *sqe = io_uring_get_sqe(&ctx->ring);

	clock_gettime(CLOCK_REALTIME, (struct timespec *)ctx->buffer);
	io_uring_prep_send(sqe, ctx->sockfd, ctx->buffer, sizeof(struct timespec), 0);
	sqe->user_data = encodeUserData(IOURING_SEND, ctx->sockfd);
}

static void receivePing(struct ctx *ctx)
{
	struct io_uring_sqe *sqe = io_uring_get_sqe(&ctx->ring);

	io_uring_prep_recv(sqe, ctx->sockfd, ctx->buffer, MAXBUFLEN, 0);
	sqe->user_data = encodeUserData(IOURING_RECV, ctx->sockfd);
}

static void recordRTT(struct ctx *ctx)
{
	struct timespec startTs = ctx->ts;

	// Send next ping.
	sendPing(ctx);

	// Store round-trip time.
	ctx->rtt[ctx->rtt_index] = diffTimespec(&ctx->ts, &startTs);
	ctx->rtt_index++;
}

static void printStats(struct ctx *ctx)
{
	double minRTT    = DBL_MAX;
	double maxRTT    = 0.0;
	double avgRTT    = 0.0;
	double stddevRTT = 0.0;

	// Calculate min, max, avg.
	for (int i = 0; i < ctx->rtt_index; i++) {
		if (ctx->rtt[i] < minRTT)
			minRTT = ctx->rtt[i];
		if (ctx->rtt[i] > maxRTT)
			maxRTT = ctx->rtt[i];

		avgRTT += ctx->rtt[i];
	}
	avgRTT /= ctx->rtt_index;

	// Calculate stddev.
	for (int i = 0; i < ctx->rtt_index; i++)
		stddevRTT += fabs(ctx->rtt[i] - avgRTT);
	stddevRTT /= ctx->rtt_index;

	fprintf(stdout, " rtt(us) min/avg/max/mdev = %.3f/%.3f/%.3f/%.3f\n",
		minRTT * 1000000, avgRTT * 1000000, maxRTT * 1000000, stddevRTT * 1000000);
}

static int completion(struct ctx *ctx, struct io_uring_cqe *cqe)
{
	char type;
	int  fd;
	int  res = cqe->res;

	decodeUserData(cqe->user_data, &type, &fd);
	if (res < 0) {
		fprintf(stderr, "unexpected %s failure: (%d) %s\n",
			opTypeToStr(type), -res, strerror(-res));
		return -1;
	}

	switch (type) {
	case IOURING_SEND:
		receivePing(ctx);
		break;
	case IOURING_RECV:
		if (res != sizeof(struct timespec)) {
			fprintf(stderr, "unexpected ping reply len: %d\n", res);
			abort();
		}

		if (!ctx->napi_check) {
			reportNapi(ctx);
			sendPing(ctx);
		} else {
			recordRTT(ctx);
		}

		--ctx->num_pings;
		break;

	default:
		fprintf(stderr, "unexpected %s completion\n",
			opTypeToStr(type));
		return -1;
		break;
	}

	return 0;
}

int main(int argc, char *argv[])
{
	struct ctx       ctx;
	struct options   opt;
	struct __kernel_timespec *tsPtr;
	struct __kernel_timespec ts;
	struct io_uring_params params;
	struct io_uring_napi napi;
	int flag, ret, af;

	memset(&opt, 0, sizeof(struct options));

	// Process flags.
	while ((flag = getopt_long(argc, argv, ":hs:bua:n:p:t:6d:", longopts, NULL)) != -1) {
		switch (flag) {
		case 'a':
			strcpy(opt.addr, optarg);
			break;
		case 'b':
			opt.busy_loop = true;
			break;
		case 'h':
			printUsage(argv[0]);
			exit(0);
			break;
		case 'n':
			opt.num_pings = atoi(optarg) + 1;
			break;
		case 'p':
			strcpy(opt.port, optarg);
			break;
		case 's':
			opt.sq_poll = !!atoi(optarg);
			break;
		case 't':
			opt.timeout = atoi(optarg);
			break;
		case 'u':
			opt.prefer_busy_poll = true;
			break;
		case '6':
			opt.ipv6 = true;
			break;
		case 'd':
			opt.defer_tw = !!atoi(optarg);
			break;
		case ':':
			printError("Missing argument", optopt);
			printUsage(argv[0]);
			exit(-1);
			break;
		case '?':
			printError("Unrecognized option", optopt);
			printUsage(argv[0]);
			exit(-1);
			break;

		default:
			fprintf(stderr, "Fatal: Unexpected case in CmdLineProcessor switch()\n");
			exit(-1);
			break;
		}
	}

	if (strlen(opt.addr) == 0) {
		fprintf(stderr, "address option is mandatory\n");
		printUsage(argv[0]);
		exit(1);
	}

	if (opt.ipv6) {
		af = AF_INET6;
		ctx.saddr6.sin6_port   = htons(atoi(opt.port));
		ctx.saddr6.sin6_family = AF_INET6;
	} else {
		af = AF_INET;
		ctx.saddr.sin_port   = htons(atoi(opt.port));
		ctx.saddr.sin_family = AF_INET;
	}

	if (opt.ipv6)
		ret = inet_pton(af, opt.addr, &ctx.saddr6.sin6_addr);
	else
		ret = inet_pton(af, opt.addr, &ctx.saddr.sin_addr);
	if (ret <= 0) {
		fprintf(stderr, "inet_pton error for %s\n", optarg);
		printUsage(argv[0]);
		exit(1);
	}

	// Connect to server.
	fprintf(stdout, "Connecting to %s... (port=%s) to send %d pings\n", opt.addr, opt.port, opt.num_pings - 1);

	if ((ctx.sockfd = socket(af, SOCK_DGRAM, 0)) < 0) {
		fprintf(stderr, "socket() failed: (%d) %s\n", errno, strerror(errno));
		exit(1);
	}

	if (opt.ipv6)
		ret = connect(ctx.sockfd, (struct sockaddr *)&ctx.saddr6, sizeof(struct sockaddr_in6));
	else
		ret = connect(ctx.sockfd, (struct sockaddr *)&ctx.saddr, sizeof(struct sockaddr_in));
	if (ret < 0) {
		fprintf(stderr, "connect() failed: (%d) %s\n", errno, strerror(errno));
		exit(1);
	}

	// Setup ring.
	memset(&params, 0, sizeof(params));
	memset(&ts, 0, sizeof(ts));
	memset(&napi, 0, sizeof(napi));

	params.flags = IORING_SETUP_SINGLE_ISSUER;
	if (opt.defer_tw) {
		params.flags |= IORING_SETUP_DEFER_TASKRUN;
	} else if (opt.sq_poll) {
		params.flags = IORING_SETUP_SQPOLL;
		params.sq_thread_idle = 50;
	} else {
		params.flags |= IORING_SETUP_COOP_TASKRUN;
	}

	ret = io_uring_queue_init_params(RINGSIZE, &ctx.ring, &params);
	if (ret) {
		fprintf(stderr, "io_uring_queue_init_params() failed: (%d) %s\n",
			ret, strerror(-ret));
		exit(1);
	}

	if (opt.timeout || opt.prefer_busy_poll) {
		napi.prefer_busy_poll = opt.prefer_busy_poll;
		napi.busy_poll_to = opt.timeout;

		ret = io_uring_register_napi(&ctx.ring, &napi);
		if (ret) {
			fprintf(stderr, "io_uring_register_napi: %d\n", ret);
			exit(1);
		}
	}

	if (opt.busy_loop)
		tsPtr = &ts;
	else
		tsPtr = NULL;

	// Use realtime scheduler.
	setProcessScheduler();

	// Copy payload.
	clock_gettime(CLOCK_REALTIME, &ctx.ts);

	// Setup context.
	ctx.napi_check = false;
	ctx.buffer_len = sizeof(struct timespec);
	ctx.num_pings  = opt.num_pings;

	ctx.rtt_index = 0;
	ctx.rtt = (double *)malloc(sizeof(double) * opt.num_pings);
	if (!ctx.rtt) {
		fprintf(stderr, "Cannot allocate results array\n");
		exit(1);
	}

	// Send initial message to get napi id.
	sendPing(&ctx);

	while (ctx.num_pings != 0) {
		int res;
		unsigned num_completed = 0;
		unsigned head;
		struct io_uring_cqe *cqe;

		do {
			res = io_uring_submit_and_wait_timeout(&ctx.ring, &cqe, 1, tsPtr, NULL);
			if (res >= 0)
				break;
			else if (res == -ETIME)
				continue;
			fprintf(stderr, "submit_and_wait: %d\n", res);
			exit(1);
		} while (1);

		io_uring_for_each_cqe(&ctx.ring, head, cqe) {
			++num_completed;
			if (completion(&ctx, cqe))
				goto out;
		}

		if (num_completed)
			io_uring_cq_advance(&ctx.ring, num_completed);
	}

	printStats(&ctx);

out:
	// Clean up.
	if (opt.timeout || opt.prefer_busy_poll) {
		ret = io_uring_unregister_napi(&ctx.ring, &napi);
		if (ret)
			fprintf(stderr, "io_uring_unregister_napi: %d\n", ret);
		if (opt.timeout          != napi.busy_poll_to ||
		    opt.prefer_busy_poll != napi.prefer_busy_poll) {
			fprintf(stderr, "Expected busy poll to = %d, got %d\n",
				opt.timeout, napi.busy_poll_to);
			fprintf(stderr, "Expected prefer busy poll = %d, got %d\n",
				opt.prefer_busy_poll, napi.prefer_busy_poll);
		}
	} else {
		ret = io_uring_unregister_napi(&ctx.ring, NULL);
		if (ret)
			fprintf(stderr, "io_uring_unregister_napi: %d\n", ret);
	}

	io_uring_queue_exit(&ctx.ring);
	free(ctx.rtt);
	close(ctx.sockfd);
	return 0;
}
