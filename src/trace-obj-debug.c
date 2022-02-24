// SPDX-License-Identifier: LGPL-2.1
/*
 * Copyright (C) 2020, VMware, Tzvetomir Stoyanov <tz.stoyanov@gmail.com>
 *
 */

#ifndef _GNU_SOURCE
/** Use GNU C Library. */
#define _GNU_SOURCE
#endif // _GNU_SOURCE

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#include <bfd.h>
#include <unistd.h>
#include <dlfcn.h>
#include <fnmatch.h>
#include <ctype.h>

#include "trace-obj-debug.h"

//#define DEBUG_INTERNALS

/* Got from demangle.h */
#define DMGL_AUTO (1 << 8)

struct debug_bfd_handle {
	bfd *bfd;
	unsigned long long addr_offset;
};

enum match_type {
	MATCH_EXACT	= 0,
	MATH_WILDCARD	= 1,
};

struct debug_symbols {
	struct debug_symbols		*next;
	struct dbg_trace_symbols	symbol;
	enum match_type			match;
};

struct debug_file {
	struct debug_file		*next;
	char				*file_name;

	/* Start and end address, where this file is mapped into the memory of the process. */
	unsigned long long		vmem_start;
	unsigned long long		vmem_end;

	/* bfd library context for this file. */
	struct debug_bfd_handle	*dbg;

	/* Symbols to resolve, search in this file only. */
	int				sym_count;
	struct debug_symbols		*sym;
};

struct dbg_trace_context {
	/* PID or name of the process. */
	int				pid;
	char				*fname;

	/* List of all libraries and object files, mapped in the memory of the process. */
	struct dbg_trace_pid_maps	*fmaps;

	/* Symbols to resolve, search in all files. */
	int				sym_count;
	struct debug_symbols		*sym;

	/* List of all libraries and object files, opened from bfd library for processing. */
	struct debug_file		*files;
};

#define RESOLVE_NAME		(1 << 0)
#define RESOLVE_VMA		(1 << 1)
#define RESOLVE_FOFFSET		(1 << 2)

struct debug_obj_job {
	unsigned int flags;
	unsigned long long addr_offset;
	struct debug_symbols *symbols;
};

struct debug_dwarf_bfd_context {
	asymbol **table;
	struct debug_obj_job *job;
};

/*
 * A hook, called from bfd library for each section of the file.
 * The logic is used for symbol name and file offset resolving from given symbol VMA.
 */
static void process_bfd_section(bfd *abfd, asection *section, void *param)
{
	struct debug_dwarf_bfd_context *context = (struct debug_dwarf_bfd_context *)param;
	unsigned int discriminator;
	const char *functionname;
	struct debug_symbols *s;
	unsigned long long vma;
	const char *filename;
	unsigned int line;
	bfd_boolean found;

	/* Skip sections that have no code. */
	if (!(section->flags & SEC_CODE))
		return;

	/* Loop through all symbols, that have to be resolved. */
	for (s = context->job->symbols; s; s = s->next) {
		if (s->symbol.vma_near)
			vma = s->symbol.vma_near;
		else if (s->symbol.vma_start)
			vma = s->symbol.vma_start;
		else
			continue;

		/* Adjust the symbol's VMA, if this section is dynamically loaded. */
		if (abfd->flags & DYNAMIC)
			vma -= context->job->addr_offset;

		/* Check if requested symbol's vma is within the current section. */
		if (vma && section->vma <= vma &&
		    (section->vma + section->size) > vma) {

			/* Found the file, where the symbol is defined. */
			if (!s->symbol.fname)
				s->symbol.fname = strdup(abfd->filename);

			/* Found the offset of the symbol within the file. */
			if (context->job->flags & RESOLVE_FOFFSET)
				s->symbol.foffset = section->filepos + (vma - section->vma);

			/* Look for the nearest function. */
			if (!s->symbol.name && (context->job->flags & RESOLVE_NAME)) {
				found = bfd_find_nearest_line_discriminator(abfd, section, context->table,
									    vma - section->vma, &filename,
									    &functionname, &line, &discriminator);
#ifdef DEBUG_INTERNALS
				printf("Find addr near 0x%X, offset 0x%X - > vma - 0x%X in %s, found %s\n\r",
					s->symbol.vma_near, context->job->addr_offset, vma, abfd->filename,
					found ? functionname : "NA");
#endif
				if (found) {
					/* Demangle the name of the function. */
					s->symbol.name = bfd_demangle(abfd, functionname, DMGL_AUTO);
					/* Found the name of the symbol. */
					if (!s->symbol.name)
						s->symbol.name = strdup(functionname);
				}
			}
		}
	}
}

