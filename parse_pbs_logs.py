#!/usr/bin/env python
#
import scandir
import os
import pandas as pd
import click
import inspect
from collections import OrderedDict
import numpy as np
import subprocess
import time
import textwrap


LOGPATH = os.path.abspath(os.path.join(os.path.dirname(__file__),
                          'accounting'))

# record_type:
# A      Job was aborted by the server.
# B      Beginning of reservation period.
#        The 'message_text' field contains
#        information describing the specified
#        advance reservation.
# C      Job was checkpointed and held.
# D      Job was deleted by request.
#        The 'message_text' will contain
#        requester=user@host to identify
#        who deleted the job.
# E      Job ended (terminated execution). The 'message_text' field
#        contains information about the job. The end of job accounting
#        record will not be written until all of the resources have been
#        freed. The 'end' entry in the job end record will include the
#        time to stage out files, delete files, and free the resources. This
#        will not change the recorded 'walltime' for the job. Possible
#        information is included in PBSPRO_FIELD_PARAMS_NAMES
# F      Resource reservation period finished.
# K      Scheduler or server requested removal of the reservation. The
#        message_text field contains: requester=user@host
#        to identify who deleted the resource reservation.
#        k Resource reservation terminated by ordinary client - e.g. an
#        owner issuing a pbs_rdel command. The message_text
#        field contains: requester=user@host to identify who
#        deleted the resource reservation.
# Q      Job entered a queue. The 'message_text' contains queue=name
#        identifying the queue into which the job was placed. There will
#        be a new Q record each time the job is routed or moved to a new
#        (or the same) queue.
# R      Job was rerun.
# S      Job execution started.
# T      Job was restarted from a checkpoint file.
# U      Created unconfirmed resources reservation on Server. The
#        'message_text' field contains requester=user@host to
#        identify who requested the resources reservation.
# Y      Resources reservation confirmed by the Scheduler. The
#        'message_text' field contains the same item (items) as in a U
#        record type.

PBS_XDMOD_COLUMNS = [
    'job_id',
    'job_array_index',
    'host',
    'queue',
    'user',
    # 'groupname',  # group
    'ctime',
    'qtime',
    'start',
    'end',
    'etime',
    'Exit_status',
    'session',
    'requestor',
    'jobname',
    'owner',
    'account',
    'session_id',
    'error_path',
    'output_path',
    'exec_host',
    'resources_used.vmem',
    'resources_used.mem',
    'resources_used.walltime',
    # 'resources_used_nodes', # exec_host
    # 'resources_used_cpus', # exec_host
    'resources_used.cput',
    'Resource_List.nodes',
    'Resource_List.procs',
    'Resource_List.neednodes',
    'Resource_List.pcput',
    'Resource_List.cput',
    'Resource_List.walltime',
    'Resource_List.ncpus',
    'Resource_List.nodect',
    'Resource_List.mem',
    'Resource_List.pmem',
    # 'node_list'  # exec_host
]

PBSPRO_FIELD_NAMES = ['datetime', 'record_type',
                      'id_string', 'message_text']
PBSPRO_FIELD_TYPES = OrderedDict([('datetime', object),
                                  ('record_type', np.str_),
                                  ('id_string', np.str_),
                                  ('message_text', np.str_)])
