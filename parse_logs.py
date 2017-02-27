#!/usr/bin/env python
#
import csv
import arrow
import scandir
import os
import time
import datetime
# import paratext
import pandas as pd
from io import StringIO

LOGPATH = os.path.abspath(os.path.join(os.path.dirname(__file__),
                          'sacct_outputs'))
FIELDS = ["jobid", "jobidraw", "cluster", "partition",
          "account", "group", "gid", "user", "uid",
          "submit", "eligible", "start", "end",
          "elapsed", "exitcode", "state", "nnodes",
          "ncpus", "reqcpus", "reqmem", "reqgres",
          "reqtres", "timelimit", "nodelist",
          "jobname"]


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


def test(csvfile, tzinfo='America/Sao_Paulo'):
    """Summary
    Returns:
        TYPE: Description
    """

    date_fields = ["submit", "eligible", "start", "end"]

    with open(csvfile, 'rb') as infile:
        for record in csv.reader(infile, delimiter='|'):
            # Arrow.get() defaults to UTC timezone
            # record[9:13]: "submit", "eligible", "start", "end", "elapsed"
            datetimes = \
                [arrow.get(dt).replace(tzinfo=tzinfo).timestamp
                 for dt in record[9:13]]
            slurm_dates = dict((k, v) for k, v in zip(date_fields, datetimes))
            if slurm_dates["eligible"] < slurm_dates["submit"]:
                diff = slurm_dates["submit"] - slurm_dates["eligible"]
                print '|'.join(record), convert_to_d_h_m_s(diff)


def test_alt(csvfile, tzinfo='America/Sao_Paulo'):
    """Summary
    Returns:
        TYPE: Description
    """
    # df = paratext.load_csv_to_pandas(csvfile,
    #                                  allow_quoted_newlines=True)
    # for k, v in paratext.load_csv_as_iterator(csvfile,
    #                                           expand=True,
    #                                           forget=True):
    #     print k, v
    start = time.clock()
    df = pd.read_csv(csvfile,
                     parse_dates=[9, 10, 11, 12],
                     infer_datetime_format=True,
                     header=None,
                     delimiter='|',
                     engine='c',
                     # converters={'elapsed': d_h_m_s_to_timedelta},
                     converters={'elapsed': reformat_timedelta_string,
                                 'timelimit': reformat_timedelta_string},
                     names=FIELDS)
    # df['timelimit'] = pd.to_timedelta(df['timelimit'])
    # buf_pd = StringIO()
    # df.info(buf=buf_pd)
    # print buf_pd.getvalue()

    # print df['start']
    # print (df['start'] == 0).all()
    # print pd.to_datetime([0], utc=True)
    # df_selected = df[df['submit'] > df['eligible']]
    df_selected = df['eligible']-df['submit']
    print df_selected
    print time.clock() - start


def parse_log_worker(csvfile, tzinfo='America/Sao_Paulo'):
    """Summary

    Args:
        flog (TYPE): Description

    Returns:
        TYPE: Description
    """

    # date_fields = ["submit", "eligible", "start", "end"]
    # return_lines = []

    df = pd.read_csv(csvfile,
                     parse_dates=[9, 10, 11, 12],
                     infer_datetime_format=True,
                     header=None,
                     delimiter='|',
                     engine='c',
                     # converters={'elapsed': d_h_m_s_to_timedelta},
                     converters={'elapsed': reformat_timedelta_string,
                                 'timelimit': reformat_timedelta_string},
                     names=FIELDS)
    # df['timelimit'] = pd.to_timedelta(df['timelimit'])
    # buf = StringIO()
    # df.info(buf=buf_pd)
    # df_sel = df[df['eligible'] < df['submit']]
    df_sel = df[df['start'] < df['submit']]
    # df_sel['d1'] = df_sel['eligible']-df_sel['submit']
    # df_sel['d2'] = df_sel['start']-df_sel['submit']
    return (csvfile, df_sel)


def use_serial():
    """Summary

    Returns:
        TYPE: Description
    """
    # gather all files
    for root, dirs, files in scandir.walk(LOGPATH):
        for file_name in files:
            if file_name.endswith(".log"):
                r = parse_log_worker(os.path.join(root, file_name))
                print(r)


def use_concurrent():
    """Summary

    Returns:
        TYPE: Description
    """
    import concurrent.futures
    from concurrent.futures import ThreadPoolExecutor

    # gather all files
    allfiles = []
    for root, dirs, files in scandir.walk(LOGPATH):
        for file_name in files:
            if file_name.endswith(".log"):
                allfiles.append(os.path.join(root, file_name))

    reduce_results = []
    with ThreadPoolExecutor(max_workers=6) as executor:
        wait_for = [executor.submit(parse_log_worker, file_name)
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
    reduced_df.to_csv('reduced_results.csv', sep='|')
    # outfile = 'jobs_with_eligible_smaller_than_submit.csv'
    # print reduce_results
    # with open(outfile, 'wb') as fout:
    #     writer = csv.writer(fout)
    #     for row in reduce_results:
    #         writer.writerow(row)


def csv2dataframe(csvfile='all.csv', convert_timedelta=None):
    """Summary

    Args:
        cvsfile (str, optional): Description

    Returns:
        TYPE: Description
    """
    start = time.clock()
    df = pd.read_csv(csvfile,
                     parse_dates=[9, 10, 11, 12],
                     infer_datetime_format=True,
                     header=None,
                     delimiter='|',
                     engine='c',
                     converters={'elapsed': reformat_timedelta_string,
                                 'timelimit': reformat_timedelta_string},
                     names=FIELDS)
    store = pd.HDFStore('all.compressed.h5',
                        complevel=9,
                        complib='blosc')
    store.put('df', df, format='table')
    store.close()
    print time.clock() - start


def analyze_h5_dataframe(h5file='all.compressed.h5', check=False):
    """Summary

    Returns:
        TYPE: Description
    """
    if not os.path.exists(h5file):
        print(('H5 file does not exist. Recreate',
               ' it with cvs2dataframe'))
        csv2dataframe()

    start = time.clock()
    df = pd.read_hdf(h5file)

    if check:
        df.to_csv('all.test.csv',
                  sep='|',
                  header=False,
                  index=False,
                  date_format='%Y-%m-%dT%H:%M:%S')

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


if __name__ == '__main__':
    # test('sacct_outputs/slurm-2013/slurm-2013-05-01.log')
    # test_alt('sacct_outputs/slurm-2014/slurm-2014-02-01.log')
    # use_concurrent()
    # use_serial()
    # csv2dataframe()
    analyze_h5_dataframe()
