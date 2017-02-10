#include <stdlib.h>
#include "src/common/env.h"
#include "src/common/xstring.h"
#include "src/common/timers_n.h"
#include "mysql_jobacct_process.h"
#include <fcntl.h>
#include "src/common/slurm_protocol_defs.h"

#define JOB_STATE_BASE	0x000000ff

static pthread_mutex_t local_file_lock = PTHREAD_MUTEX_INITIALIZER;

typedef struct {
	hostlist_t hl;
	time_t start;
	time_t end;
	bitstr_t *asked_bitmap;
} local_cluster_t;

static void _destroy_local_cluster(void *object)
{
	local_cluster_t *local_cluster = (local_cluster_t *)object;
	if(local_cluster) {
		if(local_cluster->hl)
			hostlist_destroy(local_cluster->hl);
		FREE_NULL_BITMAP(local_cluster->asked_bitmap);
		xfree(local_cluster);
	}
}

static int _write_to_file(int fd, char *data)
{
	int pos = 0, nwrite = strlen(data), amount;
	int rc = SLURM_SUCCESS;

	while (nwrite > 0) {
		amount = write(fd, &data[pos], nwrite);
		if ((amount < 0) && (errno != EINTR)) {
			error("Error writing file: %m");
			rc = errno;
			break;
		}
		nwrite -= amount;
		pos    += amount;
	}
	return rc;
}

static int _write_archive_file(MYSQL_RES *result, int start_col, int col_count,
			       time_t curr_end, char *arch_dir,
			       char *arch_type, char *insert,
			       bool with_deleted)
{
	time_t period_start = 0;
	int fd = 0;
	int rc = SLURM_SUCCESS;
	MYSQL_ROW row;
	struct tm time_tm;
	char *old_file = NULL, *new_file = NULL, *reg_file = NULL;
	char *values = NULL;
	char start_char[32];
	char end_char[32];
	int i=0;

	xassert(result);

	//START_TIMER;
	slurm_mutex_lock(&local_file_lock);
	while((row = mysql_fetch_row(result))) {
		if(period_start) {
			xstrcat(values, ",\n(");
		} else {
			period_start = atoi(row[start_col]);
			localtime_r((time_t *)&period_start, &time_tm);
			time_tm.tm_sec = 0;
			time_tm.tm_min = 0;
			time_tm.tm_hour = 0;
			time_tm.tm_mday = 1;
			snprintf(start_char, sizeof(start_char),
				 "%4.4u-%2.2u-%2.2u"
				 "T%2.2u:%2.2u:%2.2u",
				 (time_tm.tm_year + 1900),
				 (time_tm.tm_mon+1), 
				 time_tm.tm_mday,
				 time_tm.tm_hour,
				 time_tm.tm_min, 
				 time_tm.tm_sec);

			localtime_r((time_t *)&curr_end, &time_tm);
			snprintf(end_char, sizeof(end_char),
				 "%4.4u-%2.2u-%2.2u"
				 "T%2.2u:%2.2u:%2.2u",
				 (time_tm.tm_year + 1900),
				 (time_tm.tm_mon+1), 
				 time_tm.tm_mday,
				 time_tm.tm_hour,
				 time_tm.tm_min, 
				 time_tm.tm_sec);

			/* write the buffer to file */
			reg_file = xstrdup_printf(
				"%s/%s_archive_%s_%s.sql",
				arch_dir, arch_type,
				start_char, end_char);
			debug("Storing event archive at %s", reg_file);
			old_file = xstrdup_printf("%s.old", reg_file);
			new_file = xstrdup_printf("%s.new", reg_file);

			fd = creat(new_file, 0600);
			if (fd == 0) {
				error("Can't save archive, "
				      "create file %s error %m",
				      new_file);
				rc = errno;
				xfree(insert);
				break;
			}
			values = xstrdup_printf("%s\nvalues\n(", insert);
		}

		xstrfmtcat(values, "'%s'", row[0]);
		for(i=1; i<col_count; i++) {
			xstrfmtcat(values, ", '%s'", row[i]);
		}

		if(with_deleted)
			xstrcat(values, ", '1')");
		else
			xstrcat(values, ")");

		if(!fd
		   || ((rc = _write_to_file(fd, values)) != SLURM_SUCCESS)) {
			xfree(values);
			break;
		}
		xfree(values);
	}

	if(with_deleted)
	      	rc = _write_to_file(fd,
			    " on duplicate key update "
			    "deleted=1;");
	else
	      	rc = _write_to_file(fd,
			    " on duplicate key update "
			    "period_end=VALUES(period_end);");
//			END_TIMER2("write file");
//			info("write file took %s", TIME_STR);

	fsync(fd);
	close(fd);

	if (rc)
		(void) unlink(new_file);
	else {			/* file shuffle */
		int ign;	/* avoid warning */
		(void) unlink(old_file);
		ign =  link(reg_file, old_file);
		(void) unlink(reg_file);
		ign =  link(new_file, reg_file);
		(void) unlink(new_file);
	}
	xfree(old_file);
	xfree(reg_file);
	xfree(new_file);
	slurm_mutex_unlock(&local_file_lock);

	return rc;
}

static int _archive_script(acct_archive_cond_t *arch_cond, time_t last_submit)
{
	char * args[] = {arch_cond->archive_script, NULL};
	const char *tmpdir;
	struct stat st;
	char **env = NULL;
	struct tm time_tm;
	time_t curr_end;

#ifdef _PATH_TMP
	tmpdir = _PATH_TMP;
#else
	tmpdir = "/tmp";
#endif
	if (stat(arch_cond->archive_script, &st) < 0) {
		errno = errno;
		error("mysql_jobacct_process_run_script: failed to stat %s: %m",
		      arch_cond->archive_script);
		return SLURM_ERROR;
	}

	if (!(st.st_mode & S_IFREG)) {
		errno = EACCES;
		error("mysql_jobacct_process_run_script: "
		      "%s isn't a regular file",
		      arch_cond->archive_script);
		return SLURM_ERROR;
	}

	if (access(arch_cond->archive_script, X_OK) < 0) {
		errno = EACCES;
		error("mysql_jobacct_process_run_script: "
		      "%s is not executable", arch_cond->archive_script);
		return SLURM_ERROR;
	}

	env = env_array_create();

	if(arch_cond->purge_event) {
		/* use localtime to avoid any daylight savings issues */
		if(!localtime_r(&last_submit, &time_tm)) {
			error("Couldn't get localtime from "
			      "first event start %d",
			      last_submit);
			return SLURM_ERROR;
		}
		time_tm.tm_mon -= arch_cond->purge_step;
		time_tm.tm_isdst = -1;
		curr_end = mktime(&time_tm);
		env_array_append_fmt(&env, "SLURM_ARCHIVE_EVENTS", "%u",
				     arch_cond->archive_events);
		env_array_append_fmt(&env, "SLURM_ARCHIVE_LAST_EVENT", "%d",
				     curr_end);
	}

	if(arch_cond->purge_job) {
		/* use localtime to avoid any daylight savings issues */
		if(!localtime_r(&last_submit, &time_tm)) {
			error("Couldn't get localtime from first start %d",
			      last_submit);
			return SLURM_ERROR;
		}
		time_tm.tm_mon -= arch_cond->purge_job;
		time_tm.tm_isdst = -1;
		curr_end = mktime(&time_tm);

		env_array_append_fmt(&env, "SLURM_ARCHIVE_JOBS", "%u",
				     arch_cond->archive_jobs);
		env_array_append_fmt (&env, "SLURM_ARCHIVE_LAST_JOB", "%d",
				      curr_end);
	}

	if(arch_cond->purge_step) {
		/* use localtime to avoid any daylight savings issues */
		if(!localtime_r(&last_submit, &time_tm)) {
			error("Couldn't get localtime from first step start %d",
			      last_submit);
			return SLURM_ERROR;
		}
		time_tm.tm_mon -= arch_cond->purge_step;
		time_tm.tm_isdst = -1;
		curr_end = mktime(&time_tm);
		env_array_append_fmt(&env, "SLURM_ARCHIVE_STEPS", "%u",
				     arch_cond->archive_steps);
		env_array_append_fmt(&env, "SLURM_ARCHIVE_LAST_STEP", "%d",
				     curr_end);
	}

	if(arch_cond->purge_suspend) {
		/* use localtime to avoid any daylight savings issues */
		if(!localtime_r(&last_submit, &time_tm)) {
			error("Couldn't get localtime from first "
			      "suspend start %d",
			      last_submit);
			return SLURM_ERROR;
		}
		time_tm.tm_mon -= arch_cond->purge_step;
		time_tm.tm_isdst = -1;
		curr_end = mktime(&time_tm);
		env_array_append_fmt(&env, "SLURM_ARCHIVE_SUSPEND", "%u",
				     arch_cond->archive_steps);
		env_array_append_fmt(&env, "SLURM_ARCHIVE_LAST_SUSPEND", "%d",
				     curr_end);
	}

#ifdef _PATH_STDPATH
	env_array_append (&env, "PATH", _PATH_STDPATH);
#else
	env_array_append (&env, "PATH", "/bin:/usr/bin");
#endif
	execve(arch_cond->archive_script, args, env);

	env_array_free(env);

	return SLURM_SUCCESS;
}

