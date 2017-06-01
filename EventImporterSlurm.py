#!/usr/bin/env python
# -*- coding: utf-8 -*-
# @Author: ddcr
# @Date:   2017-04-10 20:15:22
# @Last Modified by:   ddcr
# @Last Modified time: 2017-05-31 23:50:53
#
# These snippets have been imported from HPCStats
# (https://github.com/edf-hpc/hpcstats) with
# minor customization

"""This module contains the EventImporterSlurm class."""

import MySQLdb
import _mysql_exceptions
import time
from datetime import datetime, timedelta
import ConfigParser
import StringIO
import os
import logging
import pandas as pd
from collections import defaultdict


logging.basicConfig(filename='exemplo.log',
                    filemode='w',
                    level=logging.DEBUG)
logger = logging.getLogger(__name__)


class HPCStatsException(Exception):
    """Base class for exceptions in HPCStats"""
    def __init__(self, msg):
        super(HPCStatsException, self).__init__(msg)
        self.msg = msg

    def __str__(self):
        return self.msg


class HPCStatsSourceError(HPCStatsException):
    """Class for source errors exceptions in HPCStats"""
    def __init__(self, msg):
        super(HPCStatsSourceError, self).__init__(msg)


class HPCStatsDBIntegrityError(HPCStatsException):
    """Class for DB integrity errors exceptions in HPCStats"""
    def __init__(self, msg):
        super(HPCStatsDBIntegrityError, self).__init__(msg)


class HPCStatsRuntimeError(HPCStatsException):
    """Class for runtime errors exceptions in HPCStats"""
    def __init__(self, msg):
        super(HPCStatsRuntimeError, self).__init__(msg)


class SlurmDBDConf(ConfigParser.ConfigParser, object):
    def __init__(self, filename):
        super(SlurmDBDConf, self).__init__()
        self.filename = filename

    def read(self):
        if not os.path.exists(self.filename):
            raise Exception('File "%s" does not exist' % (self.filename))
        with open(self.filename) as fd:
            stream = StringIO.StringIO("[slurm]\n" + fd.read())
            super(self.__class__, self).readfp(stream)


class Cluster(object):
    """Model class for Cluster table"""

    def __init__(self, name, cluster_id=None):
        self.cluster_id = cluster_id
        self.name = name

    def __str__(self):
        return self.name

    def __eq__(self, other):
        return self.name == other.name

    def find(self, db):
        """Search the Cluster in the database based on its name. If exactly
           one cluster matches in database, set cluster_id attribute properly
           and returns its value. If more than one cluster matches, raises
           HPCStatsDBIntegrityError. If no cluster is found, returns None.
        """
        raise NotImplementedError

    def get_nb_cpus(self, db):
        """Returns the total number of CPUs available on the cluster"""
        raise NotImplementedError

    def get_min_datetime(self, db):
        """Returns the start datetime of the oldest started and unfinished
           job on the cluster.
        """
        raise NotImplementedError

    def get_nb_accounts(self, db, creation_date):
        """Returns the total of users on the cluster whose account have been
           created defore date given in parameter.
        """
        raise NotImplementedError

    def get_nb_active_users(self, db, start, end):
        """Returns the total number of users who have run job(s) on the cluster
           between start and end datetimes in parameters.
        """
        raise NotImplementedError


class Node(object):
    """Model class for the Node table."""

    def __init__(self, name, cluster, model, partition,
                 cpu, memory, flops, node_id=None):
        self.node_id = node_id
        self.name = name
        self.cluster = cluster
        self.model = model
        self.partition = partition
        self.cpu = cpu
        self.memory = memory
        self.flops = flops

    def __str__(self):
        # return "%s/%s/%s: model: %s cpu:%d Gflops:%.2f memory:%dGB" % (
        return "%s/%s/%s: model: %s" % (
            self.cluster.name,
            self.partition,
            self.name,
            self.model,
            # self.cpu,
            # self.flops / float(1000**3),
            # self.memory / 1024**3
        )

    def __eq__(self, other):
        return other.name == self.name and other.cluster == self.cluster

    def find(self, db):
        """Search the Node in the database based on its name and cluster. If
           exactly one node matches in database, set node_id attribute properly
           and returns its value. If more than one node matches, raises
           HPCStatsDBIntegrityError. If no node is found, returns None.
        """
        raise NotImplementedError


