// SPDX-License-Identifier: LGPL-2.1

/*
 * Copyright 2021 VMware Inc, Yordan Karadzhov (VMware) <y.karadz@gmail.com>
 */

// trace-cruncher
#include "ftracepy-utils.h"
#include "ftracepy-docs.h"

extern PyObject *TFS_ERROR;
extern PyObject *TEP_ERROR;
extern PyObject *TRACECRUNCHER_ERROR;

static PyMethodDef PyTepRecord_methods[] = {
	{"time",
	 (PyCFunction) PyTepRecord_time,
	 METH_NOARGS,
	 PyTepRecord_time_doc,
	},
	{"CPU",
	 (PyCFunction) PyTepRecord_cpu,
	 METH_NOARGS,
	 PyTepRecord_CPU_doc,
	},
	{NULL}
};

C_OBJECT_WRAPPER(tep_record, PyTepRecord, NO_DESTROY, NO_FREE)

static PyMethodDef PyTepEvent_methods[] = {
	{"name",
	 (PyCFunction) PyTepEvent_name,
	 METH_NOARGS,
	 PyTepEvent_name_doc,
	},
	{"id",
	 (PyCFunction) PyTepEvent_id,
	 METH_NOARGS,
	 PyTepEvent_id_doc,
	},
	{"field_names",
	 (PyCFunction) PyTepEvent_field_names,
	 METH_NOARGS,
	 PyTepEvent_field_names_doc,
	},
	{"parse_record_field",
	 (PyCFunction) PyTepEvent_parse_record_field,
	 METH_VARARGS | METH_KEYWORDS,
	 PyTepEvent_parse_record_field_doc,
	},
	{"get_pid",
	 (PyCFunction) PyTepEvent_get_pid,
	 METH_VARARGS | METH_KEYWORDS,
	 PyTepEvent_get_pid_doc,
	},
	{NULL}
};

C_OBJECT_WRAPPER(tep_event, PyTepEvent, NO_DESTROY, NO_FREE)

static PyMethodDef PyTep_methods[] = {
	{"init_local",
	 (PyCFunction) PyTep_init_local,
	 METH_VARARGS | METH_KEYWORDS,
	 PyTep_init_local_doc,
	},
	{"get_event",
	 (PyCFunction) PyTep_get_event,
	 METH_VARARGS | METH_KEYWORDS,
	 PyTep_get_event_doc,
	},
	{"event_record",
	 (PyCFunction) PyTep_event_record,
	 METH_VARARGS | METH_KEYWORDS,
	 PyTep_event_record_doc,
	},
	{"process",
	 (PyCFunction) PyTep_process,
	 METH_VARARGS | METH_KEYWORDS,
	 PyTep_process_doc,
	},
	{"info",
	 (PyCFunction) PyTep_info,
	 METH_VARARGS | METH_KEYWORDS,
	 PyTep_info_doc,
	},
	{"short_kprobe_print",
	 (PyCFunction) PyTep_short_kprobe_print,
	 METH_VARARGS | METH_KEYWORDS,
	 PyTep_short_kprobe_print_doc,
	},
	{NULL}
};

C_OBJECT_WRAPPER(tep_handle, PyTep, NO_DESTROY, tep_free)