extern List setup_cluster_list_with_inx(mysql_conn_t *mysql_conn,
					acct_job_cond_t *job_cond,
					void **curr_cluster)
{
	List local_cluster_list = NULL;
	time_t now = time(NULL);
	MYSQL_RES *result = NULL;
	MYSQL_ROW row;
	hostlist_t temp_hl = NULL;
	hostlist_iterator_t h_itr = NULL;
	char *object = NULL;
	char *query = NULL;

	if(!job_cond || !job_cond->used_nodes)
		return NULL;

	if(!job_cond->cluster_list || list_count(job_cond->cluster_list) != 1) {
		error("If you are doing a query against nodes "
		      "you must only have 1 cluster "
		      "you are asking for.");
		return NULL;
	}

	temp_hl = hostlist_create(job_cond->used_nodes);
	if(!hostlist_count(temp_hl)) {
		error("we didn't get any real hosts to look for.");
		goto no_hosts;
	}
	h_itr = hostlist_iterator_create(temp_hl);

	query = xstrdup_printf("select cluster_nodes, period_start, "
			       "period_end from %s where node_name='' "
			       "&& cluster_nodes !=''",
			       event_table);

	if((object = list_peek(job_cond->cluster_list)))
		xstrfmtcat(query, " && cluster='%s'", object);

	if(job_cond->usage_start) {
		if(!job_cond->usage_end)
			job_cond->usage_end = now;

		xstrfmtcat(query, 
			   " && ((period_start < %d) "
			   "&& (period_end >= %d || period_end = 0))",
			   job_cond->usage_end, job_cond->usage_start);
	}

	debug3("%d(%d) query\n%s", mysql_conn->conn, __LINE__, query);
	if(!(result = mysql_db_query_ret(mysql_conn->db_conn, query, 0))) {
		xfree(query);
		hostlist_destroy(temp_hl);
		return NULL;
	}
	xfree(query);

	local_cluster_list = list_create(_destroy_local_cluster);
	while((row = mysql_fetch_row(result))) {
		char *host = NULL;
		int loc = 0;
		local_cluster_t *local_cluster =
			xmalloc(sizeof(local_cluster_t));
		local_cluster->hl = hostlist_create(row[0]);
		local_cluster->start = atoi(row[1]);
		local_cluster->end   = atoi(row[2]);
		local_cluster->asked_bitmap = 
			bit_alloc(hostlist_count(local_cluster->hl));
		while((host = hostlist_next(h_itr))) {
			if((loc = hostlist_find(
				    local_cluster->hl, host)) != -1) 
				bit_set(local_cluster->asked_bitmap, loc);
			free(host);
		}
		hostlist_iterator_reset(h_itr);
		if(bit_ffs(local_cluster->asked_bitmap) != -1) {
			list_append(local_cluster_list, local_cluster);
			if(local_cluster->end == 0) {
				local_cluster->end = now;
				(*curr_cluster) = local_cluster;
			}
		} else 
			_destroy_local_cluster(local_cluster);
	}
	mysql_free_result(result);
	hostlist_iterator_destroy(h_itr);
	if(!list_count(local_cluster_list)) {
		hostlist_destroy(temp_hl);
		list_destroy(local_cluster_list);
		return NULL;
	}
no_hosts:

	hostlist_destroy(temp_hl);

	return local_cluster_list;
}

extern int good_nodes_from_inx(List local_cluster_list, 
			       void **object, char *node_inx,
			       int submit)
{
	local_cluster_t **curr_cluster = (local_cluster_t **)object;

	/* check the bitmap to see if this is one of the jobs
	   we are looking for */
	if(*curr_cluster) {
		bitstr_t *job_bitmap = NULL;
		if(!node_inx || !node_inx[0])
			return 0;
		if((submit < (*curr_cluster)->start)
		   || (submit > (*curr_cluster)->end)) {
			local_cluster_t *local_cluster = NULL;

			ListIterator itr =
				list_iterator_create(local_cluster_list);
			while((local_cluster = list_next(itr))) {
				if((submit >= local_cluster->start)
				   && (submit <= local_cluster->end)) {
					*curr_cluster = local_cluster;
						break;
				}
			}
			list_iterator_destroy(itr);
			if(!local_cluster)
				return 0;
		}
		job_bitmap = bit_alloc(hostlist_count((*curr_cluster)->hl));
		bit_unfmt(job_bitmap, node_inx);
		if(!bit_overlap((*curr_cluster)->asked_bitmap, job_bitmap)) {
			FREE_NULL_BITMAP(job_bitmap);
			return 0;
		}
		FREE_NULL_BITMAP(job_bitmap);
	}
	return 1;
}

#ifdef NEWQUERY
static void _state_time_string(char **extra, uint32_t state,
	uint32_t start, uint32_t end)
{
	if (!start && !end) {
		xstrfmtcat(*extra, "t1.state='%u'", state);
		return;
	}

	/*	ignored situations for JOB_PENDING, JOB_SUSPENDED,
		JOB_RUNNING
	*/
	debug4("_state_time_string: JOB_PENDING, JOB_SUSPENDED, JOB_RUNNING ignored");

	xstrfmtcat(*extra, "(t1.state='%u' && (t1.end && ", state);
	if (start) {
		if (!end) {
			xstrfmtcat(*extra, "(t1.end >= %d)))",
				   start);
		} else {
			xstrfmtcat(*extra,
				   "(t1.end between %d and %d)))",
				   start, end);
		}
	} else if (end) {
		xstrfmtcat(*extra, "(t1.end <= %d)))", end);
	}

	return;
}
#endif