class Event(object):
    """Model class for the Event table."""
    def __init__(self, cluster, node,
                 nb_cpu,
                 start_datetime,
                 end_datetime,
                 event_type,
                 reason,
                 event_id=None):

        self.event_id = event_id
        self.cluster = cluster
        self.node = node
        self.nb_cpu = nb_cpu
        self.start_datetime = start_datetime
        self.end_datetime = end_datetime
        self.event_type = event_type
        self.reason = reason

    def __str__(self):
        return "event on node %s[%s]/%d (%s) : %s -> %s" % (
            self.node,
            self.cluster,
            self.nb_cpu,
            self.event_type,
            self.start_datetime,
            self.end_datetime
        )

    def __eq__(self, other):
        return self.node == other.node and \
            self.nb_cpu == other.nb_cpu and \
            self.start_datetime == other.start_datetime and \
            self.end_datetime == other.end_datetime and \
            self.event_type == other.event_type and \
            self.reason == other.reason

    def find(self, db):
        """Search the Event in the database based on the node, the cluster,
           the start and the end datetime. If exactly one event matches in
           database, set event_id attribute properly and returns its value.
           If more than one event matches, raises HPCStatsDBIntegrityError.
           If no event is found, returns None.
        """
        raise NotImplementedError

    def save(self, db):
        """Insert Event in database. You must make sure that the Event does not
           already exist in database yet (typically using Event.find() method
           else there is a risk of future integrity errors because of
           duplicated events. If event_id attribute is set, it raises
           HPCStatsRuntimeError.
        """
        raise NotImplementedError

    def update(self, db):
        """Update the Event in DB. The event_id attribute must be set for the
           Event, either by passing this id to __init__() or by calling
           Event.find() method previously. If event_id attribute is not set, it
           raises HPCStatsRuntimeError.
        """
        raise NotImplementedError

    def merge_event(self, event):
        """Set Event end datetime equals to event in parameter end datetime.
        """
        self.end_datetime = event.end_datetime


class Importer(object):
    def __init__(self, config, cluster):
        self.config = config
        self.cluster = cluster


class EventImporter(Importer):
    def __init__(self, config, cluster):
        super(EventImporter, self).__init__(config, cluster)

        # events loaded from slurm_acct_db
        self.events = None