/* Load the symbol table from the file. */
static asymbol **get_sym_table(bfd *handle)
{
	long size, ssize, dsize;
	asymbol **symtable;
	long count;

	if ((bfd_get_file_flags(handle) & HAS_SYMS) == 0)
		return NULL;

	dsize = bfd_get_dynamic_symtab_upper_bound(handle);
	size = dsize > 0 ? dsize : 0;

	ssize = bfd_get_symtab_upper_bound(handle);
	size += ssize > 0 ? ssize : 0;

	if (size <= 0)
		return NULL;

	symtable = (asymbol **) calloc(1, size);
	if (!symtable)
		return NULL;

	count = bfd_canonicalize_symtab(handle, symtable);
	count += bfd_canonicalize_dynamic_symtab(handle, symtable + count);
	if (count <= 0) {
		free(symtable);
		return NULL;
	}

	return symtable;
}

/* Match the requested name to the name of the symbol. Handle a wildcard match. */
static bool symbol_match(char *pattern, enum match_type match, const char *symbol)
{
	bool ret = false;

	switch (match) {
	case MATCH_EXACT:
		if (strlen(pattern) == strlen(symbol) &&
		    !strcmp(pattern, symbol))
			ret = true;
		break;
	case MATH_WILDCARD:
		if (!fnmatch(pattern, symbol, 0))
			ret = true;
		break;
	}

	return ret;
}

/* Lookup in the file's symbol table.
 * The logic is used for symbol VMA resolving from given symbol name.
 */
static int lookup_bfd_sym(struct debug_dwarf_bfd_context *context)
{
	struct debug_symbols *s, *last = NULL;
	struct debug_symbols *new, *new_list = NULL;
	unsigned long long vma;
	asymbol **sp;
	int res = 0;

	for (sp = context->table; *sp != NULL; sp++) {
		/* Skip the symbol, if it is not a function. */
		if (!((*sp)->flags & BSF_FUNCTION))
			continue;
		/* Loop through all symbols that should be resolved. */
		for (s = context->job->symbols; s; s = s->next) {
			if (!s->symbol.name)
				continue;
			last = s;
			if (!symbol_match(s->symbol.name, s->match, (*sp)->name))
				continue;
#ifdef DEBUG_INTERNALS
			printf("Matched %s, pattern %s\n\r", (*sp)->name, s->symbol.name);
#endif
			vma = (*sp)->value + (*sp)->section->vma;
			/* Adjust the VMA, if the section is dynamically loaded. */
			if ((*sp)->the_bfd->flags & DYNAMIC)
				vma += context->job->addr_offset;
			if (s->match == MATCH_EXACT) {
				/* Exact match, update the VMA. */
				s->symbol.vma_start = vma;
			} else if (s->match == MATH_WILDCARD) {
				/* Wildcard pattern match, create a new symbol. */
				new = calloc(1, sizeof(struct debug_symbols));
				if (!new)
					break;
				new->symbol.name = strdup((*sp)->name);
				new->symbol.vma_start = vma;
				new->symbol.vma_near = s->symbol.vma_near;
				new->symbol.foffset = s->symbol.foffset;
				new->symbol.cookie = s->symbol.cookie;
				if (s->symbol.fname)
					new->symbol.fname = strdup(s->symbol.fname);
				new->next = new_list;
				new_list = new;
			}
			res++;
		}
	}
	if (last && !last->next)
		last->next = new_list;

	return res;
}

