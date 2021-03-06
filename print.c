#include "sacct.h"
#include "src/common/parse_time.h"
#include "include/slurm/slurm.h"
#ifdef NEWQUERY
#include "src/common/slurm_protocol_api.h"
#endif
#include "compat_pwdgrp.h"

char *_elapsed_time(long secs, long usecs);

char *_elapsed_time(long secs, long usecs)
{
	long	days, hours, minutes, seconds;
	long    subsec = 0;
	char *str = NULL;

	if(secs < 0 || secs == NO_VAL)
		return NULL;

	while (usecs >= 1E6) {
		secs++;
		usecs -= 1E6;
	}
	if(usecs > 0) {
		/* give me 3 significant digits to tack onto the sec */
		subsec = (usecs/1000);
	}
	seconds =  secs % 60;
	minutes = (secs / 60)   % 60;
	hours   = (secs / 3600) % 24;
	days    =  secs / 86400;

	if (days)
		str = xstrdup_printf("%ld-%2.2ld:%2.2ld:%2.2ld",
				     days, hours, minutes, seconds);
	else if (hours)
		str = xstrdup_printf("%2.2ld:%2.2ld:%2.2ld",
				     hours, minutes, seconds);
	else if(subsec)
		str = xstrdup_printf("%2.2ld:%2.2ld.%3.3ld",
				     minutes, seconds, subsec);
	else
		str = xstrdup_printf("00:%2.2ld:%2.2ld",
				     minutes, seconds);
	return str;
}

static char *_find_qos_name_from_list(
	List qos_list, int qosid)
{
	ListIterator itr = NULL;
	acct_qos_rec_t *qos = NULL;

	if(!qos_list || qosid == NO_VAL)
		return NULL;

	itr = list_iterator_create(qos_list);
	while((qos = list_next(itr))) {
		if(qosid == qos->id)
			break;
	}
	list_iterator_destroy(itr);

	if(qos)
		return qos->name;
	else
		return "Unknown";
}



