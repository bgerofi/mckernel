/* gettimeofday.c COPYRIGHT FUJITSU LIMITED 2016 */

#include <affinity.h>
#include <arch-memory.h>
#include <time.h>
#include <syscall.h>
#include <registers.h>
#include <ihk/atomic.h>

extern int __kernel_gettimeofday(struct timeval *tv, void *tz);

static inline void cpu_pause_for_vsyscall(void)
{
	asm volatile ("yield" ::: "memory");
	return;
}

static inline void calculate_time_from_tsc(struct timespec *ts,
					   struct tod_data_s *tod_data)
{
	long ver;
	unsigned long current_tsc;
	__time_t sec_delta;
	long ns_delta;

	for (;;) {
		while ((ver = ihk_atomic64_read(&tod_data->version)) & 1) {
			/* settimeofday() is in progress */
			cpu_pause_for_vsyscall();
		}
		rmb();
		*ts = tod_data->origin;
		rmb();
		if (ver == ihk_atomic64_read(&tod_data->version)) {
			break;
		}

		/* settimeofday() has intervened */
		cpu_pause_for_vsyscall();
	}

	current_tsc = rdtsc();
	sec_delta = current_tsc / tod_data->clocks_per_sec;
	ns_delta = NS_PER_SEC * (current_tsc % tod_data->clocks_per_sec)
		/ tod_data->clocks_per_sec;
	/* calc. of ns_delta overflows if clocks_per_sec exceeds 18.44 GHz */

	ts->tv_sec += sec_delta;
	ts->tv_nsec += ns_delta;
	if (ts->tv_nsec >= NS_PER_SEC) {
		ts->tv_nsec -= NS_PER_SEC;
		++ts->tv_sec;
	}

	return;
}

static inline struct tod_data_s *get_tod_data_addr(void)
{
	unsigned long addr;

	asm volatile("adr	%0, _tod_data\n"
		     : "=r" (addr)
		     :
		     : "memory");

	return (struct tod_data_s *)addr;
}

int __kernel_gettimeofday(struct timeval *tv, void *tz)
{
	long ret;
	struct tod_data_s *tod_data;
	struct timespec ats;

	if(!tv && !tz) {
		/* nothing to do */
		return 0;
	}

	tod_data = get_tod_data_addr();

	/* DO it locally if supported */
	if (!tz && tod_data->do_local) {
		calculate_time_from_tsc(&ats, tod_data);

		tv->tv_sec = ats.tv_sec;
		tv->tv_usec = ats.tv_nsec / 1000;

		return 0;
	}

	/* Otherwize syscall */
	asm volatile("mov	w8, %w1\n"
		     "mov	x0, %2\n"
		     "mov	x1, %3\n"
		     "svc	#0\n"
		     "mov	%0, x0\n"
		     : "=r" (ret)
		     : "r" (__NR_gettimeofday), "r"(tv), "r"(tz)
		     : "memory");

	if (ret) {
		*(int *)0 = 0;	/* i.e. raise(SIGSEGV) */
	}
	return (int)ret;
}


/*
 * The IDs of the various system clocks (for POSIX.1b interval timers):
 * @ref.impl include/uapi/linux/time.h
 */
// #define CLOCK_REALTIME		0
// #define CLOCK_MONOTONIC		1
// #define CLOCK_PROCESS_CPUTIME_ID	2
// #define CLOCK_THREAD_CPUTIME_ID	3
#define CLOCK_MONOTONIC_RAW		4
#define CLOCK_REALTIME_COARSE		5
#define CLOCK_MONOTONIC_COARSE		6
#define CLOCK_BOOTTIME			7
#define CLOCK_REALTIME_ALARM		8
#define CLOCK_BOOTTIME_ALARM		9
#define CLOCK_SGI_CYCLE			10	/* Hardware specific */
#define CLOCK_TAI			11

#define HIGH_RES_NSEC		1 /* nsec. */
#define CLOCK_REALTIME_RES	HIGH_RES_NSEC

#define CLOCK_COARSE_RES	((NS_PER_SEC+CONFIG_HZ/2)/CONFIG_HZ) /* 10,000,000 nsec*/

typedef int	clockid_t;

int __kernel_clock_gettime(clockid_t clk_id, struct timespec *tp)
{
	long ret;
	struct tod_data_s *tod_data;
	struct timespec ats;

	if (!tp) {
		/* nothing to do */
		return 0;
	}

	tod_data = get_tod_data_addr();

	/* DO it locally if supported */
	if (tod_data->do_local && clk_id == CLOCK_REALTIME) {
		calculate_time_from_tsc(&ats, tod_data);

		tp->tv_sec = ats.tv_sec;
		tp->tv_nsec = ats.tv_nsec;

		return 0;
	}

	/* Otherwize syscall */
	asm volatile("mov	w8, %w1\n"
		     "mov	x0, %2\n"
		     "mov	x1, %3\n"
		     "svc	#0\n"
		     "mov	%0, x0\n"
		     : "=r" (ret)
		     : "r" (__NR_clock_gettime), "r"(clk_id), "r"(tp)
		     : "memory");

	return (int)ret;
}

int __kernel_clock_getres(clockid_t clk_id, struct timespec *res)
{
	long ret;

	if (!res) {
		/* nothing to do */
		return 0;
	}

	switch (clk_id) {
		case CLOCK_REALTIME:
		case CLOCK_MONOTONIC:
			res->tv_sec = 0;
			res->tv_nsec = CLOCK_REALTIME_RES;
			return 0;
			break;
		case CLOCK_REALTIME_COARSE:
		case CLOCK_MONOTONIC_COARSE:
			res->tv_sec = 0;
			res->tv_nsec = CLOCK_COARSE_RES;
			return 0;
			break;
		default:
			break;
	}

	/* Otherwise syscall */
	asm volatile("mov	w8, %w1\n"
		     "mov	x0, %2\n"
		     "mov	x1, %3\n"
		     "svc	#0\n"
		     "mov	%0, x0\n"
		     : "=r" (ret)
		     : "r" (__NR_clock_getres), "r"(clk_id), "r"(res)
		     : "memory");

	return (int)ret;
}