extern int setup_job_cond_limits(mysql_conn_t *mysql_conn,
				 acct_job_cond_t *job_cond, char **extra)
{
	int set = 0;
	ListIterator itr = NULL;
	char *object = NULL;
	char *table_level = "t2";
	jobacct_selected_step_t *selected_step = NULL;

	if(!job_cond)
		return 0;

	/*
		DDCR: as far I can tell this block is never executed when issuing
		the command sacct, irrespective of the options which have been selected
	 */
	/* THIS ASSOCID CHECK ALWAYS NEEDS TO BE FIRST!!!!!!! */
	if(job_cond->associd_list && list_count(job_cond->associd_list)) {
		set = 0;
		xstrfmtcat(*extra, ", %s as t3 where (", assoc_table);
		itr = list_iterator_create(job_cond->associd_list);
		while((object = list_next(itr))) {
			if(set) 
				xstrcat(*extra, " || ");
			xstrfmtcat(*extra, "t3.id=%s", object);
			set = 1;
		}
		list_iterator_destroy(itr);
		xstrcat(*extra, ")");
		table_level="t3";
		/* just incase the association is gone */
		if(set) 
			xstrcat(*extra, " || ");
		xstrfmtcat(*extra, "t3.id is null) && "
			   "(t2.lft between t3.lft and t3.rgt "
			   "|| t2.lft is null)");
	}

	if(job_cond->acct_list && list_count(job_cond->acct_list)) {
		set = 0;
		if(*extra)
			xstrcat(*extra, " && (");
		else
			xstrcat(*extra, " where (");
		itr = list_iterator_create(job_cond->acct_list);
		while((object = list_next(itr))) {
			if(set) 
				xstrcat(*extra, " || ");
			xstrfmtcat(*extra, "t1.account='%s'", object);
			set = 1;
		}
		list_iterator_destroy(itr);
		xstrcat(*extra, ")");
	}

	if(job_cond->userid_list && list_count(job_cond->userid_list)) {
		set = 0;
		if(*extra)
			xstrcat(*extra, " && (");
		else
			xstrcat(*extra, " where (");

		itr = list_iterator_create(job_cond->userid_list);
		while((object = list_next(itr))) {
			if(set) 
				xstrcat(*extra, " || ");
			xstrfmtcat(*extra, "t1.uid='%s'", object);
			set = 1;
		}
		list_iterator_destroy(itr);
		xstrcat(*extra, ")");
	}

	if(job_cond->groupid_list && list_count(job_cond->groupid_list)) {
		set = 0;
		if(*extra)
			xstrcat(*extra, " && (");
		else
			xstrcat(*extra, " where (");
		itr = list_iterator_create(job_cond->groupid_list);
		while((object = list_next(itr))) {
			if(set) 
				xstrcat(*extra, " || ");
			xstrfmtcat(*extra, "t1.gid='%s'", object);
			set = 1;
		}
		list_iterator_destroy(itr);
		xstrcat(*extra, ")");
	}

	if(job_cond->partition_list && list_count(job_cond->partition_list)) {
		set = 0;
		if(*extra)
			xstrcat(*extra, " && (");
		else
			xstrcat(*extra, " where (");
		itr = list_iterator_create(job_cond->partition_list);
		while((object = list_next(itr))) {
			if(set) 
				xstrcat(*extra, " || ");
			xstrfmtcat(*extra, "t1.partition='%s'", object);
			set = 1;
		}
		list_iterator_destroy(itr);
		xstrcat(*extra, ")");
	}

	/*
		DDCR: as far I can tell this block is never executed when issuing
		the command sacct, irrespective of the options which have been selected
	 */
	/* this must be done before resvid_list since we set
	   resvid_list up here */
	if(job_cond->resv_list && list_count(job_cond->resv_list)) {
		char *query = xstrdup_printf(
			"select distinct id from %s where (");
		int my_set = 0;
		MYSQL_RES *result = NULL;
		MYSQL_ROW row;

		if(job_cond->cluster_list
		   && list_count(job_cond->cluster_list)) {

			itr = list_iterator_create(job_cond->cluster_list);
			while((object = list_next(itr))) {
				if(my_set)
					xstrcat(query, " || ");
				xstrfmtcat(query, "cluster='%s'", object);
				my_set = 1;
			}
			list_iterator_destroy(itr);
		}

		if(my_set)
			xstrcat(query, ") && (");

		itr = list_iterator_create(job_cond->resv_list);
		while((object = list_next(itr))) {
			/*
				DDCR: a bug arises at this point if the previous conditional statement
					  if(job_cond->cluster_list
						   && list_count(job_cond->cluster_list)) {
					  	...
					}
					is executed, because the variable 'my_set' is set to 1 and
					and we get this malformed query fragment ") && ( ||"
			*/
			if(my_set)
				xstrcat(query, " || ");
			xstrfmtcat(query, "name='%s'", object);
			my_set = 1;
		}
		list_iterator_destroy(itr);
		xstrcat(query, ")");
		if(!(result = mysql_db_query_ret(
			     mysql_conn->db_conn, query, 0))) {
			xfree(query);
			error("couldn't query the database");
			goto no_resv;
		}
		xfree(query);
		if(!job_cond->resvid_list)
			job_cond->resvid_list = list_create(slurm_destroy_char);
		while((row = mysql_fetch_row(result))) {
			list_append(job_cond->resvid_list, xstrdup(row[0]));
		}
		mysql_free_result(result);
	}
	no_resv:

	/*
	 * DDCR: this block is never executed
	 */
	if(job_cond->resvid_list && list_count(job_cond->resvid_list)) {
		set = 0;
		if(*extra)
			xstrcat(*extra, " && (");
		else
			xstrcat(*extra, " where (");
		itr = list_iterator_create(job_cond->resvid_list);
		while((object = list_next(itr))) {
			if(set)
				xstrcat(*extra, " || ");
			xstrfmtcat(*extra, "t1.resvid='%s'", object);
			set = 1;
		}
		list_iterator_destroy(itr);
		xstrcat(*extra, ")");
	}

	if(job_cond->step_list && list_count(job_cond->step_list)) {
		set = 0;
		if(*extra)
			xstrcat(*extra, " && (");
		else
			xstrcat(*extra, " where (");
		itr = list_iterator_create(job_cond->step_list);
		while((selected_step = list_next(itr))) {
			if(set)
				xstrcat(*extra, " || ");
			xstrfmtcat(*extra, "t1.jobid=%u", selected_step->jobid);
			set = 1;
		}
		list_iterator_destroy(itr);
		xstrcat(*extra, ")");
	}

#ifdef NEWQUERY
	if (!job_cond->state_list || !list_count(job_cond->state_list)) {
		/* Only do this (default of all eligible jobs) if no
		   state is given */
		if (job_cond->usage_start) {
			if (*extra)
				xstrcat(*extra, " && (");
			else
				xstrcat(*extra, " where (");

			if (!job_cond->usage_end)
				xstrfmtcat(*extra,
					   "(t1.end >= %ld "
					   "|| t1.end = 0))",
					   job_cond->usage_start);
			else
				xstrfmtcat(*extra,
					   "(t1.eligible "
					   "&& t1.eligible < %ld "
					   "&& (t1.end >= %ld "
					   "|| t1.end = 0)))",
					   job_cond->usage_end,
					   job_cond->usage_start);
		} else if (job_cond->usage_end) {
			if (*extra)
				xstrcat(*extra, " && (");
			else
				xstrcat(*extra, " where (");
			// d6d26b0 MYSQL - Don't display ineligible jobs when
			// querying for a window of time.
			xstrfmtcat(*extra,
				   "(t1.eligible && "
				   "t1.eligible < %ld))",
				   job_cond->usage_end);
		}
	}
#else
	if(job_cond->usage_start) {
		if(*extra)
			xstrcat(*extra, " && (");
		else
			xstrcat(*extra, " where (");

		if(!job_cond->usage_end)
			xstrfmtcat(*extra,
				   "t1.end >= %d || t1.end = 0)",
				   job_cond->usage_start);
		else
			xstrfmtcat(*extra,
				   "(t1.eligible < %d "
				   "&& (t1.end >= %d || t1.end = 0)))",
				   job_cond->usage_end, job_cond->usage_start);
	} else if(job_cond->usage_end) {
		if(*extra)
			xstrcat(*extra, " && (");
		else
			xstrcat(*extra, " where (");
		xstrfmtcat(*extra,
			   "(t1.eligible < %d))", job_cond->usage_end);
	}

	if(job_cond->state_list && list_count(job_cond->state_list)) {
		int base_state;
		uint32_t cur_state;
		set = 0;
		if(*extra)
			xstrcat(*extra, " && (");
		else
			xstrcat(*extra, " where (");

		itr = list_iterator_create(job_cond->state_list);
		while((object = list_next(itr))) {
			if(set)
				xstrcat(*extra, " || ");
			xstrfmtcat(*extra, "t1.state='%s'", object);
			//=========================================================
			//
			// @author debuging
			//
			cur_state = (uint32_t)strtoul(object, NULL, 10);
			base_state = cur_state & JOB_STATE_BASE;
			switch(base_state) {
			case JOB_PENDING:
				debug("JOB_PENDING (%u)", cur_state);
				break;
			case JOB_RUNNING:
				debug("JOB_RUNNING (%u)", cur_state);
				break;
			case JOB_COMPLETE:
			case JOB_CANCELLED:
			case JOB_FAILED:
			// case JOB_PREEMPTED:
			default:
				debug("JOB STATE = '%s'", job_state_string(cur_state));
				break;
			}
			//=========================================================
			set = 1;
		}
		list_iterator_destroy(itr);
		xstrcat(*extra, ")");
	}
#endif

	/* we need to put all the associations (t2) stuff together here */
	if(job_cond->cluster_list && list_count(job_cond->cluster_list)) {
		set = 0;
		if(*extra)
			xstrcat(*extra, " && (");
		else
			xstrcat(*extra, " where (");

		itr = list_iterator_create(job_cond->cluster_list);
		while((object = list_next(itr))) {
			if(set) 
				xstrcat(*extra, " || ");
			xstrfmtcat(*extra, 
				   "(t1.cluster='%s' || %s.cluster='%s')", 
				   object, table_level, object);
			set = 1;
		}
		list_iterator_destroy(itr);
		xstrcat(*extra, ")");
	} 

	if(job_cond->wckey_list && list_count(job_cond->wckey_list)) {
		set = 0;
		if(*extra)
			xstrcat(*extra, " && (");
		else
			xstrcat(*extra, " where (");

		itr = list_iterator_create(job_cond->wckey_list);
		while((object = list_next(itr))) {
			if(set) 
				xstrcat(*extra, " || ");
			xstrfmtcat(*extra, "t1.wckey='%s'", object);
			set = 1;
		}
		list_iterator_destroy(itr);
		xstrcat(*extra, ")");
	}
	return set;
}

