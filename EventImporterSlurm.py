#!/usr/bin/env python
# -*- coding: utf-8 -*-
# @Author: ddcr
# @Date:   2017-04-10 20:15:22
# @Last Modified by:   ddcr
# @Last Modified time: 2017-06-27 18:20:05
#
# These snippets have been imported from HPCStats
# (https://github.com/edf-hpc/hpcstats) with
# minor customization

"""This module contains the EventImporterSlurm class."""

import MySQLdb
import _mysql_exceptions
import time
from datetime import datetime
import ConfigParser
import StringIO
import os
import logging
import pandas as pd
# import numpy as np
from collections import defaultdict
import sys


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


def read_all(pattern='trace_dir/veredas*.csv'):
    """Reads all of the CSVs in a directory matching the file pattern
    as TimeSeries
    """
    import glob

    # result = []
    for filename in glob.iglob(pattern):
        print 'reading {}'.format(filename)
        # ts = traces.TimeSeries.from_csv(
        #     filename,
        #     time_column
        #     )
    pass


def allen(x0, x1, y0, y1):
    """ basic relations between time intervals: pmoFDs+e
    """
    if x1 < y0:
        """precedes"""
        return pd.Series(['p'])
    elif (x1 == y0):
        """meets"""
        return pd.Series(['m'])
    elif ((x0 < y0) & ((x1 > y0) & (x1 < y1))):
        """overlaps"""
        return pd.Series(['o'])
    elif ((y1 == x1) & (y0 > x0)):
        """finishedby"""
        return pd.Series(['F'])
    elif ((y0 > x0) & (y1 < x1)):
        """contains"""
        return pd.Series(['D'])
    elif ((x0 == y0) & (x1 < y1)):
        """starts"""
        return pd.Series(['s'])
    elif (x0 == y0) & (x1 == y1):
        """equals"""
        return pd.Series(['e'])
    else:
        return None


def allen_converse(x0, x1, y0, y1):
    """ basic relations between time intervals: converse relations
    """
    if x0 > y1:
        """preceded by"""
        return pd.Series(['P'])
    elif (y1 == x0):
        """met by"""
        return pd.Series(['M'])
    elif ((y0 < x0) & ((y1 > x0) & (y1 < x1))):
        """overlapped by"""
        return pd.Series(['O'])
    elif ((x1 == y1) & (x0 > y0)):
        """finishes"""
        return pd.Series(['f'])
    elif ((x0 > y0) & (x1 < y1)):
        """during"""
        return pd.Series(['d'])
    elif ((y0 == x0) & (y1 < x1)):
        """started by"""
        return pd.Series(['S'])
    else:
        return None


def collect_events_to_dictofdf(events):
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

        if node_name == 'veredas57':
            print start_time, end_time, ev_type, reason

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


def agglutinate(df_node, dt_lag_min=pd.Timedelta('1 hour')):
    """Merge all overlapping intervals plus those that are
    separated by less than dt_lag

    Args:
        df_node (TYPE): Ordered DataFrame of event time intervals
        dt_lag (TYPE, optional): minimum separation between
                                 consecutive event time intervals
    """
    from operator import itemgetter
    from itertools import groupby
    pd.options.mode.chained_assignment = None  # default='warn'

    # < STEP 1 >
    # Convert all close event intervals
    # I have to change only the key p-> m and proceed to next step
    null_index = df_node[df_node.isnull().any(axis=1)].index

    dt_lag = df_node.ts_n-df_node.te
    condition = df_node.key.isin(['p']) & (dt_lag < dt_lag_min)
    lag_index = df_node[condition].index
    lag_index = lag_index.difference(null_index)
    df_node.loc[lag_index, 'key'] = 'm'

    # < STEP 2 >
    # ============================================================
    # CAUTION: I am assuming that df_node has consecutive indexes
    #          i.e., RangeIndex(start=0, stop=..., step=1), a
    #          monotonic ordered set (maybe it is not that relevant)
    # ============================================================
    # get indices of rows whose intervals overlap or touch
    # (except last row which has NULL values)
    ovl_index = df_node[~df_node.key.isin(['p'])].index  # operations: moFDs
    ovl_index = ovl_index.difference(null_index)

    for _, g in groupby(enumerate(ovl_index), lambda x: x[1]-x[0]):
        ovl_slice = map(itemgetter(1), g)
        # Complete list 'ovl_slice':
        # -Add index element of DataFrame 'df_node' that follows ovl_slice[-1]
        index_next_pos = df_node.index.get_loc(ovl_slice[-1])
        # Complete 'ovl_slice'
        ovl_slice.append(df_node.index[index_next_pos+1])
        ts_min = df_node.loc[ovl_slice, 'ts'].min()
        te_max = df_node.loc[ovl_slice, 'te'].max()
        df_node.loc[ovl_slice[-1], ['ts', 'te']] = [ts_min, te_max]
        df_node.drop(ovl_slice[:-1], inplace=True)   # delete slice


