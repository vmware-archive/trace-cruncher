"""
SPDX-License-Identifier: LGPL-2.1

Copyright 2019 VMware Inc, Yordan Karadzhov <ykaradzhov@vmware.com>
"""

import ctypes

# Import the Python-level symbols of numpy
import numpy as np
# Import the C-level symbols of numpy
cimport numpy as np

import json

from libcpp cimport bool

from libc.stdlib cimport free

from cpython cimport PyObject, Py_INCREF


cdef extern from 'stdint.h':
    ctypedef unsigned short uint8_t
    ctypedef unsigned short uint16_t
    ctypedef unsigned long long uint64_t

cdef extern from 'numpy/ndarraytypes.h':
    int NPY_ARRAY_CARRAY

# Declare all C functions we are going to call
cdef extern from 'libkshark-py.c':
    bool kspy_open(const char *fname)

cdef extern from 'libkshark-py.c':
    bool kspy_close()

cdef extern from 'libkshark-py.c':
    size_t kspy_trace2matrix(uint64_t **offset_array,
                             uint8_t **cpu_array,
                             uint64_t **ts_array,
                             uint16_t **pid_array,
                             int **event_array)

cdef extern from 'libkshark-py.c':
    int kspy_get_event_id(const char *sys, const char *evt)

cdef extern from 'libkshark-py.c':
    uint64_t kspy_read_event_field(uint64_t offset,
                                   int event_id,
                                   const char *field)

cdef extern from 'libkshark-py.c':
    ssize_t kspy_get_tasks(int **pids, char ***names)

cdef extern from 'libkshark-py.c':
    const char *kspy_get_function(unsigned long long addr)

cdef extern from 'libkshark-py.c':
    void kspy_register_plugin(const char *file)

cdef extern from 'libkshark-py.c':
    const char *kspy_map_instruction_address(int pid,
					     unsigned long long proc_addr,
					     unsigned long long *obj_addr)

cdef extern from 'kernelshark/libkshark.h':
    int KS_EVENT_OVERFLOW

cdef extern from 'libkshark-py.c':
    void kspy_new_session_file(const char *data_file,
                               const char *session_file)

EVENT_OVERFLOW = KS_EVENT_OVERFLOW

# Numpy must be initialized!!!
np.import_array()


cdef class KsDataWrapper:
    cdef int item_size
    cdef int data_size
    cdef int data_type
    cdef void* data_ptr

    cdef init(self,
              int data_type,
              int data_size,
              int item_size,
              void* data_ptr):
        """ This initialization cannot be done in the constructor because
            we use C-level arguments.
        """
        self.item_size = item_size
        self.data_size = data_size
        self.data_type = data_type
        self.data_ptr = data_ptr

    def __array__(self):
        """ Here we use the __array__ method, that is called when numpy
            tries to get an array from the object.
        """
        cdef np.npy_intp shape[1]
        shape[0] = <np.npy_intp> self.data_size

        ndarray = np.PyArray_New(np.ndarray,
                                 1, shape,
                                 self.data_type,
                                 NULL,
                                 self.data_ptr,
                                 self.item_size,
                                 NPY_ARRAY_CARRAY,
                                 <object>NULL)

        return ndarray

    def __dealloc__(self):
        """ Free the data. This is called by Python when all the references to
            the object are gone.
        """
        free(<void*>self.data_ptr)


def c_str2py(char *c_str):
    """ String convertion C -> Python
    """
    return ctypes.c_char_p(c_str).value.decode('utf-8')


def py_str2c(py_str):
    """ String convertion Python -> C
    """
    return py_str.encode('utf-8')


def open_file(fname):
    """ Open a tracing data file.
    """
    return kspy_open(py_str2c(fname))


def close():
    """ Open the session file.
    """
    kspy_close()


def read_event_field(offset, event_id, field):
    """ Read the value of a specific field of the trace event.
    """
    cdef uint64_t v

    v = kspy_read_event_field(offset, event_id, py_str2c(field))
    return v


def event_id(system, event):
    """ Get the unique Id of the event
    """
    return kspy_get_event_id(py_str2c(system), py_str2c(event))


def get_tasks():
    """ Get a dictionary of all task's PIDs
    """
    cdef int *pids
    cdef char **names
    cdef int size = kspy_get_tasks(&pids, &names)

    task_dict = {}

    for i in range(0, size):
        name = c_str2py(names[i])
        pid_list = task_dict.get(name)

        if pid_list is None:
            pid_list = []

        pid_list.append(pids[i])
        task_dict.update({name : pid_list})

    return task_dict

def get_function(ip):
    """ Get the name of the function from its ip
    """
    func = kspy_get_function(ip)
    if func:
        return c_str2py(kspy_get_function(ip))

    return str("0x%x" %ip)

def register_plugin(plugin):
    """
    """
    kspy_register_plugin(py_str2c(plugin))

