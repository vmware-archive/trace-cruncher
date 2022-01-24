// SPDX-License-Identifier: LGPL-2.1

/*
 * Copyright 2021 VMware Inc, Yordan Karadzhov (VMware) <y.karadz@gmail.com>
 */

// trace-cruncher
#include "ftracepy-utils.h"

extern PyObject *TFS_ERROR;
extern PyObject *TEP_ERROR;
extern PyObject *TRACECRUNCHER_ERROR;

static PyMethodDef PyTepRecord_methods[] = {
	{"time",
	 (PyCFunction) PyTepRecord_time,
	 METH_NOARGS,
	 "Get the time of the record."
	},
	{"CPU",
	 (PyCFunction) PyTepRecord_cpu,
	 METH_NOARGS,
	 "Get the CPU Id of the record."
	},
	{NULL}
};

C_OBJECT_WRAPPER(tep_record, PyTepRecord, NO_DESTROY, NO_FREE)

static PyMethodDef PyTepEvent_methods[] = {
	{"name",
	 (PyCFunction) PyTepEvent_name,
	 METH_NOARGS,
	 "Get the name of the event."
	},
	{"id",
	 (PyCFunction) PyTepEvent_id,
	 METH_NOARGS,
	 "Get the unique identifier of the event."
	},
	{"field_names",
	 (PyCFunction) PyTepEvent_field_names,
	 METH_NOARGS,
	 "Get the names of all fields."
	},
	{"parse_record_field",
	 (PyCFunction) PyTepEvent_parse_record_field,
	 METH_VARARGS | METH_KEYWORDS,
	 "Get the content of a record field."
	},
	{"get_pid",
	 (PyCFunction) PyTepEvent_get_pid,
	 METH_VARARGS | METH_KEYWORDS,
	},
	{NULL}
};

C_OBJECT_WRAPPER(tep_event, PyTepEvent, NO_DESTROY, NO_FREE)

static PyMethodDef PyTep_methods[] = {
	{"init_local",
	 (PyCFunction) PyTep_init_local,
	 METH_VARARGS | METH_KEYWORDS,
	 "Initialize from local instance."
	},
	{"get_event",
	 (PyCFunction) PyTep_get_event,
	 METH_VARARGS | METH_KEYWORDS,
	 "Get a PyTepEvent object."
	},
	{"event_record",
	 (PyCFunction) PyTep_event_record,
	 METH_VARARGS | METH_KEYWORDS,
	 "Generic print of a trace event."
	},
	{"process",
	 (PyCFunction) PyTep_process,
	 METH_VARARGS | METH_KEYWORDS,
	 "Generic print of the process that generated the trace event."
	},
	{"info",
	 (PyCFunction) PyTep_info,
	 METH_VARARGS | METH_KEYWORDS,
	 "Generic print of a trace event info."
	},
	{"short_kprobe_print",
	 (PyCFunction) PyTep_short_kprobe_print,
	 METH_VARARGS | METH_KEYWORDS,
	 "Do not print the address of the probe."
	},
	{NULL}
};

C_OBJECT_WRAPPER(tep_handle, PyTep, NO_DESTROY, tep_free)

static PyMethodDef PyTfsInstance_methods[] = {
	{"dir",
	 (PyCFunction) PyTfsInstance_dir,
	 METH_NOARGS,
	 "Get the absolute path to the instance directory."
	},
	{NULL, NULL, 0, NULL}
};

C_OBJECT_WRAPPER(tracefs_instance, PyTfsInstance,
		 tracefs_instance_destroy,
		 tracefs_instance_free)

static PyMethodDef PyDynevent_methods[] = {
	{"event",
	 (PyCFunction) PyDynevent_event,
	 METH_NOARGS,
	 "Get the name of the dynamic event."
	},
	{"system",
	 (PyCFunction) PyDynevent_system,
	 METH_NOARGS,
	 "Get the system name of the dynamic event."
	},
	{"address",
	 (PyCFunction) PyDynevent_address,
	 METH_NOARGS,
	 "Get the address / function name of the dynamic event."
	},
	{"probe",
	 (PyCFunction) PyDynevent_probe,
	 METH_NOARGS,
	 "Get the event definition."
	},
	{"set_filter",
	 (PyCFunction) PyDynevent_set_filter,
	 METH_VARARGS | METH_KEYWORDS,
	 "Define a filter for a dynamic event."
	},
	{"get_filter",
	 (PyCFunction) PyDynevent_get_filter,
	 METH_VARARGS | METH_KEYWORDS,
	 "Get the filter of a dynamic event."
	},
	{"clear_filter",
	 (PyCFunction) PyDynevent_clear_filter,
	 METH_VARARGS | METH_KEYWORDS,
	 "Clear the filter of a dynamic event."
	},
	{"enable",
	 (PyCFunction) PyDynevent_enable,
	 METH_VARARGS | METH_KEYWORDS,
	 "Enable dynamic event."
	},
	{"disable",
	 (PyCFunction) PyDynevent_disable,
	 METH_VARARGS | METH_KEYWORDS,
	 "Disable dynamic event."
	},
	{"is_enabled",
	 (PyCFunction) PyDynevent_is_enabled,
	 METH_VARARGS | METH_KEYWORDS,
	 "Check if dynamic event is enabled."
	},
	{NULL, NULL, 0, NULL}
};

