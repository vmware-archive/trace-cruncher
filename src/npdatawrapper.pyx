"""
SPDX-License-Identifier: LGPL-2.1

Copyright 2019 VMware Inc, Yordan Karadzhov (VMware) <y.karadz@gmail.com>
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

from libc cimport stdint
ctypedef stdint.int16_t int16_t
ctypedef stdint.uint16_t uint16_t
ctypedef stdint.int32_t int32_t
ctypedef stdint.uint32_t uint32_t
ctypedef stdint.int64_t int64_t
ctypedef stdint.uint64_t uint64_t

cdef extern from 'numpy/ndarraytypes.h':
    int NPY_ARRAY_CARRAY

# Numpy must be initialized!!!
np.import_array()

cdef extern from 'trace2matrix.c':
    ssize_t trace2matrix(int stream_id,
                         int16_t **event_array,
                         int16_t **cpu_array,
                         int32_t **pid_array,
                         int64_t **offset_array,
                         int64_t **ts_array)

data_columns = ['event', 'cpu', 'pid', 'offset', 'time']

data_column_types = {
    data_columns[0]: np.NPY_INT16,
    data_columns[1]: np.NPY_INT16,
    data_columns[2]: np.NPY_INT32,
    data_columns[3]: np.NPY_INT64,
    data_columns[4]: np.NPY_UINT64
    }

cdef class KsDataWrapper:
    cdef int item_size
    cdef int data_size
    cdef int data_type
    cdef void* data_ptr

    cdef init(self, int data_type,
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


def load(stream_id, evt_data=True, cpu_data=True, pid_data=True,
                    ofst_data=True, ts_data=True):
    """ Python binding of the 'kshark_load_data_matrix' function that does not
        copy the data. The input parameters can be used to avoid loading the
        data from the unnecessary fields.
    """
    cdef int16_t *evt_c
    cdef int16_t *cpu_c
    cdef int32_t *pid_c
    cdef int64_t *ofst_c
    cdef int64_t *ts_c

    cdef np.ndarray evt, cpu, pid, ofst, ts

    if not evt_data:
        evt_c = NULL

    if not cpu_data:
        cpu_c = NULL

    if not pid_data:
        pid_c = NULL

    if not ofst_data:
        ofst_c = NULL

    if not ts_data:
        ts_c = NULL

    data_dict = {}

    cdef ssize_t size

    size = trace2matrix(stream_id, &evt_c, &cpu_c, &pid_c, &ofst_c, &ts_c)
    if size <= 0:
        raise Exception('No data has been loaded.')

    if evt_data:
        column = 'event'
        array_wrapper_evt = KsDataWrapper()
        array_wrapper_evt.init(data_type=data_column_types[column],
                               data_size=size,
                               item_size=0,
                               data_ptr=<void *>evt_c)

        evt = np.array(array_wrapper_evt, copy=False)
        evt.base = <PyObject *> array_wrapper_evt
        data_dict.update({column: evt})
        Py_INCREF(array_wrapper_evt)

    if cpu_data:
        column = 'cpu'
        array_wrapper_cpu = KsDataWrapper()
        array_wrapper_cpu.init(data_type=data_column_types[column],
                               data_size=size,
                               item_size=0,
                               data_ptr=<void *> cpu_c)

        cpu = np.array(array_wrapper_cpu, copy=False)
        cpu.base = <PyObject *> array_wrapper_cpu
        data_dict.update({column: cpu})
        Py_INCREF(array_wrapper_cpu)

    if pid_data:
        column = 'pid'
        array_wrapper_pid = KsDataWrapper()
        array_wrapper_pid.init(data_type=data_column_types[column],
                               data_size=size,
                               item_size=0,
                               data_ptr=<void *>pid_c)

        pid = np.array(array_wrapper_pid, copy=False)
        pid.base = <PyObject *> array_wrapper_pid
        data_dict.update({column: pid})
        Py_INCREF(array_wrapper_pid)

    if ofst_data:
        column = 'offset'
        array_wrapper_ofst = KsDataWrapper()
        array_wrapper_ofst.init(data_type=data_column_types[column],
                                data_size=size,
                                item_size=0,
                                data_ptr=<void *> ofst_c)


        ofst = np.array(array_wrapper_ofst, copy=False)
        ofst.base = <PyObject *> array_wrapper_ofst
        data_dict.update({column: ofst})
        Py_INCREF(array_wrapper_ofst)

    if ts_data:
        column = 'time'
        array_wrapper_ts = KsDataWrapper()
        array_wrapper_ts.init(data_type=data_column_types[column],
                              data_size=size,
                              item_size=0,
                              data_ptr=<void *> ts_c)

        ts = np.array(array_wrapper_ts, copy=False)
        ts.base = <PyObject *> array_wrapper_ts
        data_dict.update({column: ts})
        Py_INCREF(array_wrapper_ts)

    return data_dict

def columns():
    return data_columns