def map_instruction_address(pid, address):
    """
    """
    cdef unsigned long long obj_addr;
    cdef const char* obj_file;
    obj_file = kspy_map_instruction_address(pid, address, &obj_addr)

    return {'obj_file' : c_str2py(obj_file), 'address' : obj_addr}

def load_data(ofst_data=True, cpu_data=True,
	      ts_data=True, pid_data=True,
	      evt_data=True):
    """ Python binding of the 'kshark_load_data_matrix' function that does not
        copy the data. The input parameters can be used to avoid loading the
        data from the unnecessary fields.
    """
    cdef uint64_t *ofst_c
    cdef uint16_t *cpu_c
    cdef uint64_t *ts_c
    cdef uint16_t *pid_c
    cdef int *evt_c

    cdef np.ndarray ofst
    cdef np.ndarray cpu
    cdef np.ndarray ts
    cdef np.ndarray pid
    cdef np.ndarray evt

    if not ofst_data:
        ofst_c = NULL

    if not cpu_data:
        cpu_c = NULL

    if not ts_data:
        ts_c = NULL

    if not pid_data:
        pid_c = NULL

    if not evt_data:
        evt_c = NULL

    data_dict = {}

    # Call the C function
    size = kspy_trace2matrix(&ofst_c, &cpu_c, &ts_c, &pid_c, &evt_c)

    if ofst_data:
        array_wrapper_ofst = KsDataWrapper()
        array_wrapper_ofst.init(data_type=np.NPY_UINT64,
                                item_size=0,
                                data_size=size,
                                data_ptr=<void *> ofst_c)


        ofst = np.array(array_wrapper_ofst, copy=False)
        ofst.base = <PyObject *> array_wrapper_ofst
        data_dict.update({'offset': ofst})
        Py_INCREF(array_wrapper_ofst)

    if cpu_data:
        array_wrapper_cpu = KsDataWrapper()
        array_wrapper_cpu.init(data_type=np.NPY_UINT8,
                               data_size=size,
                               item_size=0,
                               data_ptr=<void *> cpu_c)

        cpu = np.array(array_wrapper_cpu, copy=False)
        cpu.base = <PyObject *> array_wrapper_cpu
        data_dict.update({'cpu': cpu})
        Py_INCREF(array_wrapper_cpu)

    if ts_data:
        array_wrapper_ts = KsDataWrapper()
        array_wrapper_ts.init(data_type=np.NPY_UINT64,
                              data_size=size,
                              item_size=0,
                              data_ptr=<void *> ts_c)

        ts = np.array(array_wrapper_ts, copy=False)
        ts.base = <PyObject *> array_wrapper_ts
        data_dict.update({'time': ts})
        Py_INCREF(array_wrapper_ts)

    if pid_data:
        array_wrapper_pid = KsDataWrapper()
        array_wrapper_pid.init(data_type=np.NPY_UINT16,
                               data_size=size,
                               item_size=0,
                               data_ptr=<void *>pid_c)

        pid = np.array(array_wrapper_pid, copy=False)
        pid.base = <PyObject *> array_wrapper_pid
        data_dict.update({'pid': pid})
        Py_INCREF(array_wrapper_pid)

    if evt_data:
        array_wrapper_evt = KsDataWrapper()
        array_wrapper_evt.init(data_type=np.NPY_INT,
                               data_size=size,
                               item_size=0,
                               data_ptr=<void *>evt_c)

        evt = np.array(array_wrapper_evt, copy=False)
        evt.base = <PyObject *> array_wrapper_evt
        data_dict.update({'event': evt})
        Py_INCREF(array_wrapper_evt)

    return data_dict

def data_size(data):
    """ Get the number of trace records.
    """
    if data['offset'] is not None:
        return data['offset'].size

    if data['cpu'] is not None:
        return data['cpu'].size

    if data['time'] is not None:
        return data['time'].size

    if data['pid'] is not None:
        return data['pid'].size

    if data['event'] is not None:
        return data['event'].size

    return 0

def save_session(session, s):
    """ Save a KernelShark session description of a JSON file.
    """
    s.seek(0)
    json.dump(session, s, indent=4)
    s.truncate()


def new_session(fname, sname):
    """ Generate and save a default KernelShark session description
        file (JSON).
    """
    kspy_new_session_file(py_str2c(fname), py_str2c(sname))

    with open(sname, 'r+') as s:
        session = json.load(s)

        session['Filters']['filter mask'] = 7
        session['CPUPlots'] = []
        session['TaskPlots'] = []
        session['Splitter'] = [1, 1]
        session['MainWindow'] = [1200, 800]
        session['ViewTop'] = 0
        session['ColorScheme'] = 0.75
        session['Model']['bins'] = 1000

        session['Markers']['markA'] = {}
        session['Markers']['markA']['isSet'] = False
        session['Markers']['markB'] = {}
        session['Markers']['markB']['isSet'] = False
        session['Markers']['Active'] = 'A'

        save_session(session, s)