class EventImporterSlurm(EventImporter):
    """This EventImporter imports Events from a cluster Slurm accounting
       database.
    """

    def __init__(self, config, cluster):

        super(EventImporterSlurm, self).__init__(config, cluster)

        section = "slurm"

        self._dbhost = 'localhost'
        self._dbport = int(config.get(section, 'accountingstorageport'))
        self._dbname = config.get(section, 'accountingstorageloc')
        self._dbuser = config.get(section, 'accountingstorageuser')
        self._dbpass = config.get(section, 'accountingstoragepass', None)
        self.conn = None
        self.cur = None
        self.db_creation_time = None
        self.db_update_time = None

    def connect_db(self):
        """Connect to cluster Slurm database and set conn/cur attribute
           accordingly. Raises HPCStatsSourceError in case of problem.
        """

        try:
            conn_params = {
                'host': self._dbhost,
                'user': self._dbuser,
                'db': self._dbname,
                'port': self._dbport,
            }
            if self._dbpass is not None:
                conn_params['passwd'] = self._dbpass
            conn_params['unix_socket'] = '/var/lib/mysql/mysql.sock'

            self.conn = MySQLdb.connect(**conn_params)
            self.cur = self.conn.cursor()
        except _mysql_exceptions.OperationalError as error:
            raise HPCStatsSourceError("connection to Slurm DBD MySQL",
                                      "failed: %s" % (error))

    def disconnect_db(self):
        """Disconnect from cluster Slurm database."""

        self.cur.close()
        self.conn.close()

    def check(self):
        """Check if cluster Slurm database is available for connection."""

        self.connect_db()
        self.db_creation_time = self._db_creation_time()
        self.db_update_time = self._db_update_time()
        self.disconnect_db()

    def load(self):
        """Load Events from Slurm DB and store them into self.events
           list attribute.
        """

        self.connect_db()

        # Define the datetime from which the search must be done in Slurm DB.
        # Variables here are float timestamps since epoch, not Python Datetime
        # objects.
        #
        # The datetime used for search is the start datetime of the oldest
        # unfinished event if it exists. Else it is the end datetime of the
        # last finished event. Else (no event in DB) it is epoch.
        #
        # Consider the following status in HPCstats DB, where:
        #  - e1 is the last finished event
        #  - e2 is the oldest unfinised event
        #
        #     t0   t1     t2
        # e1  +-----------+
        # e2       +
        #
        # -> Result: t1 because we have to search for e2 in source to check if
        #               it finally has a end datetime as of now.
        #
        #     t0   t1     t2
        # e1       +------+
        # e2  +
        #
        # -> Result: t0 because we have to search for e2 in source to check if
        #               it finally has a end datetime as of now.
        #
        #     t0   t1     t2
        # e1  +----+
        # e2              +
        #
        # -> Result: t2 because there is no reason to find in source a new
        #               event that would have started between t1 and t2.
        #
        #     t0   t1
        # e1  +----+
        # e2  (none)
        #
        # -> Result: t1
        #
        # e1  (none)
        # e2  (none)
        #
        # -> Result: epoch

        # Domingos Rodrigues:
        # I do not have an HPCStats DB, so I do not need
        # to check from when to search events.

        # datetime_end_last_event = get_datetime_end_last_event(self.db,
        #                                                       self.cluster)
        # datetime_start_oldest_unfinished_event = \
        #     get_datetime_start_oldest_unfinished_event(self.db, self.cluster)

        # if datetime_start_oldest_unfinished_event:
        #     datetime_search = datetime_start_oldest_unfinished_event
        # elif datetime_end_last_event:
        #     datetime_search = datetime_end_last_event
        # else:

        # search since epoch by default
        datetime_search = datetime.fromtimestamp(0)

        # get all events since datetime_search
        self.events = self.get_new_events(datetime_search)

    def _db_creation_time(self):
        """Returns the timestamp when cluster_db_acct was loaded
           in this server.
        """
        req = """
               SELECT create_time
                 FROM INFORMATION_SCHEMA.TABLES
                WHERE table_name = 'cluster_event_table'
              """

        self.cur.execute(req)
        row = self.cur.fetchone()
        if row[0] is not None:
            return row[0]  # datetime.fromtimestamp(row[0])
        else:
            return None

    def _db_update_time(self):
        """Returns the timestamp about the latest INSERT, DELETE,
           UPDATE of a table of DB
        """
        req = """
               SELECT update_time
                 FROM INFORMATION_SCHEMA.TABLES
                WHERE table_name = 'cluster_event_table'
              """

        self.cur.execute(req)
        row = self.cur.fetchone()
        if row[0] is not None:
            return row[0]  # datetime.fromtimestamp(row[0])
        else:
            return None

    def _is_old_schema(self):
        """Returns True if it detects the old-schema (<15.08)in the SlurmDBD
           database, False otherwise.
        """

        # req = "SHOW COLUMNS FROM %s_event_table LIKE 'cpu_count'" \
        #       % (self.cluster.name)
        req = "SHOW COLUMNS FROM cluster_event_table LIKE 'cpu_count'"
        self.cur.execute(req)
        row = self.cur.fetchone()
        if row is not None:
            logger.debug("detected old-schema <15.08 in event table")
            return True
        logger.debug("detected new-schema >=15.08 in event table")
        return False

    def get_new_events(self, start):
        """Get all new Events from Slurm DB since start datetime. Parameter
           start must be a valid datetime. Returns a list of Events. The list
           is empty if none found.
        """

        logger.info("searching new events since %s", str(start))
        timestamp = int(round(time.mktime(start.timetuple())))

        old_schema = self._is_old_schema()

        events = []

        if old_schema is True:
            cpu_field = 'cpu_count'
        else:
            cpu_field = 'tres'

        req = """
               SELECT period_start,
                      period_end,
                      node_name,
                      %s,
                      state,
                      reason
                 FROM cluster_event_table
                WHERE node_name <> ''
                  AND period_start >= %%s
                ORDER BY period_start
              """ % (cpu_field)
        params = (timestamp, )

        self.cur.execute(req, params)

        while (1):
            row = self.cur.fetchone()
            if row is None:
                break

            datetime_start = datetime.fromtimestamp(row[0])

            timestamp_end = row[1]
            if timestamp_end == 0:
                # This means the event wasnt closed by the time I
                # got this database from cluster veredas (at LCC-CENAPAD).
                # As a first approximation I can use the time when this
                # database was dumped from the cluster. It is better than
                # using the present timestamp [ie, now()]
                #
                # Houston, we have a problem ...
                # InnoDB tables always have an update_time of NULL, no matter
                # what (https://bugs.mysql.com/bug.php?id=14374)
                # I have to put my hand here ...
                #
                # datetime_end = self.db_update_time
                datetime_end = datetime(2017, 3, 28, 0, 0, 0)
            else:
                datetime_end = datetime.fromtimestamp(timestamp_end)

            node_name = row[2]
            node = Node(node_name, self.cluster,
                        None, None, None, None, None)
            # searched_node = Node(node_name, self.cluster,
            #                      None, None, None, None, None)
            # node = self.app.arch.find_node(searched_node)
            # if node is None:
            #     raise HPCStatsSourceError(
            #         "event node %s not found in loaded nodes" % (node_name)
            #     )

            if old_schema is True:
                nb_cpu = row[3]
            else:
                # nb_cpu = extract_tres_cpu(row[3])
                # if nb_cpu == -1:
                raise HPCStatsSourceError(
                    "unable to extract cpu_count from event tres"
                )

            event_type = EventImporterSlurm.txt_slurm_event_type(row[4])
            reason = row[5]

            event = Event(node=node,
                          cluster=self.cluster,
                          nb_cpu=nb_cpu,
                          start_datetime=datetime_start,
                          end_datetime=datetime_end,
                          event_type=event_type,
                          reason=reason)
            events.append(event)

        return self.merge_successive_events(events)

    def merge_successive_events(self, events):
        """Merge successive Events in the list. For example, if the list
           contains 2 events on node A from X to Y and from Y to Z, this method
           will merge them into one event on node A from Y to Z. Ex::

               [ { node: N1, reason: R1, start: X, end Y },
                 { node: N1, reason: R1, start: Y, end Z } ]
               -> [ { node: N1, reason: R1, start: X, end: Z } ]
        """

        event_index = 0
        nb_events = len(events)
        logger.debug("merge: nb_events: %d", nb_events)

        # iterate over the list of new events
        while event_index < nb_events - 1:

            event = events[event_index]
            logger.debug("merge: current event_index: %d", event_index)
            # find the next event in the list for the same node
            next_event_index = event_index + 1

            while next_event_index < nb_events:
                next_event = events[next_event_index]
                if next_event.node == event.node and \
                   next_event.start_datetime == event.end_datetime and \
                   next_event.event_type == event.event_type:
                    break
                else:
                    next_event_index += 1

            logger.debug("merge: computed next_event_index: %d",
                         next_event_index)
            # If search index is at the end of the list, it means the next
            # event has not been found in the list..
            if next_event_index == nb_events:
                logger.debug("no event to merge: %d (%s, %s -> %s)",
                             event_index,
                             event.node,
                             event.start_datetime,
                             event.end_datetime)
                # we can jump to next event in the list
                event_index += 1
            else:
                next_event = events[next_event_index]
                logger.debug("merging %s (%d) with %s (%d)",
                             event,
                             event_index,
                             next_event,
                             next_event_index)
                event.end_datetime = next_event.end_datetime
                # remove the next event out of the list
                events.pop(next_event_index)
                nb_events -= 1
        return events

    def update(self):
        """Update Events in DB."""
        raise NotImplementedError

    @staticmethod
    def txt_slurm_event_type(reason_uid):
        """Convert reason_uid integer that holds node state in Slurm bitmap
           convention to string representing this state into human readable
           format.
        """

        states = []

        slurm_base_states = [
            (0x0000, 'UNKNOWN'),
            (0x0001, 'DOWN'),
            (0x0002, 'IDLE'),
            (0x0003, 'ALLOCATED'),
            # (0x0004, 'ERROR'),
            # (0x0005, 'MIXED'),
            # (0x0006, 'FUTURE'),
            # (0x0007, 'END'),
            (0x0004, 'FUTURE'),
            (0x0005, 'END'),
        ]
        slurm_extra_states = [
            # (0x0010, 'NET'),
            # (0x0020, 'RES'),
            # (0x0040, 'UNDRAIN'),
            (0x0100, 'RESUME'),
            (0x0200, 'DRAIN'),
            (0x0400, 'COMPLETING'),
            (0x0800, 'NO_RESPOND'),
            (0x1000, 'POWER_SAVE'),
            (0x2000, 'FAIL'),
            (0x4000, 'POWER_UP'),
            (0x8000, 'MAINT'),
        ]

        for hexval, txtstate in slurm_base_states:
            if (reason_uid & 0xf) == hexval:
                states.append(txtstate)

        for hexval, txtstate in slurm_extra_states:
            if reason_uid & hexval:
                states.append(txtstate)

        if not len(states):
            states.append('UNKNOWN')

        return '+'.join(states)