/* Process a bfd object from the file. */
static int process_bfd_object(bfd *abfd, struct debug_obj_job *job)
{
	struct debug_dwarf_bfd_context context;
	int ret = 0;

	memset(&context, 0, sizeof(context));
	context.job = job;
	if (bfd_check_format_matches(abfd, bfd_object, NULL) ||
	    bfd_check_format_matches(abfd, bfd_core, NULL)) {
		context.table = get_sym_table(abfd);
		if (!context.table)
			return -1;

		/* Resolve VMA from the symbol table. */
		if (job->flags & RESOLVE_VMA)
			lookup_bfd_sym(&context);

		/* Resolve symbol name and file offset from file's sections. */
		if ((job->flags & RESOLVE_NAME) || (job->flags & RESOLVE_FOFFSET))
			bfd_map_over_sections(abfd, process_bfd_section, &context);

		free(context.table);
	} else {
		ret = -1;
	}

	return ret;
}

/* Open a bfd archive file and read all objects. */
static int read_all_bfd(bfd *abfd, struct debug_obj_job *job)
{
	bfd *last_arfile = NULL;
	bfd *arfile = NULL;
	int ret = 0;

	if (bfd_check_format(abfd, bfd_archive)) {
		for (;;) {
			bfd_set_error(bfd_error_no_error);
			arfile = bfd_openr_next_archived_file(abfd, arfile);
			if (!arfile) {
				if (bfd_get_error() != bfd_error_no_more_archived_files)
					break;
			}
			ret = read_all_bfd(arfile, job);
			if (last_arfile)
				bfd_close(last_arfile);
			last_arfile = arfile;
		}
		if (last_arfile)
			bfd_close(last_arfile);
	} else
		ret = process_bfd_object(abfd, job);

	return ret;
}

/**
 * resolve_symbol_vma - name -> (vma, file offset) resolving
 * @obj - pointer to object, returned by trace_obj_debug_create()
 * @symbols - link list with desired symbols, with given name
 *
 * Get VMA and file offset of the symbols with given name.
 * Return 0 on success, -1 on error.
 */
static int resolve_symbol_vma(struct debug_bfd_handle *obj,
			      struct debug_symbols *symbols)
{
	struct debug_obj_job job;
	int ret;

	memset(&job, 0, sizeof(job));
	job.flags |= RESOLVE_VMA;
	job.flags |= RESOLVE_FOFFSET;
	job.symbols = symbols;
	job.addr_offset = obj->addr_offset;
	ret = read_all_bfd(obj->bfd, &job);

	return ret;
}

/**
 * resolve_symbol_name - vma -> name resolving
 * @obj - pointer to object, returned by trace_obj_debug_create()
 * @symbols - link list with desired symbols, with given VMA
 *
 * Get names of the symbols with given VMA, look for nearest symbol to that VMA.
 * Return 0 on success, -1 on error.
 */
static int resolve_symbol_name(struct debug_bfd_handle *obj,
			       struct debug_symbols *symbols)
{
	struct debug_obj_job job;

	if (!obj || !obj->bfd)
		return -1;
	memset(&job, 0, sizeof(job));
	job.flags |= RESOLVE_NAME;
	job.addr_offset = obj->addr_offset;
	job.symbols = symbols;
	return read_all_bfd(obj->bfd, &job);
}

/**
 * debug_handle_destroy - Close file opened with trace_obj_debug_create()
 * @obj - pointer to object, returned by trace_obj_debug_create()
 *
 * Close the file and free any allocated resources, related to file's debug
 * information.
 */
static void debug_handle_destroy(struct debug_bfd_handle *obj)
{
	if (obj && obj->bfd)
		bfd_close(obj->bfd);
	free(obj);
}

/**
 * debug_handle_create - Open binary file for parsing ELF and DWARF information
 * @name: Name of the binary ELF file.
 *
 * Return pointer to trace_obj_debug structure, that can be passed to other APIs
 * for extracting debug information from the file. NULL in case of an error.
 */
static struct debug_bfd_handle *debug_handle_create(char *file)
{
	struct debug_bfd_handle *obj = NULL;

	obj = calloc(1, sizeof(*obj));
	if (!obj)
		return NULL;

