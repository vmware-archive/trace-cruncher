/* SPDX-License-Identifier: LGPL-2.1 */

/*
 * Copyright 2022 VMware Inc, Yordan Karadzhov <y.karadz@gmail.com>
 */

#ifndef _TC_FTRACE_PY_DOCS
#define _TC_FTRACE_PY_DOCS

PyDoc_STRVAR(PyTepRecord_time_doc,
	     "time()\n"
	     "--\n\n"
	     "Get the time of the record.\n"
	     "\n"
	     "Returns\n"
	     "-------\n"
	     "t : int\n"
	     "    Timestamp in nanoseconds"
);

PyDoc_STRVAR(PyTepRecord_CPU_doc,
	     "CPU()\n"
	     "--\n\n"
	     "Get the CPU Id number of the record.\n"
	     "\n"
	     "Returns\n"
	     "-------\n"
	     "cpu : int\n"
	     "    CPU Id number."
);

PyDoc_STRVAR(PyTepEvent_name_doc,
	     "name()\n"
	     "--\n\n"
	     "Get the name of the event.\n"
	     "\n"
	     "Returns\n"
	     "-------\n"
	     "name : string\n"
	     "    Event name"
);

PyDoc_STRVAR(PyTepEvent_id_doc,
	     "id()\n"
	     "--\n\n"
	     "Get the unique Id number of the event.\n"
	     "\n"
	     "Returns\n"
	     "-------\n"
	     "id : int\n"
	     "    Id number."
);

PyDoc_STRVAR(PyTepEvent_field_names_doc,
	     "field_names()\n"
	     "--\n\n"
	     "Get the names of all fields of a given event.\n"
	     "\n"
	     "Returns\n"
	     "-------\n"
	     "list of strings\n"
	     "    All fields of the event"
);

PyDoc_STRVAR(PyTepEvent_parse_record_field_doc,
	     "parse_record_field(record, field)\n"
	     "--\n\n"
	     "Get the content of a record field.\n"
	     "\n"
	     "Parameters\n"
	     "----------\n"
	     "record : PyTepRecord\n"
	     "    Event record to derive the field value from.\n"
	     "\n"
	     "field : string\n"
	     "    The name of the field.\n"
	     "\n"
	     "Returns\n"
	     "-------\n"
	     "fld : int or string \n"
	     "    The value of the field."
);

PyDoc_STRVAR(PyTepEvent_get_pid_doc,
	     "get_pid(record)\n"
	     "--\n\n"
	     "Get the Process Id of the record.\n"
	     "\n"
	     "Parameters\n"
	     "----------\n"
	     "record : PyTepRecord\n"
	     "    Event record to derive the PID from.\n"
	     "\n"
	     "Returns\n"
	     "-------\n"
	     "pid : int\n"
	     "    PID value."
);

PyDoc_STRVAR(PyTep_init_local_doc,
	     "init_local(dir)\n"
	     "--\n\n"
	     "Initialize tep from the local events\n"
	     "\n"
	     "Create Trace Events Parser (tep) from a trace instance path.\n"
	     "\n"
	     "Parameters\n"
	     "----------\n"
	     "dir : string\n"
	     "    The instance directory that contains the events.\n"
	     "\n"
	     "system : string or list of strings (optional)\n"
	     "    One or more system names, to load the events from. This argument is optional.\n"
);

PyDoc_STRVAR(PyTep_get_event_doc,
	     "get_event(system, name)\n"
	     "--\n\n"
	     "Get a Tep Event for a given trace event.\n"
	     "\n"
	     "Parameters\n"
	     "----------\n"
	     "system : string\n"
	     "    The system of the event.\n"
	     "\n"
	     "name : string\n"
	     "    The name of the event.\n"
	     "\n"
	     "Returns\n"
	     "-------\n"
	     "evt : PyTepEvent\n"
	     "    A Tep Event corresponding to the given trace event."
);

PyDoc_STRVAR(PyTep_event_record_doc,
	     "event_record(event, record)\n"
	     "--\n\n"
	     "Generic print of a recorded trace event.\n"
	     "\n"
	     "Parameters\n"
	     "----------\n"
	     "event : PyTepEvent\n"
	     "    The event descriptor.\n"
	     "\n"
	     "record : PyTepRecord\n"
	     "    The record.\n"
	     "\n"
	     "Returns\n"
	     "-------\n"
	     "rec : string\n"
	     "    The recorded tracing data in a human-readable form."
);

PyDoc_STRVAR(PyTep_process_doc,
	     "process(event, record)\n"
	     "--\n\n"
	     "Generic print of the process that generated the trace event.\n"
	     "\n"
	     "Parameters\n"
	     "----------\n"
	     "event : PyTepEvent\n"
	     "    The event descriptor.\n"
	     "\n"
	     "record : PyTepRecord\n"
	     "    The record.\n"
	     "\n"
	     "Returns\n"
	     "-------\n"
	     "proc : string\n"
	     "    The name of the process and its PID number."
);

PyDoc_STRVAR(PyTep_info_doc,
	     "info(event, record)\n"
	     "--\n\n"
	     "Generic print of a trace event information.\n"
	     "\n"
	     "Parameters\n"
	     "----------\n"
	     "event : PyTepEvent\n"
	     "    The event descriptor.\n"
	     "\n"
	     "record : PyTepRecord\n"
	     "    The record.\n"
	     "\n"
	     "Returns\n"
	     "-------\n"
	     "dir : string\n"
	     "    The recorded values of the event fields in a human-readable form."
);

PyDoc_STRVAR(PyTep_short_kprobe_print_doc,
	     "short_kprobe_print(system, event, id)\n"
	     "--\n\n"
	     "Do not print the address of the probe.\n"
	     "\n"
	     "Parameters\n"
	     "----------\n"
	     "system : string\n"
	     "    The system of the event.\n"
	     "\n"
	     "event : string\n"
	     "    The name of the event.\n"
	     "\n"
	     "id : int (optional)\n"
	     "    The Id number of the event. This argument is optional."
);

PyDoc_STRVAR(PyTfsInstance_dir_doc,
	     "dir()\n"
	     "--\n\n"
	     "Get the absolute path to the Ftrace instance directory.\n"
	     "\n"
	     "Returns\n"
	     "-------\n"
	     "dir : string\n"
	     "    Ftrace instance directory.\n"
	     "\n"
);

PyDoc_STRVAR(PyTfsInstance_reset_doc,
	     "reset()\n"
	     "--\n\n"
	     "Reset a Ftrace instance to its default state.\n"
);

