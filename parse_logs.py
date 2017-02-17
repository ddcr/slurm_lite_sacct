#!/usr/bin/env python
#
import csv
import arrow
import scandir
import os

LOGPATH = os.path.abspath(os.path.join(os.path.dirname(__file__),
                          'sacct_outputs'))


def convert_to_d_h_m_s(seconds):
    """Return the tuple of days, hours, minutes and seconds."""
    minutes, seconds = divmod(seconds, 60)
    hours, minutes = divmod(minutes, 60)
    days, hours = divmod(hours, 24)
    return '{0}-{1}:{2}:{3}'.format(days, hours, minutes, seconds)


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


def parse_log_worker(csvfile, tzinfo='America/Sao_Paulo'):
    """Summary

    Args:
        flog (TYPE): Description

    Returns:
        TYPE: Description
    """
    date_fields = ["submit", "eligible", "start", "end"]
    return_lines = []
    try:
        with open(csvfile, 'rb') as fd:
            for record in csv.reader(fd, delimiter='|'):
                datetimes = \
                    [arrow.get(dt).replace(tzinfo=tzinfo).timestamp
                     for dt in record[9:13]]
                slurm_dates = dict((k, v) for k, v in zip(date_fields,
                                                          datetimes))
                if slurm_dates["eligible"] < slurm_dates["submit"]:
                    diff = slurm_dates["submit"] - slurm_dates["eligible"]
                    line = '{0} {1}'.format('|'.join(record),
                                            convert_to_d_h_m_s(diff))
                    return_lines.append(line)
        return(return_lines)
    except Exception as e:
        return("Log file {0} error parsing ({1})".format(csvfile, e))


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
    with ThreadPoolExecutor(max_workers=2) as executor:
        wait_for = [executor.submit(parse_log_worker, file_name)
                    for file_name in allfiles]
        for f in concurrent.futures.as_completed(wait_for, timeout=None):
            error = f.exception()
            if error is None:
                # print('Main: {0} {1}'.format(f, f.result()))
                reduce_results.extend(f.result())
            else:
                print('Main: error: {0} {1}'.format(f, error))

    with open('parse_logs_reduce.csv', 'wb') as fout:
        writer = csv.writer(fout)
        for row in reduce_results:
            writer.writerow(row)


if __name__ == '__main__':
    # test('slurm-2013-05-01.log')
    use_concurrent()
    # use_serial()
