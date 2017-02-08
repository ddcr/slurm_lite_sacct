Extract sql commands
====================

(a) Step 1: Create SQL files ...
for i in `find sacct_outputs -name '*.err'`; do 
    grep "select t1.id" $i > ${i%.err}.sql;
done

(b) Step 2: Do some cleaning of the SQL script files ...
for f in `find sacct_outputs -name '*.sql'`; 
    do sed -i "s/t1.id, t1.jobid, t1.associd, t1.wckey, t1.wckeyid, t1.uid, t1.gid, t1.resvid, t3.name, t1.partition, t1.blockid, t1.cluster, t1.account, t1.eligible, t1.submit, t1.start, t1.end, t1.suspended, t1.timelimit, t1.name, t1.track_steps, t1.state, t1.comp_code, t1.priority, t1.req_cpus, t1.alloc_cpus, t1.alloc_nodes, t1.nodelist, t1.node_inx, t1.kill_requid, t1.qos, t2.user, t2.cluster, t2.acct, t2.lft/count(*)/" $f; 
done

(c) Execute the SQL commands
for f in `find sacct_outputs -name '*.sql'`; do 
    mysql -uroot -p<PASSWD> slurm_acct_db < $f > ${f%.sql}.cnt ;
done

(d) Count the lines in LOG files
for f in `find sacct_outputs -name '*.log'`; do
    wc -l $f | cut -d' ' -f1 >> ${f%.log}.cnt;
done

(e) Extract duplicated lines listed in *.err files ...
for f in `find sacct_outputs -name '*.err'`; do 
    grep -Eo "sacct: debug4: [0-9]{1,10}" $f | wc -l >> ${f%.err}.cnt;
done

(f) Each file *.cnt contains the countings from the LOG files and the duplicated records listed in the *.err files. They should add up the the
number of records from the executed mysql command ...

for f in `find sacct_outputs -name '*.cnt'`; do
    cat $f | tr '\n' ' ' > ${f%.cnt}.cnt1 ;
done
