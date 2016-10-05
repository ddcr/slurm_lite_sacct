#ifndef _HAVE_MYSQL_JOBACCT_PROCESS_H
#define _HAVE_MYSQL_JOBACCT_PROCESS_H

#include <sys/types.h>
#include <pwd.h>
#include <stdlib.h>
#include "src/common/assoc_mgr.h"
#include "src/common/jobacct_common.h"
#include "src/slurmdbd/read_config.h"
#include "src/slurmctld/slurmctld.h"
#include "src/database/mysql_common.h"
#include "src/common/slurm_accounting_storage.h"

//extern int acct_db_init;
extern char *acct_coord_table;
extern char *acct_table;
extern char *assoc_day_table;
extern char *assoc_hour_table;
extern char *assoc_month_table;
extern char *assoc_table;
extern char *cluster_day_table;
extern char *cluster_hour_table;
extern char *cluster_month_table;
extern char *cluster_table;
extern char *event_table;
extern char *job_table;
extern char *last_ran_table;
extern char *qos_table;
extern char *resv_table;
extern char *step_table;
extern char *txn_table;
extern char *user_table;
extern char *suspend_table;
extern char *wckey_day_table;
extern char *wckey_hour_table;
extern char *wckey_month_table;
extern char *wckey_table;

extern List setup_cluster_list_with_inx(mysql_conn_t *mysql_conn,
					acct_job_cond_t *job_cond,
					void **curr_cluster);
extern int good_nodes_from_inx(List local_cluster_list, 
			       void **object, char *node_inx,
			       int submit);
extern int setup_job_cond_limits(mysql_conn_t *mysql_conn,
				 acct_job_cond_t *job_cond, char **extra);

extern List mysql_jobacct_process_get_jobs(mysql_conn_t *mysql_conn, uid_t uid,
					   acct_job_cond_t *job_cond);

extern int mysql_jobacct_process_archive(mysql_conn_t *mysql_conn,
					 acct_archive_cond_t *arch_cond);

extern int mysql_jobacct_process_archive_load(mysql_conn_t *mysql_conn,
					      acct_archive_rec_t *arch_rec);
#endif
