/* SPDX-License-Identifier: LGPL-2.1 */

/*
 * Copyright 2022 VMware Inc, Tzvetomir Stoyanov (VMware) <tz.stoyanov@gmail.com>
 */

#ifndef _TC_TRACE_DEBUG_UTILS_
#define _TC_TRACE_DEBUG_UTILS_

/* --- Debug symbols--- */
struct dbg_trace_pid_maps {
	struct dbg_trace_pid_maps	*next;
	struct dbg_trace_proc_addr_map	*lib_maps;
	unsigned int			nr_lib_maps;
	char				*proc_name;
	int				pid;
};
int dbg_trace_get_filemap(struct dbg_trace_pid_maps **file_maps, int pid);
void dbg_trace_free_filemap(struct dbg_trace_pid_maps *maps);

struct dbg_trace_symbols {
	char *name;			/* symbol's name */
	char *fname;			/* symbol's file */
	int cookie;
	unsigned long long vma_start;	/* symbol's start VMA */
	unsigned long long vma_near;	/* symbol's requested VMA */
	unsigned long long foffset;	/* symbol's offset in the binary file*/
};

struct dbg_trace_proc_addr_map {
	unsigned long long	start;
	unsigned long long	end;
	char			*lib_name;
};

struct dbg_trace_context;
struct dbg_trace_context *dbg_trace_context_create_file(char *file, bool libs);
struct dbg_trace_context *dbg_trace_context_create_pid(int pid, bool libs);

void dbg_trace_context_destroy(struct dbg_trace_context *debug);
int dbg_trace_context_add_file(struct dbg_trace_context *dbg, char *file_name,
			       unsigned long long vmem_start,
			       unsigned long long vmem_end,
			       unsigned long long pgoff);

int dbg_trace_resolve_symbols(struct dbg_trace_context *obj);
int dbg_trace_add_resolve_symbol(struct dbg_trace_context *obj,
				 unsigned long long vma, char *name, int cookie);

void dbg_trace_walk_resolved_symbols(struct dbg_trace_context *obj,
				     int (*callback)(struct dbg_trace_symbols *, void *),
				     void *context);

#endif /* _TC_TRACE_DEBUG_UTILS_ */
