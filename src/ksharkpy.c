// SPDX-License-Identifier: LGPL-2.1

/*
 * Copyright (C) 2019 VMware Inc, Yordan Karadzhov (VMware) <y.karadz@gmail.com>
 */

/** Use GNU C Library. */
#define _GNU_SOURCE 1

// C
#include <stdio.h>
#include <dlfcn.h>

// Python
#include <Python.h>

// trace-cruncher
#include "ksharkpy-utils.h"
#include "common.h"

extern PyObject *KSHARK_ERROR;
extern PyObject *TRACECRUNCHER_ERROR;

static PyMethodDef ksharkpy_methods[] = {
	{"open",
	 (PyCFunction) PyKShark_open,
	 METH_VARARGS | METH_KEYWORDS,
	 "Open trace data file"
	},
	{"close",
	 (PyCFunction) PyKShark_close,
	 METH_VARARGS | METH_KEYWORDS,
	 "Close trace data file"
	},
	{"open_tep_buffer",
	 (PyCFunction) PyKShark_open_tep_buffer,
	 METH_VARARGS | METH_KEYWORDS,
	 "Open trace data buffer"
	},
	{"set_clock_offset",
	 (PyCFunction) PyKShark_set_clock_offset,
	 METH_VARARGS | METH_KEYWORDS,
	 "Set the clock offset of the data stream"
	},
	{"get_tasks",
	 (PyCFunction) PyKShark_get_tasks,
	 METH_VARARGS | METH_KEYWORDS,
	 "Get all tasks recorded in a trace file"
	},
	{"event_id",
	 (PyCFunction) PyKShark_event_id,
	 METH_VARARGS | METH_KEYWORDS,
	 "Get the Id of the event from its name"
	},
	{"event_name",
	 (PyCFunction) PyKShark_event_name,
	 METH_VARARGS | METH_KEYWORDS,
	 "Get the name of the event from its Id number"
	},
	{"read_event_field",
	 (PyCFunction) PyKShark_read_event_field,
	 METH_VARARGS | METH_KEYWORDS,
	 "Get the value of an event field having a given name"
	},
	{"new_session_file",
	 (PyCFunction) PyKShark_new_session_file,
	 METH_VARARGS | METH_KEYWORDS,
	 "Create new session description file"
	},
	{NULL, NULL, 0, NULL}
};

static struct PyModuleDef ksharkpy_module = {
	PyModuleDef_HEAD_INIT,
	"ksharkpy",
	"",
	-1,
	ksharkpy_methods
};

PyMODINIT_FUNC PyInit_ksharkpy(void)
{
	PyObject *module = PyModule_Create(&ksharkpy_module);

	KSHARK_ERROR = PyErr_NewException("tracecruncher.ksharkpy.ks_error",
					  NULL, NULL);
	PyModule_AddObject(module, "ks_error", KSHARK_ERROR);

	TRACECRUNCHER_ERROR = PyErr_NewException("tracecruncher.tc_error",
						 NULL, NULL);
	PyModule_AddObject(module, "tc_error", TRACECRUNCHER_ERROR);

	return module;
}