PyDoc_STRVAR(PyTfsInstance_delete_doc,
	     "delete()\n"
	     "--\n\n"
	     "Reset a Ftrace instance to its default state and delete it from the system\n"
);

PyDoc_STRVAR(PyDynevent_event_doc,
	     "event()\n"
	     "--\n\n"
	     "Get the name of the dynamic event.\n"
	     "\n"
	     "Returns\n"
	     "-------\n"
	     "name : string\n"
	     "    The unique name of the event."
);

PyDoc_STRVAR(PyDynevent_system_doc,
	     "system()\n"
	     "--\n\n"
	     "Get the system name of the dynamic event.\n"
	     "\n"
	     "Returns\n"
	     "-------\n"
	     "system : string\n"
	     "    The unique name of the event's system."
);

PyDoc_STRVAR(PyDynevent_address_doc,
	     "address()\n"
	     "--\n\n"
	     "Get the address / function name of the dynamic event.\n"
	     "\n"
	     "Returns\n"
	     "-------\n"
	     "address : string\n"
	     "    The address / function name."
);

PyDoc_STRVAR(PyDynevent_probe_doc,
	     "probe()\n"
	     "--\n\n"
	     "Get the event definition.\n"
	     "\n"
	     "Returns\n"
	     "-------\n"
	     "probe : string\n"
	     "    The descriptor of the probe."
);

PyDoc_STRVAR(PyDynevent_register_doc,
	     "register()\n"
	     "--\n\n"
	     "Register dynamic event.\n"
);

PyDoc_STRVAR(PyDynevent_unregister_doc,
	     "unregister()\n"
	     "--\n\n"
	     "Unregister dynamic event.\n"
);

PyDoc_STRVAR(PyDynevent_set_filter_doc,
	     "set_filter(filter, instance)\n"
	     "--\n\n"
	     "Define a filter for a dynamic event.\n"
	     "\n"
	     "Parameters\n"
	     "----------\n"
	     "filter : string\n"
	     "    The filter descriptor.\n"
	     "\n"
	     "instance : PyTfsInstance (optional)\n"
	     "    The Ftrace instance. This argument is optional. If not provided, the 'top' instance is used.\n"
);

PyDoc_STRVAR(PyDynevent_get_filter_doc,
	     "get_filter(instance)\n"
	     "--\n\n"
	     "Get the filter of a dynamic event.\n"
	     "\n"
	     "Parameters\n"
	     "----------\n"
	     "instance : PyTfsInstance (optional)\n"
	     "    The Ftrace instance. This argument is optional. If not provided, the 'top' instance is used.\n"
	     "\n"
	     "Returns\n"
	     "-------\n"
	     "filter : string\n"
	     "    The filter descriptor."
);

PyDoc_STRVAR(PyDynevent_clearfilter_doc,
	     "clear_filter(instance)\n"
	     "--\n\n"
	     "Clear the filter of a dynamic event.\n"
	     "\n"
	     "Parameters\n"
	     "----------\n"
	     "instance : PyTfsInstance (optional)\n"
	     "    The Ftrace instance. This argument is optional. If not provided, the 'top' instance is used.\n"
	     "\n"
	     "Returns\n"
	     "-------\n"
	     "filter : string\n"
	     "    The filter descriptor."
);

PyDoc_STRVAR(PyDynevent_enable_doc,
	     "enable(instance)\n"
	     "--\n\n"
	     "Enable dynamic event.\n"
	     "\n"
	     "Parameters\n"
	     "----------\n"
	     "instance : PyTfsInstance (optional)\n"
	     "    The Ftrace instance to be used. This argument is optional. If not provided, the 'top' instance is used.\n"
);

PyDoc_STRVAR(PyDynevent_disable_doc,
	     "disable(instance)\n"
	     "--\n\n"
	     "Disable dynamic event.\n"
	     "\n"
	     "Parameters\n"
	     "----------\n"
	     "instance : PyTfsInstance (optional)\n"
	     "    The Ftrace instance to be used. This argument is optional. If not provided, the 'top' instance is used.\n"
);

PyDoc_STRVAR(PyDynevent_is_enabled_doc,
	     "is_enabled(instance)\n"
	     "--\n\n"
	     "Check if dynamic event is enabled.\n"
	     "\n"
	     "Parameters\n"
	     "----------\n"
	     "instance : PyTfsInstance (optional)\n"
	     "    The Ftrace instance to be used. This argument is optional. If not provided, the 'top' instance is used.\n"
	     "\n"
	     "Returns\n"
	     "-------\n"
	     "bool : enb\n"
	     "    True if the event is enable."
);

PyDoc_STRVAR(PyTraceHist_add_value_doc,
	     "add_value(value)\n"
	     "--\n\n"
	     "Add value field to be used as a weight of the individual histogram entries.\n"
	     "\n"
	     "Parameters\n"
	     "----------\n"
	     "value : string\n"
	     "    The key name of the value field.\n"
);

PyDoc_STRVAR(PyTraceHist_sort_keys_doc,
	     "sork_keys()\n"
	     "--\n\n"
	     "Set key felds or values to sort on.\n"
	     "\n"
	     "Parameters\n"
	     "----------\n"
	     "keys : string or list of strings\n"
	     "    The keys to sort.\n"
);

PyDoc_STRVAR(PyTraceHist_sort_key_direction_doc,
	     "sort_key_direction(sort_key, direction)\n"
	     "--\n\n"
	     "Set the direction of a sort for key field.\n"
	     "\n"
	     "Parameters\n"
	     "----------\n"
	     "sort_key : string\n"
	     "    The key to set sort direction.\n"
	     "\n"
	     "direction : int or string\n"
	     "    Use 0, 'a', 'asc' or 'ascending' for ascending direction.\n"
	     "    Use 1, 'd', 'desc' or 'descending' for descending direction.\n"
);

PyDoc_STRVAR(PyTraceHist_start_doc,
	     "start(instance)\n"
	     "--\n\n"
	     "Start acquiring data.\n"
	     "\n"
	     "Parameters\n"
	     "----------\n"
	     "instance : PyTfsInstance (optional)\n"
	     "    The Ftrace instance to be used. This argument is optional. If not provided, the 'top' instance is used.\n"
);

PyDoc_STRVAR(PyTraceHist_stop_doc,
	     "stop(instance)\n"
	     "--\n\n"
	     "Stop (pause) acquiring data.\n"
	     "\n"
	     "Parameters\n"
	     "----------\n"
	     "instance : PyTfsInstance (optional)\n"
	     "    The Ftrace instance to be used. This argument is optional. If not provided, the 'top' instance is used.\n"
);