static int dynevent_destroy(struct tracefs_dynevent *devt)
{
	return tracefs_dynevent_destroy(devt, true);
}
C_OBJECT_WRAPPER(tracefs_dynevent, PyDynevent,
		 dynevent_destroy,
		 tracefs_dynevent_free)

static PyMethodDef PyTraceHist_methods[] = {
	{"add_value",
	 (PyCFunction) PyTraceHist_add_value,
	 METH_VARARGS | METH_KEYWORDS,
	 "Add value field."
	},
	{"sort_keys",
	 (PyCFunction) PyTraceHist_sort_keys,
	 METH_VARARGS | METH_KEYWORDS,
	 "Set key felds or values to sort on."
	},
	{"sort_key_direction",
	 (PyCFunction) PyTraceHist_sort_key_direction,
	 METH_VARARGS | METH_KEYWORDS,
	 "Set the direction of a sort key field."
	},
	{"start",
	 (PyCFunction) PyTraceHist_start,
	 METH_VARARGS | METH_KEYWORDS,
	 "Start acquiring data."
	},
	{"stop",
	 (PyCFunction) PyTraceHist_stop,
	 METH_VARARGS | METH_KEYWORDS,
	 "Pause acquiring data."
	},
	{"resume",
	 (PyCFunction) PyTraceHist_resume,
	 METH_VARARGS | METH_KEYWORDS,
	 "Continue acquiring data."
	},
	{"clear",
	 (PyCFunction) PyTraceHist_clear,
	 METH_VARARGS | METH_KEYWORDS,
	 "Reset the histogram."
	},
	{"read",
	 (PyCFunction) PyTraceHist_read,
	 METH_VARARGS | METH_KEYWORDS,
	 "Read the content of the histogram."
	},
	{"close",
	 (PyCFunction) PyTraceHist_close,
	 METH_VARARGS | METH_KEYWORDS,
	 "Destroy the histogram."
	},
	{NULL, NULL, 0, NULL}
};

C_OBJECT_WRAPPER(tracefs_hist, PyTraceHist,
		 NO_DESTROY,
		 tracefs_hist_free)

static PyMethodDef PySynthEvent_methods[] = {
	{"add_start_fields",
	 (PyCFunction) PySynthEvent_add_start_fields,
	 METH_VARARGS | METH_KEYWORDS,
	 "Add fields from the start event to save."
	},
	{"add_end_fields",
	 (PyCFunction) PySynthEvent_add_end_fields,
	 METH_VARARGS | METH_KEYWORDS,
	 "Add fields from the end event to save."
	},
	{NULL, NULL, 0, NULL}
};

C_OBJECT_WRAPPER(tracefs_synth, PySynthEvent,
		 tracefs_synth_destroy,
		 tracefs_synth_free)