PBSPRO_MESSAGE_TEXT_NAMES = [
    'user',
    'group',
    'account',
    'jobname',
    'queue',
    'resvname',  # The name of the resource
                 # reservation, if applicable.
    'resvID',  # The ID of the resource reservation, if applicable.
    'ctime',  # Time in seconds when job was created
              # (first submitted)
    'qtime',  # Time in seconds when job was queued
              # into current queue.
    'etime',  # Time in seconds when job became
              # eligible to run, i.e. was enqueued
              # in an execution queue and was in the
              # 'Q' state. Reset when a job moves
              # queues. Not affected by qaltering.
    'start',  # Time when job execution started.
    'array_indices',
    'exec_host',  # List of each vnode with vnode-level,
                  # consumable resources allocated from
                  # that vnode.
                  # exec_host=vnodeA/P*C [+vnodeB/P * C]
                  # where P is a unique index and C is
                  # the number of CPUs assigned to
                  # the job, 1 if omitted.
    'exec_vnode',
    #  ===================================================================
    #  Resource_List.resource=limit List of the specified resource limits.
    'Resource_List.mppnodes',
    'Resource_List.ncpus',
    'Resource_List.nodect',  # Deprecated. Number of chunks in resource
                             # request from selection directive, or number
                             # of vnodes requested from node specification.
                             # Otherwise defaults to value of 1. Read-only.
                             # Type: integer
    'Resource_List.nodes',
    'Resource_List.place',  # The place statement controls how the job is
                            # placed on the vnodes from which resources
                            # may be allocated for the job:
                            #
                            # free            Place job on any vnode(s).
                            # pack            All chunks will be taken
                            #                 from one host.
                            # scatter         Only one chunk will be taken
                            #                 from a host.
                            # exclusive       Only this job uses the
                            #                 vnodes chosen.
                            # shared          This job can share the
                            #                 vnodes chosen.
                            # group=resource  Chunks will be grouped according
                            #                 to a resource. All vnodes in
                            #                 the group must have a common
                            #                 value
                            #                 for the resource, which can be
                            #                 either the built-in resource host
                            #                 or a site-defined vnode level
                            #                 resource.
    'Resource_List.select',
    'Resource_List.walltime',
    #  ===================================================================
    'session',  # Session number of job.
    # 'alt_id' # NOT SUPPORTED
    'end',  # Time in seconds when job ended execution.
    'Exit_status',  # The exit status of the job.
    #  ===================================================================
    # resources_used.RES=value  Provides the aggregate amount (value) of
    #                           specified resource RES used during the
    #                           duration of the job.
    'resources_used.cput',  # Amount of CPU time used by the job for all
                            # processes on all vnodes. Establishes a job
                            # resource limit. Non-consumable. Type: time.
    'resources_used.mem',  # Amount of physical memory i.e. workingset
                           # allocated to the job, either job-wide or
                           # vnode-level. Consumable. Type: size.
    'resources_used.walltime',  # Actual elapsed time during which the
                                # job can run. Establishes a job resource
                                # limit. Non-consumable. Type: time.
                                # Default: 5 years
    'resources_used.vmem',  # Amount of virtual memory for use by all
                            # concurrent processes in the job. Establishes
                            # a job resource limit, or when used within a
                            # chunk, establishes a per-chunk limit.
                            # Consumable. Type: size
    'resources_used.ncpus',  # Number of processors requested. Cannot be
                             # shared across vnodes. Consumable. Type: integer.
    'resources_used.cpupercent'  # MOM calculates an integer value called
                                 # cpupercent each polling cycle. This is
                                 # a moving weighted average of CPU usage
                                 # for the cycle, given as the average
                                 # percentage usage of one CPU. For example,
                                 # a value of 50 means that during a certain
                                 # period, the job used 50 percent of one CPU.
                                 # A value of 300 means that during the period,
                                 # the job used an average of three CPUs.
]
PBSPRO_MESSAGE_TEXT_TYPES = OrderedDict(
    [('user', np.str_),
     ('group', np.str_),
     ('account', np.str_),
     ('jobname', np.str_),
     ('queue', np.str_),
     ('resvname', np.str_),
     ('resvID', np.str_),
     ('ctime', np.int64),
     ('qtime', np.int64),
     ('etime', np.int64),
     ('start', np.int64),
     ('array_indices', np.str_),
     ('exec_host', np.str_),
     ('exec_vnode', np.str_),
     ('Resource_List.mppnodes', np.str_),
     ('Resource_List.ncpus', np.int64),
     ('Resource_List.nodect', np.int64),
     ('Resource_List.nodes', np.str_),
     ('Resource_List.place', np.str_),
     ('Resource_List.select', np.str_),
     ('Resource_List.walltime', object),
     ('session', np.int64),
     ('end', np.int64),
     ('Exit_status', np.int64),
     ('resources_used.cput', object),
     ('resources_used.mem', np.str_),
     ('resources_used.walltime', object),
     ('resources_used.vmem', np.str_),
     ('resources_used.ncpus', np.int64),
     ('resources_used.cpupercent', np.int64)])
empty_row = dict((k, np.nan) for k in PBSPRO_MESSAGE_TEXT_NAMES)
pd.options.display.max_rows = 5