PyDoc_STRVAR(PyTraceHist_resume_doc,
	     "resume(instance)\n"
	     "--\n\n"
	     "Resune (continue) acquiring data.\n"
	     "\n"
	     "Parameters\n"
	     "----------\n"
	     "instance : PyTfsInstance (optional)\n"
	     "    The Ftrace instance to be used. This argument is optional. If not provided, the 'top' instance is used.\n"
);

PyDoc_STRVAR(PyTraceHist_clear_doc,
	     "clear(instance)\n"
	     "--\n\n"
	     "Clear (reset) the histogram.\n"
	     "\n"
	     "Parameters\n"
	     "----------\n"
	     "instance : PyTfsInstance (optional)\n"
	     "    The Ftrace instance to be used. This argument is optional. If not provided, the 'top' instance is used.\n"
);

PyDoc_STRVAR(PyTraceHist_read_doc,
	     "read(instance)\n"
	     "--\n\n"
	     "Read the content of the histogram."
	     "\n"
	     "Parameters\n"
	     "----------\n"
	     "instance : PyTfsInstance (optional)\n"
	     "    The Ftrace instance to be used. This argument is optional. If not provided, the 'top' instance is used.\n"
	     "\n"
	     "Returns\n"
	     "-------\n"
	     "data : string\n"
	     "    Summary of the data being acquired."
);

PyDoc_STRVAR(PyTraceHist_close_doc,
	     "close(instance)\n"
	     "--\n\n"
	     "Close (destroy) the histogram..\n"
	     "\n"
	     "Parameters\n"
	     "----------\n"
	     "instance : PyTfsInstance (optional)\n"
	     "    The Ftrace instance to be used. This argument is optional. If not provided, the 'top' instance is used.\n"
);

PyDoc_STRVAR(PySynthEvent_add_start_fields_doc,
	     "add_start_fields(fields, names)\n"
	     "--\n\n"
	     "Add fields from the start event to save.\n"
	     "\n"
	     "Parameters\n"
	     "----------\n"
	     "fields : List of strings\n"
	     "    Names of the fields from the start event to be saved.\n"
	     "\n"
	     "names : List of strings (optional)\n"
	     "    Can be used to rename the saved fields. The order of the new names,\n"
	     "    provided here corresponds to the order of original names provided in\n"
	     "    @fields. If an element of the list is 'None', the original name will\n"
	     "    be used for the corresponding field."
	     "    This argument is optional. If not provided, the original names are used.\n"
	     "\n"
	     "Example\n"
	     "-------\n"
	     "    evt.add_start_fields(fields=['target_cpu', 'prio'],\n"
             "                         names=['cpu', None])"
);

PyDoc_STRVAR(PySynthEvent_add_end_fields_doc,
	     "add_end_fields(fields, names)\n"
	     "--\n\n"
	     "Add fields from the end event to save.\n"
	     "\n"
	     "Parameters\n"
	     "----------\n"
	     "fields : List of strings\n"
	     "    Names of the fields from the end event to be saved.\n"
	     "\n"
	     "names : List of strings (optional)\n"
	     "    Can be used to rename the saved fields. The order of the new names,\n"
	     "    provided here corresponds to the order of original names provided in\n"
	     "    @fields. If an element of the list is 'None', the original name will\n"
	     "    be used for the corresponding field."
	     "    This argument is optional. If not provided, the original names are used.\n"
	     "\n"
	     "Example\n"
	     "-------\n"
	     "    evt.add_end_fields(fields=['target_cpu', 'prio'],\n"
             "                       names=['cpu', None])"
);

PyDoc_STRVAR(PySynthEvent_add_delta_start_doc,
	     "add_delta_start(name, start_field, end_field)\n"
	     "--\n\n"
	     "Add arithmetic 'start - end' field to save.\n"
	     "\n"
	     "Parameters\n"
	     "----------\n"
	     "name : string"
	     "    Name for the new arithmetic field."
	     "start_field : string\n"
	     "    The field from the start event to be used for the arithmetic calculation.\n"
	     "\n"
	     "end_field : string\n"
	     "    The field from the end event to be used for the arithmetic calculation.\n"
);

PyDoc_STRVAR(PySynthEvent_add_delta_end_doc,
	     "add_delta_end(name, start_field, end_field)\n"
	     "--\n\n"
	     "Add arithmetic 'end - start' field to save.\n"
	     "\n"
	     "Parameters\n"
	     "----------\n"
	     "name : string"
	     "    Name for the new arithmetic field."
	     "start_field : string\n"
	     "    The field from the start event to be used for the arithmetic calculation.\n"
	     "\n"
	     "end_field : string\n"
	     "    The field from the end event to be used for the arithmetic calculation.\n"
);

PyDoc_STRVAR(PySynthEvent_add_delta_T_doc,
	     "add_delta_T(name, hd)\n"
	     "--\n\n"
	     "Add time-difference (end - start) field.\n"
	     "The default time resolution is in microseconds.\n"
	     "\n"
	     "Parameters\n"
	     "----------\n"
	     "name : string"
	     "    Name for the new arithmetic field."
	     "hd : bool\n"
	     "    If True, use 'hd' time resolution (nanoseconds).\n"
);

PyDoc_STRVAR(PySynthEvent_add_sum_doc,
	     "add_sum(name, start_field, end_field)\n"
	     "--\n\n"
	     "Add arithmetic 'start + end' field to save.\n"
	     "\n"
	     "Parameters\n"
	     "----------\n"
	     "name : string"
	     "    Name for the new arithmetic field."
	     "start_field : string\n"
	     "    The field from the start event to be used for the arithmetic calculation.\n"
	     "\n"
	     "end_field : string\n"
	     "    The field from the end event to be used for the arithmetic calculation.\n"
);

PyDoc_STRVAR(PySynthEvent_register_doc,
	     "register(instance)\n"
	     "--\n\n"
	     "Register synthetic event.\n"
	     "Parameters\n"
	     "----------\n"
	     "instance : PyTfsInstance (optional)\n"
	     "    The Ftrace instance to be used. This argument is optional. If not provided, the 'top' instance is used.\n"
);

PyDoc_STRVAR(PySynthEvent_unregister_doc,
	     "unregister()\n"
	     "--\n\n"
	     "Unregister synthetic event.\n"
);

PyDoc_STRVAR(PySynthEvent_enable_doc,
	     "enable(instance)\n"
	     "--\n\n"
	     "Enable synthetic event.\n"
	     "\n"
	     "Parameters\n"
	     "----------\n"
	     "instance : PyTfsInstance (optional)\n"
	     "    The Ftrace instance to be used. This argument is optional. If not provided, the 'top' instance is used.\n"
);