def collect_events_from_nodes(nodes_df,
                              csvfile='cluster_events.csv',
                              dtmin=pd.Timedelta('1 minute')):
    """Given a dictionary of node DataFrames, do some pre-processing
       before merging all event in one big dataframe fro the whole cluster.

    Args:
        nodes_df (Dict): Dictionary of node Dataframe
        csvfile (str, optional): CSV of the cluster DataFrame
        dtmin (Timedelta, optional): Minimum time interval allowed
    """
    import natsort

    csvfile_untouched = '{}.preproc.csv'.format(csvfile[:-4])
    csv_kw_wrt = {
        'columns': ['ts', 'te', 'node', 'reason'],
        'sep': '\t'
    }

    all_df_nodes = []
    cols = ['ts', 'te', 'ts_n', 'te_n']
    for node_name in natsort.natsorted(nodes_df.iterkeys()):
        df_node = nodes_df[node_name]
        df_node['node'] = node_name

        # remove very short events (~ dtmin): reset index of df
        df_node = df_node[~(df_node.te-df_node.ts < dtmin)]
        df_node.reset_index(inplace=True, drop=True)

        # csv_kw_wrt.update({'header': True, 'mode': 'a'})
        # df_node.to_csv(csvfile_untouched, **csv_kw_wrt)

        # === merge events ===
        df_node[['ts_n', 'te_n']] = df_node.shift(-1)[['ts', 'te']]
        df_node[['key']] = df_node[cols].apply(
            lambda r: allen(r.ts, r.te, r.ts_n, r.te_n), axis=1)

        # --PLOT--
        # csv_kw_wrt.update({'columns': ['ts', 'te', 'key', 'reason']})
        # df_node.to_csv('ola-{}.csv'.format(node_name), **csv_kw_wrt)
        # --PLOT--
        # print 'Node: {}'.format(node_name)
        agglutinate(df_node)
        # print '='*80

        # --PLOT--
        # df_node.to_csv('ola-{}.n.csv'.format(node_name), **csv_kw_wrt)
        # --PLOT--

        all_df_nodes.append(df_node)

    df_cluster = pd.concat(all_df_nodes)
    df_cluster.sort_values(['ts', 'te'], inplace=True)
    csv_kw_wrt.update({'header': True, 'mode': 'w'})
    df_cluster.to_csv(csvfile, **csv_kw_wrt)