	bfd_init();
	obj->bfd = bfd_openr(file, NULL);
	if (!obj->bfd)
		goto error;
	obj->bfd->flags |= BFD_DECOMPRESS;

	return obj;

error:
	debug_handle_destroy(obj);
	return NULL;
}

/* Get the full path of process's executable, using the /proc fs. */
static char *get_full_name(int pid)
{
	char mapname[PATH_MAX+1];
	char fname[PATH_MAX+1];
	int ret;

	sprintf(fname, "/proc/%d/exe", pid);
	ret = readlink(fname, mapname, PATH_MAX);
	if (ret >= PATH_MAX || ret < 0)
		return NULL;
	mapname[ret] = 0;

	return strdup(mapname);
}

/* Get or create a bfd debug context for an object file. */
static struct debug_file *get_mapped_file(struct dbg_trace_context *dbg,
					  char *fname,
					  unsigned long long vmem_start)
{
	struct debug_file *file = dbg->files;

	/* Search if the file is already added. */
	while (file) {
		if (!strcmp(fname, file->file_name) &&
		    vmem_start && file->vmem_end == vmem_start)
			return file;
		file = file->next;
	}

	file = calloc(1, sizeof(*file));
	if (!file)
		return NULL;
	file->file_name = strdup(fname);
	if (!file->file_name)
		goto error;
	file->dbg = debug_handle_create(fname);
	file->next = dbg->files;
	dbg->files = file;
	return file;

error:
	free(file->file_name);
	debug_handle_destroy(file->dbg);
	free(file);
	return NULL;
}

/* Destroy a bfd debug context. */
void dbg_trace_context_destroy(struct dbg_trace_context *dbg)
{
	struct debug_file *fdel;
	struct debug_symbols *sdel;

	while (dbg->sym) {
		sdel = dbg->sym;
		dbg->sym = dbg->sym->next;
		free(sdel->symbol.name);
		free(sdel->symbol.fname);
		free(sdel);
	}
	while (dbg->files) {
		fdel = dbg->files;
		dbg->files = dbg->files->next;
		debug_handle_destroy(fdel->dbg);
		while (fdel->sym) {
			sdel = fdel->sym;
			fdel->sym = fdel->sym->next;
			free(sdel->symbol.name);
			free(sdel->symbol.fname);
			free(sdel);
		}
		free(fdel);
	}

	free(dbg->fname);
	dbg_trace_free_filemap(dbg->fmaps);
	free(dbg);
}

/* Add an object file, mapped to specific memory of the process. */
int dbg_trace_context_add_file(struct dbg_trace_context *dbg, char *file_name,
			       unsigned long long vmem_start,
			       unsigned long long vmem_end,
			       unsigned long long pgoff)
{
	struct debug_file *file;

	file = get_mapped_file(dbg, file_name, vmem_start);
	if (!file)
		return -1;
	if (file->vmem_end == vmem_start) {
		file->vmem_end = vmem_end;
	} else {
		file->vmem_start = vmem_start;
		file->vmem_end = vmem_end;
		if (file->dbg)
			file->dbg->addr_offset = vmem_start - pgoff;
	}

	return 0;
}

/**
 * dbg_trace_context_create_pid - create debug context for given PID
 * @pid - ID of running process
 * @libs - if true: inspect also all libraries, uased by the given process.
 *
 * Returns a pointer to allocated debug context, or NULL in case of an error.
 */
struct dbg_trace_context *dbg_trace_context_create_pid(int pid, bool libs)
{
	struct dbg_trace_context *dbg;
	unsigned int i;

	dbg = calloc(1, sizeof(*dbg));
	if (!dbg)
		return NULL;

	dbg->pid = pid;
	/* Get the full path of the process executable. */
	dbg->fname = get_full_name(pid);
	if (!dbg->fname) {
		free(dbg);
		return NULL;
	}

	/* Get the memory map of all libraries, linked to the process. */
	dbg_trace_get_filemap(&dbg->fmaps, pid);