extern List mysql_jobacct_process_get_jobs(mysql_conn_t *mysql_conn, uid_t uid,
					   acct_job_cond_t *job_cond)
{

	char *query = NULL;
	char *extra = NULL;
	char *tmp = NULL;
	jobacct_selected_step_t *selected_step = NULL;
	ListIterator itr = NULL;
	int set = 0, is_admin=1;
	char *table_level="t2";
	MYSQL_RES *result = NULL, *step_result = NULL;
	MYSQL_ROW row, step_row;
	int i, last_id = -1, curr_id = -1;
	jobacct_job_rec_t *job = NULL;
	jobacct_step_rec_t *step = NULL;
	time_t now = time(NULL);
	List job_list = list_create(destroy_jobacct_job_rec);
	uint16_t private_data = 0;
	acct_user_rec_t user;
	local_cluster_t *curr_cluster = NULL;
	List local_cluster_list = NULL;
	DEF_TIMERS_DDCR;
	// unsigned int num_fields;
	char *last_state = NULL, *curr_state = NULL;
	// char c;

	/* if this changes you will need to edit the corresponding 
	 * enum below also t1 is job_table */
	char *job_req_inx[] = {
		"t1.id",
		"t1.jobid",
		"t1.associd",
		"t1.wckey",
		"t1.wckeyid",
		"t1.uid",
		"t1.gid",
		"t1.resvid",
#ifdef NEWQUERY
		"t3.name",
#endif
		"t1.partition",
		"t1.blockid",
		"t1.cluster",
		"t1.account",
		"t1.eligible",
		"t1.submit",
		"t1.start",
		"t1.end",
		"t1.suspended",
#ifdef NEWQUERY
		"t1.timelimit",
#endif
		"t1.name",
		"t1.track_steps",
		"t1.state",
		"t1.comp_code",
		"t1.priority",
		"t1.req_cpus",
		"t1.alloc_cpus",
		"t1.alloc_nodes",
		"t1.nodelist",
		"t1.node_inx",
		"t1.kill_requid",
		"t1.qos",
		"t2.user",
		"t2.cluster",
		"t2.acct",
		"t2.lft"
	};

	enum {
		JOB_REQ_ID,
		JOB_REQ_JOBID,
		JOB_REQ_ASSOCID,
		JOB_REQ_WCKEY,
		JOB_REQ_WCKEYID,
		JOB_REQ_UID,
		JOB_REQ_GID,
		JOB_REQ_RESVID,
#ifdef NEWQUERY
		JOB_REQ_RESV_NAME,
#endif
		JOB_REQ_PARTITION,
		JOB_REQ_BLOCKID,
		JOB_REQ_CLUSTER1,
		JOB_REQ_ACCOUNT1,
		JOB_REQ_ELIGIBLE,
		JOB_REQ_SUBMIT,
		JOB_REQ_START,
		JOB_REQ_END,
		JOB_REQ_SUSPENDED,
#ifdef NEWQUERY
		JOB_REQ_TIMELIMIT,
#endif
		JOB_REQ_NAME,
		JOB_REQ_TRACKSTEPS,
		JOB_REQ_STATE,
		JOB_REQ_COMP_CODE,
		JOB_REQ_PRIORITY,
		JOB_REQ_REQ_CPUS,
		JOB_REQ_ALLOC_CPUS,
		JOB_REQ_ALLOC_NODES,
		JOB_REQ_NODELIST,
		JOB_REQ_NODE_INX,
		JOB_REQ_KILL_REQUID,
		JOB_REQ_QOS,
		JOB_REQ_USER_NAME,
		JOB_REQ_CLUSTER,
		JOB_REQ_ACCOUNT,
		JOB_REQ_LFT,
		JOB_REQ_COUNT
	};

	/* if this changes you will need to edit the corresponding 
	 * enum below also t1 is step_table */
	char *step_req_inx[] = {
		"t1.stepid",
		"t1.start",
		"t1.end",
		"t1.suspended",
		"t1.name",
		"t1.nodelist",
		"t1.node_inx",
		"t1.state",
		"t1.kill_requid",
		"t1.comp_code",
		"t1.nodes",
		"t1.cpus",
		"t1.tasks",
		"t1.task_dist",
		"t1.user_sec",
		"t1.user_usec",
		"t1.sys_sec",
		"t1.sys_usec",
		"t1.max_vsize",
		"t1.max_vsize_task",
		"t1.max_vsize_node",
		"t1.ave_vsize",
		"t1.max_rss",
		"t1.max_rss_task",
		"t1.max_rss_node",
		"t1.ave_rss",
		"t1.max_pages",
		"t1.max_pages_task",
		"t1.max_pages_node",
		"t1.ave_pages",
		"t1.min_cpu",
		"t1.min_cpu_task",
		"t1.min_cpu_node",
		"t1.ave_cpu"
	};

	enum {
		STEP_REQ_STEPID,
		STEP_REQ_START,
		STEP_REQ_END,
		STEP_REQ_SUSPENDED,
		STEP_REQ_NAME,
		STEP_REQ_NODELIST,
		STEP_REQ_NODE_INX,
		STEP_REQ_STATE,
		STEP_REQ_KILL_REQUID,
		STEP_REQ_COMP_CODE,
		STEP_REQ_NODES,
		STEP_REQ_CPUS,
		STEP_REQ_TASKS,
		STEP_REQ_TASKDIST,
		STEP_REQ_USER_SEC,
		STEP_REQ_USER_USEC,
		STEP_REQ_SYS_SEC,
		STEP_REQ_SYS_USEC,
		STEP_REQ_MAX_VSIZE,
		STEP_REQ_MAX_VSIZE_TASK,
		STEP_REQ_MAX_VSIZE_NODE,
		STEP_REQ_AVE_VSIZE,
		STEP_REQ_MAX_RSS,
		STEP_REQ_MAX_RSS_TASK,
		STEP_REQ_MAX_RSS_NODE,
		STEP_REQ_AVE_RSS,
		STEP_REQ_MAX_PAGES,
		STEP_REQ_MAX_PAGES_TASK,
		STEP_REQ_MAX_PAGES_NODE,
		STEP_REQ_AVE_PAGES,
		STEP_REQ_MIN_CPU,
		STEP_REQ_MIN_CPU_TASK,
		STEP_REQ_MIN_CPU_NODE,
		STEP_REQ_AVE_CPU,
		STEP_REQ_COUNT
	};

	memset(&user, 0, sizeof(acct_user_rec_t));
	user.uid = uid;

	private_data = slurm_get_private_data();
	if (private_data & PRIVATE_DATA_JOBS) {
		/* This only works when running though the slurmdbd.
		 * THERE IS NO AUTHENTICATION WHEN RUNNNING OUT OF THE
		 * SLURMDBD!
		 */
		if(slurmdbd_conf) {
			is_admin = 0;
			/* we have to check the authentication here in the
			 * plugin since we don't know what accounts are being
			 * referenced until after the query.  Here we will
			 * set if they are an operator or greater and then
			 * check it below after the query.
			 */
			if((uid == slurmdbd_conf->slurm_user_id || uid == 0)
			   || assoc_mgr_get_admin_level(mysql_conn, uid)
			   >= ACCT_ADMIN_OPERATOR)
				is_admin = 1;
			else {
				assoc_mgr_fill_in_user(mysql_conn, &user, 1,
						       NULL);
			}
		}
	}


	/* Here we set up environment to check used nodes of jobs.
	   Since we store the bitmap of the entire cluster we can use
	   that to set up a hostlist and set up the bitmap to make
	   things work.  This should go before the setup of conds
	   since we could update the start/end time.
	*/
	if(job_cond && job_cond->used_nodes) {
		debug4("DDCR: entering setup_cluster_list_with_inx");
		local_cluster_list = setup_cluster_list_with_inx(
			mysql_conn, job_cond, (void **)&curr_cluster);
		if(!local_cluster_list) {
			list_destroy(job_list);
			return NULL;
		}
	}

	setup_job_cond_limits(mysql_conn, job_cond, &extra);

	xfree(tmp);
	xstrfmtcat(tmp, "%s", job_req_inx[0]);
	for(i=1; i<JOB_REQ_COUNT; i++) {
		xstrfmtcat(tmp, ", %s", job_req_inx[i]);
	}

	/* This is here to make sure we are looking at only this user
	 * if this flag is set.  We also include any accounts they may be
	 * coordinator of.
	 */
	debug3("mysql_jobacct_process_get_jobs (%d): is_admin=%u", __LINE__,
		is_admin);
	debug3("mysql_jobacct_process_get_jobs (%d): private_data & PRIVATE_DATA_JOBS = %u",
		__LINE__, (private_data & PRIVATE_DATA_JOBS));

	if(!is_admin && (private_data & PRIVATE_DATA_JOBS)) {
		query = xstrdup_printf("select lft from %s where user='%s'", 
				       assoc_table, user.name);
		if(user.coord_accts) {
			acct_coord_rec_t *coord = NULL;
			itr = list_iterator_create(user.coord_accts);
			while((coord = list_next(itr))) {
				xstrfmtcat(query, " || acct='%s'",
					   coord->name);
			}
			list_iterator_destroy(itr);
		}
		debug3("%d(%d) query\n%s", mysql_conn->conn, __LINE__, query);
		if(!(result = mysql_db_query_ret(
			     mysql_conn->db_conn, query, 0))) {
			xfree(extra);
			xfree(query);
			list_destroy(job_list);
			if(local_cluster_list)
				list_destroy(local_cluster_list);
			return NULL;
		}
		xfree(query);
		set = 0;
		while((row = mysql_fetch_row(result))) {
			if(set) {
				xstrfmtcat(extra,
					   " || (%s between %s.lft and %s.rgt)",
					   row[0], table_level, table_level);
			} else {
				set = 1;
				if(extra)
					xstrfmtcat(extra,
						   " && ((%s between %s.lft "
						   "and %s.rgt)",
						   row[0], table_level,
						   table_level);
				else
					xstrfmtcat(extra, 
						   " where ((%s between %s.lft "
						   "and %s.rgt)",
						   row[0], table_level,
						   table_level);
			}
		}
		if(set)
			xstrcat(extra,")");
		mysql_free_result(result);
	}
#ifdef NEWQUERY
	// setup_job_cluster_cond_limits(mysql_conn, job_cond, &extra);
	if(job_cond->state_list && list_count(job_cond->state_list)) {
		char *object = NULL;
		set = 0;
		if(extra)
			xstrcat(extra, " && (");
		else
			xstrcat(extra, " where (");
		itr = list_iterator_create(job_cond->state_list);
		while((object = list_next(itr))) {
			if(set)
				xstrcat(extra, " || ");
			// xstrfmtcat(*extra, "t1.state='%s'", object);
			_state_time_string(&extra, (uint32_t)strtoul(object, NULL, 10),
				job_cond->usage_start, job_cond->usage_end);
			set = 1;
		}
		list_iterator_destroy(itr);
		xstrcat(extra, ")");
	}

// 201d691 MySQL - Fix querying jobs with reservations when
// the id's have rolled.
	query = xstrdup_printf("select %s from %s as t1 "
			       "left join %s as t2 "
			       "on t1.associd=t2.id "
			       "left join %s as t3 "
			       "on t1.resvid=t3.id && "
			       "((t1.start && "
			       "(t3.start < t1.start && "
			       "(t3.end >= t1.start || "
			       "t3.end = 0))) || "
			       "((t3.start < t1.submit && "
			       "(t3.end >= t1.submit || "
			       "t3.end = 0)) || "
			       "(t3.start > t1.submit)))",
			       tmp, job_table, assoc_table, resv_table);
#else
	query = xstrdup_printf("select %s from %s as t1 left join %s as t2 "
			       "on t1.associd=t2.id",
			       tmp, job_table, assoc_table);
#endif
	xfree(tmp);
	if(extra) {
		xstrcat(query, extra);
		xfree(extra);
	}

	/* Here we want to order them this way in such a way so it is
	   easy to look for duplicates
	*/
#ifdef NEWQUERY
	xstrcat(query, " group by jobid, submit desc");
#else
	if(job_cond && !job_cond->duplicates) 
		xstrcat(query, " order by t1.cluster, jobid, submit desc");
	else
		xstrcat(query, " order by t1.cluster, submit desc");
#endif

	debug3("%d(%d) query\n%s", mysql_conn->conn, __LINE__, query);
	// debug3("DDCR: Change this line (%d)", __LINE__);

	// debug("DDCR> pause ... Enter character:");
	// c = getchar();
	// debug("Restart");

	// START_TIMER;
	if(!(result = mysql_db_query_ret(mysql_conn->db_conn, query, 0))) {
		xfree(query);
		list_destroy(job_list);
		if(local_cluster_list)
			list_destroy(local_cluster_list);
		return NULL;
	}
	xfree(query);
	// END_TIMER2("querying <job_table>");

	// debug("DDCR> pause ... Enter character:");
	// c = getchar();
	// debug("Restart");

	START_TIMER_DDCR;
	while((row = mysql_fetch_row(result))) {
		char *id = row[JOB_REQ_ID];
		bool job_ended = 0;
#ifdef NEWQUERY
		int start = atoi(row[JOB_REQ_START]);
#endif
		int submit = atoi(row[JOB_REQ_SUBMIT]);

		curr_id = atoi(row[JOB_REQ_JOBID]);

		if(job_cond && !job_cond->duplicates && curr_id == last_id) {
			curr_state = job_state_string(atoi(row[JOB_REQ_STATE]));
			// debug4("%d: %s", last_id, last_state);
			debug4("RR %d: CURR/PREVIOUS STATES=%s %s", curr_id, 
				curr_state, last_state);
			// debug4("================================================");
			continue;
		}
		last_state = curr_state;

#ifdef NEWQUERY
		if (!good_nodes_from_inx(local_cluster_list,
					 (void **)&curr_cluster,
					 row[JOB_REQ_NODE_INX], start)) {
			debug4("DDCR: No good_nodes_from_inx: %s", row[JOB_REQ_NODE_INX]);
			last_id = curr_id;
			continue;
		}
#else
		last_id = curr_id;

		/* check the bitmap to see if this is one of the jobs
		   we are looking for */
		if(!good_nodes_from_inx(local_cluster_list,
					(void **)&curr_cluster,
					row[JOB_REQ_NODE_INX], submit)){
			debug4("DDCR: No good_nodes_from_inx: %s", row[JOB_REQ_NODE_INX]);
			continue;
		}
#endif

		job = create_jobacct_job_rec();
#ifdef NEWQUERY
		if (curr_id == last_id)
			/* put in reverse so we order by the submit getting
			   larger which it is given to us in reverse
			   order from the database */
			list_prepend(job_list, job);
		else
			list_append(job_list, job);
		last_id = curr_id;
		job->alloc_cpus = atoi(row[JOB_REQ_ALLOC_CPUS]);  /* 17 */
#else
		list_append(job_list, job);
		job->alloc_cpus = atoi(row[JOB_REQ_ALLOC_CPUS]);  /* 17 */
#endif
		job->alloc_nodes = atoi(row[JOB_REQ_ALLOC_NODES]);  /* 16 */
		job->associd = atoi(row[JOB_REQ_ASSOCID]);
		job->resvid = atoi(row[JOB_REQ_RESVID]);

		/* we want a blank wckey if the name is null */
		if(row[JOB_REQ_WCKEY])
			job->wckey = xstrdup(row[JOB_REQ_WCKEY]);
		else
			job->wckey = xstrdup("");
		job->wckeyid = atoi(row[JOB_REQ_WCKEYID]);

		if(row[JOB_REQ_CLUSTER] && row[JOB_REQ_CLUSTER][0])
			job->cluster = xstrdup(row[JOB_REQ_CLUSTER]);           /* 2 */
		else if(row[JOB_REQ_CLUSTER1] && row[JOB_REQ_CLUSTER1][0])
			job->cluster = xstrdup(row[JOB_REQ_CLUSTER1]);          /* 2 */

		if(row[JOB_REQ_USER_NAME])
			job->user = xstrdup(row[JOB_REQ_USER_NAME]);            /* 7 */
		else
			job->uid = atoi(row[JOB_REQ_UID]);                      /* 8 */

		if(row[JOB_REQ_LFT])
			job->lft = atoi(row[JOB_REQ_LFT]);

		if(row[JOB_REQ_ACCOUNT] && row[JOB_REQ_ACCOUNT][0])
			job->account = xstrdup(row[JOB_REQ_ACCOUNT]);           /* 4 */
		else if(row[JOB_REQ_ACCOUNT1] && row[JOB_REQ_ACCOUNT1][0])
			job->account = xstrdup(row[JOB_REQ_ACCOUNT1]);          /* 4 */

		if(row[JOB_REQ_BLOCKID])
			job->blockid = xstrdup(row[JOB_REQ_BLOCKID]);

		job->eligible = atoi(row[JOB_REQ_ELIGIBLE]);                /* 10 */
		job->submit = submit;                                       /* 9 */
		job->start = atoi(row[JOB_REQ_START]);                      /* 11 */
		job->end = atoi(row[JOB_REQ_END]);                          /* 12 */
#ifdef NEWQUERY
		job->timelimit = atoi(row[JOB_REQ_TIMELIMIT]);
#endif

		/* since the job->end could be set later end it here */
		if(job->end) {
			job_ended = 1;
			if(!job->start || (job->start > job->end))
				job->start = job->end;
		}

		/* below is the calculation of job->elapsed   13 */
		if(job_cond && !job_cond->without_usage_truncation
		   && job_cond->usage_start) {
			if(job->start && (job->start < job_cond->usage_start))
				job->start = job_cond->usage_start;

			if(!job->end || job->end > job_cond->usage_end)
				job->end = job_cond->usage_end;

			if(!job->start)
				job->start = job->end;

			job->elapsed = job->end - job->start;

			if(row[JOB_REQ_SUSPENDED]) {
				MYSQL_RES *result2 = NULL;
				MYSQL_ROW row2;
				/* get the suspended time for this job */
				query = xstrdup_printf(
					"select start, end from %s where "
					"(start < %d && (end >= %d "
					"|| end = 0)) && id=%s "
					"order by start",
					suspend_table,
					job_cond->usage_end,
					job_cond->usage_start,
					id);

				debug4("%d(%d) query\n%s",
				       mysql_conn->conn, __LINE__, query);
				if(!(result2 = mysql_db_query_ret(
					     mysql_conn->db_conn,
					     query, 0))) {
					list_destroy(job_list);
					job_list = NULL;
					break;
				}
				xfree(query);
				while((row2 = mysql_fetch_row(result2))) {
					int local_start =
						atoi(row2[0]);
					int local_end =
						atoi(row2[1]);

					if(!local_start)
						continue;

					if(job->start > local_start)
						local_start = job->start;
					if(job->end < local_end)
						local_end = job->end;

					if((local_end - local_start) < 1)
						continue;

					job->elapsed -=
						(local_end - local_start);
					job->suspended +=
						(local_end - local_start);
				}
				mysql_free_result(result2);
			}
			/* test> seems this branch is never reached */
			debug("Calculation of job elapsed time with truncation [%d]\n", 
				curr_id);
		} else {
			job->suspended = atoi(row[JOB_REQ_SUSPENDED]);
#ifdef NEWQUERY
			if (job->state == JOB_SUSPENDED)
				job->suspended = now - job->suspended;
#endif
			if(!job->start) {
				job->elapsed = 0;
			} else if(!job->end) {
				job->elapsed = now - job->start;
			} else {
				job->elapsed = job->end - job->start;
			}

			job->elapsed -= job->suspended;
		}

		if((int)job->elapsed < 0)
			job->elapsed = 0;

		job->jobid = curr_id;                                /* 1 */
		job->jobname = xstrdup(row[JOB_REQ_NAME]);           /* 21 */
		job->gid = atoi(row[JOB_REQ_GID]);                   /* 6: need to check 5 */
		job->exitcode = atoi(row[JOB_REQ_COMP_CODE]);        /* 14 */

		if(row[JOB_REQ_PARTITION])
			job->partition = xstrdup(row[JOB_REQ_PARTITION]); /* 3 */

		if(row[JOB_REQ_NODELIST])
			job->nodes = xstrdup(row[JOB_REQ_NODELIST]);      /* 20 */

		if (!job->nodes || !strcmp(job->nodes, "(null)")) {
			xfree(job->nodes);
			job->nodes = xstrdup("(unknown)");
		}

		job->track_steps = atoi(row[JOB_REQ_TRACKSTEPS]);
		job->state = atoi(row[JOB_REQ_STATE]);                /* 15 */
		job->priority = atoi(row[JOB_REQ_PRIORITY]);
		job->req_cpus = atoi(row[JOB_REQ_REQ_CPUS]);          /* 18 */
// if (row[JOB_REQ_GRES_REQ])
//     job->req_gres = xstrdup(row[JOB_REQ_GRES_REQ]);
// else
//     job->req_gres = xstrdup("");
//job->req_mem = slurm_atoul(row[JOB_REQ_REQ_MEM]);
		job->requid = atoi(row[JOB_REQ_KILL_REQUID]);
		job->qos = atoi(row[JOB_REQ_QOS]);
		job->show_full = 1;

		/* If I am looking for specific steps */
		if(job_cond && job_cond->step_list
		   && list_count(job_cond->step_list)) {
			set = 0;
			itr = list_iterator_create(job_cond->step_list);
			while((selected_step = list_next(itr))) {
				if(selected_step->jobid != job->jobid) {
					continue;
				} else if (selected_step->stepid
					   == (uint32_t)NO_VAL) {
					job->show_full = 1;
					break;
#ifdef NEWQUERY
				} else if (selected_step->stepid == INFINITE)
					selected_step->stepid =
						SLURM_BATCH_SCRIPT;
#else
			    }
#endif

				if(set)
					xstrcat(extra, " || ");
				else
					xstrcat(extra, " && (");

				xstrfmtcat(extra, "t1.stepid=%u",
					   selected_step->stepid);
				set = 1;
				job->show_full = 0;
#ifdef NEWQUERY
				/* Set it back just in case we are
				   looking at a job array.
				*/
				if (selected_step->stepid == SLURM_BATCH_SCRIPT)
					selected_step->stepid = INFINITE;
#endif
			}
			list_iterator_destroy(itr);
			if(set)
				xstrcat(extra, ")");
		   	info("DDCR: Step list was specified\n");
		   	debug3("%d(%d) extra for step job query\n%s", mysql_conn->conn, __LINE__, extra);
		}

		/* 
			DDCR:
			In recent SLURM versions the buildup of the `*tmp` string
			(see below) is done outside the main while loop
		 */
		/* build query string for job steps */
		for(i=0; i<STEP_REQ_COUNT; i++) {
			if(i) 
				xstrcat(tmp, ", ");
			xstrcat(tmp, step_req_inx[i]);
		}
		query =	xstrdup_printf("select %s from %s as t1 where t1.id=%s",
				       tmp, step_table, id);
		xfree(tmp);

		/* a string "extra" vem da condicional 
		   if(job_cond && job_cond->step_list [...] */
		if(extra) {
			xstrcat(query, extra);
			xfree(extra);
		}

		// debug4("query = %s", query);

		if(!(step_result = mysql_db_query_ret(
			     mysql_conn->db_conn, query, 0))) {
			xfree(query);
			list_destroy(job_list);
			if(local_cluster_list)
				list_destroy(local_cluster_list);
			// debug4("SAIU (nao tem step jobs)>> t1.id=%s (%d)\n", id, curr_id);
			return NULL;
		}
		xfree(query);

		/* Querying the steps in the fashion was faster than
		   doing only 1 query and then matching the steps up
		   later with the job.
		*/
		// info("NOTE:========== new job (db_indx=%s [SLURM ID=%u]) ==========\n", id, curr_id);
		while ((step_row = mysql_fetch_row(step_result))) {
			/* check the bitmap to see if this is one of the steps
			   we are looking for */
			// debug4("START step job listing>> t1.id=%s (%d)\n", id, curr_id);
			if(!good_nodes_from_inx(local_cluster_list,
						(void **)&curr_cluster,
						step_row[STEP_REQ_NODE_INX],
#ifdef NEWQUERY
						start))
#else
						submit))
