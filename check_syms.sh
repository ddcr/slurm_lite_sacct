#!/bin/bash
# @Author: ddcr
# @Date:   2016-09-19 00:26:17
# @Last Modified by:   ddcr
# @Last Modified time: 2016-09-19 00:27:48

declare -a syms=("acct_storage_p_close_connection" \
				 "acct_storage_p_commit" \
				 "acct_storage_p_commit" \
				 "acct_storage_p_add_users" \
				 "acct_storage_p_add_coord" \
				 "acct_storage_p_add_accts" \
				 "acct_storage_p_add_clusters" \
				 "acct_storage_p_add_associations" \
				 "acct_storage_p_add_associations" \
				 "acct_storage_p_add_qos" \
				 "acct_storage_p_add_wckeys" \
				 "acct_storage_p_add_wckeys" \
				 "acct_storage_p_add_reservation" \
				 "acct_storage_p_modify_reservation" \
				 "acct_storage_p_remove_reservation" \
				 "acct_storage_p_get_usage" \
				 "acct_storage_p_get_usage" \
				 "acct_storage_p_roll_usage" \
				 "clusteracct_storage_p_node_down" \
				 "clusteracct_storage_p_node_up" \
				 "clusteracct_storage_p_cluster_procs" \
				 "clusteracct_storage_p_get_usage" \
				 "clusteracct_storage_p_get_usage" \
				 "clusteracct_storage_p_register_ctld" \
				 "jobacct_storage_p_job_start" \
				 "jobacct_storage_p_job_complete" \
				 "jobacct_storage_p_step_start" \
				 "jobacct_storage_p_step_complete" \
				 "jobacct_storage_p_suspend" \
				 "jobacct_storage_p_archive" \
				 "jobacct_storage_p_archive_load" \
				 "jobacct_storage_p_archive_load" \
				 "acct_storage_p_update_shares_used" \
				 "acct_storage_p_flush_jobs_on_cluster")

for i in "${syms[@]}"
do
   grep  "$i" accounting_storage_mysql.c
done