static PyMethodDef PyTfsInstance_methods[] = {
	{"dir",
	 (PyCFunction) PyTfsInstance_dir,
	 METH_NOARGS,
	 PyTfsInstance_dir_doc,
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
	 PyDynevent_event_doc,
	},
	{"system",
	 (PyCFunction) PyDynevent_system,
	 METH_NOARGS,
	 PyDynevent_system_doc,
	},
	{"address",
	 (PyCFunction) PyDynevent_address,
	 METH_NOARGS,
	 PyDynevent_address_doc,
	},
	{"probe",
	 (PyCFunction) PyDynevent_probe,
	 METH_NOARGS,
	 PyDynevent_probe_doc,
	},
	{"register",
	 (PyCFunction) PyDynevent_register,
	 METH_NOARGS,
	 PyDynevent_register_doc,
	},
	{"unregister",
	 (PyCFunction) PyDynevent_unregister,
	 METH_NOARGS,
	 PyDynevent_unregister_doc,
	},
	{"set_filter",
	 (PyCFunction) PyDynevent_set_filter,
	 METH_VARARGS | METH_KEYWORDS,
	 PyDynevent_set_filter_doc,
	},
	{"get_filter",
	 (PyCFunction) PyDynevent_get_filter,
	 METH_VARARGS | METH_KEYWORDS,
	 PyDynevent_get_filter_doc,
	},
	{"clear_filter",
	 (PyCFunction) PyDynevent_clear_filter,
	 METH_VARARGS | METH_KEYWORDS,
	 PyDynevent_clearfilter_doc,
	},
	{"enable",
	 (PyCFunction) PyDynevent_enable,
	 METH_VARARGS | METH_KEYWORDS,
	 PyDynevent_enable_doc,
	},
	{"disable",
	 (PyCFunction) PyDynevent_disable,
	 METH_VARARGS | METH_KEYWORDS,
	 PyDynevent_disable_doc
	},
	{"is_enabled",
	 (PyCFunction) PyDynevent_is_enabled,
	 METH_VARARGS | METH_KEYWORDS,
	 PyDynevent_is_enabled_doc,
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
	 PyTraceHist_add_value_doc,
	},
	{"sort_keys",
	 (PyCFunction) PyTraceHist_sort_keys,
	 METH_VARARGS | METH_KEYWORDS,
	 PyTraceHist_sort_keys_doc,
	},
	{"sort_key_direction",
	 (PyCFunction) PyTraceHist_sort_key_direction,
	 METH_VARARGS | METH_KEYWORDS,
	 PyTraceHist_sort_key_direction_doc,
	},
	{"start",
	 (PyCFunction) PyTraceHist_start,
	 METH_VARARGS | METH_KEYWORDS,
	 PyTraceHist_start_doc,
	},
	{"stop",
	 (PyCFunction) PyTraceHist_stop,
	 METH_VARARGS | METH_KEYWORDS,
	 PyTraceHist_stop_doc,
	},
	{"resume",
	 (PyCFunction) PyTraceHist_resume,
	 METH_VARARGS | METH_KEYWORDS,
	 PyTraceHist_resume_doc,
	},
	{"clear",
	 (PyCFunction) PyTraceHist_clear,
	 METH_VARARGS | METH_KEYWORDS,
	 PyTraceHist_clear_doc,
	},
	{"read",
	 (PyCFunction) PyTraceHist_read,
	 METH_VARARGS | METH_KEYWORDS,
	 PyTraceHist_read_doc,
	},
	{"close",
	 (PyCFunction) PyTraceHist_close,
	 METH_VARARGS | METH_KEYWORDS,
	 PyTraceHist_close_doc
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
	 PySynthEvent_add_start_fields_doc,
	},
	{"add_end_fields",
	 (PyCFunction) PySynthEvent_add_end_fields,
	 METH_VARARGS | METH_KEYWORDS,
	 PySynthEvent_add_end_fields_doc,
	},
	{"add_delta_start",
	 (PyCFunction) PySynthEvent_add_delta_start,
	 METH_VARARGS | METH_KEYWORDS,
	 PySynthEvent_add_delta_start_doc,
	},
	{"add_delta_end",
	 (PyCFunction) PySynthEvent_add_delta_end,
	 METH_VARARGS | METH_KEYWORDS,
	 PySynthEvent_add_delta_start_doc,
	},
	{"add_delta_T",
	 (PyCFunction) PySynthEvent_add_delta_T,
	 METH_VARARGS | METH_KEYWORDS,
	 PySynthEvent_add_delta_T_doc,
	},
	{"add_sum",
	 (PyCFunction) PySynthEvent_add_delta_T,
	 METH_VARARGS | METH_KEYWORDS,
	 PySynthEvent_add_sum_doc,
	},
	{"register",
	 (PyCFunction) PySynthEvent_register,
	 METH_NOARGS,
	 PySynthEvent_register_doc,
	},
	{"unregister",
	 (PyCFunction) PySynthEvent_unregister,
	 METH_NOARGS,
	 PySynthEvent_unregister_doc,
	},
	{"enable",
	 (PyCFunction) PySynthEvent_enable,
	 METH_VARARGS | METH_KEYWORDS,
	 PySynthEvent_enable_doc,
	},
	{"disable",
	 (PyCFunction) PySynthEvent_disable,
	 METH_VARARGS | METH_KEYWORDS,
	 PySynthEvent_disable_doc,
	},
	{"is_enabled",
	 (PyCFunction) PySynthEvent_is_enabled,
	 METH_VARARGS | METH_KEYWORDS,
	 PyDynevent_is_enabled_doc,
	},
	{"set_filter",
	 (PyCFunction) PySynthEvent_set_filter,
	 METH_VARARGS | METH_KEYWORDS,
	 PySynthEvent_set_filter_doc,
	},
	{"get_filter",
	 (PyCFunction) PySynthEvent_get_filter,
	 METH_VARARGS | METH_KEYWORDS,
	 PySynthEvent_get_filter_doc,
	},
	{"clear_filter",
	 (PyCFunction) PySynthEvent_clear_filter,
	 METH_VARARGS | METH_KEYWORDS,
	 PySynthEvent_clearfilter_doc,
	},
	{"repr",
	 (PyCFunction) PySynthEvent_repr,
	 METH_VARARGS | METH_KEYWORDS,
	 PySynthEvent_repr_doc,
	},
	{NULL, NULL, 0, NULL}
};