	for (i = 0; i < dbg->fmaps->nr_lib_maps; i++) {
		if (!libs && strcmp(dbg->fname, dbg->fmaps->lib_maps[i].lib_name))
			continue;
		/* Create a bfd debug object for each file. */
		dbg_trace_context_add_file(dbg, dbg->fmaps->lib_maps[i].lib_name,
					   dbg->fmaps->lib_maps[i].start,
					   dbg->fmaps->lib_maps[i].end, 0);
	}

	return dbg;
}

/* Get the full path of a library. */
static char *get_lib_full_path(char *libname)
{
	void *h = dlmopen(LM_ID_NEWLM, libname, RTLD_LAZY);
	char dldir[PATH_MAX+1];
	char *fname = NULL;
	int ret;

	if (!h)
		return NULL;

	ret = dlinfo(h, RTLD_DI_ORIGIN, dldir);
	dlclose(h);

	if (ret || asprintf(&fname, "%s/%s", dldir, libname) <= 0)
		return NULL;

	return fname;
}

/* Get the memory map of all libraries, linked to an executable file. */
static int debug_obj_file_add_libs(struct dbg_trace_context *dbg,
				   struct debug_file *file)
{
	char line[PATH_MAX];
	char *libname;
	char *trimmed;
	char *fullname;
	FILE *fp = NULL;
	int ret = -1;

	setenv("LD_TRACE_LOADED_OBJECTS", "1", 1);
	fp = popen(file->file_name, "r");
	if (!fp)
		goto out;

	while (fgets(line, sizeof(line), fp) != NULL) {
		libname = strchr(line, ' ');
		trimmed = line;
		if (libname) {
			*libname = '\0';
			while (isspace(*trimmed))
				trimmed++;
			if (*trimmed != '/') {
				fullname = get_lib_full_path(trimmed);
				if (fullname) {
					get_mapped_file(dbg, fullname, 0);
					free(fullname);
				}
			} else {
				get_mapped_file(dbg, trimmed, 0);
			}
		}
	}

out:
	unsetenv("LD_TRACE_LOADED_OBJECTS");
	if (fp)
		pclose(fp);
	return ret;
}

/**
 * dbg_trace_context_create_file - create debug context for given executable file
 * @fname - full path to an executable file
 * @libs - if true: inspect also all libraries, used by the given file.
 *
 * Returns a pointer to allocated debug context, or NULL in case of an error.
 */
struct dbg_trace_context *dbg_trace_context_create_file(char *fname, bool libs)
{
	struct dbg_trace_context *dbg;
	struct debug_file *file;

	dbg = calloc(1, sizeof(*dbg));
	if (!dbg)
		return NULL;

	dbg->fname = strdup(fname);
	file = get_mapped_file(dbg, fname, 0);
	if (!file)
		goto error;
	if (libs)
		debug_obj_file_add_libs(dbg, file);

#ifdef DEBUG_INTERNALS
	printf("Created debug object for %s:\n\r", dbg->fname);
	file = dbg->files;
	while (file) {
		printf("\t%s\n\r", file->file_name);
		file = file->next;
	}
#endif
	return dbg;

error:
	dbg_trace_context_destroy(dbg);
	return NULL;
}

static void set_unknown(struct debug_symbols *sym, char *file)
{
	while (sym) {
		if (!sym->symbol.fname)
			sym->symbol.fname = strdup(file);
		sym = sym->next;
	}
}

/* Perform the requested symbols resolving, using the bfd library. */
int dbg_trace_resolve_symbols(struct dbg_trace_context *obj)
{
	struct debug_file *file;

	for (file = obj->files; file; file = file->next) {
		if (!file->dbg) {
			set_unknown(file->sym, file->file_name);
			continue;
		}
		/* Resolve near VMA -> name. */
		resolve_symbol_name(file->dbg, file->sym);
		/* Resolve name -> exact VMA. */
		resolve_symbol_vma(file->dbg, file->sym);
		resolve_symbol_vma(file->dbg, obj->sym);
	}

	return 0;
}

