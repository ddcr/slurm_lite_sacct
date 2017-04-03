#!/usr/bin/env python
#
import csv
# import arrow
import scandir
import os
import time
import datetime
# import paratext
import pandas as pd
# from io import StringIO
import click
import sys
import inspect
import shutil

LOGPATH = os.path.abspath(os.path.join(os.path.dirname(__file__),
                          'sacct_outputs'))
FIELDS = ["jobid", "jobidraw", "cluster", "partition",
          "account", "group", "gid", "user", "uid",
          "submit", "eligible", "start", "end",
          "elapsed", "exitcode", "state", "nnodes",
          "ncpus", "reqcpus", "reqmem", "reqgres",
          "reqtres", "timelimit", "nodelist",
          "jobname"]


@click.group()
def cli():
    pass


def convert_to_d_h_m_s(seconds):
    """Return the tuple of days, hours, minutes and seconds."""
    minutes, seconds = divmod(seconds, 60)
    hours, minutes = divmod(minutes, 60)
    days, hours = divmod(hours, 24)
    return '{0}-{1}:{2}:{3}'.format(days, hours, minutes, seconds)


def d_h_m_s_to_timedelta(c):
    """ Date format needs to be handled differently if
        the days field is included """
    if '-' in c:
        tobj = time.strptime(c, '%d-%H:%M:%S')
        c = datetime.timedelta(tobj.tm_mday,
                               tobj.tm_sec,
                               0,
                               0,
                               tobj.tm_min,
                               tobj.tm_hour)
    else:
        tobj = time.strptime(c, '%H:%M:%S')
        c = datetime.timedelta(0,
                               tobj.tm_sec,
                               0,
                               0,
                               tobj.tm_min,
                               tobj.tm_hour)
    return(pd.Timedelta(c))


def reformat_timedelta_string(c):
    if '-' in c:
        c1 = c.split('-')
        return '{0} days {1}'.format(c1[0], c1[1])
    elif c in ["Partition_Limit", "UNLIMITED"]:
        return pd.NaT
    else:
        return c


@cli.command()
@click.option("--convert/--no-convert", default=True,
              help="convert timelimit/elapsed to %d days %H:%M:%S",
              show_default=True)
@click.argument("csvfile", default='all.csv')
def test(csvfile, convert=False, tzinfo='America/Sao_Paulo'):
    """Summary
    This is a testing area. Experiment a processing task in a single
    logfile, before commiting to full directory of files.

    Parameters
    ----------
    csvfile : TYPE
        Description
    convert : bool, optional
        Description
    tzinfo : str, optional
        Description
    """
    # df = paratext.load_csv_to_pandas(csvfile,
    #                                  allow_quoted_newlines=True)
    # for k, v in paratext.load_csv_as_iterator(csvfile,
    #                                           expand=True,
    #                                           forget=True):
    #     print k, v
    start = time.clock()
    csv_kw = {'parse_dates': [9, 10, 11, 12],
              'infer_datetime_format': True,
              'header': None,
              'delimiter': '|',
              'engine': 'c',
              'names': FIELDS}

    if convert:
        csv_kw['converters'] = {'elapsed': reformat_timedelta_string}
    df = pd.read_csv(csvfile, **csv_kw)
    if convert:
        df['elapsed'] = pd.to_timedelta(df['elapsed'])

    # idx = (df['eligible'] < df['submit'])
    # print any(idx)
    print time.clock() - start


def compstats(csvfile, df, **kwargs):
    """Summary
        Compute statistics on Dataframe from CSV file

    Parameters
    ----------
    csvfile : TYPE
        CSV file
    df : TYPE
        Pandas Dataframe holding csvfile
    **kwargs : TYPE
        Extra keywords

    Returns
    -------
    TYPE
        Description
    """

    dfout = pd.DataFrame()
    for field in ['jobid', 'user', 'account', 'submit', 'submit',
                  'eligible', 'start', 'end', 'elapsed', 'state']:
        dfout[field] = df[field]
    dfout['tqueue'] = df['eligible']-df['submit']
    dfout = dfout[dfout['tqueue'] > pd.Timedelta(days=1,
                                                 seconds=0,
                                                 microseconds=0,
                                                 milliseconds=0,
                                                 minutes=0,
                                                 hours=0,
                                                 weeks=0)]
    return(dfout)