C_OBJECT_WRAPPER(tracefs_synth, PySynthEvent,
		 tracefs_synth_destroy,
		 tracefs_synth_free)

static PyMethodDef PyUserTrace_methods[] = {
	{"add_function",
	 (PyCFunction) PyUserTrace_add_function,
	 METH_VARARGS | METH_KEYWORDS,
	 "Add tracepoint on user function."
	},
	{"add_ret_function",
	 (PyCFunction) PyUserTrace_add_ret_function,
	 METH_VARARGS | METH_KEYWORDS,
	 "Add tracepoint on user function return."
	},
	{"enable",
	 (PyCFunction) PyUserTrace_enable,
	 METH_VARARGS | METH_KEYWORDS,
	 "Enable tracing of the configured user tracepoints."
	},
	{"disable",
	 (PyCFunction) PyUserTrace_disable,
	 METH_VARARGS | METH_KEYWORDS,
	 "Disable tracing of the configured user tracepoints."
	},
	{NULL, NULL, 0, NULL}
};
C_OBJECT_WRAPPER(py_utrace_context, PyUserTrace,
		 py_utrace_destroy, py_utrace_free)

static PyMethodDef ftracepy_methods[] = {
	{"dir",
	 (PyCFunction) PyFtrace_dir,
	 METH_NOARGS,
	 PyFtrace_dir_doc,
	},
	{"detach",
	 (PyCFunction) PyFtrace_detach,
	 METH_VARARGS | METH_KEYWORDS,
	 PyFtrace_detach_doc,
	},
	{"attach",
	 (PyCFunction) PyFtrace_attach,
	 METH_VARARGS | METH_KEYWORDS,
	 PyFtrace_attach_doc,
	},
	{"is_attached",
	 (PyCFunction) PyFtrace_is_attached,
	 METH_VARARGS | METH_KEYWORDS,
	 PyFtrace_is_attached_doc,
	},
	{"create_instance",
	 (PyCFunction) PyFtrace_create_instance,
	 METH_VARARGS | METH_KEYWORDS,
	 PyFtrace_create_instance_doc,
	},
	{"find_instance",
	 (PyCFunction) PyFtrace_find_instance,
	 METH_VARARGS | METH_KEYWORDS,
	 PyFtrace_find_instance_doc,
	},
	{"available_tracers",
	 (PyCFunction) PyFtrace_available_tracers,
	 METH_VARARGS | METH_KEYWORDS,
	 PyFtrace_available_tracers_doc,
	},
	{"set_current_tracer",
	 (PyCFunction) PyFtrace_set_current_tracer,
	 METH_VARARGS | METH_KEYWORDS,
	 PyFtrace_set_current_tracer_doc,
	},
	{"get_current_tracer",
	 (PyCFunction) PyFtrace_get_current_tracer,
	 METH_VARARGS | METH_KEYWORDS,
	 PyFtrace_get_current_tracer_doc,
	},
	{"available_event_systems",
	 (PyCFunction) PyFtrace_available_event_systems,
	 METH_VARARGS | METH_KEYWORDS,
	 PyFtrace_available_event_systems_doc,
	},
	{"available_system_events",
	 (PyCFunction) PyFtrace_available_system_events,
	 METH_VARARGS | METH_KEYWORDS,
	 PyFtrace_available_event_systems_doc,
	},
	{"enable_event",
	 (PyCFunction) PyFtrace_enable_event,
	 METH_VARARGS | METH_KEYWORDS,
	 PyFtrace_enable_event_doc,
	},
	{"disable_event",
	 (PyCFunction) PyFtrace_disable_event,
	 METH_VARARGS | METH_KEYWORDS,
	 PyFtrace_disable_event_doc,
	},
	{"enable_events",
	 (PyCFunction) PyFtrace_enable_events,
	 METH_VARARGS | METH_KEYWORDS,
	 PyFtrace_enable_events_doc,
	},
	{"disable_events",
	 (PyCFunction) PyFtrace_disable_events,
	 METH_VARARGS | METH_KEYWORDS,
	 PyFtrace_disable_events_doc,
	},
	{"event_is_enabled",
	 (PyCFunction) PyFtrace_event_is_enabled,
	 METH_VARARGS | METH_KEYWORDS,
	 PyFtrace_event_is_enabled_doc,
	},
	{"set_event_filter",
	 (PyCFunction) PyFtrace_set_event_filter,
	 METH_VARARGS | METH_KEYWORDS,
	 PyFtrace_set_event_filter_doc,
	},
	{"clear_event_filter",
	 (PyCFunction) PyFtrace_clear_event_filter,
	 METH_VARARGS | METH_KEYWORDS,
	 PyFtrace_clear_event_filter_doc,
	},
	{"tracing_ON",
	 (PyCFunction) PyFtrace_tracing_ON,
	 METH_VARARGS | METH_KEYWORDS,
	 PyFtrace_tracing_ON_doc
	},
	{"tracing_OFF",
	 (PyCFunction) PyFtrace_tracing_OFF,
	 METH_VARARGS | METH_KEYWORDS,
	 PyFtrace_tracing_OFF_doc
	},
	{"is_tracing_ON",
	 (PyCFunction) PyFtrace_is_tracing_ON,
	 METH_VARARGS | METH_KEYWORDS,
	 PyFtrace_is_tracing_ON_doc,
	},
	{"set_event_pid",
	 (PyCFunction) PyFtrace_set_event_pid,
	 METH_VARARGS | METH_KEYWORDS,
	 PyFtrace_set_event_pid_doc,
	},
	{"set_ftrace_pid",
	 (PyCFunction) PyFtrace_set_ftrace_pid,
	 METH_VARARGS | METH_KEYWORDS,
	 PyFtrace_set_ftrace_pid_doc,
	},
	{"enable_option",
	 (PyCFunction) PyFtrace_enable_option,
	 METH_VARARGS | METH_KEYWORDS,
	 PyFtrace_enable_option_doc
	},
	{"disable_option",
	 (PyCFunction) PyFtrace_disable_option,
	 METH_VARARGS | METH_KEYWORDS,
	 PyFtrace_disable_option_doc
	},
	{"option_is_set",
	 (PyCFunction) PyFtrace_option_is_set,
	 METH_VARARGS | METH_KEYWORDS,
	 PyFtrace_option_is_set_doc,
	},
	{"supported_options",
	 (PyCFunction) PyFtrace_supported_options,
	 METH_VARARGS | METH_KEYWORDS,
	 PyFtrace_supported_options_doc,
	},
	{"enabled_options",
	 (PyCFunction) PyFtrace_enabled_options,
	 METH_VARARGS | METH_KEYWORDS,
	 PyFtrace_enabled_options_doc
	},
	{"tc_event_system",
	 (PyCFunction) PyFtrace_tc_event_system,
	 METH_NOARGS,
	 "Get the name of the event system used by trace-cruncher."
	},
	{"no_arg",
	 (PyCFunction) PyFtrace_no_arg,
	 METH_NOARGS,
	 "Returns a default value for an optional function argument."
	},
	{"kprobe",
	 (PyCFunction) PyFtrace_kprobe,
	 METH_VARARGS | METH_KEYWORDS,
	 PyFtrace_kprobe_doc,
	},
	{"kretprobe",
	 (PyCFunction) PyFtrace_kretprobe,
	 METH_VARARGS | METH_KEYWORDS,
	 PyFtrace_kretprobe_doc,
	},
	{"eprobe",
	 (PyCFunction) PyFtrace_eprobe,
	 METH_VARARGS | METH_KEYWORDS,
	 PyFtrace_eprobe_doc,
	},
	{"uprobe",
	 (PyCFunction) PyFtrace_uprobe,
	 METH_VARARGS | METH_KEYWORDS,
	 "Define a uprobe."
	},
	{"uretprobe",
	 (PyCFunction) PyFtrace_uretprobe,
	 METH_VARARGS | METH_KEYWORDS,
	 "Define a uretprobe."
	},
	{"hist",
	 (PyCFunction) PyFtrace_hist,
	 METH_VARARGS | METH_KEYWORDS,
	 PyFtrace_hist_doc,
	},
	{"synth",
	 (PyCFunction) PyFtrace_synth,
	 METH_VARARGS | METH_KEYWORDS,
	 PyFtrace_synth_doc,
	},
	{"user_trace",
	 (PyCFunction) PyFtrace_user_trace,
	 METH_VARARGS | METH_KEYWORDS,
	 "Create a context for tracing a user process using uprobes"
	},
	{"set_ftrace_loglevel",
	 (PyCFunction) PyFtrace_set_ftrace_loglevel,
	 METH_VARARGS | METH_KEYWORDS,
	 PyFtrace_set_ftrace_loglevel_doc,
	},
	{"trace_process",
	 (PyCFunction) PyFtrace_trace_process,
	 METH_VARARGS | METH_KEYWORDS,
	 PyFtrace_trace_process_doc,
	},
	{"trace_shell_process",
	 (PyCFunction) PyFtrace_trace_shell_process,
	 METH_VARARGS | METH_KEYWORDS,
	 PyFtrace_trace_shell_process_doc,
	},
	{"read_trace",
	 (PyCFunction) PyFtrace_read_trace,
	 METH_VARARGS | METH_KEYWORDS,
	 PyFtrace_read_trace_doc,
	},
	{"wait",
	 (PyCFunction) PyFtrace_wait,
	 METH_VARARGS | METH_KEYWORDS,
	 PyFtrace_wait_doc,
	},
	{"iterate_trace",
	 (PyCFunction) PyFtrace_iterate_trace,
	 METH_VARARGS | METH_KEYWORDS,
	 PyFtrace_iterate_trace_doc,
	},
	{"hook2pid",
	 (PyCFunction) PyFtrace_hook2pid,
	 METH_VARARGS | METH_KEYWORDS,
	 PyFtrace_hook2pid_doc,
	},
	{"error_log",
	 (PyCFunction) PyFtrace_error_log,
	 METH_VARARGS | METH_KEYWORDS,
	 PyFtrace_error_log_doc,
	},
	{"clear_error_log",
	 (PyCFunction) PyFtrace_clear_error_log,
	 METH_VARARGS | METH_KEYWORDS,
	 PyFtrace_clear_error_log_doc,
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

	if (!PyUserTraceTypeInit())
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
	PyModule_AddObject(module, "py_utrace_context", (PyObject *) &PyUserTraceType);

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