PyDoc_STRVAR(PySynthEvent_disable_doc,
	     "disable(instance)\n"
	     "--\n\n"
	     "Disable synthetic event.\n"
	     "\n"
	     "Parameters\n"
	     "----------\n"
	     "instance : PyTfsInstance (optional)\n"
	     "    The Ftrace instance to be used. This argument is optional. If not provided, the 'top' instance is used.\n"
);

PyDoc_STRVAR(PySynthEvent_is_enabled_doc,
	     "is_enabled(instance)\n"
	     "--\n\n"
	     "Check if synthetic event is enabled.\n"
	     "\n"
	     "Parameters\n"
	     "----------\n"
	     "instance : PyTfsInstance (optional)\n"
	     "    The Ftrace instance to be used. This argument is optional. If not provided, the 'top' instance is used.\n"
	     "\n"
	     "Returns\n"
	     "-------\n"
	     "bool : enb\n"
	     "    True if the event is enable."
);

PyDoc_STRVAR(PySynthEvent_set_filter_doc,
	     "set_filter(filter, instance)\n"
	     "--\n\n"
	     "Define a filter for a synthetic event.\n"
	     "\n"
	     "Parameters\n"
	     "----------\n"
	     "filter : string\n"
	     "    The filter descriptor.\n"
	     "\n"
	     "instance : PyTfsInstance (optional)\n"
	     "    The Ftrace instance. This argument is optional. If not provided, the 'top' instance is used.\n"
);

PyDoc_STRVAR(PySynthEvent_get_filter_doc,
	     "get_filter(instance)\n"
	     "--\n\n"
	     "Get the filter of a synthetic event.\n"
	     "\n"
	     "Parameters\n"
	     "----------\n"
	     "instance : PyTfsInstance (optional)\n"
	     "    The Ftrace instance. This argument is optional. If not provided, the 'top' instance is used.\n"
	     "\n"
	     "Returns\n"
	     "-------\n"
	     "filter : string\n"
	     "    The filter descriptor."
);

PyDoc_STRVAR(PySynthEvent_clearfilter_doc,
	     "clear_filter(instance)\n"
	     "--\n\n"
	     "Clear the filter of a synthetic event.\n"
	     "\n"
	     "Parameters\n"
	     "----------\n"
	     "instance : PyTfsInstance (optional)\n"
	     "    The Ftrace instance. This argument is optional. If not provided, the 'top' instance is used.\n"
	     "\n"
	     "Returns\n"
	     "-------\n"
	     "filter : string\n"
	     "    The filter descriptor."
);

PyDoc_STRVAR(PySynthEvent_repr_doc,
	     "repr(event=True, hist_start=True, hist_end=True)\n"
	     "--\n\n"
	     "Show a representative descriptor of the synth. event.\n"
	     "\n"
	     "Parameters\n"
	     "----------\n"
	     "event : bool (optional)\n"
	     "    Show the descriptor of the event.\n"
	     "\n"
	     "hist_start : bool (optional)\n"
	     "    Show the descriptor of the 'start' histogram.\n"
	     "\n"
	     "hist_end : bool (optional)\n"
	     "    Show the descriptor of the 'end' histogram.\n"
	     "\n"
	     "Returns\n"
	     "-------\n"
	     "evt : string\n"
	     "    The format configuration string of the synthetic event that is passed to the kernel."
);

PyDoc_STRVAR(PyFtrace_dir_doc,
	     "dir()\n"
	     "--\n\n"
	     "Get the absolute path to the 'tracefs' directory.\n"
	     "\n"
	     "Returns\n"
	     "-------\n"
	     "dir : string\n"
	     "    Path to 'tracefs' directory.\n"
	     "\n"
);

PyDoc_STRVAR(PyFtrace_set_dir_doc,
	     "set_dir(path)\n"
	     "--\n\n"
	     "Set custom path to the 'tracefs' directory.\n"
	     "\n"
	     "Parameters\n"
	     "----------\n"
	     "path : string\n"
	     "    Full path to the ftrace directory mount point\n"
	     "\n"
);

PyDoc_STRVAR(PyFtrace_detach_doc,
	     "detach(object)\n"
	     "--\n\n"
	     "Detach object from the \'ftracepy\' module.\n"
	     "\n"
	     "Parameters\n"
	     "----------\n"
	     "object : PyTep/PyTfsInstance/PyTepRecord/PyTepEvent/PyDynevent/PyTraceHist/PySynthEvent\n"
	     "    The object to be detached.\n"
);

PyDoc_STRVAR(PyFtrace_attach_doc,
	     "attach(object)\n"
	     "--\n\n"
	     "Attach object from the \'ftracepy\' module.\n"
	     "\n"
	     "Parameters\n"
	     "----------\n"
	     "object : PyTep/PyTfsInstance/PyTepRecord/PyTepEvent/PyDynevent/PyTraceHist/PySynthEvent\n"
	     "    The object to be attached.\n"
);

PyDoc_STRVAR(PyFtrace_is_attached_doc,
	     "is_attached(object)\n"
	     "--\n\n"
	     "Check if the object is attached to the \'ftracepy\' module.\n"
	     "\n"
	     "Parameters\n"
	     "----------\n"
	     "object : PyTep/PyTfsInstance/PyTepRecord/PyTepEvent/PyDynevent/PyTraceHist/PySynthEvent\n"
	     "    The object to check.\n"
);

PyDoc_STRVAR(PyFtrace_create_instance_doc,
	     "create_instance(name, tracing_on=True)\n"
	     "--\n\n"
	     "Create new tracefs instance.\n"
	     "\n"
	     "Parameters\n"
	     "----------\n"
	     "name : string (optional)\n"
	     "    The name of the new Ftrace instance. This argument is optional. If not provided, a pseudo-random name is used.\n"
	     "\n"
	     "tracing_on : bool (optional)\n"
	     "    Specifies if the tracing is 'on' or 'off' in the Ftrace new instance.\n"
	     "\n"
	     "Returns\n"
	     "-------\n"
	     "instance : PyTfsInstance\n"
	     "    New tracefs instance descriptor object."
);