def filtcorr(csvfile, df, **kwargs):
    """Summary
        Filter CSV file and correct wrong fields 'submit', 'eligible'
    Parameters
    ----------
    csvfile : TYPE
        Description
    df : TYPE
        Description

    Returns
    -------
    TYPE
        Description
    """
    # 1 job in this situation:
    # Sol: make submit time equal to eligible time
    # df_sel = df[(df['eligible'] < df['start']) &
    #             (df['start'] < df['submit'])]

    # 1695 jobs in this situation:
    # Sol: swap eligible and submit time fields
    # df_sel = df[(df['eligible'] < df['submit']) &
    #             (df['submit'] <= df['start'])]

    # 477713 jobs in this situation: zero-elapsed tasks
    # df_sel = df[df['start'] == df['end']]

    # 175256 jobs in this situation: zero-elapsed tasks
    # df_sel = df[(df['start'] == df['eligible']) &
    #             (df['start'] == df['end'])]

    # 174394 jobs in this situation: zero-elapsed tasks
    # df_sel = df[(df['submit'] == df['eligible']) &
    #             (df['eligible'] == df['start']) &
    #             (df['start'] == df['end'])]

    df_sel = pd.DataFrame()
    is_sel1 = False
    is_sel2 = False
    is_sel3 = False

    idx_sel1 = (df['eligible'] < df['submit'])
    is_sel1 = any(idx_sel1)

    if is_sel1:
        idx_stmt = (df['start'] < df['submit'])
        idx_sel2 = idx_sel1 & idx_stmt
        idx_sel3 = idx_sel1 & ~idx_stmt

        is_sel2 = any(idx_sel2)
        is_sel3 = any(idx_sel3)

        if is_sel2:
            df.loc[idx_sel2, ['submit']] = \
                df.loc[idx_sel2, ['eligible']].values

        if is_sel3:
            df.loc[idx_sel3, ['submit', 'eligible']] = \
                df.loc[idx_sel3, ['eligible', 'submit']].values

    idx_sel4 = ((df.jobid.isin([234149, 1250231, 1249906, 1261161])) &
                (df.state == 'CANCELLED by 0'))
    is_sel4 = any(idx_sel4)
    if is_sel4:
        df = df[~idx_sel4]

    if ((is_sel1 and (is_sel2 or is_sel3)) or is_sel4):
        csvfile_dst = os.path.splitext(csvfile)[0] + '.old.log'
        shutil.move(csvfile, csvfile_dst)
        df.to_csv(csvfile,
                  sep='|',
                  header=False,
                  index=False,
                  date_format='%Y-%m-%dT%H:%M:%S')
    return(df_sel)


def parse_log_process(fn, csvfile, **kwargs):
    """Summary
        scan directory of logfiles concurrently and do some processing

    Parameters
    ----------
    csvfile : TYPE
        SLURM CSV file
    fn : TYPE
        Task to be done on file csvfile
    **kwargs : TYPE
        Keywords of function `fn`

    Returns
    -------
    TYPE
        Description

    Deleted Parameters
    ------------------
    convert : bool, optional
        Description
    """
    csv_kw = {'parse_dates': [9, 10, 11, 12],
              'infer_datetime_format': True,
              'header': None,
              'delimiter': '|',
              'engine': 'c',
              'names': FIELDS}

    # extract key from **kwargs
    try:
        convert = kwargs.pop('convert')
    except KeyError:
        convert = False

    if csvfile.endswith('old.log'):
        return(csvfile, pd.DataFrame())

    if convert:
        csv_kw['converters'] = {'elapsed': reformat_timedelta_string}

    df = pd.read_csv(csvfile, **csv_kw)

    if convert:
        df['elapsed'] = pd.to_timedelta(df['elapsed'])

    df_proc = fn(csvfile, df, **kwargs)
    return(csvfile, df_proc)