#endif
				continue;

			step = create_jobacct_step_rec();
#ifdef NEWQUERY
			step->tot_cpu_sec = 0;
			step->tot_cpu_usec = 0;
#endif
			step->job_ptr = job;
			if(!job->first_step_ptr)
				job->first_step_ptr = step;
			list_append(job->steps, step);
			step->stepid = atoi(step_row[STEP_REQ_STEPID]);

			step->state = atoi(step_row[STEP_REQ_STATE]);
			step->exitcode = atoi(step_row[STEP_REQ_COMP_CODE]);
			step->ncpus = atoi(step_row[STEP_REQ_CPUS]);
			step->nnodes = atoi(step_row[STEP_REQ_NODES]);

			step->ntasks = atoi(step_row[STEP_REQ_TASKS]);
			step->task_dist = atoi(step_row[STEP_REQ_TASKDIST]);
			if(!step->ntasks)
				step->ntasks = step->ncpus;

			step->start = atoi(step_row[STEP_REQ_START]);

			step->end = atoi(step_row[STEP_REQ_END]);

			// NOTE: CORRECTIONS ACCORDING TO fix_sacct_db.pl
			if(!step->start)
				debug4("STEP: NO TIME_START>> %u.%u: time_start=%d (%s)\n",
					job->jobid, step->stepid, step->start,
					job_state_string(step->state));

			/* assume step->start is nonzero */
			if(!step->end) {
				query =	xstrdup_printf(
					"UPDATE %s SET end=%d "
					"where id=%s and stepid=%u",
					step_table, step->start,
					id, step->stepid);
				// debug4("NO TIME_END>> step %u.%u: time_end=%d\n  %s", 
				// 	job->jobid, 
				// 	step->stepid,
				// 	step->end, 
				// 	query);
				debug4("STEP: NO TIME_END>> %u.%u: time_end=%d (%s)\n", 
					job->jobid, 
					step->stepid,
					job_state_string(step->state));
				xfree(query);
			}

			if (!step->start & !step->end)
				debug4("STEP: NO TIME_START and TIME_END>> %u.%u: time_start=%d, time_end=%d (%s)\n",
					job->jobid,
					step->stepid,
					step->start, 
					step->end,
					job_state_string(step->state));

			// NOTE: FIX STEP STATE
			// if (step->state == JOB_PENDING || step->state == JOB_RUNNING ) {
			// 	debug4('%s: time_start=%d, time_end=%d', 
			// 			job_state_string(step->state),
			// 			step->start, step->end);
			// }

			/* if the job has ended end the step also */
			if(!step->end && job_ended) {
				step->end = job->end;
				step->state = job->state;
			}

			if(job_cond && !job_cond->without_usage_truncation
			   && job_cond->usage_start) {
				if(step->start
				   && (step->start < job_cond->usage_start))
					step->start = job_cond->usage_start;

				if(!step->start && step->end)
					step->start = step->end;

				if(!step->end
				   || (step->end > job_cond->usage_end))
					step->end = job_cond->usage_end;
			}

			/* figure this out by start stop */
			step->suspended = atoi(step_row[STEP_REQ_SUSPENDED]);