PyDoc_STRVAR(PyFtrace_find_instance_doc,
	     "find_instance(name)\n"
	     "--\n\n"
	     "Find an existing ftrace instance.\n"
	     "\n"
	     "Parameters\n"
	     "----------\n"
	     "name : string (optional)\n"
	     "    The name of the new Ftrace instance. This argument is optional. If not provided, a pseudo-random name is used.\n"
	     "\n"
	     "Returns\n"
	     "-------\n"
	     "instance : PyTfsInstance\n"
	     "    New tracefs instance object, describing an already existing tracefs instance.\n"
	     "    The returned objects is detached from the 'ftracepy' module."
);

PyDoc_STRVAR(PyFtrace_available_instances_doc,
	     "available_instances()\n"
	     "--\n\n"
	     "Get a list of all trace instances available on the system.\n"
	     "\n"
	     "Returns\n"
	     "-------\n"
	     "instances : List of instances\n"
	     "    List of all available instances."
);

PyDoc_STRVAR(PyFtrace_available_tracers_doc,
	     "available_tracers()\n"
	     "--\n\n"
	     "Get a list of all tracers available on the system.\n"
	     "\n"
	     "Parameters\n"
	     "----------\n"
	     "instance : PyTfsInstance (optional)\n"
	     "    The Ftrace instance. This argument is optional. If not provided, the 'top' instance is used.\n"
	     "\n"
	     "Returns\n"
	     "-------\n"
	     "tracers : List of strings\n"
	     "    List of all available tracers."
);

PyDoc_STRVAR(PyFtrace_set_current_tracer_doc,
	     "set_current(tracer, instance)\n"
	     "--\n\n"
	     "Enable a tracer.\n"
	     "\n"
	     "Parameters\n"
	     "----------\n"
	     "tracer : string\n"
	     "    The tracer to be enabled.\n"
	     "\n"
	     "instance : PyTfsInstance (optional)\n"
	     "    The Ftrace instance. This argument is optional. If not provided, the 'top' instance is used.\n"
);

PyDoc_STRVAR(PyFtrace_get_current_tracer_doc,
	     "get_current(instance)\n"
	     "--\n\n"
	     "Check the enabled tracer.\n"
	     "\n"
	     "Parameters\n"
	     "----------\n"
	     "instance : PyTfsInstance (optional)\n"
	     "    The Ftrace instance. This argument is optional. If not provided, the 'top' instance is used.\n"
	     "\n"
	     "Returns\n"
	     "-------\n"
	     "tracer : string\n"
	     "    The name of the currently enabled tracer. 'nop' if no tracer is enabled."
);

PyDoc_STRVAR(PyFtrace_available_event_systems_doc,
	     "available_event_systems(instance, sort=False)\n"
	     "--\n\n"
	     "Get a list of available trace event systems.\n"
	     "\n"
	     "Parameters\n"
	     "----------\n"
	     "instance : PyTfsInstance (optional)\n"
	     "    The Ftrace instance. This argument is optional. If not provided, the 'top' instance is used.\n"
	     "\n"
	     "sort : bool (optional)\n"
	     "    Show in sorted order.\n"
	     "\n"
	     "Returns\n"
	     "-------\n"
	     "systems : listo of strings\n"
	     "    Available trace event systems."
);

PyDoc_STRVAR(PyFtrace_available_system_events_doc,
	     "available_system_events(system, instance, sort=False)\n"
	     "--\n\n"
	     "Get a list of available trace event for a given system.\n"
	     "\n"
	     "Parameters\n"
	     "----------\n"
	     "system : string\n"
	     "    The system of the events to be listed.\n"
	     "instance : PyTfsInstance (optional)\n"
	     "    The Ftrace instance. This argument is optional. If not provided, the 'top' instance is used.\n"
	     "\n"
	     "sort : bool (optional)\n"
	     "    Show in sorted order.\n"
	     "\n"
	     "Returns\n"
	     "-------\n"
	     "evelts : listo of strings\n"
	     "    Available trace events."
);

PyDoc_STRVAR(PyFtrace_enable_event_doc,
	     "enable_event(instance, system, event)\n"
	     "--\n\n"
	     "Enable static trece event.\n"
	     "\n"
	     "Parameters\n"
	     "----------\n"
	     "instance : PyTfsInstance (optional)\n"
	     "    The Ftrace instance. This argument is optional. If not provided, the 'top' instance is used.\n"
	     "\n"
	     "system : string (optional)\n"
	     "    The system of the event. This argument is optional.\n"
	     "    If not provided, all systems will be searched for events having the names provided.\n"
	     "\n"
	     "event : string (optional)\n"
	     "    The event's name. This argument is optional. If not provided, all events will be enabled. The same will happen if the argument is 'all'.\n"
);

PyDoc_STRVAR(PyFtrace_disable_event_doc,
	     "disable_event(instance, system, event)\n"
	     "--\n\n"
	     "Disable static trece event.\n"
	     "\n"
	     "Parameters\n"
	     "----------\n"
	     "instance : PyTfsInstance (optional)\n"
	     "    The Ftrace instance. This argument is optional. If not provided, the 'top' instance is used.\n"
	     "\n"
	     "system : string (optional)\n"
	     "    The system of the event. This argument is optional.\n"
	     "    If not provided, all systems will be searched for events having the names provided.\n"
	     "\n"
	     "event : string (optional)\n"
	     "    The event's name. This argument is optional. If not provided, all events will be disabled. The same will happen if the argument is 'all'.\n"
);

PyDoc_STRVAR(PyFtrace_enable_events_doc,
	     "enable_events(event, instance)\n"
	     "--\n\n"
	     "Enable multiple static trece events.\n"
	     "\n"
	     "Parameters\n"
	     "----------\n"
	     "events : dictionary (string : list of strings)"
	     "    Key - system, value - list of events from this system. Use 'all' to enable all evelts from this system.\n"
	     "instance : PyTfsInstance (optional)\n"
	     "    The Ftrace instance. This argument is optional. If not provided, the 'top' instance is used.\n"
	     "\n"
	     "Example\n"
	     "-------\n"
	     "    ft.enable_events(events={'sched': ['sched_switch', 'sched_waking'],\n"
	     "                             'irq':   ['all']})"
);

PyDoc_STRVAR(PyFtrace_disable_events_doc,
	     "disable_events(event, instance)\n"
	     "--\n\n"
	     "Disable multiple static trece events.\n"
	     "\n"
	     "Parameters\n"
	     "----------\n"
	     "events : dictionary (string : list of strings)"
	     "    Key - system, value - list of events from this system. Use 'all' to disable all evelts from this system.\n"
	     "instance : PyTfsInstance (optional)\n"
	     "    The Ftrace instance. This argument is optional. If not provided, the 'top' instance is used.\n"
	     "\n"
	     "Example\n"
	     "-------\n"
	     "    ft.enable_events(events={'sched': ['sched_switch', 'sched_waking'],\n"
	     "                             'irq':   ['all']})"
);