static PyMethodDef ftracepy_methods[] = {
	{"dir",
	 (PyCFunction) PyFtrace_dir,
	 METH_NOARGS,
	 "Get the absolute path to the tracefs directory."
	},
	{"detach",
	 (PyCFunction) PyFtrace_detach,
	 METH_VARARGS | METH_KEYWORDS,
	 "Detach object from the \'ftracepy\' module."
	},
	{"attach",
	 (PyCFunction) PyFtrace_attach,
	 METH_VARARGS | METH_KEYWORDS,
	 "Attach object to the \'ftracepy\' module."
	},
	{"is_attached",
	 (PyCFunction) PyFtrace_is_attached,
	 METH_VARARGS | METH_KEYWORDS,
	 "Check if the object is attached to the \'ftracepy\' module."
	},
	{"create_instance",
	 (PyCFunction) PyFtrace_create_instance,
	 METH_VARARGS | METH_KEYWORDS,
	 "Create new tracefs instance."
	},
	{"find_instance",
	 (PyCFunction) PyFtrace_find_instance,
	 METH_VARARGS | METH_KEYWORDS,
	 "Find an existing ftrace instance."
	},
	{"available_tracers",
	 (PyCFunction) PyFtrace_available_tracers,
	 METH_VARARGS | METH_KEYWORDS,
	 "Get a list of available tracers."
	},
	{"set_current_tracer",
	 (PyCFunction) PyFtrace_set_current_tracer,
	 METH_VARARGS | METH_KEYWORDS,
	 "Enable a tracer."
	},
	{"get_current_tracer",
	 (PyCFunction) PyFtrace_get_current_tracer,
	 METH_VARARGS | METH_KEYWORDS,
	 "Check the enabled tracer."
	},
	{"available_event_systems",
	 (PyCFunction) PyFtrace_available_event_systems,
	 METH_VARARGS | METH_KEYWORDS,
	 "Get a list of available trace event systems."
	},
	{"available_system_events",
	 (PyCFunction) PyFtrace_available_system_events,
	 METH_VARARGS | METH_KEYWORDS,
	 "Get a list of available trace event for a given system."
	},
	{"enable_event",
	 (PyCFunction) PyFtrace_enable_event,
	 METH_VARARGS | METH_KEYWORDS,
	 "Enable trece event."
	},
	{"disable_event",
	 (PyCFunction) PyFtrace_disable_event,
	 METH_VARARGS | METH_KEYWORDS,
	 "Disable trece event."
	},
	{"enable_events",
	 (PyCFunction) PyFtrace_enable_events,
	 METH_VARARGS | METH_KEYWORDS,
	 "Enable multiple trece event."
	},
	{"disable_events",
	 (PyCFunction) PyFtrace_disable_events,
	 METH_VARARGS | METH_KEYWORDS,
	 "Disable multiple trece event."
	},
	{"event_is_enabled",
	 (PyCFunction) PyFtrace_event_is_enabled,
	 METH_VARARGS | METH_KEYWORDS,
	 "Check if event is enabled."
	},
	{"set_event_filter",
	 (PyCFunction) PyFtrace_set_event_filter,
	 METH_VARARGS | METH_KEYWORDS,
	 "Define event filter."
	},
	{"clear_event_filter",
	 (PyCFunction) PyFtrace_clear_event_filter,
	 METH_VARARGS | METH_KEYWORDS,
	 "Clear event filter."
	},
	{"tracing_ON",
	 (PyCFunction) PyFtrace_tracing_ON,
	 METH_VARARGS | METH_KEYWORDS,
	 "Start tracing."
	},
	{"tracing_OFF",
	 (PyCFunction) PyFtrace_tracing_OFF,
	 METH_VARARGS | METH_KEYWORDS,
	 "Stop tracing."
	},
	{"is_tracing_ON",
	 (PyCFunction) PyFtrace_is_tracing_ON,
	 METH_VARARGS | METH_KEYWORDS,
	 "Check if tracing is ON."
	},
	{"set_event_pid",
	 (PyCFunction) PyFtrace_set_event_pid,
	 METH_VARARGS | METH_KEYWORDS,
	 "."
	},
	{"set_ftrace_pid",
	 (PyCFunction) PyFtrace_set_ftrace_pid,
	 METH_VARARGS | METH_KEYWORDS,
	 "."
	},
	{"enable_option",
	 (PyCFunction) PyFtrace_enable_option,
	 METH_VARARGS | METH_KEYWORDS,
	 "Enable trece option."
	},
	{"disable_option",
	 (PyCFunction) PyFtrace_disable_option,
	 METH_VARARGS | METH_KEYWORDS,
	 "Disable trece option."
	},
	{"option_is_set",
	 (PyCFunction) PyFtrace_option_is_set,
	 METH_VARARGS | METH_KEYWORDS,
	 "Check if trece option is enabled."
	},
	{"supported_options",
	 (PyCFunction) PyFtrace_supported_options,
	 METH_VARARGS | METH_KEYWORDS,
	 "Gat a list of all supported options."
	},
	{"enabled_options",
	 (PyCFunction) PyFtrace_enabled_options,
	 METH_VARARGS | METH_KEYWORDS,
	 "Gat a list of all supported options."
	},
	{"tc_event_system",
	 (PyCFunction) PyFtrace_tc_event_system,
	 METH_NOARGS,
	 "Get the name of the event system used by trace-cruncher."
	},
	{"register_kprobe",
	 (PyCFunction) PyFtrace_register_kprobe,
	 METH_VARARGS | METH_KEYWORDS,
	 "Define a kprobe."
	},
	{"register_kretprobe",
	 (PyCFunction) PyFtrace_register_kretprobe,
	 METH_VARARGS | METH_KEYWORDS,
	 "Define a kretprobe."
	},
	{"hist",
	 (PyCFunction) PyFtrace_hist,
	 METH_VARARGS | METH_KEYWORDS,
	 "Define a histogram."
	},
	{"synth",
	 (PyCFunction) PyFtrace_synth,
	 METH_VARARGS | METH_KEYWORDS,
	 "Define a synthetic event."
	},
	{"set_ftrace_loglevel",
	 (PyCFunction) PyFtrace_set_ftrace_loglevel,
	 METH_VARARGS | METH_KEYWORDS,
	 "Set the verbose level of the ftrace libraries."
	},
	{"trace_process",
	 (PyCFunction) PyFtrace_trace_process,
	 METH_VARARGS | METH_KEYWORDS,
	 "Trace a process."
	},
	{"trace_shell_process",
	 (PyCFunction) PyFtrace_trace_shell_process,
	 METH_VARARGS | METH_KEYWORDS,
	 "Trace a process executed within a shell."
	},
	{"read_trace",
	 (PyCFunction) PyFtrace_read_trace,
	 METH_VARARGS | METH_KEYWORDS,
	 "Trace a shell process."
	},
	{"iterate_trace",
	 (PyCFunction) PyFtrace_iterate_trace,
	 METH_VARARGS | METH_KEYWORDS,
	 "Trace a shell process."
	},
	{"hook2pid",
	 (PyCFunction) PyFtrace_hook2pid,
	 METH_VARARGS | METH_KEYWORDS,
	 "Trace only particular process."
	},
	{"error_log",
	 (PyCFunction) PyFtrace_error_log,
	 METH_VARARGS | METH_KEYWORDS,
	 "Get the content of the error log."
	},
	{"clear_error_log",
	 (PyCFunction) PyFtrace_clear_error_log,
	 METH_VARARGS | METH_KEYWORDS,
	 "Clear the content of the error log."
	},
	{NULL, NULL, 0, NULL}
};

