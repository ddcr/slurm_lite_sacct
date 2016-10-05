"acct_storage_p_get_connection",       void *(*get_conn)          (bool make_agent, int conn_num, bool rollback);
"acct_storage_p_close_connection",     int  (*close_conn)         (void **db_conn);
"acct_storage_p_commit",               int  (*commit)             (void *db_conn, bool commit);
"acct_storage_p_add_users",            int  (*add_users)          (void *db_conn, uint32_t uid, List user_list);
"acct_storage_p_add_coord",            int  (*add_coord)          (void *db_conn, uint32_t uid, List acct_list, acct_user_cond_t *user_cond);
"acct_storage_p_add_accts",            int  (*add_accts)          (void *db_conn, uint32_t uid, List acct_list);
"acct_storage_p_add_clusters",         int  (*add_clusters)       (void *db_conn, uint32_t uid, List cluster_list);
"acct_storage_p_add_associations",     int  (*add_associations)   (void *db_conn, uint32_t uid, List association_list);
"acct_storage_p_add_qos",              int  (*add_qos)            (void *db_conn, uint32_t uid, List qos_list);
"acct_storage_p_add_wckeys",           int  (*add_wckeys)         (void *db_conn, uint32_t uid, List wckey_list);
"acct_storage_p_add_reservation",      int  (*add_reservation)    (void *db_conn, acct_reservation_rec_t *resv);
"acct_storage_p_modify_users",         List (*modify_users)       (void *db_conn, uint32_t uid, acct_user_cond_t *user_cond, acct_user_rec_t *user);
"acct_storage_p_modify_accounts",      List (*modify_accts)       (void *db_conn, uint32_t uid, acct_account_cond_t *acct_cond, acct_account_rec_t *acct);
"acct_storage_p_modify_clusters",      List (*modify_clusters)    (void *db_conn, uint32_t uid, acct_cluster_cond_t *cluster_cond, acct_cluster_rec_t *cluster);
"acct_storage_p_modify_associations",  List (*modify_associations)(void *db_conn, uint32_t uid, acct_association_cond_t *assoc_cond, acct_association_rec_t *assoc);
"acct_storage_p_modify_qos",           List (*modify_qos)         (void *db_conn, uint32_t uid, acct_qos_cond_t *qos_cond, acct_qos_rec_t *qos);
"acct_storage_p_modify_wckeys",        List (*modify_wckeys)      (void *db_conn, uint32_t uid, acct_wckey_cond_t *wckey_cond, acct_wckey_rec_t *wckey);
"acct_storage_p_modify_reservation",   int  (*modify_reservation) (void *db_conn, acct_reservation_rec_t *resv);
"acct_storage_p_remove_users",         List (*remove_users)       (void *db_conn, uint32_t uid, acct_user_cond_t *user_cond);
"acct_storage_p_remove_coord",         List (*remove_coord)       (void *db_conn, uint32_t uid, List acct_list, acct_user_cond_t *user_cond);
"acct_storage_p_remove_accts",         List (*remove_accts)       (void *db_conn, uint32_t uid, acct_account_cond_t *acct_cond);
"acct_storage_p_remove_clusters",      List (*remove_clusters)    (void *db_conn, uint32_t uid, acct_cluster_cond_t *cluster_cond);
"acct_storage_p_remove_associations",  List (*remove_associations)(void *db_conn, uint32_t uid, acct_association_cond_t *assoc_cond);
"acct_storage_p_remove_qos",           List (*remove_qos)         (void *db_conn, uint32_t uid, acct_qos_cond_t *qos_cond);
"acct_storage_p_remove_wckeys",        List (*remove_wckeys)      (void *db_conn, uint32_t uid, acct_wckey_cond_t *wckey_cond);
"acct_storage_p_remove_reservation",   int  (*remove_reservation) (void *db_conn, acct_reservation_rec_t *resv);
"acct_storage_p_get_users",            List (*get_users)          (void *db_conn, uint32_t uid, acct_user_cond_t *user_cond);
"acct_storage_p_get_accts",            List (*get_accts)          (void *db_conn, uint32_t uid, acct_account_cond_t *acct_cond);
"acct_storage_p_get_clusters",         List (*get_clusters)       (void *db_conn, uint32_t uid, acct_cluster_cond_t *cluster_cond);
"acct_storage_p_get_config",           List (*get_config)         (void *db_conn);
"acct_storage_p_get_associations",     List (*get_associations)   (void *db_conn, uint32_t uid, acct_association_cond_t *assoc_cond);
"acct_storage_p_get_qos",              List (*get_qos)            (void *db_conn, uint32_t uid, acct_qos_cond_t *qos_cond);
"acct_storage_p_get_wckeys",           List (*get_wckeys)         (void *db_conn, uint32_t uid, acct_wckey_cond_t *wckey_cond);
"acct_storage_p_get_reservations",     List (*get_resvs)          (void *db_conn, uint32_t uid, acct_reservation_cond_t *resv_cond);
"acct_storage_p_get_txn",              List (*get_txn)            (void *db_conn, uint32_t uid, acct_txn_cond_t *txn_cond);
"acct_storage_p_get_usage",            int  (*get_usage)          (void *db_conn, uint32_t uid, void *in, int type, time_t start,  time_t end);
"acct_storage_p_roll_usage",           int (*roll_usage)          (void *db_conn, time_t sent_start, time_t sent_end, uint16_t archive_data);
"clusteracct_storage_p_node_down",     int  (*node_down)          (void *db_conn, char *cluster, struct node_record *node_ptr, time_t event_time, char *reason);
"clusteracct_storage_p_node_up",       int  (*node_up)            (void *db_conn, char *cluster, struct node_record *node_ptr, time_t event_time);
"clusteracct_storage_p_cluster_procs", int  (*cluster_procs)      (void *db_conn, char *cluster, char *cluster_nodes, uint32_t procs, time_t event_time);
"clusteracct_storage_p_get_usage",     int  (*c_get_usage)        (void *db_conn, uint32_t uid, void *cluster_rec, int type, time_t start, time_t end);
"clusteracct_storage_p_register_ctld", int  (*register_ctld)      (void *db_conn, char *cluster, uint16_t port);
"jobacct_storage_p_job_start",         int  (*job_start)          (void *db_conn, char *cluster_name, struct job_record *job_ptr);
"jobacct_storage_p_job_complete",      int  (*job_complete)       (void *db_conn, struct job_record *job_ptr);
"jobacct_storage_p_step_start",        int  (*step_start)         (void *db_conn, struct step_record *step_ptr);
"jobacct_storage_p_step_complete",     int  (*step_complete)      (void *db_conn, struct step_record *step_ptr);
"jobacct_storage_p_suspend",           int  (*job_suspend)        (void *db_conn, struct job_record *job_ptr);
"jobacct_storage_p_get_jobs_cond",     List (*get_jobs_cond)      (void *db_conn, uint32_t uid, acct_job_cond_t *job_cond);
"jobacct_storage_p_archive",           int (*archive_dump)        (void *db_conn, acct_archive_cond_t *arch_cond);
"jobacct_storage_p_archive_load",      int (*archive_load)        (void *db_conn, acct_archive_rec_t *arch_rec);
"acct_storage_p_update_shares_used",   int (*update_shares_used)  (void *db_conn, List shares_used);
"acct_storage_p_flush_jobs_on_cluster" int (*flush_jobs)          (void *db_conn, char *cluster, time_t event_time);