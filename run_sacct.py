#!/usr/bin/env python
#
import datetime
import calendar
import re
import os
import time
import sys
import textwrap
import subprocess
import multiprocessing
from multiprocessing.pool import ThreadPool


cmd_format = ["jobid",
              "jobidraw",
              "cluster",
              "partition",
              "account",
              "group",
              "gid",
              "user",
              "uid",
              "submit",
              "eligible",
              "start",
              "end",
              "elapsed",
              "exitcode",
              "state",
              "nnodes",
              "ncpus",
              "reqcpus",
              "reqmem",
              "timelimit",
              "nodelist",
              "jobname"]
cmd_state = ["CANCELLED",
             "COMPLETED",
             "FAILED",
             "NODE_FAIL",
             "PREEMPTED",
             "TIMEOUT"]
cmd_options = ["--allusers",
               "--parsable2",
               "--noheader",
               "--allocations",
               "--clusters veredas"]
cmd_options_string = ' '.join(cmd_options)
cmd_format_string = '--format {0}'.format(','.join(cmd_format))
cmd_state_string = '--format {0}'.format(','.join(cmd_state))


class MyError(Exception):
    pass


def get_sacct_cmd():
    os.chdir(os.path.dirname(os.path.abspath(__file__)))
    cwd = os.getcwd()
    os.environ['SLURM_CONF'] = os.path.join(cwd, 'etc/slurm/slurm.conf')
    # make sure the fields SlurmUser and SlurmGroup have the
    # right values
    return '{}/sacct.exe'.format(cwd)


def get_month_day_range(date_w_time):
    """
    For a datetime 'date_w_time' returns the start and end datetime
    for the month of 'date_w_time'.

    Month with 31 days:
    >>> date = datetime.date(2011, 7, 27)
    >>> get_month_day_range(date)
    (datetime.date(2011, 7, 1), datetime.date(2011, 7, 31))

    Month with 28 days:
    >>> date = datetime.date(2011, 2, 15)
    >>> get_month_day_range(date)
    (datetime.date(2011, 2, 1), datetime.date(2011, 2, 28))
    """
    first_day = date_w_time.replace(day=1,
                                    hour=0,
                                    minute=0,
                                    second=0)
    last_day = date_w_time.replace(
        day=calendar.monthrange(date_w_time.year, date_w_time.month)[1],
        hour=23,
        minute=59,
        second=59
    )
    return first_day.isoformat(sep='T'), last_day.isoformat(sep='T')


def start_process():
    p = multiprocessing.current_process()
    print 'Starting {0} with pid = {1}'.format(p.name, p.pid)


def work_sacct(datetime_t, sacct_fmt='sacct.exe', outdir='', debug_error=True):
    """Summary

    Args:
        datetime_range (TYPE): Description

    Returns:
        TYPE: Description
    """
    ymd_label = re.split('T', datetime_t[0])[0]
    fileout = os.path.join(outdir, 'slurm-{}.log'.format(ymd_label))
    fileerr = os.path.join(outdir, 'slurm-{}.err'.format(ymd_label))

    cmd_datetime = '--starttime {0} --endtime {1}'.format(datetime_t[0],
                                                          datetime_t[1])
    if debug_error:
        # make subprocess to raise an error for debugging purposes
        os.environ['SLURM_CONF'] = ''
        cmd = [sacct_fmt,
               '-v',  # raises error
               cmd_datetime,
               ]
    else:
        cmd = [sacct_fmt,
               cmd_options_string,
               cmd_format_string,
               cmd_state_string,
               cmd_datetime,
               ]

    if not os.path.isfile(fileout):
        with open(fileout, 'wb') as fout:
            with open(fileerr, 'wb') as ferr:
                try:
                    subprocess.check_call(' '.join(cmd),
                                          shell=True,
                                          stdout=fout,
                                          stderr=ferr)
                except subprocess.CalledProcessError as e:
                    a = textwrap.dedent("""\
                                        {0}:
                                          Error code raised: {1}
                                          Need to remove file:
                                          {2}
                                        """.format(e.cmd,
                                                   e.returncode,
                                                   fileout))
                    raise MyError(a)
                else:
                    a = textwrap.dedent("""\
                                        {0}:
                                           Finished
                                        """.format(''.join(cmd)))
                finally:
                    time.sleep(2)
                    return(a)
    else:
        return('PASSING: FIle {0} exists'.format(fileout))


def use_multiprocessing():

    cmd_sacct = get_sacct_cmd()
    # pool_size = multiprocessing.cpu_count()
    pool_size = 4
    tp = ThreadPool(processes=pool_size,
                    initializer=start_process,
                    )

    # for y in [2011, 2012, 2013, 2014, 2015, 2016]:
    for y in [2011]:
        year_range = [get_month_day_range(datetime.datetime(year=y,
                                                            month=m,
                                                            day=1))
                      for m in range(1, 13)
                      ]
        for dx in year_range:
            tp.apply_async(work_sacct,
                           args=(dx,),
                           kwds={'sacct_fmt': cmd_sacct}
                           )
    tp.close()
    tp.join()


def use_concurrent():
    import concurrent.futures
    # from concurrent.futures import ThreadPoolExecutor
    from concurrent.futures import ProcessPoolExecutor

    cmd_sacct = get_sacct_cmd()
    kwds = {'sacct_fmt': cmd_sacct}
    # for y in [2011, 2012, 2013, 2014, 2015, 2016]:
    for y in [2011]:
        #
        # First create the output directory
        outdir = './sacct_outputs/slurm-{0}'.format(y)
        kwds['outdir'] = outdir
        if not os.path.isdir(outdir):
            try:
                os.makedirs(outdir)
            except OSError:
                raise MyError(
                    "I was not able to create"
                    "directory {}".format(outdir)
                )
                sys.exit(1)

        year_range = [get_month_day_range(datetime.datetime(year=y,
                                                            month=m,
                                                            day=1))
                      for m in range(1, 13)
                      ]
        # The with statement ensures that threads are cleaned up promptly
        # with ThreadPoolExecutor(max_workers=4) as executor:
        with ProcessPoolExecutor(max_workers=4) as executor:
            wait_for = [executor.submit(work_sacct, datetime_t, **kwds)
                        for datetime_t in year_range]
            for f in concurrent.futures.as_completed(wait_for, timeout=None):
                error = f.exception()
                if error is None:
                    print('Main: {0} {1}'.format(f, f.result()))
                else:
                    print('Main: error: {0} {1}'.format(f, error))


if __name__ == '__main__':
    """
    """
    use_concurrent()
