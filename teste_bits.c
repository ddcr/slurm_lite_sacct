#include <stdlib.h>
#include <stdio.h>

enum job_states {
        JOB_PENDING,            /* queued waiting for initiation */
        JOB_RUNNING,            /* allocated resources and executing */
        JOB_SUSPENDED,          /* allocated resources, execution suspended */
        JOB_COMPLETE,           /* completed execution successfully */
        JOB_CANCELLED,          /* cancelled by user */
        JOB_FAILED,             /* completed execution unsuccessfully */
        JOB_TIMEOUT,            /* terminated on reaching time limit */
        JOB_NODE_FAIL,          /* terminated on node failure */
        JOB_END                 /* not a real state, last entry in table */
};
#define JOB_COMPLETING (0x8000)


int main(int argc, char const *argv[])
{
	enum job_states * state_id;
	int i;

    for (i = 0; i<JOB_END; i++) {
    	state_id = malloc( sizeof( enum job_states ) );
    	*state_id = ( enum job_states ) i;
	}
    state_id = malloc( sizeof( enum job_states ) );
    *state_id = ( enum job_states ) JOB_COMPLETING;

	// printf("JOB_PENDING & (~JOB_COMPLEING) = %d\n", 
	// 	JOB_PENDING & (~JOB_COMPLETING))

	return 0;
}