#ifdef NEWQUERY
			if (!step->start) {
				step->elapsed = 0;
			} else if (!step->end) {
#else
			if(!step->end) {
#endif
				// NOTE: DDCR: better put step->elapsed = 0
				step->elapsed = now - step->start;
				// info("DDCR: step %u.%u");
			} else {
				step->elapsed = step->end - step->start;
			}
			step->elapsed -= step->suspended;

			if((int)step->elapsed < 0)
				step->elapsed = 0;

#ifdef NEWQUERY
			/*NOTE: DDCR Restored this block to the same ordering as in recent versions */
			step->stepname = xstrdup(step_row[STEP_REQ_NAME]);
			step->nodes = xstrdup(step_row[STEP_REQ_NODELIST]);
			step->requid = atoi(step_row[STEP_REQ_KILL_REQUID]);

			step->sacct.min_cpu =
				atof(step_row[STEP_REQ_MIN_CPU]);

			if(step->sacct.min_cpu != NO_VAL) {
				step->user_cpu_sec = atoi(step_row[STEP_REQ_USER_SEC]);
				step->user_cpu_usec =
					atoi(step_row[STEP_REQ_USER_USEC]);
				step->sys_cpu_sec = atoi(step_row[STEP_REQ_SYS_SEC]);
				step->sys_cpu_usec = atoi(step_row[STEP_REQ_SYS_USEC]);
				step->tot_cpu_sec += step->user_cpu_sec + step->sys_cpu_sec;
				step->tot_cpu_usec += step->user_cpu_usec + step->sys_cpu_usec;
				step->sacct.max_vsize =
					atoi(step_row[STEP_REQ_MAX_VSIZE]);
				step->sacct.max_vsize_id.taskid = 
					atoi(step_row[STEP_REQ_MAX_VSIZE_TASK]);
				step->sacct.ave_vsize = 
					atof(step_row[STEP_REQ_AVE_VSIZE]);
				step->sacct.max_rss =
					atoi(step_row[STEP_REQ_MAX_RSS]);
				step->sacct.max_rss_id.taskid = 
					atoi(step_row[STEP_REQ_MAX_RSS_TASK]);
				step->sacct.ave_rss = 
					atof(step_row[STEP_REQ_AVE_RSS]);
				step->sacct.max_pages =
					atoi(step_row[STEP_REQ_MAX_PAGES]);
				step->sacct.max_pages_id.taskid = 
					atoi(step_row[STEP_REQ_MAX_PAGES_TASK]);
				step->sacct.ave_pages =
					atof(step_row[STEP_REQ_AVE_PAGES]);
				step->sacct.min_cpu_id.taskid = 
					atoi(step_row[STEP_REQ_MIN_CPU_TASK]);
				step->sacct.ave_cpu = atof(step_row[STEP_REQ_AVE_CPU]);
				step->sacct.max_vsize_id.nodeid = 
					atoi(step_row[STEP_REQ_MAX_VSIZE_NODE]);
				step->sacct.max_rss_id.nodeid = 
					atoi(step_row[STEP_REQ_MAX_RSS_NODE]);
				step->sacct.max_pages_id.nodeid = 
					atoi(step_row[STEP_REQ_MAX_PAGES_NODE]);
				step->sacct.min_cpu_id.nodeid = 
					atoi(step_row[STEP_REQ_MIN_CPU_NODE]);
			}
#else
			step->user_cpu_sec = atoi(step_row[STEP_REQ_USER_SEC]);
			step->user_cpu_usec =
				atoi(step_row[STEP_REQ_USER_USEC]);
			step->sys_cpu_sec = atoi(step_row[STEP_REQ_SYS_SEC]);
			step->sys_cpu_usec = atoi(step_row[STEP_REQ_SYS_USEC]);
			job->tot_cpu_sec += 
				step->tot_cpu_sec += 
				step->user_cpu_sec + step->sys_cpu_sec;
			job->tot_cpu_usec += 
				step->tot_cpu_usec += 
				step->user_cpu_usec + step->sys_cpu_usec;
			step->sacct.max_vsize =
				atoi(step_row[STEP_REQ_MAX_VSIZE]);
			step->sacct.max_vsize_id.taskid = 
				atoi(step_row[STEP_REQ_MAX_VSIZE_TASK]);
			step->sacct.ave_vsize = 
				atof(step_row[STEP_REQ_AVE_VSIZE]);
			step->sacct.max_rss =
				atoi(step_row[STEP_REQ_MAX_RSS]);
			step->sacct.max_rss_id.taskid = 
				atoi(step_row[STEP_REQ_MAX_RSS_TASK]);
			step->sacct.ave_rss = 
				atof(step_row[STEP_REQ_AVE_RSS]);
			step->sacct.max_pages =
				atoi(step_row[STEP_REQ_MAX_PAGES]);
			step->sacct.max_pages_id.taskid = 
				atoi(step_row[STEP_REQ_MAX_PAGES_TASK]);
			step->sacct.ave_pages =
				atof(step_row[STEP_REQ_AVE_PAGES]);
			step->sacct.min_cpu =
				atof(step_row[STEP_REQ_MIN_CPU]);
			step->sacct.min_cpu_id.taskid = 
				atoi(step_row[STEP_REQ_MIN_CPU_TASK]);
			step->sacct.ave_cpu = atof(step_row[STEP_REQ_AVE_CPU]);
			step->stepname = xstrdup(step_row[STEP_REQ_NAME]);
			step->nodes = xstrdup(step_row[STEP_REQ_NODELIST]);
			step->sacct.max_vsize_id.nodeid = 
				atoi(step_row[STEP_REQ_MAX_VSIZE_NODE]);
			step->sacct.max_rss_id.nodeid = 
				atoi(step_row[STEP_REQ_MAX_RSS_NODE]);
			step->sacct.max_pages_id.nodeid = 
				atoi(step_row[STEP_REQ_MAX_PAGES_NODE]);
			step->sacct.min_cpu_id.nodeid = 
				atoi(step_row[STEP_REQ_MIN_CPU_NODE]);
			step->requid = atoi(step_row[STEP_REQ_KILL_REQUID]);
#endif

/*			info("got step %u.%u: %d += %d+%d",
	 			     job->jobid,
	 			     step->stepid,
	 			     step->tot_cpu_sec,
	 			     step->user_cpu_sec,
	 			     step->sys_cpu_sec);
*/
		}
		mysql_free_result(step_result);

		if(!job->track_steps) {
			/* If we don't have track_steps we want to see
			   if we have multiple steps.  If we only have
			   1 step check the job name against the step
			   name in most all cases it will be
			   different.  If it is different print out
			   the step separate.
			*/
			if(list_count(job->steps) > 1)
				job->track_steps = 1;
			else if(step && step->stepname && job->jobname) {
				if(strcmp(step->stepname, job->jobname))
					job->track_steps = 1;
			}
			// info("JOBID=%d: no track_steps [%d]", curr_id,
			// 	job->track_steps);
		}
		/* need to reset here to make the above test valid */
		step = NULL;
	}
	mysql_free_result(result);
	END_TIMER2_DDCR("END LOOP");

	if(local_cluster_list)
		list_destroy(local_cluster_list);

	return job_list;
}

