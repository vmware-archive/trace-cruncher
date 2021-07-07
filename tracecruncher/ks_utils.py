"""
SPDX-License-Identifier: LGPL-2.1

Copyright 2019 VMware Inc, Yordan Karadzhov (VMware) <y.karadz@gmail.com>
"""

import os
import json

from . import npdatawrapper as dw
from . import ksharkpy as ks


def size(data):
    """ Get the number of trace records.
    """
    for key in dw.data_column_types:
        if data[key] is not None:
            return data[key].size

    raise Exception('Data size is unknown.')


class trace_file_stream:
    def __init__(self, file_name='', buffer_name='top'):
        """ Constructor.
        """
        self.file_name = file_name
        self.buffer_name = buffer_name
        self.stream_id = -1

        if file_name:
            self.open(file_name)

    def open(self, file_name):
        """ Open a trace file for reading.
        """
        self.file_name = file_name
        self.stream_id = ks.open(self.file_name)

    def open_buffer(self, file_name, buffer_name):
        """ Open a aprticular buffer in a trace file for reading.
        """
        self.file_name = file_name
        self.buffer_name = buffer_name
        self.stream_id = ks.open_buffer(self.file_name, buffer_name)

    def close(self):
        """ Close this trace data stream.
        """
        if self.stream_id >= 0:
            ks.close(self.stream_id)
            self.stream_id = -1

    def set_clock_offset(self, offset):
        """ Set the clock offset to be append to the timestamps of this trace
            data stream.
        """
        ks.set_clock_offset(stream_id=self.stream_id, offset=offset)

    def load(self, cpu_data=True, pid_data=True, evt_data=True,
             ofst_data=True, ts_data=True):
        """ Load the trace data.
        """
        return dw.load(stream_id=self.stream_id,
                       ofst_data=ofst_data,
                       cpu_data=cpu_data,
                       ts_data=ts_data,
                       pid_data=pid_data,
                       evt_data=evt_data)

    def get_tasks(self):
        """ Get a dictionary (name and PID) of all tasks presented in the
            tracing data.
        """
        return ks.get_tasks(stream_id=self.stream_id)

    def event_id(self, name):
        """ Retrieve the unique ID of the event from its name.
        """
        return ks.event_id(stream_id=self.stream_id, name=name)

    def event_name(self, event_id):
        """ Retrieve the name of the event from its unique ID.
        """
        return ks.event_name(stream_id=self.stream_id, event_id=event_id)

    def read_event_field(self, offset, event_id, field):
        """ Retrieve the value of a trace event field.
        """
        return ks.read_event_field(stream_id=self.stream_id,
                                   offset=offset,
                                   event_id=event_id,
                                   field=field)

    def __enter__(self):
        """
        """
        self.open(self.file_name)
        return self

    def __exit__(self,
                 exception_type,
                 exception_value,
                 traceback):
        """
        """
        self.close()

    def __del__(self):
        """
        """
        self.close()


class ks_session:
    def __init__(self, session_name):
        """ Constructor.
        """
        self.gui_session(session_name)

    def gui_session(self, session_name):
        """ Generate a default KernelShark session description
            file (JSON).
        """
        self.name, extension = os.path.splitext(session_name)
        json_file = session_name
        if extension != '.json':
            json_file += '.json'

        ks.new_session_file(session_file=json_file)

        self.session_file = open(json_file, 'r+')
        self.session_doc = json.load(self.session_file)

        self.session_doc['Splitter'] = [1, 1]
        self.session_doc['MainWindow'] = [1200, 800]
        self.session_doc['ViewTop'] = 0
        self.session_doc['ColorScheme'] = 0.75
        self.session_doc['Model']['bins'] = 1000

        self.session_doc['Markers']['markA'] = {}
        self.session_doc['Markers']['markA']['isSet'] = False
        self.session_doc['Markers']['markB'] = {}
        self.session_doc['Markers']['markB']['isSet'] = False
        self.session_doc['Markers']['Active'] = 'A'

        for stream_doc in self.session_doc["data streams"]:
            stream_doc['CPUPlots'] = []
            stream_doc['TaskPlots'] = []

        self.session_doc['ComboPlots'] = []

    def set_cpu_plots(self, stream, plots):
        """ Add a list of CPU plots to the KernelShark session description
            file.
        """
        for stream_doc in self.session_doc['data streams']:
            if stream_doc['stream id'] == stream.stream_id:
                stream_doc['CPUPlots'] = list(map(int, plots))

    def set_task_plots(self, stream, plots):
        """ Add a list of Task plots to the KernelShark session description
            file.
        """
        for stream_doc in self.session_doc['data streams']:
            if stream_doc['stream id'] == stream.stream_id:
                stream_doc['TaskPlots'] = list(map(int, plots))

    def set_time_range(self, tmin, tmax):
        """ Set the time range of the KernelShark visualization model.
        """
        self.session_doc['Model']['range'] = [int(tmin), int(tmax)]

    def set_marker_a(self, row):
        """ Set the position of Marker A.
        """
        self.session_doc['Markers']['markA']['isSet'] = True
        self.session_doc['Markers']['markA']['row'] = int(row)

    def set_marker_b(self, row):
        """ Set the position of Marker B.
        """
        self.session_doc['Markers']['markB']['isSet'] = True
        self.session_doc['Markers']['markB']['row'] = int(row)

    def set_first_visible_row(self, row):
        """ Set the number of the first visible row in the text data viewer.
        """
        self.session_doc['ViewTop'] = int(row)

    def add_plugin(self, stream, plugin):
        """ In the KernelShark session description file, add a plugin to be
            registered to a given trace data stream.
        """
        for stream_doc in self.session_doc["data streams"]:
            if stream_doc['stream id'] == stream.stream_id:
                stream_doc['plugins']['registered'].append([plugin, True])

    def add_event_filter(self, stream, events):
        """ In the KernelShark session description file, add a list of
            event IDs to be filtered out.
        """
        for stream_doc in self.session_doc["data streams"]:
            if stream_doc['stream id'] == stream.stream_id:
                stream_doc['filters']['hide event filter'] = events

    def save(self):
        """ Save a KernelShark session description of a JSON file.
        """
        self.session_file.seek(0)
        json.dump(self.session_doc, self.session_file, indent=4)
        self.session_file.truncate()


def open_file(file_name):
    """ Open a trace file for reading.
    """
    return trace_file_stream(file_name)


def open_buffer(file_name, buffer_name):
    """ Open a aprticular buffer in a trace file for reading.
    """
    s = trace_file_stream()
    s.open_buffer(file_name, buffer_name)
    return s