if __name__ == '__main__':
    import cPickle as pickle
    from hostlist import pd_collect_hosts, expand_hostlist

    events_pkl_file = 'dic_of_node_events.pkl'
    if not os.path.exists(events_pkl_file):
        # import events and serialize
        print "'{0}' not found ... recreating ".format(events_pkl_file)
        config = SlurmDBDConf('etc/slurm/slurm.conf')
        config.read()

        events = import_events_from_mysql(config)
        nodes_df = collect_events_to_dictofdf(events)

        with open(events_pkl_file, 'wb') as fout:
            pickle.dump(nodes_df, fout)

    with open(events_pkl_file, 'rb') as fin:
        nodes_df = pickle.load(fin)

    df_cluster_events_file = 'cluster_events.csv'
    if not os.path.exists(df_cluster_events_file):
        collect_events_from_nodes(nodes_df, df_cluster_events_file)

    csv_kw_read = {'infer_datetime_format': True,
                   'parse_dates': [0, 1],
                   'delimiter': '\t',
                   'engine': 'c',
                   'usecols': ['ts', 'te', 'node']}
    csv_kw_wrt = {'header': True,
                  'index': False,
                  'sep': '\t'}

    if not os.path.exists('prova_1.2.csv'):
        with open(df_cluster_events_file, 'r') as fin:
            cluster_df = pd.read_csv(fin, **csv_kw_read)

        csv_kw_wrt.update({'columns': ['ts', 'te', 'node']})
        cluster_df.to_csv('prova_1.csv', **csv_kw_wrt)

        # < STEP 0 >:
        # Aggregate identical time intervals
        grouper_dup = cluster_df.groupby(['ts', 'te'], as_index=False)
        cluster_df = grouper_dup.agg({'node': pd_collect_hosts})
        cluster_df.to_csv('prova_1.1.csv', **csv_kw_wrt)

        # < STEP 1 >:
        # There are time intervals almost equal, ie, with same (ts, te).
        # Find these intervals and set them equal
        # grp_ts = cluster_df.groupby('ts', as_index=False)
        grp_ts = cluster_df.groupby(['ts', cluster_df.te.dt.date],
                                    as_index=False)
        for ts_lbl, ts_slice in grp_ts.groups.items():  # this is a dict
            if len(ts_slice) == 1:
                continue
            te_values = cluster_df.loc[ts_slice, 'te']
            te_values_max = te_values.max()
            dte = te_values.max()-te_values.min()
            if (dte < pd.Timedelta('1 day')):
                cluster_df.loc[ts_slice, 'te'] = te_values_max
        # Now aggregate
        grouper_dup = cluster_df.groupby(['ts', 'te'], as_index=False)
        cluster_df = grouper_dup.agg({'node': pd_collect_hosts})
        cluster_df.to_csv('prova_1.2.csv', **csv_kw_wrt)

    csv_kw_read.update({'usecols': ['ts', 'te', 'node']})
    with open('prova_1.2.csv', 'r') as fin:
        cluster_df = pd.read_csv(fin, **csv_kw_read)

    # remove events with duration less than 6 hours (1/4 day)
    dt = cluster_df.te-cluster_df.ts
    cluster_df = cluster_df[~(dt < pd.Timedelta('6 hours'))]

    ts_min = cluster_df.ts.min()
    te_max = cluster_df.te.max()

    # bin edges (duration = 1 day)
    result = pd.Series(0, index=pd.date_range(ts_min.floor('D'),
                                              te_max.ceil('D'),
                                              freq='D'))
    cluster_df['bin_ts'] = cluster_df.ts.dt.floor('D')
    cluster_df['bin_te'] = cluster_df.te.dt.floor('D')

    csv_kw_wrt.update({'columns': ['ts', 'bin_ts', 'te', 'te_bin', 'node']})
    # csv_kw_wrt.update({'columns': ['ts', 'te', 'node']})
    cluster_df.to_csv('prova_1.3.csv', **csv_kw_wrt)

    # result.to_csv('series.csv')
    # <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
    sys.exit(1)

    # remove all intervals less than 2/3 day
    dt = cluster_df.te-cluster_df.ts
    cluster_df = cluster_df[~(dt < pd.Timedelta('16 hours'))]

    # < STEP 1>:
    # group values by day
    # extract date field from 'ts', 'te' and sort values
    cluster_df[['te', 'ts']] = cluster_df[['te', 'ts']].transform(
        lambda r: pd.to_datetime(r.dt.date))
    cluster_df.sort_values(['ts', 'te'], inplace=True)

    # -- PLOT
    csv_kw_wrt.update({'columns': ['ts', 'te', 'node']})
    cluster_df.to_csv('prova_1.3.csv', **csv_kw_wrt)

    # group by days
    grouper_day = cluster_df.groupby(['ts', 'te'], as_index=False)
    cluster_df = grouper_day.agg({'node': pd_collect_hosts})

    # -- PLOT
    csv_kw_wrt.update({'columns': ['ts', 'te', 'node']})
    cluster_df.to_csv('prova_1.4.csv', **csv_kw_wrt)

    # result = pd.Series('', index=pd.date_range(cluster_df.ts.min(),
    #                                            cluster_df.te.max(),
    #                                            freq='D'))
    # for r in cluster_df.itertuples():
    #     print r.ts, r.te, r.node
    #     print result.loc[r.ts:r.te].index
    #     print '.'*80

    # for r in cluster_df.itertuples():
    #     for ind in result.loc[r.ts:r.te].index:
    #         result[ind] = pd_collect_hosts([r.node, result[ind]])
    # result = result.transform(lambda x: len(expand_hostlist(x)))
    # result.to_csv('series.csv')

    # check date ordering
    cols = ['ts', 'te', 'ts_n', 'te_n']
    from_col = ['ts', 'te']
    to_col = ['ts_n', 'te_n']
    cluster_df[to_col] = cluster_df.shift(-1)[from_col]
    cluster_df[['key']] = cluster_df[cols].apply(
        lambda r: allen(r.ts, r.te, r.ts_n, r.te_n), axis=1)

    print cluster_df.key.unique()
    # # dt1 = cluster_df.te-cluster_df.ts
    # # dt2 = cluster_df.te_n-cluster_df.ts_n
    # # cluster_df['dt'] = abs(dt2.dt.total_seconds()-dt1.dt.total_seconds())

    # cluster_df_o = cluster_df[cluster_df.key == 'o']
    # print cluster_df_o.te-cluster_df_o.ts_n

    csv_kw_wrt.update({'columns': ['ts', 'te', 'key', 'node']})
    cluster_df.to_csv('prova_1.5.csv', **csv_kw_wrt)