def test0(events):
    """TEST0
    """
    import pprint

    pp = pprint.PrettyPrinter(indent=4)
    intervals = defaultdict(list)
    for e in events:
        ev_type = e.event_type
        # exclude UNKNOWN event types (usually the reason is 'cold-start'
        # and of very short duration time)
        if ev_type in ['UNKNOWN']:
            continue

        start_time = e.start_datetime
        end_time = e.end_datetime
        dt = (end_time - start_time).total_seconds()
        reason = e.reason
        node = e.node.name

        # mystr = ''.join(('Node={0} (Reason={1}) ',
        #                  'Dur={2} Start={3} End={4}'))
        # if reason in ['(null)', 'cold-start']:
        #     print mystr.format(node, reason, dt, start_time, end_time

        # intervals[dt].append((node, reason, start_time, end_time))
        intervals[start_time].append((node, reason, dt))
    pp.pprint(intervals)


def test1(events):
    """TEST1
    """
    edata = defaultdict(list)
    for e in events:
        ev_type = e.event_type
        # if ev_type in ['UNKNOWN']:
        #     continue
        start_time = e.start_datetime
        end_time = e.end_datetime
        dt = (end_time - start_time).total_seconds()
        node = e.node.name

        if ev_type in ['UNKNOWN']:
                edata[dt].append(node)

    for k in sorted(edata.iterkeys()):
        # print 'dt = {0}'.format(k)
        # for e in edata[k]:
        #     print '\t {0}'.format(e)
        k_1 = timedelta(seconds=k)
        print '{0}: {1} ({2})'.format(k_1, 8*len(edata[k]), edata[k])