/* Add VMA -> name resolving request. */
static int add_resolve_vma2name(struct dbg_trace_context *obj,
				unsigned long long vma, int cookie)
{
	struct debug_symbols *s = NULL;
	struct debug_file *file;

	file = obj->files;
	while (file) {
		/* Find the file, where the requested VMA is. */
		if (vma >= file->vmem_start && vma <= file->vmem_end)
			break;
		file = file->next;
	}

	if (file) {
		s = file->sym;
		while (s) {
			/* Check if the given VMA is already added for resolving. */
			if (s->symbol.vma_near == vma)
				break;
			s = s->next;
		}
		if (!s) {
			s = calloc(1, sizeof(*s));
			if (!s)
				return -1;
			s->symbol.cookie = cookie;
			s->symbol.vma_near = vma;
			s->symbol.fname = strdup(file->file_name);
			if (!s->symbol.fname)
				goto error;
			s->next = file->sym;
			file->sym = s;
			file->sym_count++;
		}
	}

	if (s)
		return 0;
error:
	if (s) {
		free(s->symbol.fname);
		free(s);
	}
	return -1;
}

/* Add name - VMA resolving request, The @name can have wildcards. */
static int add_resolve_name2vma(struct dbg_trace_context *obj, char *name, int cookie)
{
	struct debug_symbols *s = NULL;

	s = obj->sym;
	while (s) {
		/* Check if the given name is already added for resolving. */
		if (s->symbol.name && !strcmp(name, s->symbol.name))
			break;
		s = s->next;
	}
	if (!s) {
		s = calloc(1, sizeof(*s));
		if (!s)
			return -1;
		s->symbol.cookie = cookie;
		s->symbol.name = strdup(name);
		if (!s->symbol.name)
			goto error;
		if (strchr(name, '*') || strchr(name, '?'))
			s->match = MATH_WILDCARD;

		s->next = obj->sym;
		obj->sym = s;
		obj->sym_count++;
	}

	return 0;

error:
	if (s) {
		free(s->symbol.name);
		free(s);
	}
	return -1;
}

/**
 * dbg_trace_add_resolve_symbol - add new resolving request
 * @obj - debug object context
 * @vma - VMA->name resolving, if @vma is not 0
 * @name - name-VMA resolving, if @name is not NULL
 * @cookie - a cookie, attached to each successful resolving from this request
 *
 * Returns 0 if the request is added successfully, or -1 in case of an error.
 */
int dbg_trace_add_resolve_symbol(struct dbg_trace_context *obj,
				 unsigned long long vma, char *name, int cookie)
{
	int ret = -1;

	if (!obj)
		return -1;

	if (!name && vma) /* vma -> name resolving */
		ret = add_resolve_vma2name(obj, vma, cookie);
	else if (name) /* name -> vma resolving */
		ret = add_resolve_name2vma(obj, name, cookie);

	return ret;
}

static int walk_symbols(struct debug_symbols *sym,
			int (*callback)(struct dbg_trace_symbols *, void *),
			void *context)
{
	while (sym) {
		if (callback(&sym->symbol, context))
			return -1;
		sym = sym->next;
	}

	return 0;
}

/**
 * dbg_trace_walk_resolved_symbols - walk through all resolved symbols
 * @obj - debug object context
 * @callback - a callback hook, called for each resolved symbol.
 *		If the callback returns non-zero, the walk stops.
 * @context - a user specified context, passed to the callback
 */
void dbg_trace_walk_resolved_symbols(struct dbg_trace_context *obj,
				     int (*callback)(struct dbg_trace_symbols *, void *),
				     void *context)
{
	struct debug_file *file;

	walk_symbols(obj->sym, callback, context);
	file = obj->files;
	while (file) {
		walk_symbols(file->sym, callback, context);
		file = file->next;
	}
}

/**
 * trace_debug_free_symbols - free array of debug symbols
 * @symbols - array with debug symbols
 * @count - count of the @symbols array
 */
void trace_debug_free_symbols(struct dbg_trace_symbols *symbols, int count)
{
	int i;

	if (!symbols)
		return;

	for (i = 0; i < count; i++) {
		free(symbols[i].name);
		free(symbols[i].fname);
	}
	free(symbols);

}