PyDoc_STRVAR(PyFtrace_event_is_enabled_doc,
	     "event_is_enabled(instance, system, event)\n"
	     "--\n\n"
	     "Check if a static trece event is enabled.\n"
	     "\n"
	     "Parameters\n"
	     "----------\n"
	     "instance : PyTfsInstance (optional)\n"
	     "    The Ftrace instance. This argument is optional. If not provided, the 'top' instance is used.\n"
	     "\n"
	     "system : string (optional)\n"
	     "    The system of the event. Can be 'all'.\n"
	     "\n"
	     "event : string (optional)\n"
	     "    The event's name. Can be 'all'.\n"
);

PyDoc_STRVAR(PyFtrace_set_event_filter_doc,
	     "set_event_filter(system, event, filter, instance)\n"
	     "--\n\n"
	     "Define filter for static event.\n"
	     "\n"
	     "Parameters\n"
	     "----------\n"
	     "system : string (optional)\n"
	     "    The system of the event. Can be 'all'.\n"
	     "\n"
	     "event : string (optional)\n"
	     "    The event's name. Can be 'all'.\n"
	     "\n"
	     "filter : string\n"
	     "    The filter descriptor.\n"
	     "\n"
	     "instance : PyTfsInstance (optional)\n"
	     "    The Ftrace instance. This argument is optional. If not provided, the 'top' instance is used.\n"
);

PyDoc_STRVAR(PyFtrace_clear_event_filter_doc,
	     "clear_event_filter(system, event, filter, instance)\n"
	     "--\n\n"
	     "Clear the filter for static event.\n"
	     "\n"
	     "Parameters\n"
	     "----------\n"
	     "system : string (optional)\n"
	     "    The system of the event. Can be 'all'.\n"
	     "\n"
	     "event : string (optional)\n"
	     "    The event's name. Can be 'all'.\n"
	     "\n"
	     "instance : PyTfsInstance (optional)\n"
	     "    The Ftrace instance. This argument is optional. If not provided, the 'top' instance is used.\n"
);

PyDoc_STRVAR(PyFtrace_tracing_ON_doc,
	     "tracing_ON(instance)\n"
	     "--\n\n"
	     "Start tracing.\n"
	     "\n"
	     "Parameters\n"
	     "----------\n"
	     "instance : PyTfsInstance (optional)\n"
	     "    The Ftrace instance. This argument is optional. If not provided, the 'top' instance is used.\n"
);

PyDoc_STRVAR(PyFtrace_tracing_OFF_doc,
	     "tracing_OFF(instance)\n"
	     "--\n\n"
	     "Stop tracing.\n"
	     "\n"
	     "Parameters\n"
	     "----------\n"
	     "instance : PyTfsInstance (optional)\n"
	     "    The Ftrace instance. This argument is optional. If not provided, the 'top' instance is used.\n"
);

PyDoc_STRVAR(PyFtrace_is_tracing_ON_doc,
	     "is_tracing_ON(instance)\n"
	     "--\n\n"
	     "Check if tracing is ON."
	     "\n"
	     "Parameters\n"
	     "----------\n"
	     "instance : PyTfsInstance (optional)\n"
	     "    The Ftrace instance. This argument is optional. If not provided, the 'top' instance is used.\n"
);

PyDoc_STRVAR(PyFtrace_set_event_pid_doc,
	     "set_event_pid(pid, instance)\n"
	     "--\n\n"
	     "Have Ftrace events only trace the tasks with PID values listed.\n"
	     "\n"
	     "Parameters\n"
	     "----------\n"
	     "pid : int or list of ints.\n"
	     "    PID values of the tasks to be traced.\n"
	     "\n"
	     "instance : PyTfsInstance (optional)\n"
	     "    The Ftrace instance. This argument is optional. If not provided, the 'top' instance is used.\n"
);

PyDoc_STRVAR(PyFtrace_set_ftrace_pid_doc,
	     "set_ftrace_pid(pid, instance)\n"
	     "--\n\n"
	     "Have Ftrace function tracer only trace the tasks with PID values listed.\n"
	     "\n"
	     "Parameters\n"
	     "----------\n"
	     "pid : int or list of ints.\n"
	     "    PID values of the tasks to be traced.\n"
	     "\n"
	     "instance : PyTfsInstance (optional)\n"
	     "    The Ftrace instance. This argument is optional. If not provided, the 'top' instance is used.\n"
);

PyDoc_STRVAR(PyFtrace_enable_option_doc,
	     "enable_option(option, instance)\n"
	     "--\n\n"
	     "Enable trecing option.\n"
	     "\n"
	     "Parameters\n"
	     "----------\n"
	     "option : string\n"
	     "    The name of a Ftrace option.\n"
	     "\n"
	     "instance : PyTfsInstance (optional)\n"
	     "    The Ftrace instance. This argument is optional. If not provided, the 'top' instance is used.\n"
);

PyDoc_STRVAR(PyFtrace_disable_option_doc,
	     "disable_option(option, instance)\n"
	     "--\n\n"
	     "Disable trecing option.\n"
	     "\n"
	     "Parameters\n"
	     "----------\n"
	     "option : string\n"
	     "    The name of a Ftrace option.\n"
	     "\n"
	     "instance : PyTfsInstance (optional)\n"
	     "    The Ftrace instance. This argument is optional. If not provided, the 'top' instance is used.\n"
);

PyDoc_STRVAR(PyFtrace_option_is_set_doc,
	     "option_is_set(option, instance)\n"
	     "--\n\n"
	     "Check if trece option is enabled.\n"
	     "\n"
	     "Parameters\n"
	     "----------\n"
	     "option : string\n"
	     "    The name of a Ftrace option.\n"
	     "\n"
	     "instance : PyTfsInstance (optional)\n"
	     "\n"
	     "Returns\n"
	     "-------\n"
	     "is_set : bool\n"
	     "    True if the option is enabled.\n"
);

PyDoc_STRVAR(PyFtrace_supported_options_doc,
	     "supported_options(instance)\n"
	     "--\n\n"
	     "Gat a list of all supported tracing options.\n"
	     "\n"
	     "Parameters\n"
	     "----------\n"
	     "instance : PyTfsInstance (optional)\n"
	     "    The Ftrace instance. This argument is optional. If not provided, the 'top' instance is used.\n"
	     "\n"
	     "Returns\n"
	     "-------\n"
	     "options : list of strings\n"
	     "    The names of all supported options."
);