def collect_events_to_df(events):
    """For each node collects the events into a convenient pandas DataFrame
        sorted by the starting time of the event.
        Merge all dataframes in a python dictionary.
    """
    import natsort

    node_events = defaultdict(list)
    col_names = ['ts', 'te', 'cpu', 'event_type', 'reason']
    for e in events:
        ev_type = e.event_type
        start_time = e.start_datetime
        end_time = e.end_datetime
        reason = e.reason
        node_name = e.node.name

        t = (start_time, end_time, 8, ev_type, reason)

        node_events[node_name].append(t)

    # now for each node sort list of tuples by start_time
    nodes_df = dict()
    for node_name in natsort.natsorted(node_events.iterkeys()):
        sort_ev = sorted(node_events[node_name],
                         key=lambda el: (el[0], el[1]))
        nodes_df[node_name] = pd.DataFrame(sort_ev, columns=col_names)
    return(nodes_df)


def import_events_from_mysql(config):
    """read events from mysql database
    """
    cluster = Cluster('veredas')
    eventobj = EventImporterSlurm(config, cluster)
    eventobj.check()
    eventobj.load()
    events = eventobj.events
    return(events)


def df_merge_events1(df_node):
    """Merge events when their time spans intersect or touch
    """
    t_limits = df_node[['ts', 'te']]
    t_span = list(t_limits.itertuples(index=False))
    lm = len(t_span)
    indices_deleted = []
    for i in xrange(lm):
        if i in indices_deleted:
            raise 'a[{}] was deleted'.format(i)
        for j in xrange(i+1, lm):
            print 'indices_deleted = {}'.format(indices_deleted)
            print 'a[{0}] against a[{1}]'.format(i, j)
            if j in indices_deleted:
                raise 'a[{}] was deleted'.format(j)

            a = t_span[i]
            b = t_span[j]

            if allen.takesplaceafter(a, b):
                raise 'Sanity check: The DataFrame is not properly sorted'

            if allen.takesplacebefore(a, b):
                """no need to pursue further: all incoming intervals
                   obey this rule, so no further interval union is
                   going to be necessary"""
                print '\n\tNext a'
                print '-'*120
                break

            delete_a = False
            for check_method in ['containedby', 'contains', 'finishedby',
                                 'finishes', 'isequalto', 'meets', 'metby',
                                 'overlapedby', 'overlaps', 'startedby',
                                 'starts']:
                check = getattr(allen, check_method)
                if check(a, b):
                    print '\ta[{0}]=({1}, {2}) a[{3}]=({4}, {5})'.format(
                        i, a.ts, a.te, j, b.ts, b.te)

                    if check_method in ['meets', 'overlaps',
                                        'finishedby', 'starts']:
                        print '\t{0} {1} {2}'.format(i, check_method, j)
                        print '\t\t new a[{0}]=({1}, {2})'.format(
                            j, a.ts, b.te)
                        print '\t\t a[{0}] is enlarged'.format(j)
                        print '\t\t a[{0}] is DELETED'.format(i)
                        delete_a = True
                        indices_deleted.append(i)
                    elif check_method == 'contains':
                        print '\t{0} {1} {2}'.format(i, check_method, j)
                        print '\t\t new a[{0}]=({1}, {2})'.format(
                            j, a.ts, a.te)
                        print '\t\t a[{0}] COPIED TO a[{1}]'.format(i, j)
                        print '\t\t a[{0}] is DELETED'.format(i)
                        delete_a = True
                        indices_deleted.append(i)
                    else:
                        raise NotImplementedError
                    print '-'*120
            if delete_a:
                break