#define _STRINGIFY(x) #x
#define STRINGIFY(x) _STRINGIFY(x)
/**
 * dbg_trace_get_filemap - get a memory map of a process, using /proc fs
 * @pid_maps - return: list of files, mapped into the process memory
 * @pid - id of a process
 *
 * Returns 0 on success, -1 in case of an error.
 */
int dbg_trace_get_filemap(struct dbg_trace_pid_maps **pid_maps, int pid)
{
	struct dbg_trace_pid_maps *maps = *pid_maps;
	struct dbg_trace_proc_addr_map *map;
	unsigned long long begin, end;
	struct dbg_trace_pid_maps *m;
	char mapname[PATH_MAX+1];
	char fname[PATH_MAX+1];
	char buf[PATH_MAX+100];
	unsigned int i;
	FILE *f;
	int ret;
	int res;

	sprintf(fname, "/proc/%d/exe", pid);
	ret = readlink(fname, mapname, PATH_MAX);
	if (ret >= PATH_MAX || ret < 0)
		return -ENOENT;
	mapname[ret] = 0;

	sprintf(fname, "/proc/%d/maps", pid);
	f = fopen(fname, "r");
	if (!f)
		return -ENOENT;

	while (maps) {
		if (pid == maps->pid)
			break;
		maps = maps->next;
	}

	ret = -ENOMEM;
	if (!maps) {
		maps = calloc(1, sizeof(*maps));
		if (!maps)
			goto out_fail;
		maps->pid = pid;
		maps->next = *pid_maps;
		*pid_maps = maps;
	} else {
		for (i = 0; i < maps->nr_lib_maps; i++)
			free(maps->lib_maps[i].lib_name);
		free(maps->lib_maps);
		maps->lib_maps = NULL;
		maps->nr_lib_maps = 0;
		free(maps->proc_name);
	}

	maps->proc_name = strdup(mapname);
	if (!maps->proc_name)
		goto out_fail;

	while (fgets(buf, sizeof(buf), f)) {
		mapname[0] = '\0';
		res = sscanf(buf, "%llx-%llx %*s %*x %*s %*d %"STRINGIFY(PATH_MAX)"s",
			     &begin, &end, mapname);
		if (res == 3 && mapname[0] != '\0') {
			map = realloc(maps->lib_maps,
				      (maps->nr_lib_maps + 1) * sizeof(*map));
			if (!map)
				goto out_fail;
			map[maps->nr_lib_maps].end = end;
			map[maps->nr_lib_maps].start = begin;
			map[maps->nr_lib_maps].lib_name = strdup(mapname);
			if (!map[maps->nr_lib_maps].lib_name)
				goto out_fail;
			maps->lib_maps = map;
			maps->nr_lib_maps++;
		}
	}

	fclose(f);
	return 0;

out_fail:
	fclose(f);
	if (maps) {
		for (i = 0; i < maps->nr_lib_maps; i++)
			free(maps->lib_maps[i].lib_name);
		if (*pid_maps != maps) {
			m = *pid_maps;
			while (m) {
				if (m->next == maps) {
					m->next = maps->next;
					break;
				}
				m = m->next;
			}
		} else
			*pid_maps = maps->next;
		free(maps->lib_maps);
		maps->lib_maps = NULL;
		maps->nr_lib_maps = 0;
		free(maps->proc_name);
		maps->proc_name = NULL;
		free(maps);
	}
	return ret;
}

static void procmap_free(struct dbg_trace_pid_maps *maps)
{
	unsigned int i;

	if (!maps)
		return;
	if (maps->lib_maps) {
		for (i = 0; i < maps->nr_lib_maps; i++)
			free(maps->lib_maps[i].lib_name);
		free(maps->lib_maps);
	}
	free(maps->proc_name);
	free(maps);
}

/**
 * dbg_trace_free_filemap - Free list of files, associated with given process
 * @maps - list of files, returned by dbg_trace_get_filemap()
 */
void dbg_trace_free_filemap(struct dbg_trace_pid_maps *maps)
{
	struct dbg_trace_pid_maps *del;

	while (maps) {
		del = maps;
		maps = maps->next;
		procmap_free(del);
	}
}
