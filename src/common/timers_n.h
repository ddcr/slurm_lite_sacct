#ifndef _HAVE_TIMERS_N_H
#define _HAVE_TIMERS_N_H

#include <sys/time.h>

#define DEF_TIMERS_DDCR	struct timeval tv1, tv2; char tv_str[20] = ""; long delta_t;
#define START_TIMER_DDCR	gettimeofday(&tv1, NULL)
#define END_TIMER_DDCR	gettimeofday(&tv2, NULL); \
	slurm_diff_tv_str_ddcr(&tv1, &tv2, tv_str, 50, NULL, 0, &delta_t)
#define END_TIMER2_DDCR(from) gettimeofday(&tv2, NULL); \
	slurm_diff_tv_str_ddcr(&tv1, &tv2, tv_str, 50, from, 0, &delta_t)
#define END_TIMER3_DDCR(from, limit) gettimeofday(&tv2, NULL); \
	slurm_diff_tv_str_ddcr(&tv1, &tv2, tv_str, 50, from, limit, &delta_t)
#define DELTA_TIMER_DDCR	delta_t
#define TIME_STR_DDCR 	tv_str

/*
 * slurm_diff_tv_str - build a string showing the time difference between two
 *		       times
 * IN tv1 - start of event
 * IN tv2 - end of event
 * OUT tv_str - place to put delta time in format "usec=%ld"
 * IN len_tv_str - size of tv_str in bytes
 * IN from - Name to be printed on long diffs
 * IN limit - limit to wait
 * OUT delta_t - raw time difference in usec
 */
extern void slurm_diff_tv_str_ddcr(struct timeval *tv1,struct timeval *tv2,
			      char *tv_str, int len_tv_str, const char *from,
			      long limit, long *delta_t);

/* Block daemon indefinitely.
 */
extern void block_daemon(void);

#endif
