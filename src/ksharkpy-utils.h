/* SPDX-License-Identifier: LGPL-2.1 */

/*
 * Copyright 2021 VMware Inc, Yordan Karadzhov <y.karadz@gmail.com>
 */

#ifndef _TC_KSHARK_PY_UTILS
#define _TC_KSHARK_PY_UTILS

// Python
#include <Python.h>

// trace-cruncher
#include "common.h"

C_OBJECT_WRAPPER_DECLARE(kshark_data_stream, PyKSharkStream)

PyObject *PyKShark_open(PyObject *self, PyObject *args, PyObject *kwargs);

PyObject *PyKShark_close(PyObject* self, PyObject* noarg);

PyObject *PyKShark_open_tep_buffer(PyObject *self, PyObject *args,
						   PyObject *kwargs);

PyObject *PyKShark_set_clock_offset(PyObject* self, PyObject* args,
						    PyObject *kwargs);

PyObject *PyKShark_get_tasks(PyObject* self, PyObject* args, PyObject *kwargs);

PyObject *PyKShark_event_id(PyObject *self, PyObject *args, PyObject *kwargs);

PyObject *PyKShark_event_name(PyObject *self, PyObject *args,
					      PyObject *kwargs);

PyObject *PyKShark_read_event_field(PyObject *self, PyObject *args,
						    PyObject *kwargs);

PyObject *PyKShark_new_session_file(PyObject *self, PyObject *args,
						    PyObject *kwargs);

#endif