func_mapping = {
    'compstats': compstats,
    'filtcorr': filtcorr
}


def str2func(ctx, param, value):
    click.echo('param={0}, value={1}'.format(param, value))
    try:
        func = func_mapping[value]
        if inspect.isfunction(func):
            return(func)
        else:
            raise click.BadParamenter("Task '%s' is not a function" % param)
    except KeyError:
        raise click.BadParameter("Task '%s' is not defined" % param)


@cli.command('run')
@click.option('--task',
              type=click.Choice(['compstats', 'filtcorr']),
              help="Task to perform on each CSV file",
              default='filtcorr',
              callback=str2func,
              show_default=True)
@click.option('--by', '-b',
              multiple=True,
              type=str,
              help="field(s) used to sort the reduced dataframe",
              show_default=True)
@click.option('--ascending/--no-ascending',
              default=True,
              help="sort by ascending order",
              show_default=True)
@click.option("--convert/--no-convert",
              default=False,
              help="convert timelimit/elapsed to %d days %H:%M:%S",
              show_default=True)
@click.argument("rootdir", default='sacct_outputs')
def run_concurrent(task, by, ascending, convert, rootdir):
    """
        Parallel scan/processing of a directory with SLURM logfiles
    """
    import concurrent.futures
    from concurrent.futures import ThreadPoolExecutor

    kwtask = {}
    if convert:
        kwtask = {'convert': True}
    click.echo("Task selected: {}".format(task))
    click.echo("Order resulting dataframe by field(s): {}".format(by))
    click.echo("Ascending order?: {}".format(ascending))
    click.echo(''.join(("Convert timelimit/elapsed to pandas",
                        " timedelta repr?: {}".format(convert))))
    click.echo("rootdir: {}".format(rootdir))

    click.confirm('Do you want to continue?', abort=True)
    click.echo("Well done ...")

    # gather all files
    allfiles = []
    for root, dirs, files in scandir.walk(rootdir):
        for file_name in files:
            if file_name.endswith(".log"):
                allfiles.append(os.path.join(root, file_name))

    reduce_results = []
    with ThreadPoolExecutor(max_workers=6) as executor:
        wait_for = [executor.submit(parse_log_process,
                                    task,
                                    file_name,
                                    **kwtask)
                    for file_name in allfiles]
        for f in concurrent.futures.as_completed(wait_for, timeout=None):
            error = f.exception()
            res = f.result()
            if error is None:
                reduce_results.append(res[1])
                print('Main: {0} {1}'.format(f, res[0]))
            else:
                print('Main: error: {0} {1}'.format(f, error))
    reduced_df = pd.concat(reduce_results)
    if len(by) > 0:
        by = list(by)
        print 'Sorting reduced DataFrame'
        reduced_df.sort_values(inplace=True, by=by, ascending=ascending)
    reduced_df.to_csv('reduced_results.csv',
                      sep='|',
                      header=False,
                      index=False,
                      date_format='%Y-%m-%dT%H:%M:%S')


@cli.command()
@click.option("--convert/--no-convert", default=True,
              help="convert timelimit/elapsed to %d days %H:%M:%S",
              show_default=True)