extern int mysql_jobacct_process_archive(mysql_conn_t *mysql_conn,
					 acct_archive_cond_t *arch_cond)
{
	int rc = SLURM_SUCCESS;
	char *query = NULL;
	time_t last_submit = time(NULL);
	time_t curr_end;
	char *tmp = NULL;
	int i=0;
	struct tm time_tm;

//	DEF_TIMERS;

	/* if this changes you will need to edit the corresponding 
	 * enum below */
	char *event_req_inx[] = {
		"node_name",
		"cluster",
		"cpu_count",
		"state",
		"period_start",
		"period_end",
		"reason",
		"cluster_nodes",
	};

	/* if this changes you will need to edit the corresponding 
	 * enum below */
	char *job_req_inx[] = {
		"id",
		"jobid",
		"associd",
		"wckey",
		"wckeyid",
		"uid",
		"gid",
		"resvid",
		"partition",
		"blockid",
		"cluster",
		"account",
		"eligible",
		"submit",
		"start",
		"end",
		"suspended",
		"name",
		"track_steps",
		"state",
		"comp_code",
		"priority",
		"req_cpus",
		"alloc_cpus",
		"alloc_nodes",
		"nodelist",
		"node_inx",
		"kill_requid",
		"qos"
	};

	/* if this changes you will need to edit the corresponding 
	 * enum below */
	char *step_req_inx[] = {
		"id",
		"stepid",
		"start",
		"end",
		"suspended",
		"name",
		"nodelist",
		"node_inx",
		"state",
		"kill_requid",
		"comp_code",
		"nodes",
		"cpus",
		"tasks",
		"task_dist",
		"user_sec",
		"user_usec",
		"sys_sec",
		"sys_usec",
		"max_vsize",
		"max_vsize_task",
		"max_vsize_node",
		"ave_vsize",
		"max_rss",
		"max_rss_task",
		"max_rss_node",
		"ave_rss",
		"max_pages",
		"max_pages_task",
		"max_pages_node",
		"ave_pages",
		"min_cpu",
		"min_cpu_task",
		"min_cpu_node",
		"ave_cpu"
	};


	/* if this changes you will need to edit the corresponding 
	 * enum below */
	char *suspend_req_inx[] = {
		"id",
		"associd",
		"start",
		"end",
	};

	enum {
		EVENT_REQ_NODE,
		EVENT_REQ_CLUSTER,
		EVENT_REQ_CPUS,
		EVENT_REQ_STATE,
		EVENT_REQ_START,
		EVENT_REQ_END,
		EVENT_REQ_REASON,
		EVENT_REQ_NODES,
		EVENT_REQ_COUNT
	};

	enum {
		JOB_REQ_ID,
		JOB_REQ_JOBID,
		JOB_REQ_ASSOCID,
		JOB_REQ_WCKEY,
		JOB_REQ_WCKEYID,
		JOB_REQ_UID,
		JOB_REQ_GID,
		JOB_REQ_RESVID,
		JOB_REQ_PARTITION,
		JOB_REQ_BLOCKID,
		JOB_REQ_CLUSTER,
		JOB_REQ_ACCOUNT,
		JOB_REQ_ELIGIBLE,
		JOB_REQ_SUBMIT,
		JOB_REQ_START,
		JOB_REQ_END,
		JOB_REQ_SUSPENDED,
		JOB_REQ_NAME,
		JOB_REQ_TRACKSTEPS,
		JOB_REQ_STATE,
		JOB_REQ_COMP_CODE,
		JOB_REQ_PRIORITY,
		JOB_REQ_REQ_CPUS,
		JOB_REQ_ALLOC_CPUS,
		JOB_REQ_ALLOC_NODES,
		JOB_REQ_NODELIST,
		JOB_REQ_NODE_INX,
		JOB_REQ_KILL_REQUID,
		JOB_REQ_QOS,
		JOB_REQ_COUNT		
	};

	enum {
		STEP_REQ_ID,
		STEP_REQ_STEPID,
		STEP_REQ_START,
		STEP_REQ_END,
		STEP_REQ_SUSPENDED,
		STEP_REQ_NAME,
		STEP_REQ_NODELIST,
		STEP_REQ_NODE_INX,
		STEP_REQ_STATE,
		STEP_REQ_KILL_REQUID,
		STEP_REQ_COMP_CODE,
		STEP_REQ_NODES,
		STEP_REQ_CPUS,
		STEP_REQ_TASKS,
		STEP_REQ_TASKDIST,
		STEP_REQ_USER_SEC,
		STEP_REQ_USER_USEC,
		STEP_REQ_SYS_SEC,
		STEP_REQ_SYS_USEC,
		STEP_REQ_MAX_VSIZE,
		STEP_REQ_MAX_VSIZE_TASK,
		STEP_REQ_MAX_VSIZE_NODE,
		STEP_REQ_AVE_VSIZE,
		STEP_REQ_MAX_RSS,
		STEP_REQ_MAX_RSS_TASK,
		STEP_REQ_MAX_RSS_NODE,
		STEP_REQ_AVE_RSS,
		STEP_REQ_MAX_PAGES,
		STEP_REQ_MAX_PAGES_TASK,
		STEP_REQ_MAX_PAGES_NODE,
		STEP_REQ_AVE_PAGES,
		STEP_REQ_MIN_CPU,
		STEP_REQ_MIN_CPU_TASK,
		STEP_REQ_MIN_CPU_NODE,
		STEP_REQ_AVE_CPU,
		STEP_REQ_COUNT
	};

	enum {
		SUSPEND_REQ_ID,
		SUSPEND_REQ_ASSOCID,
		SUSPEND_REQ_START,
		SUSPEND_REQ_END,
		SUSPEND_REQ_COUNT
	};

	if(!arch_cond) {
		error("No arch_cond was given to archive from.  returning");
		return SLURM_ERROR;
	}

	if(!localtime_r(&last_submit, &time_tm)) {
		error("Couldn't get localtime from first start %d",
		      last_submit);
		return SLURM_ERROR;
	}
	time_tm.tm_sec = 0;
	time_tm.tm_min = 0;
	time_tm.tm_hour = 0;
	time_tm.tm_mday = 1;
	time_tm.tm_isdst = -1;
	last_submit = mktime(&time_tm);
	last_submit--;
	debug("archive: adjusted last submit is (%d)", last_submit);
	
	if(arch_cond->archive_script)
		return _archive_script(arch_cond, last_submit);
	else if(!arch_cond->archive_dir) {
		error("No archive dir given, can't process");
		return SLURM_ERROR;
	}

	if(arch_cond->purge_event) {
		/* remove all data from step table that was older than
		 * period_start * arch_cond->purge_event. 
		 */
		/* use localtime to avoid any daylight savings issues */
		if(!localtime_r(&last_submit, &time_tm)) {
			error("Couldn't get localtime from first submit %d",
			      last_submit);
			return SLURM_ERROR;
		}
		time_tm.tm_mday = 1;
		time_tm.tm_mon -= arch_cond->purge_event;
		time_tm.tm_isdst = -1;
		curr_end = mktime(&time_tm);

		debug4("from %d - %d months purging events from before %d", 
		       last_submit, arch_cond->purge_event, curr_end);
		
		if(arch_cond->archive_events) {
			char *insert = NULL;
			MYSQL_RES *result = NULL;
			
			xfree(tmp);
			xstrfmtcat(tmp, "%s", event_req_inx[0]);
			for(i=1; i<EVENT_REQ_COUNT; i++) {
				xstrfmtcat(tmp, ", %s", event_req_inx[i]);
			}

			/* get all the events started before this time
			   listed */
			query = xstrdup_printf("select %s from %s where "
					       "period_start <= %d "
					       "&& period_end != 0 "
					       "order by period_start asc",
					       tmp, event_table, curr_end);

			insert = xstrdup_printf("insert into %s (%s) ",
						event_table, tmp);
			xfree(tmp);

//			START_TIMER;
			debug3("%d(%d) query\n%s", mysql_conn->conn,
			       __LINE__, query);
			if(!(result = mysql_db_query_ret(
				     mysql_conn->db_conn, query, 0))) {
				xfree(insert);
				xfree(query);
				return SLURM_ERROR;
			}
			xfree(query);
//			END_TIMER2("step query");
//			info("event query took %s", TIME_STR);

			if(!mysql_num_rows(result)) {
				xfree(insert);
				mysql_free_result(result);
				goto exit_events;
			}

			rc = _write_archive_file(
				result, EVENT_REQ_START, EVENT_REQ_COUNT,
				curr_end, arch_cond->archive_dir, 
				"event", insert, false);
			
			xfree(insert);
			mysql_free_result(result);

			if(rc != SLURM_SUCCESS)
				return rc;
		}
		query = xstrdup_printf("delete from %s where "
				       "period_start <= %d && period_end != 0",
				       event_table, curr_end);
		debug3("%d(%d) query\n%s", mysql_conn->conn, __LINE__, query);
		rc = mysql_db_query(mysql_conn->db_conn, query);
		xfree(query);
		if(rc != SLURM_SUCCESS) {
			error("Couldn't remove old event data");
			return SLURM_ERROR;
		}
	}

exit_events:

	if(arch_cond->purge_suspend) {
		/* remove all data from step table that was older than
		 * period_start * arch_cond->purge_suspend. 
		 */
		/* use localtime to avoid any daylight savings issues */
		if(!localtime_r(&last_submit, &time_tm)) {
			error("Couldn't get localtime from first submit %d",
			      last_submit);
			return SLURM_ERROR;
		}
		time_tm.tm_mday = 1;
		time_tm.tm_mon -= arch_cond->purge_suspend;
		time_tm.tm_isdst = -1;
		curr_end = mktime(&time_tm);

		debug4("from %d - %d months purging suspend from before %d", 
		       last_submit, arch_cond->purge_suspend, curr_end);
		
		if(arch_cond->archive_suspend) {
			char *insert = NULL;
			MYSQL_RES *result = NULL;
			
			xfree(tmp);
			xstrfmtcat(tmp, "%s", suspend_req_inx[0]);
			for(i=1; i<SUSPEND_REQ_COUNT; i++) {
				xstrfmtcat(tmp, ", %s", suspend_req_inx[i]);
			}

			/* get all the suspend started before this time
			   listed */
			query = xstrdup_printf("select %s from %s where "
					       "start <= %d && end != 0 "
					       "order by start asc",
					       tmp, suspend_table, curr_end);

			insert = xstrdup_printf("insert into %s (%s) ",
						suspend_table, tmp);
			xfree(tmp);

//			START_TIMER;
			debug3("%d(%d) query\n%s", mysql_conn->conn,
			       __LINE__, query);
			if(!(result = mysql_db_query_ret(
				     mysql_conn->db_conn, query, 0))) {
				xfree(insert);
				xfree(query);
				return SLURM_ERROR;
			}
			xfree(query);
//			END_TIMER2("step query");
//			info("suspend query took %s", TIME_STR);

			if(!mysql_num_rows(result)) {
				xfree(insert);
				mysql_free_result(result);
				goto exit_suspend;
			}

			rc = _write_archive_file(
				result, SUSPEND_REQ_START, SUSPEND_REQ_COUNT,
				curr_end, arch_cond->archive_dir, 
				"suspend", insert, false);
			
			xfree(insert);
			mysql_free_result(result);

			if(rc != SLURM_SUCCESS)
				return rc;
		}
		query = xstrdup_printf("delete from %s where start <= %d "
				       "&& end != 0",
				       suspend_table, curr_end);
		debug3("%d(%d) query\n%s", mysql_conn->conn, __LINE__, query);
		rc = mysql_db_query(mysql_conn->db_conn, query);
		xfree(query);
		if(rc != SLURM_SUCCESS) {
			error("Couldn't remove old suspend data");
			return SLURM_ERROR;
		}
	}

exit_suspend:

	if(arch_cond->purge_step) {
		/* remove all data from step table that was older than
		 * start * arch_cond->purge_step. 
		 */
		/* use localtime to avoid any daylight savings issues */
		if(!localtime_r(&last_submit, &time_tm)) {
			error("Couldn't get localtime from first start %d",
			      last_submit);
			return SLURM_ERROR;
		}
		time_tm.tm_mon -= arch_cond->purge_step;
		time_tm.tm_isdst = -1;
		curr_end = mktime(&time_tm);

		debug4("from %d - %d months purging steps from before %d", 
		       last_submit, arch_cond->purge_step, curr_end);
		
		if(arch_cond->archive_steps) {
			char *insert = NULL;
			MYSQL_RES *result = NULL;

			xfree(tmp);
			xstrfmtcat(tmp, "%s", step_req_inx[0]);
			for(i=1; i<STEP_REQ_COUNT; i++) {
				xstrfmtcat(tmp, ", %s", step_req_inx[i]);
			}

			/* get all the steps submitted before this time
			   listed */
			query = xstrdup_printf("select %s from %s where "
					       "start <= %d && end != 0 "
					       "&& !deleted "
					       "order by start asc",
					       tmp, step_table, curr_end);

			xstrcat(tmp, ", deleted");
			insert = xstrdup_printf("insert into %s (%s) ",
						step_table, tmp);
			xfree(tmp);

//			START_TIMER;
			debug3("%d(%d) query\n%s", mysql_conn->conn,
			       __LINE__, query);
			if(!(result = mysql_db_query_ret(
				     mysql_conn->db_conn, query, 0))) {
				xfree(insert);
				xfree(query);
				return SLURM_ERROR;
			}
			xfree(query);
//			END_TIMER2("step query");
//			info("step query took %s", TIME_STR);

			if(!mysql_num_rows(result)) {
				xfree(insert);
				mysql_free_result(result);
				goto exit_steps;
			}
			
			rc = _write_archive_file(
				result, STEP_REQ_START, STEP_REQ_COUNT,
				curr_end, arch_cond->archive_dir, 
				"step", insert, true);
			
			xfree(insert);
			mysql_free_result(result);

			if(rc != SLURM_SUCCESS)
				return rc;
		}

		query = xstrdup_printf("delete from %s where start <= %d "
				       "&& end != 0",
				       step_table, curr_end);
		debug3("%d(%d) query\n%s", mysql_conn->conn, __LINE__, query);
		rc = mysql_db_query(mysql_conn->db_conn, query);
		xfree(query);
		if(rc != SLURM_SUCCESS) {
			error("Couldn't remove old step data");
			return SLURM_ERROR;
		}
	}
exit_steps:
	
	if(arch_cond->purge_job) {
		/* remove all data from step table that was older than
		 * last_submit * arch_cond->purge_job. 
		 */
		/* use localtime to avoid any daylight savings issues */
		if(!localtime_r(&last_submit, &time_tm)) {
			error("Couldn't get localtime from first submit %d",
			      last_submit);
			return SLURM_ERROR;
		}
		time_tm.tm_mday = 1;
		time_tm.tm_mon -= arch_cond->purge_job;
		time_tm.tm_isdst = -1;
		curr_end = mktime(&time_tm);

		debug4("from %d - %d months purging jobs from before %d", 
		       last_submit, arch_cond->purge_job, curr_end);

		if(arch_cond->archive_jobs) {
			char *insert = NULL;
			MYSQL_RES *result = NULL;
						
			xfree(tmp);
			xstrfmtcat(tmp, "%s", job_req_inx[0]);
			for(i=1; i<JOB_REQ_COUNT; i++) {
				xstrfmtcat(tmp, ", %s", job_req_inx[i]);
			}
			/* get all the jobs submitted before this time
			   listed */
			query = xstrdup_printf("select %s from %s where "
					       "submit < %d && end != 0 "
					       "&& !deleted "
					       "order by submit asc",
					       tmp, job_table, curr_end);

			xstrcat(tmp, ", deleted");
			insert = xstrdup_printf("insert into %s (%s) ",
						job_table, tmp);
			xfree(tmp);

//			START_TIMER;
			debug3("%d(%d) query\n%s", mysql_conn->conn,
			       __LINE__, query);
			if(!(result = mysql_db_query_ret(
				     mysql_conn->db_conn, query, 0))) {
				xfree(insert);
				xfree(query);
				return SLURM_ERROR;
			}
			xfree(query);
//			END_TIMER2("job query");
//			info("job query took %s", TIME_STR);

			if(!mysql_num_rows(result)) {
				xfree(insert);
				mysql_free_result(result);
				goto exit_jobs;
			}
			
			rc = _write_archive_file(
				result, JOB_REQ_SUBMIT, JOB_REQ_COUNT,
				curr_end, arch_cond->archive_dir, 
				"job", insert, true);
			
			xfree(insert);
			mysql_free_result(result);

			if(rc != SLURM_SUCCESS)
				return rc;
		}

		query = xstrdup_printf("delete from %s where submit <= %d "
				       "&& end != 0",
				       job_table, curr_end);
		debug3("%d(%d) query\n%s", mysql_conn->conn, __LINE__, query);
		rc = mysql_db_query(mysql_conn->db_conn, query);
		xfree(query);
		if(rc != SLURM_SUCCESS) {
			error("Couldn't remove old job data");
			return SLURM_ERROR;
		}
	}
exit_jobs:

	return SLURM_SUCCESS;
}

