#!/bin/bash
#
#'2013-05-01T00:00:00', '2013-05-31T23:59:59'

starttime="2011-05-31T00:00:00"
# starttime="2013-01-01T00:00:00"
# endtime="2013-01-31T23:59:59"
endtime="now"

# Parse arguments
while [ $# -gt 0 ]; do
    arg="$1"
    if [ -n "$arg" ]; then
        case "$arg" in
            -h )        echo "-S [USAGE_START] -E [USAGE_END]" >&2 ;;
			-S )		starttime=$2; shift ;;
			-E )		endtime=$2; shift ;;
            * ) # arguments to mpiexec and command-name followed by arguments
                break;;
        esac
    fi
    shift
done
echo "starttime=$starttime"
echo "endtime=$endtime"

# --state=CANCELLED,COMPLETED,FAILED,NODE_FAIL,TIMEOUT \
# --state=CA,CD,F,NF,TO \

./sacct -vvvvv \
	--allusers \
	--parsable2 \
	--noheader \
	--allocations \
	--clusters veredas \
	--format=jobid,cluster,partition,account,group,gid,user,uid,submit,eligible,start,end,elapsed,exitcode,state,nnodes,ncpus,reqcpus,timelimit,nodelist,jobname \
	--state=CA,CD,F,NF,TO \
	--starttime $starttime \
	--endtime $endtime