def df_merge_events2(df_node):
    """Merge events when their time spans intersect or touch
    """

    lm1 = len(df_node)-1
    indices_deleted = []
    for i in xrange(lm1):
        i1 = i+1

        if i in indices_deleted:
            raise 'a[{}] was deleted'.format(i)
        if i1 in indices_deleted:
            raise 'a[{}] was deleted'.format(i1)

        print 'indices_deleted = {}'.format(indices_deleted)
        print 'a[{0}] against a[{1}]'.format(i, i1)

        a = tuple(df_node.iloc[i, [0, 1]])
        b = tuple(df_node.iloc[i1, [0, 1]])

        if allen.takesplaceafter(a, b):
            raise 'Sanity check: The DataFrame is not properly sorted'

        for check_method in ['containedby', 'contains', 'finishedby',
                             'finishes', 'isequalto', 'meets', 'metby',
                             'overlapedby', 'overlaps', 'startedby',
                             'starts']:
            check = getattr(allen, check_method)

            if check(a, b):
                print '\ta[{0}]=({1}, {2}) a[{3}]=({4}, {5})'.format(
                    i, a[0], a[1], i1, b[0], b[1])

                if check_method in ['meets', 'overlaps',
                                    'finishedby', 'starts']:
                    print '\t{0} {1} {2}'.format(i, check_method, i1)
                    print '\t\t new a[{0}]=({1}, {2})'.format(
                        i1, a[0], b[1])
                    df_node.loc[i1, 'ts'] = df_node.loc[i, 'ts']
                    print '\t\t a[{0}] is enlarged'.format(i1)
                    print '\t\t a[{0}] is DELETED'.format(i)
                    indices_deleted.append(i)
                elif check_method == 'contains':
                    print '\t{0} {1} {2}'.format(i, check_method, i1)
                    print '\t\t new a[{0}]=({1}, {2})'.format(
                        i1, a[0], a[1])
                    df_node.loc[i1] = df_node.loc[i]
                    print '\t\t a[{0}] COPIED TO a[{1}]'.format(i, i1)
                    print '\t\t a[{0}] is DELETED'.format(i)
                    indices_deleted.append(i)
                else:
                    raise NotImplementedError
        print '-'*120

    # now remove rows and reindex
    df_node.drop(df_node.index[indices_deleted], inplace=True)


