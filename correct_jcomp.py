#!/usr/bin/env python
#
import csv
from collections import OrderedDict

fields = ["JobId", "UserId", "GroupId", "Name", "JobState", "Partition",
          "TimeLimit", "StartTime", "EndTime", "NodeList", "NodeCnt",
          "ProcCnt", "WorkDir"]

with open('list_of_jobids_problem.txt', 'rb') as fd:
    jobids_list = [int(x) for x in fd.readlines()]

with open('jobcompletion_corrected.log', 'wb') as outfile:
    with open('update_job.sql', 'wb') as sqlfile:
        with open('undo_update_job.sql', 'wb') as undofile:
            with open('jobcompletion.log', 'rb') as infile:
                for record in csv.reader(infile, delimiter=' '):
                    line = ' '.join(record)
                    jobcomp = OrderedDict(s.split('=', 1) for s in record
                                          if '=' in s)
                    # Inspect for troubled lines where the fields "Name"
                    # and WorkDir may contain garbled strings that pose
                    # difficulties to parsing.
                    jobid = jobcomp['JobId']
                    if int(jobid) == jobids_list[0]:
                        # take whole substring between Name=... JobState=...
                        pos_start = line.find('Name=')+5
                        pos_end = line.find('JobState')-1
                        str_orig = line[pos_start:pos_end]
                        str_repl = jobcomp['Name']
                        # print line
                        # print str_orig
                        # print str_repl
                        # print '='*80
                        # eliminate spurious key=value pairs from
                        # the python dictionary "jobcomp", i.e.
                        # take only the keys listed in 'fields'
                        line = ' '.join(["{}={}".format(k, v) for k, v
                                         in jobcomp.items() if k in fields])

                        mysql1 = 'UPDATE job_table\nSET name = '
                        mysql2 = 'REPLACE(name, \"{0}\", \"{1}\")\n'
                        mysql3 = 'WHERE jobid={0};\n'
                        mysql_cmd = ''.join([mysql1,
                                             mysql2.format(str_orig, str_repl),
                                             mysql3.format(jobid)])

                        undo_mysql_cmd = ''.join([mysql1,
                                                  mysql2.format(str_repl,
                                                                str_orig),
                                                  mysql3.format(jobid)])
                        # mysql: table `job_table` has no WorkDir column, so
                        # ignore it
                        if str_orig != str_repl:
                            sqlfile.write(mysql_cmd)
                            undofile.write(undo_mysql_cmd)
                        # eliminate spurious keys
                        line = ' '.join(["{}={}".format(k, v) for k, v
                                         in jobcomp.items() if k in fields])
                        # next troubled line
                        try:
                            jobids_list.pop(0)
                            if not jobids_list:
                                # Empty list
                                break
                        except IndexError:
                            break
                    outfile.write(line+'\n')
