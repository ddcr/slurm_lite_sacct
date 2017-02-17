#!/bin/bash
#
#'2013-05-01T00:00:00', '2013-05-31T23:59:59'

list_join() { 
    local OLDIFS=$IFS
    IFS=${1:?"Missing separator"}; shift
    echo "$*"
    IFS=$OLDIFS
}


starttime="2015-04-01T00:00:00" 
endtime="2015-04-30T23:59:59"
# starttime="2014-01-01T00:00:00"
# endtime="2014-01-31T23:59:59"

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
# echo "starttime=$starttime"
# echo "endtime=$endtime"

# --state=CANCELLED,COMPLETED,FAILED,NODE_FAIL,TIMEOUT \
# --state=CA,CD,F,NF,TO \

declare -a new_fmt=(
        'jobid'
        'jobidraw'
        'cluster'
        'partition'
        'account'
        'group'
        'gid'
        'user'
        'uid'
        'submit'
        'eligible'
        'start'
        'end'
        'elapsed'
        'exitcode'
        'state'
        'nnodes'
        'ncpus'
        'reqcpus'
        'reqmem'
        "reqgres",
        "reqtres",
        'timelimit'
        'nodelist'
        'jobname'
        );

declare -a old_fmt=(
        'jobid'
        'cluster'
        'partition'
        'account'
        'group'
        'gid'
        'user'
        'uid'
        'submit'
        'eligible'
        'start'
        'end'
        'elapsed'
        'exitcode'
        'state'
        'nnodes'
        'ncpus'
        'reqcpus'
        'timelimit'
        'nodelist'
        'jobname'
        );

./sacct -vvvvv \
	--allusers \
	--parsable2 \
	--noheader \
	--allocations \
	--clusters veredas \
	--format=$(list_join , "${new_fmt[@]}") \
	--state=CA,CD,F,NF,TO \
	--starttime $starttime \
	--endtime $endtime