PyDoc_STRVAR(PyFtrace_enabled_options_doc,
	     "enabled_options(instance)\n"
	     "--\n\n"
	     "Gat a list of all currently enabled tracing options.\n"
	     "\n"
	     "Parameters\n"
	     "----------\n"
	     "instance : PyTfsInstance (optional)\n"
	     "    The Ftrace instance. This argument is optional. If not provided, the 'top' instance is used.\n"
	     "\n"
	     "Returns\n"
	     "-------\n"
	     "options : list of strings\n"
	     "    The names of all enabled options."
);

PyDoc_STRVAR(PyFtrace_kprobe_doc,
	     "kprobe(event, function, probe)\n"
	     "--\n\n"
	     "Define a kprobe.\n"
	     "\n"
	     "Parameters\n"
	     "----------\n"
	     "event : string\n"
	     "    The name of the kprobe.\n"
	     "\n"
	     "function : string\n"
	     "    Name of a function or address.\n"
	     "\n"
	     "probe : string\n"
	     "    Definition of the probe.\n"
	     "\n"
	     "Returns\n"
	     "-------\n"
	     "object : PyDynevent\n"
	     "   New kprobe descriptor object."
);

PyDoc_STRVAR(PyFtrace_kretprobe_doc,
	     "kretprobe(event, function, probe='$retval')\n"
	     "--\n\n"
	     "Define a kretprobe.\n"
	     "\n"
	     "Parameters\n"
	     "----------\n"
	     "event : string\n"
	     "    The name of the kprobe.\n"
	     "\n"
	     "function : string\n"
	     "    Name of a function or address.\n"
	     "\n"
	     "probe : string (option)\n"
	     "    Definition of the probe.\n"
	     "\n"
	     "Returns\n"
	     "-------\n"
	     "object : PyDynevent\n"
	     "   New kretprobe descriptor object."
);

PyDoc_STRVAR(PyFtrace_eprobe_doc,
	     "eprobe(event, target_system, target_event, fetch_fields)\n"
	     "--\n\n"
	     "Define an eprobe.\n"
	     "\n"
	     "Parameters\n"
	     "----------\n"
	     "event : string\n"
	     "    The name of the kprobe.\n"
	     "\n"
	     "target_system : string\n"
	     "    The system of the event the probe to be attached to.\n"
	     "\n"
	     "target_event : string\n"
	     "    The name of the event the probe to be attached to.\n"
	     "\n"
	     "fetch_fields : string\n"
	     "    Definition of the probe.\n"
	     "\n"
	     "Returns\n"
	     "-------\n"
	     "object : PyDynevent\n"
	     "   New eprobe descriptor object."
);

PyDoc_STRVAR(PyFtrace_hist_doc,
	     "hist(system, event, key, type, axes, name)\n"
	     "--\n\n"
	     "Define a kernel histogram.\n"
	     "\n"
	     "Parameters\n"
	     "----------\n"
	     "system : string\n"
	     "    The system of the event the histogram to be attached to.\n"
	     "\n"
	     "event : string\n"
	     "    The name of the event the histogram to be attached to.\n"
	     "\n"
	     "key : string (optional)\n"
	     "    Name of a field from the histogram's event, to be used as a key.\n"
	     "\n"
	     "type : string or int (optional)\n"
	     "    Definition of the type of the probe. The following types are supported:\n"
	     "        'normal', 'n' or 0 for displaying a number;\n"
	     "        'hex', 'h' or 1 for displaying a number as a hex value;\n"
	     "        'sym' or 2 for displaying an address as a symbol;\n"
	     "        'sym_offset', 'so' or 3 for displaying an address as a symbol and offset;\n"
	     "        'syscall', 'sc' or 4 for displaying a syscall id as a system call name;\n"
	     "        'execname', 'e' or 5 for displaying a common_pid as a program name;\n"
	     "        'log', 'l' or 6 for displaying log2 value rather than raw number;\n"
	     "\n"
	     "axes : dictionary (string : string or int) (optional)\n"
	     "    Definition of the N dimentional histogram.\n"
	     "name : string (optional)\n"
	     "    The name of the histogram.\n"
	     "\n"
	     "Returns\n"
	     "-------\n"
	     "object : PyTraceHist\n"
	     "   New histogram descriptor object.\n"
	     "\n"
	     "Examples\n"
	     "-------\n"
	     "hist = ft.hist(name='h1',\n"
             "               system='kmem',\n"
             "               event='kmalloc',\n"
             "               key='bytes_req')\n"
	     "\n"
	     "hist = ft.hist(name='h1',\n"
             "               system='kmem',\n"
             "               event='kmalloc',\n"
             "               axes={'call_site': 'sym',\n"
             "                     'bytes_req': 'n'})"
);

PyDoc_STRVAR(PyFtrace_synth_doc,
	     "synth(name, start_sys, start_evt, end_sys, end_evt, start_match, end_match, match_name)\n"
	     "--\n\n"
	     "Define a synthetic event.\n"
	     "\n"
	     "Parameters\n"
	     "----------\n"
	     "name : string\n"
	     "    The name of the synthetic event.\n"
	     "\n"
	     "start_sys : string\n"
	     "    The system of the start event.\n"
	     "\n"
	     "start_evt : string\n"
	     "    The name of the start event.\n"
	     "\n"
	     "end_sys : string\n"
	     "    The system of the end event.\n"
	     "\n"
	     "end_evt : string\n"
	     "    The name of the end event.\n"
	     "\n"
	     "start_match : string\n"
	     "    The name of the field from the start event that needs to match.\n"
	     "\n"
	     "end_match : string\n"
	     "    The name of the field from the end event that needs to match.\n"
	     "\n"
	     "match_name : string (optional)"
	     "    If used, the match value will be recorded as a field, using the name provided."
	     "\n"
	     "Returns\n"
	     "-------\n"
	     "object : PySynthEvent\n"
	     "   New synthetic event descriptor object.\n"
	     "\n"
	     "Example\n"
	     "-------\n"
	     "synth = ft.synth(name='synth_wakeup',\n"
	     "                 start_sys='sched', start_evt='sched_waking',\n"
	     "                 end_sys='sched',   end_evt='sched_switch',\n"
	     "                 start_match='pid', end_match='next_pid',\n"
	     "                 match_name='pid')"
);