class MyError(Exception):
    pass


@click.group()
def cli():
    pass


@cli.command('test')
@click.argument("csvfile", default='accounting/20101124')
def test(csvfile):
    """
    Testing general functions
    """
    csv_kw = {'parse_dates': [0],
              'infer_datetime_format': True,
              'header': None,
              'delimiter': ';',
              'engine': 'c',
              'dtype': PBSPRO_FIELD_TYPES,
              'names': PBSPRO_FIELD_TYPES.keys()}

    df = pd.read_csv(csvfile, **csv_kw)
    kwargs = {}
    # df_proc = pbs_get_message_text_kv(csvfile, df, **kwargs)
    df_proc = pbs_parse_process(csvfile, df, **kwargs)
    df_proc.to_csv('test.csv', sep='|')

    writer = pd.ExcelWriter('test.xlsx', engine='xlsxwriter')
    # write each column to a different worksheet
    # for c in df_proc.columns:
    #     if c in PBSPRO_FIELD_NAMES[:-1]:
    #         # continue
    #         df_proc[c].to_frame(name=c).\
    #             to_excel(writer, sheet_name=c, na_rep='-')
    df_proc.to_excel(writer, na_rep='-')

    col = df_proc.columns.get_loc('group')+1
    last_row = len(df_proc['group'])

    format1 = writer.book.add_format({'bg_color': '#FFC7CE',
                                      'font_color': '#9C0006'})
    worksheet = writer.sheets['Sheet1']
    worksheet.conditional_format(1, col, last_row, col,
                                 {'type': 'text',
                                  'criteria': 'containing',
                                  'value': 'bio519',
                                  'format': format1})
    writer.save()


def pbs_get_message_text_kv(csvfile, df, **kwargs):
    """Summary
        Parse PBSPro logfiles and extract all accounting keys
        from the 'message_text' field present in each line
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

    # Ignore all non-"end" events.
    df = df[df['record_type'] == 'E']
    if len(df) == 0:
        return(pd.DataFrame())
    df.index = pd.Index(range(len(df)))

    # df_split is of type Series
    df_split = df['message_text'].str.split(' ')

    list_of_keys = []
    for i, kv_items in enumerate(df_split):
        keys = [kv_item.split('=', 1)[0] for kv_item in kv_items]
        list_of_keys.append((' '.join(keys), len(keys)))

    list_of_keys_unique = list(set(list_of_keys))
    df_keys = pd.DataFrame(list_of_keys_unique, columns=['keys', 'length'])
    df_keys.index = [csvfile]*len(list_of_keys_unique)
    return(df_keys)


def pbs_parse_process(csvfile, df, **kwargs):
    """Summary
        Parse PBSPro logfile and convert data to a
        pandas DataFrame
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
    # Ignore all non-"end" events.
    df = df[df['record_type'] == 'E'].copy()
    # reset the index
    df.index = pd.Index(range(len(df)))
    if df.empty:
        return(df)

    # split the long string in 'message_text'
    # The values of the new columns of dataframe df_split
    # are strings of the type "key=value".
    # df_split is of type Series
    try:
        df_split = df['message_text'].str.split(' ')
    except Exception:
        print '-->{0}<--'.format(csvfile)
        print '-->{0}<--'.format(df)

    # initialize ditionary of values
    df_dict = OrderedDict([(k, []) for k in PBSPRO_MESSAGE_TEXT_NAMES])
    for i, message_text_line in enumerate(df_split):
        try:
            kv = dict(kv_type.split('=', 1) for kv_type in message_text_line
                      if kv_type.strip())
        except Exception:
            print '-->{0}<--'.format(csvfile)
            print '-->{0}<--'.format(message_text_line)
        kv_row = empty_row.copy()
        kv_row.update(kv)
        # row now has the missing values in the right places
        for k, v in kv_row.items():
            df_dict[k].append(v)

    # This piece of code would be correct and simpler if all entries in the
    # PBS logs had the same number of (key=value) pairs
    # =========================================================================
    # each column of df_split (of type 'key=value') is splitted further
    # for i, column in enumerate(df_split):
    #     df_kv = df_split[column].str.split('=', 1, expand=True)
    #     x = set(df_kv[0])
    #     if len(x) > 1:
    #         print 'csvfile = {0} [{1}]'.format(csvfile, x)
    #         print('Wrong!')
    #     key_header = x.pop()
    #     df[key_header] = df_kv[1].astype(
    #         PBSPRO_MESSAGE_TEXT_TYPES[key_header]
    #     )
    # =========================================================================

    df_msg_text = pd.DataFrame.from_dict(df_dict, orient='columns')
    for col in ['ctime', 'qtime', 'etime', 'start',
                'Resource_List.ncpus', 'Resource_List.nodect',
                'session', 'end', 'Exit_status',
                'resources_used.ncpus', 'resources_used.cpupercent']:
        df_msg_text[col] = pd.to_numeric(df_msg_text[col], errors='raise')

    # for col in ['Resource_List.walltime', 'resources_used.cput',
    #             'resources_used.walltime']:
    #     df_msg_text[col] = pd.to_timedelta(df_msg_text[col])

    df_final = pd.concat([df, df_msg_text], axis=1)

    return(df_final)