extern int mysql_jobacct_process_archive_load(mysql_conn_t *mysql_conn,
					      acct_archive_rec_t *arch_rec)
{
	char *data = NULL;
	int error_code = SLURM_SUCCESS;

	if(!arch_rec) {
		error("We need a acct_archive_rec to load anything.");
		return SLURM_ERROR;
	}

	if(arch_rec->insert) {
		data = xstrdup(arch_rec->insert);
	} else if(arch_rec->archive_file) {
		uint32_t data_size = 0;
		int data_allocated, data_read = 0;
		int state_fd = open(arch_rec->archive_file, O_RDONLY);
		if (state_fd < 0) {
			info("No archive file (%s) to recover", 
			     arch_rec->archive_file);
			error_code = ENOENT;
		} else {
			data_allocated = BUF_SIZE;
			data = xmalloc(data_allocated);
			while (1) {
				data_read = read(state_fd, &data[data_size],
						 BUF_SIZE);
				if (data_read < 0) {
					if (errno == EINTR)
						continue;
					else {
						error("Read error on %s: %m", 
						      arch_rec->archive_file);
						break;
					}
				} else if (data_read == 0)	/* eof */
					break;
				data_size      += data_read;
				data_allocated += data_read;
				xrealloc(data, data_allocated);
			}
			close(state_fd);
		}
		if(error_code != SLURM_SUCCESS) {
			xfree(data);
			return error_code;
		}
	} else {
		error("Nothing was set in your "
		      "acct_archive_rec so I am unable to process.");
		return SLURM_ERROR;
	}

	if(!data) {
		error("It doesn't appear we have anything to load.");
		return SLURM_ERROR;
	}

	debug3("%d(%d) query\n%s", mysql_conn->conn, __LINE__, data);
	error_code = mysql_db_query_check_after(mysql_conn->db_conn, data);
	xfree(data);
	if(error_code != SLURM_SUCCESS) {
		error("Couldn't load old data");
		return SLURM_ERROR;
	}

	return SLURM_SUCCESS;
}
