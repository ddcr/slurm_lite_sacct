#include <stdio.h>
#include <unistd.h>
#include <sys/time.h>
#include "src/common/log.h"
#include "src/common/slurm_time.h"

#include "src/common/parse_time.h"

/*
 * slurm_diff_tv_str - build a string showing the time difference between two
 *		       times
 * IN tv1 - start of event
 * IN tv2 - end of event
 * OUT tv_str - place to put delta time in format "usec=%ld"
 * IN len_tv_str - size of tv_str in bytes
 * IN from - where the function was called form
 */
extern void slurm_diff_tv_str_ddcr(struct timeval *tv1, struct timeval *tv2,
			      char *tv_str, int len_tv_str, const char *from,
			      long limit, long *delta_t)
{
	char p[64] = "";
	struct tm tm;
	int debug_limit = limit;

	(*delta_t)  = (tv2->tv_sec - tv1->tv_sec) * 1000000;
	(*delta_t) += tv2->tv_usec;
	(*delta_t) -= tv1->tv_usec;

	snprintf(tv_str, len_tv_str, "usec=%ld", *delta_t); 
	if (from) {
		if (!limit) {
			/* NOTE: The slurmctld scheduler's default run time
			 * limit is 4 seconds, but that would not typically
			 * be reached. See "max_sched_time=" logic in
			 * src/slurmctld/job_scheduler.c */
			limit = 3000000;
			debug_limit = 1000000;
		}
		if ((*delta_t > debug_limit) || (*delta_t > limit)) {
			if (!slurm_localtime_r(&tv1->tv_sec, &tm))
				error("localtime_r(): %m");
			if (strftime(p, sizeof(p), "%T", &tm) == 0)
				error("strftime(): %m");
			if (*delta_t > limit) {
				verbose("Warning: Note very large processing "
					"time from %s: %s began=%s.%3.3d",
					from, tv_str, p,
					(int)(tv1->tv_usec / 1000));
			} else {	/* Log anything over 1 second here */
				debug("Note large processing time from %s: "
				      "%s began=%s.%3.3d",
				      from, tv_str, p,
				      (int)(tv1->tv_usec / 1000));
			}
		}
	}
}

/* block_daemon()
 *
 * This function allows to block any daemon
 * in a specific function. Once the daemon
 * is block gdb can be attached and by resetting
 * the block variable restored to normal operation.
 */
extern void
block_daemon(void)
{
	int block;

	block = 1;
	while (block == 1) {
		info("%s: attachme, attachme...", __func__);
		sleep(2);
	}
}