def parse_log_process(fn, csvfile, **kwargs):
    """Summary
        scan directory of logfiles concurrently and do some processing

    Parameters
    ----------
    csvfile : TYPE
        PBS CSV file
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
    csv_kw = {'parse_dates': [0],
              'infer_datetime_format': True,
              'header': None,
              'delimiter': ';',
              'engine': 'c',
              'names': PBSPRO_FIELD_NAMES}

    # extract key from **kwargs
    try:
        convert = kwargs.pop('convert')
    except KeyError:
        convert = False

    if csvfile.endswith('.back'):
        return(csvfile, pd.DataFrame())

    df = pd.read_csv(csvfile, **csv_kw)

    df_proc = fn(csvfile, df, **kwargs)

    return(csvfile, df_proc)


func_mapping = {
    'pbs_get_message_text_kv': pbs_get_message_text_kv,
    'pbs_parse_process': pbs_parse_process
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
              help="Task to perform on each PBSPro file",
              type=click.Choice(func_mapping.keys()),
              default='pbs_get_message_text_kv',
              callback=str2func,
              show_default=True)
@click.argument("rootdir", default='accounting')
def run_concurrent(task, rootdir):
    """
        Parallel scan/processing of a directory with SLURM logfiles
    """
    import concurrent.futures
    from concurrent.futures import ThreadPoolExecutor

    kwtask = {}
    click.echo("Task selected: {}".format(task))
    click.echo("rootdir: {}".format(rootdir))

    click.confirm('Do you want to continue?', abort=True)
    click.echo("Well done ...")

    # gather all files
    allfiles = []
    for root, dirs, files in scandir.walk(rootdir):
        for file_name in files:
            if file_name.endswith(".back") or file_name == "README":
                continue
            allfiles.append(os.path.join(root, file_name))

    # filenames are of type YYYYMMDD: sort them in chronological order
    allfiles.sort()

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
                if isinstance(res[1], pd.DataFrame) and res[1].empty:
                    continue
                reduce_results.append(res[1])
                print('Main: {0} {1}'.format(f, res[0]))
            else:
                print('Main: error: {0} {1}'.format(f, error))

        # print reduce_results
        if isinstance(reduce_results[0], set):
            pbspro_fields = set()
            for m in reduce_results:
                pbspro_fields.update(m)
            print pbspro_fields
        elif isinstance(reduce_results[0], pd.DataFrame):
            df_reduced = pd.concat(reduce_results)
            df_reduced.index = pd.Index(range(len(df_reduced)))
            # check if there are columns with NaN values and
            # declared as of integer type.
            # http://pandas.pydata.org/pandas-docs/
            #      stable/gotchas.html#support-for-integer-na
            # The lack of NaN rep in integer columns is a pandas "gotcha".
            # The usual workaround is to simply use floats
            dict_dtypes = dict(PBSPRO_FIELD_TYPES, **PBSPRO_MESSAGE_TEXT_TYPES)
            for c in df_reduced.columns:
                if df_reduced[c].isnull().values.any():
                    if dict_dtypes[c] == np.int64:
                        dict_dtypes[c] = np.float64

            df_reduced.astype(dict_dtypes)
            # sorting job entries by datetime
            df_reduced.sort_values(['datetime'], inplace=True)
            df_reduced.info()
            df_reduced.to_csv('reduced_res.csv', sep='|')


@cli.command('work')
@click.argument("csvfile", default='reduced_res.csv')
def work(csvfile):
    """
        Work on final aggregated PBSPro accounting
    """
    print 'csvfile = {}'.format(csvfile)
    # print dict(PBSPRO_FIELD_TYPES, **PBSPRO_MESSAGE_TEXT_TYPES)
    df_reduced = pd.read_csv(
        csvfile,
        sep='|',
        engine='c',
        infer_datetime_format=True,
        parse_dates=['datetime'],
        low_memory=False,
        # or
        # dtype=dict(PBSPRO_FIELD_TYPES, **PBSPRO_MESSAGE_TEXT_TYPES)
    )

    for c in ['ctime', 'etime', 'qtime', 'start', 'end']:
        df_reduced[c] = pd.to_datetime(df_reduced[c],
                                       unit='s',
                                       utc=True,
                                       errors='raise')

    def write_to_excel(df_reduced):
        """
            PRINT TO EXCEL
        """
        writer = pd.ExcelWriter('reduced_res.xlsx', engine='xlsxwriter')
        df_reduced.to_excel(writer)

        col = df_reduced.columns.get_loc('exec_host')+1
        last_row = len(df_reduced['exec_host'])

        format1 = writer.book.add_format({'bg_color': '#FFC7CE',
                                          'font_color': '#9C0006'})
        worksheet = writer.sheets['Sheet1']
        # worksheet.conditional_format(1, col, last_row, col,
        #                              {'type': 'text',
        #                               'criteria': 'containing',
        #                               'value': 'bio519',
        #                               'format': format1})
        worksheet.conditional_format(1, col, last_row, col,
                                     {'type': 'blanks',
                                      'format': format1})
        writer.save()

    def split_id_string(df_reduced):
        """
            SPLIT id_string
        """
        # split id_string. 'jobIdData' is a new DataFrame
        jobIdData = df_reduced['id_string'].str.split('.',
                                                      expand=True)
        jobIdData.columns = ["sequence", "host"]

        # check if first part is job_id.[array_index] (or job_id-array_index)
        # or job_id[]

        with pd.option_context("display.max_rows", 100,
                               "display.max_columns", 5):
            mask = jobIdData['sequence'].str.contains('[\[\]]')
            tmp = jobIdData['sequence'].str.extract(
                '(?P<job_id>\d+)\[?(?P<array_index>\d+)?\]?',
                expand=True
            )
            jobIdData['job_id'] = tmp['job_id']
            jobIdData['array_index'] = tmp['array_index']
            print jobIdData[mask]

    def check_time_problems(df_reduced):
        """
            PROBLEMS WITH times
            Here we also know that jobs with start=0
            have empty 'exec_host' fields
        """
        df_sel = df_reduced[df_reduced['etime'] > df_reduced['start']]

        col_sel = ['id_string', 'ctime', 'qtime', 'etime', 'start', 'end',
                   'exec_host']
        print df_sel[col_sel].to_string()
        print len(df_sel)

    def parse_hostlist(reduced_res):
        """
            Parse 'exec_host' data.
            Jobs with field start=0, found in routine 'check_time_problems',
            do not have 'hostlist' information (NULL) as well
        """
        def parseExecHost(exec_hosts):
            """
            Mimic Shredder/Pbs.php (OpenXDMod 6.5)
            """
            hostList = []
            nodeCpus = {}
            nodeCount = cpuCount = 0
            for vnode in exec_hosts:
                (host, cpupart) = vnode.split('/')
                ic = cpupart.split('*')
                if len(ic) == 1:
                    ic.append(1)
                for cnt in range(int(ic[1])):
                    hc = {'node': host, 'cpu': '{0}.{1}'.format(ic[0], cnt+1)}
                    hostList.append(hc)
                    if host in nodeCpus:
                        nodeCpus[host] += 1
                    else:
                        nodeCpus[host] = 1
                        nodeCount += 1
                cpuCount += int(ic[1])
            # nodeCount
            nodeCount = len(nodeCpus.keys())
            nodes = ','.join(nodeCpus.keys())
            return({'host_list': hostList,
                    'node_list': nodes,
                    'node_count': nodeCount,
                    'cpu_count': cpuCount})

        data = df_reduced['exec_host'].str.split('+', expand=False)
        with pd.option_context("display.max_rows", 20,
                               "max_colwidth", 500):
            print data.apply(lambda x: parseExecHost(x))

    def parseMemory(reduced_res):
        """
            job 5337.veredas0 (accounting/20100913) has
            no info in memory fields
        """
        def scaleMemory(x):
            """ x = [quantity, unit]
                when np.nan occurs the integer column
                is recast to float
            """
            return {
                'b': lambda: int(x[0]),
                'kb': lambda: int(x[0])*1024,
                'mb': lambda: int(x[0])*1024*1024,
                'gb': lambda: int(x[0])*1024*1024*1024,
                'tb': lambda: int(x[0])*1024*1024*1024*1024,
            }.get(x[1], lambda: np.nan)()

        for mem_col in ['resources_used.vmem', 'resources_used.mem',
                        'Resource_List.mem', 'Resource_List.mem']:
            if mem_col in df_reduced.columns:
                mask = df_reduced[mem_col].isnull()
                if mask.any():
                    print 'df_reduced[{0}] has NULLs'.format(mem_col)
                tmp = df_reduced[mem_col].str.extract(
                    '(?P<quantity>\d*\.?\d+)(?P<unit>\D+)?$',
                    expand=False
                )
                tmp = tmp.apply(scaleMemory, axis=1)
                print tmp

    def parseTime(reduced_res):
        for time_col in ['resources_used.cput', 'resources_used.walltime',
                         'Resource_List.pcput', 'Resource_List.cput',
                         'Resource_List.walltime']:
            if time_col in df_reduced.columns:
                mask = df_reduced[time_col].isnull()
                if mask.any():
                    print 'df_reduced[{0}] has NULLs'.format(time_col)
                    print 'TOTAL len(df_reduced[{0}]={1}'.format(
                        time_col, len(df_reduced)
                    )
                    print 'len(df_reduced[{0}]={1}'.format(
                        time_col, len(df_reduced[mask])
                    )

    # check_time_problems(df_reduced)
    # mask = df_reduced['exec_host'].isnull()
    # df_reduced = df_reduced[~mask]
    # parseMemory(df_reduced)
    # parseTime(df_reduced)

    # PBSPRO_FIELD_NAMES.pop()
    # PBSPRO_FIELD_NAMES.extend(PBSPRO_MESSAGE_TEXT_NAMES)
    # print set(PBS_XDMOD_COLUMNS).difference(set(PBSPRO_FIELD_NAMES))


@cli.command('shred')
@click.argument("rootdir", default='accounting')
def xdmod_shredder(rootdir):
    """
        Shred PBS files for OpenXDMod
        xdmod-shredder logs to the console
    """
    cmd_options = ['--dry-run',
                   '-r veredas',
                   '-f pbs']
    cmd_options_string = ' '.join(cmd_options)
    cmd = ['xdmod_shredder',
           '-v',
           '-d',
           cmd_options_string
           ]

    # Geting files
    dates = pd.date_range('2008-5-23', '2012-12-05', freq='D').tolist()
    dates = [os.path.join(rootdir, d.strftime('%Y%m%d')) for d in dates]
    for filename in dates:
        if not os.path.isfile(filename) or os.path.getsize(filename) == 0:
            continue
        logger = os.path.fileext
        cmd_string = '{0} -i {1}'.format(' '.join(cmd), filename)
        with open(logger, 'wb') as flog:
            try:
                subprocess.check_call(cmd_string,
                                      shell=True,
                                      stdout=flog,
                                      stderr=flog)
            except subprocess.CalledProcessError as e:
                a = textwrap.dedent("""\
                                    {0}:
                                      Error code raised: {1}
                                      Need to remove file:
                                      {2}
                                    """.format(e.cmd,
                                               e.returncode,
                                               filename))
                raise MyError(a)
            else:
                a = textwrap.dedent("""\
                                    {0}:
                                       Finished
                                    """.format(' '.join(cmd)))
            finally:
                time.sleep(2)
                return(a)


if __name__ == '__main__':
    cli()