if __name__ == '__main__':
    import cPickle as pickle
    import natsort
    from allen_interval_algebra import AllenIntervalRules as allen

    # node_df = events_df(events)['veredas2']
    # result = pd.Series(0,
    #                   index=pd.date_range(node_df.ts.min(), node_df.te.max(),
    #                                        freq='H'))

    events_pkl_file = 'dic_of_node_events.pkl'
    if not os.path.exists(events_pkl_file):
        print "'{0}' not found ... recreating ".format(events_pkl_file)
        config = SlurmDBDConf('etc/slurm/slurm.conf')
        config.read()

        # import events and serialie
        events = import_events_from_mysql(config)
        # save them
        nodes_df = collect_events_to_df(events)
        with open(events_pkl_file, 'wb') as fout:
            pickle.dump(nodes_df, fout)

    with open(events_pkl_file, 'rb') as fin:
        nodes_df = pickle.load(fin)

    tdelta_bins = [pd.Timedelta(0), pd.Timedelta('1 minute'),
                   pd.Timedelta('1 hour'), pd.Timedelta('1 day'),
                   pd.Timedelta('7 days'), pd.Timedelta('30 days'),
                   pd.Timedelta('60 days')]
    tdelta_categories = ['<1m', '<1h', '<1d', '<1w', '<30d', '<60d']

    for node_name in natsort.natsorted(nodes_df.iterkeys()):
        print '{}'.format(node_name)

        # determine event duration
        df_node = nodes_df[node_name]
        diff_tv = df_node.te-df_node.ts
        # do binning
        df_node['dt'] = pd.cut(diff_tv,
                               bins=tdelta_bins,
                               labels=tdelta_categories)

        # remove events with duration less than one minute
        df_node = df_node[~(df_node.dt == '<1m')]
        df_node.reset_index(inplace=True, drop=True)

        df_node.to_csv('processed.csv',
                       mode='a',
                       header=False,
                       sep='\t')
        # Now merge/delete events in the timeline
        df_merge_events2(df_node)
        df_node.to_csv('processed.n.csv',
                       mode='a',
                       header=False,
                       sep='\t')

        # with pd.option_context('display.max_rows', None,
        #                        'display.max_columns', 6,
        #                        'display.width', 132):
        #     print df_node[['ts', 'te', 'dt', 'reason']]
        print '='*80