void print_fields(type_t type, void *object)
{
	jobacct_job_rec_t *job = (jobacct_job_rec_t *)object;
	jobacct_step_rec_t *step = (jobacct_step_rec_t *)object;
	jobcomp_job_rec_t *job_comp = (jobcomp_job_rec_t *)object;
	print_field_t *field = NULL;
	int curr_inx = 1;
	struct passwd *pw = NULL;
	struct	group *gr = NULL;
	char outbuf[FORMAT_STRING_SIZE];
#ifdef NEWQUERY
	uint32_t tmp_uint32 = NO_VAL;
	char tmp1[128];
#endif

	switch(type) {
	case JOB:
		step = NULL;
		if(!job->track_steps)
			step = (jobacct_step_rec_t *)job->first_step_ptr;
		/* set this to avoid printing out info for things that
		   don't mean anything.  Like an allocation that never
		   ran anything.
		*/
		if(!step)
			job->track_steps = 1;

		break;
	default:
		break;
	}

	list_iterator_reset(print_fields_itr);
	while((field = list_next(print_fields_itr))) {
		char *tmp_char = NULL;
		int tmp_int = NO_VAL, tmp_int2 = NO_VAL;

		switch(field->type) {
		case PRINT_ALLOC_CPUS:
			switch(type) {
			case JOB:
				tmp_int = job->alloc_cpus;
				// we want to use the step info
				if(!step)
					break;
			case JOBSTEP:
				tmp_int = step->ncpus;
				break;
			case JOBCOMP:
			default:
				tmp_int = NO_VAL;
				break;
			}
			field->print_routine(field,
					     tmp_int,
					     (curr_inx == field_count));
			break;
		case PRINT_ACCOUNT:
			switch(type) {
			case JOB:
				tmp_char = job->account;
				break;
			case JOBSTEP:
				tmp_char = step->job_ptr->account;
				break;
			case JOBCOMP:
			default:
				tmp_char = "n/a";
				break;
			}
			field->print_routine(field,
					     tmp_char,
					     (curr_inx == field_count));
			break;
		case PRINT_ASSOCID:
			switch(type) {
			case JOB:
				tmp_int = job->associd;
				break;
			case JOBSTEP:
				tmp_int = step->job_ptr->associd;
				break;
			case JOBCOMP:
			default:
				tmp_int = NO_VAL;
				break;
			}
			field->print_routine(field,
					     tmp_int,
					     (curr_inx == field_count));
			break;
		case PRINT_AVECPU:
			switch(type) {
			case JOB:
				if(!job->track_steps)
					tmp_int = job->sacct.ave_cpu;
				break;
			case JOBSTEP:
				tmp_int = step->sacct.ave_cpu;
				break;
			case JOBCOMP:
			default:
				break;
			}
			tmp_char = _elapsed_time((int)tmp_int, 0);

			field->print_routine(field,
					     tmp_char,
					     (curr_inx == field_count));
			xfree(tmp_char);
			break;
		case PRINT_AVEPAGES:
			switch(type) {
			case JOB:
				if(!job->track_steps)
					tmp_int = job->sacct.ave_pages;
				break;
			case JOBSTEP:
				tmp_int = step->sacct.ave_pages;
				break;
			case JOBCOMP:
			default:
				break;
			}
			if(tmp_int != NO_VAL)
				convert_num_unit((float)tmp_int,
						 outbuf, sizeof(outbuf),
						 UNIT_KILO);

			field->print_routine(field,
					     outbuf,
					     (curr_inx == field_count));
			break;
		case PRINT_AVERSS:
			switch(type) {
			case JOB:
				if(!job->track_steps)
					tmp_int = job->sacct.ave_rss;
				break;
			case JOBSTEP:
				tmp_int = step->sacct.ave_rss;
				break;
			case JOBCOMP:
			default:
				break;
			}
			if(tmp_int != NO_VAL)
				convert_num_unit((float)tmp_int,
						 outbuf, sizeof(outbuf),
						 UNIT_KILO);

			field->print_routine(field,
					     outbuf,
					     (curr_inx == field_count));
			break;
		case PRINT_AVEVSIZE:
			switch(type) {
			case JOB:
				if(!job->track_steps)
					tmp_int = job->sacct.ave_vsize;
				break;
			case JOBSTEP:
				tmp_int = step->sacct.ave_vsize;
				break;
			case JOBCOMP:
			default:
				break;
			}
			if(tmp_int != NO_VAL)
				convert_num_unit((float)tmp_int,
						 outbuf, sizeof(outbuf),
						 UNIT_KILO);

			field->print_routine(field,
					     outbuf,
					     (curr_inx == field_count));
			break;
		case PRINT_BLOCKID:
			switch(type) {
			case JOB:
				tmp_char = job->blockid;
				break;
			case JOBSTEP:
				break;
			case JOBCOMP:
				tmp_char = job_comp->blockid;
				break;
			default:
				break;
			}
			field->print_routine(field,
					     tmp_char,
					     (curr_inx == field_count));
			break;
		case PRINT_CLUSTER:
			switch(type) {
			case JOB:
				tmp_char = job->cluster;
				break;
			case JOBSTEP:
				tmp_char = step->job_ptr->cluster;
				break;
			case JOBCOMP:
			default:
				break;
			}
			field->print_routine(field,
					     tmp_char,
					     (curr_inx == field_count));
			break;
		case PRINT_CPU_TIME:
			switch(type) {
			case JOB:
				tmp_int = job->elapsed * job->alloc_cpus;
				break;
			case JOBSTEP:
				tmp_int = step->elapsed * step->ncpus;
				break;
			case JOBCOMP:
				break;
			default:
				break;
			}
			field->print_routine(field,
					     tmp_int,
					     (curr_inx == field_count));
			break;
		case PRINT_CPU_TIME_RAW:
			switch(type) {
			case JOB:
				tmp_int = job->elapsed * job->alloc_cpus;
				break;
			case JOBSTEP:
				tmp_int = step->elapsed * step->ncpus;
				break;
			case JOBCOMP:
				break;
			default:
				break;
			}
			field->print_routine(field,
					     tmp_int,
					     (curr_inx == field_count));
			break;
		case PRINT_ELAPSED:
			switch(type) {
			case JOB:
				tmp_int = job->elapsed;
				break;
			case JOBSTEP:
				tmp_int = step->elapsed;
				break;
			case JOBCOMP:
				tmp_int = job_comp->end_time
					- job_comp->start_time;
				break;
			default:
				tmp_int = NO_VAL;
				break;
			}
			field->print_routine(field,
					     tmp_int,
					     (curr_inx == field_count));
			break;
		case PRINT_ELIGIBLE:
			switch(type) {
			case JOB:
				tmp_int = job->eligible;
				break;
			case JOBSTEP:
				tmp_int = step->start;
				break;
			case JOBCOMP:
				break;
			default:
				break;
			}
			field->print_routine(field,
					     tmp_int,
					     (curr_inx == field_count));
			break;
		case PRINT_END:
			switch(type) {
			case JOB:
				tmp_int = job->end;
				break;
			case JOBSTEP:
				tmp_int = step->end;
				break;
			case JOBCOMP:
				tmp_int = parse_time(job_comp->end_time, 1);
				break;
			default:
				tmp_int = NO_VAL;
				break;
			}
			field->print_routine(field,
					     tmp_int,
					     (curr_inx == field_count));
			break;
		case PRINT_EXITCODE:
			tmp_int = 0;
			tmp_int2 = 0;
			switch(type) {
			case JOB:
				tmp_int = job->exitcode;
				break;
			case JOBSTEP:
				tmp_int = step->exitcode;
				break;
			case JOBCOMP:
			default:
				break;
			}
#ifdef NEWQUERY
			if (tmp_int != NO_VAL) {
				if (WIFSIGNALED(tmp_int))
					tmp_int2 = WTERMSIG(tmp_int);
				tmp_int = WEXITSTATUS(tmp_int);
				if (tmp_int >= 128)
					tmp_int -= 128;
			}
			snprintf(outbuf, sizeof(outbuf), "%d:%d",
				 tmp_int, tmp_int2);
#else
			if (WIFSIGNALED(tmp_int))
				tmp_int2 = WTERMSIG(tmp_int);

			snprintf(outbuf, sizeof(outbuf), "%d:%d",
				 WEXITSTATUS(tmp_int), tmp_int2);
#endif
			field->print_routine(field,
					     outbuf,
					     (curr_inx == field_count));
			break;
		case PRINT_GID:
			switch(type) {
			case JOB:
				tmp_int = job->gid;
				break;
			case JOBSTEP:
				tmp_int = NO_VAL;
				break;
			case JOBCOMP:
				tmp_int = job_comp->gid;
				break;
			default:
				tmp_int = NO_VAL;
				break;
			}
			field->print_routine(field,
					     tmp_int,
					     (curr_inx == field_count));
			break;
		case PRINT_GROUP:
			switch(type) {
			case JOB:
				tmp_int = job->gid;
				break;
			case JOBSTEP:
				tmp_int = NO_VAL;
				break;
			case JOBCOMP:
				tmp_int = job_comp->gid;
				break;
			default:
				tmp_int = NO_VAL;
				break;
			}
			tmp_char = NULL;
			// if ((gr=getgrgid(tmp_int)))
			if ((gr=compat_getgrgid(tmp_int)))
				tmp_char=gr->gr_name;

			field->print_routine(field,
					     tmp_char,
					     (curr_inx == field_count));
			break;
		case PRINT_JOBID:
#ifdef NEWQUERY
		/* repeat here */
		case PRINT_JOBIDRAW:
#endif
			switch(type) {
			case JOB:
				tmp_char = xstrdup_printf("%u", job->jobid);
				break;
			case JOBSTEP:
				tmp_char = xstrdup_printf("%u.%u",
							  step->job_ptr->jobid,
							  step->stepid);
				break;
			case JOBCOMP:
				tmp_char = xstrdup_printf("%u",
							  job_comp->jobid);
				break;
			default:
				break;
			}
			field->print_routine(field,
					     tmp_char,
					     (curr_inx == field_count));
			xfree(tmp_char);
			break;
		case PRINT_JOBNAME:
			switch(type) {
			case JOB:
				tmp_char = job->jobname;
				break;
			case JOBSTEP:
				tmp_char = step->stepname;
				break;
			case JOBCOMP:
				tmp_char = job_comp->jobname;
				break;
			default:
				tmp_char = NULL;
				break;
			}
			field->print_routine(field,
					     tmp_char,
					     (curr_inx == field_count));
			break;
		case PRINT_LAYOUT:
			switch(type) {
			case JOB:
				/* below really should be step.  It is
				   not a typo */
				if(!job->track_steps)
					tmp_char = slurm_step_layout_type_name(
						step->task_dist);
				break;
			case JOBSTEP:
				tmp_char = slurm_step_layout_type_name(
					step->task_dist);
				break;
			case JOBCOMP:
				break;
			default:
				tmp_char = NULL;
				break;
			}
			field->print_routine(field,
					     tmp_char,
					     (curr_inx == field_count));
			break;
		case PRINT_MAXPAGES:
			switch(type) {
			case JOB:
				if(!job->track_steps)
					tmp_int = job->sacct.max_pages;
				break;
			case JOBSTEP:
				tmp_int = step->sacct.max_pages;
				break;
			case JOBCOMP:
			default:
				break;
			}
			if(tmp_int != NO_VAL)
				convert_num_unit((float)tmp_int,
						 outbuf, sizeof(outbuf),
						 UNIT_KILO);

			field->print_routine(field,
					     outbuf,
					     (curr_inx == field_count));
			break;
		case PRINT_MAXPAGESNODE:
			switch(type) {
			case JOB:
				if(!job->track_steps)
					tmp_char = find_hostname(
						job->sacct.max_pages_id.nodeid,
						job->nodes);
				break;
			case JOBSTEP:
				tmp_char = find_hostname(
					step->sacct.max_pages_id.nodeid,
					step->nodes);
				break;
			case JOBCOMP:
			default:
				tmp_char = NULL;
				break;
			}
			field->print_routine(field,
					     tmp_char,
					     (curr_inx == field_count));
			xfree(tmp_char);
			break;
		case PRINT_MAXPAGESTASK:
			switch(type) {
			case JOB:
				if(!job->track_steps)
					tmp_int =
						job->sacct.max_pages_id.taskid;
				break;
			case JOBSTEP:
				tmp_int = step->sacct.max_pages_id.taskid;
				break;
			case JOBCOMP:
			default:
				tmp_int = NO_VAL;
				break;
			}
			field->print_routine(field,
					     tmp_int,
					     (curr_inx == field_count));
			break;
		case PRINT_MAXRSS:
			switch(type) {
			case JOB:
				if(!job->track_steps)
					tmp_int = job->sacct.max_rss;
				break;
			case JOBSTEP:
				tmp_int = step->sacct.max_rss;
				break;
			case JOBCOMP:
			default:
				break;
			}
			if(tmp_int != NO_VAL)
				convert_num_unit((float)tmp_int,
						 outbuf, sizeof(outbuf),
						 UNIT_KILO);

			field->print_routine(field,
					     outbuf,
					     (curr_inx == field_count));
			break;
		case PRINT_MAXRSSNODE:
			switch(type) {
			case JOB:
				if(!job->track_steps)
					tmp_char = find_hostname(
						job->sacct.max_rss_id.nodeid,
						job->nodes);
				break;
			case JOBSTEP:
				tmp_char = find_hostname(
					step->sacct.max_rss_id.nodeid,
					step->nodes);
				break;
			case JOBCOMP:
			default:
				tmp_char = NULL;
				break;
			}
			field->print_routine(field,
					     tmp_char,
					     (curr_inx == field_count));
			xfree(tmp_char);
			break;
		case PRINT_MAXRSSTASK:
			switch(type) {
			case JOB:
				if(!job->track_steps)
					tmp_int = job->sacct.max_rss_id.taskid;
				break;
			case JOBSTEP:
				tmp_int = step->sacct.max_rss_id.taskid;
				break;
			case JOBCOMP:
			default:
				tmp_int = NO_VAL;
				break;
			}
			field->print_routine(field,
					     tmp_int,
					     (curr_inx == field_count));
			break;
		case PRINT_MAXVSIZE:
			switch(type) {
			case JOB:
				if(!job->track_steps)
					tmp_int = job->sacct.max_vsize;
				break;
			case JOBSTEP:
				tmp_int = step->sacct.max_vsize;
				break;
			case JOBCOMP:
			default:
				tmp_int = NO_VAL;
				break;
			}
			if(tmp_int != NO_VAL)
				convert_num_unit((float)tmp_int,
						 outbuf, sizeof(outbuf),
						 UNIT_KILO);

			field->print_routine(field,
					     outbuf,
					     (curr_inx == field_count));
			break;
		case PRINT_MAXVSIZENODE:
			switch(type) {
			case JOB:
				if(!job->track_steps)
					tmp_char = find_hostname(
						job->sacct.max_vsize_id.nodeid,
						job->nodes);
				break;
			case JOBSTEP:
				tmp_char = find_hostname(
					step->sacct.max_vsize_id.nodeid,
					step->nodes);
				break;
			case JOBCOMP:
			default:
				tmp_char = NULL;
				break;
			}
			field->print_routine(field,
					     tmp_char,
					     (curr_inx == field_count));
			xfree(tmp_char);
			break;
		case PRINT_MAXVSIZETASK:
			switch(type) {
			case JOB:
				if(!job->track_steps)
					tmp_int =
						job->sacct.max_vsize_id.taskid;
				break;
			case JOBSTEP:
				tmp_int = step->sacct.max_vsize_id.taskid;
				break;
			case JOBCOMP:
			default:
				tmp_int = NO_VAL;
				break;
			}
			field->print_routine(field,
					     tmp_int,
					     (curr_inx == field_count));
			break;
		case PRINT_MINCPU:
			switch(type) {
			case JOB:
				if(!job->track_steps)
					tmp_int = job->sacct.min_cpu;
				break;
			case JOBSTEP:
				tmp_int = step->sacct.min_cpu;
				break;
			case JOBCOMP:
			default:
				break;
			}
			tmp_char = _elapsed_time((int)tmp_int, 0);
			field->print_routine(field,
					     tmp_char,
					     (curr_inx == field_count));
			xfree(tmp_char);
			break;
		case PRINT_MINCPUNODE:
			switch(type) {
			case JOB:
				if(!job->track_steps)
					tmp_char = find_hostname(
						job->sacct.min_cpu_id.nodeid,
						job->nodes);
				break;
			case JOBSTEP:
				tmp_char = find_hostname(
					step->sacct.min_cpu_id.nodeid,
					step->nodes);
				break;
			case JOBCOMP:
			default:
				tmp_char = NULL;
				break;
			}
			field->print_routine(field,
					     tmp_char,
					     (curr_inx == field_count));
			xfree(tmp_char);
			break;
		case PRINT_MINCPUTASK:
			switch(type) {
			case JOB:
				if(!job->track_steps)
					tmp_int = job->sacct.min_cpu_id.taskid;
				break;
			case JOBSTEP:
				tmp_int = step->sacct.min_cpu_id.taskid;
				break;
			case JOBCOMP:
			default:
				tmp_int = NO_VAL;
				break;
			}
			field->print_routine(field,
					     tmp_int,
					     (curr_inx == field_count));
			break;
		case PRINT_NODELIST:
			switch(type) {
			case JOB:
				tmp_char = job->nodes;
				break;
			case JOBSTEP:
				tmp_char = step->nodes;
				break;
			case JOBCOMP:
				tmp_char = job_comp->nodelist;
				break;
			default:
				break;
			}
			field->print_routine(field,
					     tmp_char,
					     (curr_inx == field_count));
			break;
		case PRINT_NNODES:
			switch(type) {
			case JOB:
				tmp_int = job->alloc_nodes;
				tmp_char = job->nodes;
				break;
			case JOBSTEP:
				tmp_int = step->nnodes;
				tmp_char = step->nodes;
				break;
			case JOBCOMP:
				tmp_int = job_comp->node_cnt;
				tmp_char = job_comp->nodelist;
				break;
			default:
				break;
			}

			if(!tmp_int) {
				hostlist_t hl = hostlist_create(tmp_char);
				tmp_int = hostlist_count(hl);
				hostlist_destroy(hl);
			}
			convert_num_unit((float)tmp_int,
					 outbuf, sizeof(outbuf), UNIT_NONE);
			field->print_routine(field,
					     outbuf,
					     (curr_inx == field_count));
			break;
		case PRINT_NTASKS:
			switch(type) {
			case JOB:
				if(!job->track_steps && !step)
					tmp_int = job->alloc_cpus;
				// we want to use the step info
				if(!step)
					break;
			case JOBSTEP:
				tmp_int = step->ntasks;
				break;
			case JOBCOMP:
			default:

				break;
			}
			field->print_routine(field,
					     tmp_int,
					     (curr_inx == field_count));
			break;
		case PRINT_PRIO:
			switch(type) {
			case JOB:
				tmp_int = job->priority;
				break;
			case JOBSTEP:

				break;
			case JOBCOMP:

				break;
			default:

				break;
			}
			field->print_routine(field,
					     tmp_int,
					     (curr_inx == field_count));
			break;
		case PRINT_PARTITION:
			switch(type) {
			case JOB:
				tmp_char = job->partition;
				break;
			case JOBSTEP:

				break;
			case JOBCOMP:
				tmp_char = job_comp->partition;
				break;
			default:

				break;
			}
			field->print_routine(field,
					     tmp_char,
					     (curr_inx == field_count));
			break;
		case PRINT_QOS:
			switch(type) {
			case JOB:
				tmp_int = job->qos;
				break;
			case JOBSTEP:

				break;
			case JOBCOMP:

				break;
			default:

				break;
			}
			if(!qos_list)
				qos_list = acct_storage_g_get_qos(
					acct_db_conn, getuid(), NULL);

			tmp_char = _find_qos_name_from_list(qos_list,
							    tmp_int);
			field->print_routine(field,
					     tmp_char,
					     (curr_inx == field_count));
			break;
		case PRINT_QOSRAW:
			switch(type) {
			case JOB:
				tmp_int = job->qos;
				break;
			case JOBSTEP:

				break;
			case JOBCOMP:

				break;
			default:

				break;
			}
			field->print_routine(field,
					     tmp_int,
					     (curr_inx == field_count));
			break;
#ifdef NEWQUERY
        case PRINT_REQ_GRES:
        	switch(type) {
			case JOB:
				// tmp_char = job->req_gres;
				tmp_char = NULL;
				break;
			case JOBSTEP:
				// tmp_char = step->job_ptr->req_gres;
				tmp_char = NULL;
				break;
			case JOBCOMP:
			default:
				tmp_char = NULL;
				break;
			}
			field->print_routine(field,
						 tmp_char,
						 (curr_inx == field_count));
			break;
		/* I am going to assume the default min_per_cpu from slurm.conf
		 * I suspect that only very few jobs requested explicitly
		 *  memory constraints */
		case PRINT_REQ_MEM:
			switch(type) {
			case JOB:
			case JOBSTEP:
				tmp_uint32 = slurm_get_def_mem_per_task();
				break;
			case JOBCOMP:
				break;
			default:
				tmp_uint32 = NO_VAL;
				break;
			}
			if (tmp_uint32 != (uint32_t)NO_VAL) {
				bool per_cpu = false;
				if (tmp_uint32 & MEM_PER_CPU) {
					tmp_uint32 &= (~MEM_PER_CPU);
					per_cpu = true;
				}
				convert_num_unit((float)tmp_uint32,
						 outbuf, sizeof(outbuf),
						 UNIT_MEGA);
				if (per_cpu)
					sprintf(outbuf+strlen(outbuf), "c");
				else
					sprintf(outbuf+strlen(outbuf), "n");
			}
			field->print_routine(field,
					     outbuf,
					     (curr_inx == field_count));
			break;
#endif
		case PRINT_REQ_CPUS:
			switch(type) {
			case JOB:
				tmp_int = job->req_cpus;
				break;
			case JOBSTEP:
				tmp_int = step->ncpus;
				break;
			case JOBCOMP:

				break;
			default:

				break;
			}
			field->print_routine(field,
					     tmp_int,
					     (curr_inx == field_count));
			break;
		case PRINT_RESV:
			switch(type) {
			case JOB:
				if(job->start)
					tmp_int = job->start - job->eligible;
				else
					tmp_int = time(NULL) - job->eligible;
				break;
			case JOBSTEP:
				break;
			case JOBCOMP:

				break;
			default:

				break;
			}
			field->print_routine(field,
					     tmp_int,
					     (curr_inx == field_count));
			break;
		case PRINT_RESV_CPU:
			switch(type) {
			case JOB:
				if(job->start)
					tmp_int = (job->start - job->eligible)
						* job->req_cpus;
				else
					tmp_int = (time(NULL) - job->eligible)
						* job->req_cpus;
				break;
			case JOBSTEP:
				break;
			case JOBCOMP:

				break;
			default:

				break;
			}
			field->print_routine(field,
					     tmp_int,
					     (curr_inx == field_count));
			break;
		case PRINT_RESV_CPU_RAW:
			switch(type) {
			case JOB:
				if(job->start)
					tmp_int = (job->start - job->eligible)
						* job->req_cpus;
				else
					tmp_int = (time(NULL) - job->eligible)
						* job->req_cpus;
				break;
			case JOBSTEP:
				break;
			case JOBCOMP:

				break;
			default:

				break;
			}
			field->print_routine(field,
					     tmp_int,
					     (curr_inx == field_count));
			break;
		case PRINT_START:
			switch(type) {
			case JOB:
				tmp_int = job->start;
				break;
			case JOBSTEP:
				tmp_int = step->start;
				break;
			case JOBCOMP:
#ifdef NEWQUERY
				// Just to avoid the error message that parse_time spits out
				if(job_comp->end_time) {
					if(!job_comp->start_time || (job_comp->start_time > job_comp->end_time)){
						debug2("%s - %s", job_comp->start_time, job_comp->end_time);
						job_comp->start_time = xstrdup(job_comp->end_time);
					}
				}
#endif
				tmp_int = parse_time(job_comp->start_time, 1);
				break;
			default:

				break;
			}
			field->print_routine(field,
					     tmp_int,
					     (curr_inx == field_count));
			break;
		case PRINT_STATE:
			switch(type) {
			case JOB:
				tmp_int = job->state;
				tmp_int2 = job->requid;
				break;
			case JOBSTEP:
				tmp_int = step->state;
				tmp_int2 = step->requid;
				break;
			case JOBCOMP:
				tmp_char = job_comp->state;
				break;
			default:

				break;
			}

			if ((tmp_int == JOB_CANCELLED) && (tmp_int2 != NO_VAL))
				snprintf(outbuf, FORMAT_STRING_SIZE,
					 "%s by %d",
					 job_state_string(tmp_int),
					 tmp_int2);
			else if(tmp_int != NO_VAL)
				snprintf(outbuf, FORMAT_STRING_SIZE,
					 "%s",
					 job_state_string(tmp_int));
			else if(tmp_char)
				snprintf(outbuf, FORMAT_STRING_SIZE,
					 "%s",
					 tmp_char);

			field->print_routine(field,
					     outbuf,
					     (curr_inx == field_count));
			break;
		case PRINT_SUBMIT:
			switch(type) {
			case JOB:
				tmp_int = job->submit;
				break;
			case JOBSTEP:
				tmp_int = step->start;
				break;
			case JOBCOMP:
				tmp_int = parse_time(job_comp->start_time, 1);
				break;
			default:

				break;
			}
			field->print_routine(field,
					     tmp_int,
					     (curr_inx == field_count));
			break;
		case PRINT_SUSPENDED:
			switch(type) {
			case JOB:
				tmp_int = job->suspended;
				break;
			case JOBSTEP:
				tmp_int = step->suspended;
				break;
			case JOBCOMP:

				break;
			default:

				break;
			}
			field->print_routine(field,
					     tmp_int,
					     (curr_inx == field_count));
			break;
		case PRINT_SYSTEMCPU:
			switch(type) {
			case JOB:
				tmp_int = job->sys_cpu_sec;
				tmp_int2 = job->sys_cpu_usec;
				break;
			case JOBSTEP:
				tmp_int = step->sys_cpu_sec;
				tmp_int2 = step->sys_cpu_usec;

				break;
			case JOBCOMP:

				break;
			default:

				break;
			}
			tmp_char = _elapsed_time(tmp_int, tmp_int2);

			field->print_routine(field,
					     tmp_char,
					     (curr_inx == field_count));
			xfree(tmp_char);
			break;
		case PRINT_TIMELIMIT:
			switch(type) {
			case JOB:
#ifdef NEWQUERY
				if (job->timelimit == INFINITE)
					tmp_char = "UNLIMITED";
				else if (job->timelimit == NO_VAL){
					tmp_char = "Partition_Limit";
				}
				else if (job->timelimit) {
					mins2time_str(job->timelimit,
						      tmp1, sizeof(tmp1));
					tmp_char = tmp1;
				}
#endif
				break;
			case JOBSTEP:

				break;
			case JOBCOMP:
				tmp_char = job_comp->timelimit;
				break;
			default:

				break;
			}
			field->print_routine(field,
					     tmp_char,
					     (curr_inx == field_count));
			break;
		case PRINT_TOTALCPU:
			switch(type) {
			case JOB:
				tmp_int = job->tot_cpu_sec;
				tmp_int2 = job->tot_cpu_usec;
				break;
			case JOBSTEP:
				tmp_int = step->tot_cpu_sec;
				tmp_int2 = step->tot_cpu_usec;
				break;
			case JOBCOMP:

				break;
			default:

				break;
			}
			tmp_char = _elapsed_time(tmp_int, tmp_int2);

			field->print_routine(field,
					     tmp_char,
					     (curr_inx == field_count));
			xfree(tmp_char);
			break;
#ifdef NEWQUERY
		case PRINT_TRESR:
			switch(type) {
			case JOB:
				// tmp_char = job->tres_req_str;
				tmp_char = NULL;
				break;
			case JOBSTEP:
			case JOBCOMP:
			default:
				tmp_char = NULL;
				break;
			}
			field->print_routine(field,
								 tmp_char,
								 (curr_inx == field_count));
			xfree(tmp_char);
			break;
#endif
		case PRINT_UID:
			switch(type) {
			case JOB:
				if(job->user) {
					// if ((pw=getpwnam(job->user)))
					if ((pw=compat_getpwnam(job->user)))
						tmp_int = pw->pw_uid;
				} else
					tmp_int = job->uid;
				break;
			case JOBSTEP:
				break;
			case JOBCOMP:
				tmp_int = job_comp->uid;
				break;
			default:

				break;
			}

			field->print_routine(field,
					     tmp_int,
					     (curr_inx == field_count));
			break;
		case PRINT_USER:
			switch(type) {
			case JOB:
				if(job->user)
					tmp_char = job->user;
				else if(job->uid != -1) {
					// if ((pw=getpwuid(job->uid)))
					if ((pw=compat_getpwuid(job->uid)))
						tmp_char = pw->pw_name;
				}
				break;
			case JOBSTEP:

				break;
			case JOBCOMP:
				tmp_char = job_comp->uid_name;
				break;
			default:

				break;
			}

			field->print_routine(field,
					     tmp_char,
					     (curr_inx == field_count));
			break;
		case PRINT_USERCPU:
			switch(type) {
			case JOB:
				tmp_int = job->user_cpu_sec;
				tmp_int2 = job->tot_cpu_usec;
				break;
			case JOBSTEP:
				tmp_int = step->user_cpu_sec;
				tmp_int2 = step->user_cpu_usec;

				break;
			case JOBCOMP:

				break;
			default:

				break;
			}
			tmp_char = _elapsed_time(tmp_int, tmp_int2);

			field->print_routine(field,
					     tmp_char,
					     (curr_inx == field_count));
			xfree(tmp_char);
			break;
		case PRINT_WCKEY:
			switch(type) {
			case JOB:
				tmp_char = job->wckey;
				break;
			case JOBSTEP:

				break;
			case JOBCOMP:

				break;
			default:

				break;
			}
			field->print_routine(field,
					     tmp_char,
					     (curr_inx == field_count));
			break;
		case PRINT_WCKEYID:
			switch(type) {
			case JOB:
				tmp_int = job->wckeyid;
				break;
			case JOBSTEP:

				break;
			case JOBCOMP:

				break;
			default:

				break;
			}
			field->print_routine(field,
					     tmp_int,
					     (curr_inx == field_count));
			break;
		default:
			break;
		}
		curr_inx++;
	}
	printf("\n");
}
