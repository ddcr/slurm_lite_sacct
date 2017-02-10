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
    with open('jobcompletion.log', 'rb') as infile:
        for irec, record_line in enumerate(csv.reader(infile, delimiter=' ')):
            line_to_write = ' '.join(record_line)
            jobcomp = OrderedDict(s.split('=', 1) for s in record_line
                                  if '=' in s)
            # Inspect for trouble
            # text. In these cases when the line "record_line" is splitted,
            # The Fields Name (job name) and WorkDir sometimes contain garbled
            # there will be key-value pairs that are meaningless. We may
            # safely discard those pairs that do no not contain the fields
            # we need.
            jobid = jobcomp['JobId']
            if int(jobid) == jobids_list[0]:
                # eliminate spurious keys
                line_to_write = ' '.join(["{}={}".format(k, v) for k, v
                                          in jobcomp.items() if k in fields])
                # next troubled line
                try:
                    jobids_list.pop(0)
                    if not jobids_list:
                        # Empty list
                        break
                except IndexError:
                    break
            outfile.write(line_to_write+'\n')