PyDoc_STRVAR(PyFtrace_available_dynamic_events_doc,
	     "available_dynamic_events(types)\n"
	     "--\n\n"
	     "Get list of dynamic events in the system."
	     "By default, the dynamic events of all types are returned."
	     "When the list is destroyed, the events are not deleted from the system.\n"
	     "\n"
	     "Parameters\n"
	     "----------\n"
	     "types : string\n"
	     "    String with requested types of dynamic events, optional."
	     "The following types are supported:\n"
	     "     kprobe\n"
	     "     kretprobe\n"
	     "     uprobe\n"
	     "     uretprobe\n"
	     "     eprobe\n"
	     "     synthetic\n"
);

PyDoc_STRVAR(PyFtrace_set_ftrace_loglevel_doc,
	     "set_ftrace_loglevel(level)\n"
	     "--\n\n"
	     "Set the verbose level of the ftrace libraries.\n"
	     "\n"
	     "Parameters\n"
	     "----------\n"
	     "level : int\n"
	     "    The following log levels are supported:\n"
	     "     <= 0 - none;\n"
	     "        1 - critical;\n"
	     "        2 - error;\n"
	     "        3 - nwarning;\n"
	     "        4 - info;\n"
	     "        5 - debug;\n"
	     "     >= 6 - all;\n"
);

PyDoc_STRVAR(PyFtrace_trace_process_doc,
	     "trace_process(argv, plugin='__main__', callback='callback', instance)\n"
	     "--\n\n"
	     "Trace a process.\n"
	     "\n"
	     "Parameters\n"
	     "----------\n"
	     "argv : list of strings \n"
	     "    The argument list of the program to execute. The first argument should  the file being executed.\n"
	     "\n"
	     "plugin : string (optional)\n"
	     "    Location to search for definition of a callback function.\n"
	     "\n"
	     "callback : string (optional)\n"
	     "    A callback function to be processed on every trace event.\n"
	     "\n"
	     "instance : PyTfsInstance (optional)\n"
	     "    The Ftrace instance. This argument is optional. If not provided, the 'top' instance is used.\n"
);

PyDoc_STRVAR(PyFtrace_trace_shell_process_doc,
	     "trace_shell_process(argv, plugin='__main__', callback='callback', instance)\n"
	     "--\n\n"
	     "Trace a process executed within a shell.\n"
	     "\n"
	     "Parameters\n"
	     "----------\n"
	     "argv : list of strings \n"
	     "    The argument list of the program to execute. The first argument should  the file being executed.\n"
	     "\n"
	     "plugin : string (optional)\n"
	     "    Location to search for definition of a callback function.\n"
	     "\n"
	     "callback : string (optional)\n"
	     "    A callback function to be processed on every trace event.\n"
	     "\n"
	     "instance : PyTfsInstance (optional)\n"
	     "    The Ftrace instance. This argument is optional. If not provided, the 'top' instance is used.\n"
	     );

PyDoc_STRVAR(PyFtrace_read_trace_doc,
	     "read_trace(instance)\n"
	     "--\n\n"
	     "Redirect the trace data stream to stdout. Use 'Ctrl+c' to stop.\n"
	     "\n"
	     "Parameters\n"
	     "----------\n"
	     "instance : PyTfsInstance (optional)\n"
	     "    The Ftrace instance. This argument is optional. If not provided, the 'top' instance is used.\n"
	     );

PyDoc_STRVAR(PyFtrace_wait_doc,
	     "wait(signals, pids, kill, time)\n"
	     "--\n\n"
	     "Blocking wait until a condition is met: given signal is received, or all PIDs from the list are terminated, or given time is passed\n"
	     "\n"
	     "Parameters\n"
	     "----------\n"
	     "signals : List of signals to wait for (optional)\n"
	     "    A list of POSIX signal names. If any of those signals is received, the function returns."
	     "    By default, SIGINT and SIGTERM are tracked. Pass an empty list to disable this condition\n"
	     "pids : List of pids to track (optonal)\n"
	     "    A list of PIDs to wait for. If all PIDs from this list are terminated, the function returns."
	     "    By default no PIDs are tracked."
	     "kill : Boolean, whether to kill tasks from pids list (optonal)\n"
	     "    If true is passed and if a pids list is provided, before the funcion exit all"
	     "    not-terminated tasks from pids list are killed, using the SIGTERM signal."
	     "    By default this behaviour is disabled."
	     "time : Time, in msec to wait (optonal)\n"
	     "    If time is provided, the function waits for given number of miliseconds before return."
	     );

PyDoc_STRVAR(PyFtrace_iterate_trace_doc,
	     "iterate_trace(plugin='__main__', callback='callback', instance)\n"
	     "--\n\n"
	     "User provided processing (via callback) of every trace event. Use 'Ctrl+c' to stop.\n"
	     "\n"
	     "Parameters\n"
	     "----------\n"
	     "plugin : string (optional)\n"
	     "    Location to search for definition of a callback function.\n"
	     "\n"
	     "callback : string (optional)\n"
	     "    A callback function to be processed on every trace event.\n"
	     "\n"
	     "instance : PyTfsInstance (optional)\n"
	     "    The Ftrace instance. This argument is optional. If not provided, the 'top' instance is used.\n"
	     );

PyDoc_STRVAR(PyFtrace_hook2pid_doc,
	     "hook2pid(pid, fork, instance)\n"
	     "--\n\n"
	     "Trace only particular process (or processes).\n"
	     "\n"
	     "Parameters\n"
	     "----------\n"
	     "pid : int or list of ints\n"
	     "    The process (or processes) to trace.\n"
	     "\n"
	     "fork : bool (optional)\n"
	     "    If trrace all child processes.\n"
	     "\n"
	     "instance : PyTfsInstance (optional)\n"
	     "    The Ftrace instance. This argument is optional. If not provided, the 'top' instance is used.\n"
);

PyDoc_STRVAR(PyFtrace_error_log_doc,
	     "error_log(instance)\n"
	     "--\n\n"
	     "Get the content of the error log.\n"
	     "\n"
	     "Parameters\n"
	     "----------\n"
	     "instance : PyTfsInstance (optional)\n"
	     "    The Ftrace instance. This argument is optional. If not provided, the 'top' instance is used.\n"
	     );

PyDoc_STRVAR(PyFtrace_clear_error_log_doc,
	     "clear_error_log(instance)\n"
	     "--\n\n"
	     "Clear the content of the error log.\n"
	     "\n"
	     "Parameters\n"
	     "----------\n"
	     "instance : PyTfsInstance (optional)\n"
	     "    The Ftrace instance. This argument is optional. If not provided, the 'top' instance is used.\n"
	     );

#endif // _TC_FTRACE_PY_DOCS