@click.argument("csvfile", default='all.csv')
def csv2df(csvfile, convert):
    """Summary
        Reads the SLURM csv file into Pandas dataframe
    Args:
        cvsfile (str, optional): Description

    Returns:
        TYPE: Description
    """
    # click.echo('csvfile={0} convert={1}'.format(csvfile, convert))
    start = time.clock()
    csv_kw = {'parse_dates': [9, 10, 11, 12],
              'infer_datetime_format': True,
              'header': None,
              'delimiter': '|',
              'engine': 'c',
              'names': FIELDS}

    if convert:
        csv_kw['converters'] = {'elapsed': reformat_timedelta_string,
                                'timelimit': reformat_timedelta_string}

    df = pd.read_csv(csvfile, **csv_kw)
    df['timelimit'] = pd.to_timedelta(df['timelimit'])
    df['elapsed'] = pd.to_timedelta(df['elapsed'])
    print df.info()

    h5filename = os.path.splitext(csvfile)[0]+'.blosc.h5'
    store = pd.HDFStore(h5filename,
                        complevel=9,
                        complib='blosc')
    store.put('df', df, format='table')
    store.close()
    print time.clock() - start


@cli.command('analyze_df_h5')
@click.option("--check/--no-check",
              default=False,
              help="check if dataframe from HDF5 comes from CSV file",
              show_default=True)
@click.argument("h5file", default='all.compressed.h5')
def analyze_df_h5(h5file, check):
    """Summary
        Statistical analysis of dataframe with SLURM logs
    """
    # click.echo('h5file={0} check={1}'.format(h5file, check))
    if not os.path.exists(h5file):
        print(('H5 file does not exist. Recreate',
               ' it with cvs2df --no-convert'))
        sys.exit(1)

    start = time.clock()
    df = pd.read_hdf(h5file)

    if check:
        csvfile = os.path.splitext(h5file)[0]+'.test.csv'
        # rearrange the fields timelimit and elapsed
        df.to_csv(csvfile,
                  sep='|',
                  header=False,
                  index=False,
                  date_format='%Y-%m-%dT%H:%M:%S')
        sys.exit(1)

    # get all duplicated jobids
    # df_dupes = df[df.duplicated(['jobid'], keep=False)]
    df_sort = df.sort_values(by=['submit', 'jobid'],
                             ascending=True)
    df_sort.to_csv('duplicated.csv',
                   sep='|',
                   header=False,
                   index=False,
                   date_format='%Y-%m-%dT%H:%M:%S')
    print time.clock() - start


@cli.command()
@click.argument('jobcompfile', default="jobcompletion.log")
def parse_correct_jcomp(jobcompfile):
    """Summary
        Read JOBCOMP file and correct fields
    Returns
    -------
    TYPE
        Description
    """
    from collections import OrderedDict

    JOBCOMP_FIELDS = ["JobId", "UserId", "GroupId", "Name", "JobState",
                      "Partition", "TimeLimit", "StartTime", "EndTime",
                      "NodeList", "NodeCnt", "ProcCnt", "WorkDir"]

    # jobcompfile_basename = os.path.splitext(jobcompfile)[0]
    # jobcompfile_correct = jobcompfile_basename + '.corr.log'
    # jobcompfile_sql = jobcompfile_basename + '.corr.sql'
    # jobcompfile_sql_undo = jobcompfile_basename + '.corr.undo.sql'

    # Find JobIds for which the fields 'Name', 'WorkDir' have garbled text
    # and then correct them. Write corrections back to logfile and
    # write a SQL script to correct the database 'slurm_acct_db'

    start = time.clock()
    # with open(jobcompfile_correct, 'wb') as out_fd:
    #     with open(jobcompfile_sql, 'wb') as sql_fd:
    #         with open(jobcompfile_sql_undo, 'wb') as undo_sql_fd:
    with open(jobcompfile, 'rb') as in_fd:
        for record in csv.reader(in_fd, delimiter=' '):
            kv_pairs = OrderedDict(s.split('=', 1) for s in record
                                   if '=' in s)

            if len(kv_pairs) == 13:
                if set(kv_pairs.keys()) != set(JOBCOMP_FIELDS):
                    print('PROBLEM')
                    print record
                    sys.exit(1)
                else:
                    continue

    print 'Time = ', time.clock() - start


if __name__ == '__main__':
    cli()