static struct PyModuleDef ftracepy_module = {
	PyModuleDef_HEAD_INIT,
	"ftracepy",
	"Python interface for Ftrace.",
	-1,
	ftracepy_methods
};

PyMODINIT_FUNC PyInit_ftracepy(void)
{
	if (!PyTepTypeInit())
		return NULL;

	if (!PyTepEventTypeInit())
		return NULL;

	if (!PyTepRecordTypeInit())
		return NULL;

	if (!PyTfsInstanceTypeInit())
		return NULL;

	if (!PyDyneventTypeInit())
		return NULL;

	if (!PyTraceHistTypeInit())
		return NULL;

	if (!PySynthEventTypeInit())
		return NULL;

	TFS_ERROR = PyErr_NewException("tracecruncher.ftracepy.tfs_error",
				       NULL, NULL);

	TEP_ERROR = PyErr_NewException("tracecruncher.ftracepy.tep_error",
				       NULL, NULL);

	TRACECRUNCHER_ERROR = PyErr_NewException("tracecruncher.tc_error",
						 NULL, NULL);

	PyObject *module = PyModule_Create(&ftracepy_module);

	PyModule_AddObject(module, "tep_handle", (PyObject *) &PyTepType);
	PyModule_AddObject(module, "tep_event", (PyObject *) &PyTepEventType);
	PyModule_AddObject(module, "tep_record", (PyObject *) &PyTepRecordType);
	PyModule_AddObject(module, "tracefs_instance", (PyObject *) &PyTfsInstanceType);
	PyModule_AddObject(module, "tracefs_dynevent", (PyObject *) &PyDyneventType);
	PyModule_AddObject(module, "tracefs_hist", (PyObject *) &PyTraceHistType);
	PyModule_AddObject(module, "tracefs_synth", (PyObject *) &PySynthEventType);

	PyModule_AddObject(module, "tfs_error", TFS_ERROR);
	PyModule_AddObject(module, "tep_error", TEP_ERROR);
	PyModule_AddObject(module, "tc_error", TRACECRUNCHER_ERROR);

	if (geteuid() != 0) {
		PyErr_SetString(TFS_ERROR,
				"Permission denied. Root privileges are required.");
		return NULL;
	}

	Py_AtExit(PyFtrace_at_exit);

	return module;
}
