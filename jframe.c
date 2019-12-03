/*
 * This file and its contents are supplied under the terms of the
 * Common Development and Distribution License ("CDDL"), version 1.0.
 * You may only use this file in accordance with the terms of version
 * 1.0 of the CDDL.
 *
 * A full copy of the text of the CDDL should have accompanied this
 * source.  A copy of the CDDL is also available via the Internet at
 * http://www.illumos.org/license/CDDL.
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>
#include <libproc.h>
#include <dlfcn.h>
#include <limits.h>

typedef struct jvm_agent jvm_agent_t;
typedef int java_stack_f(void *, prgregset_t, const char *, int, int, void *);

#define	JVM_DB_VERSION	1
static void *libjvm;
typedef jvm_agent_t *(*j_agent_create_f)(struct ps_prochandle *, int);
typedef void (*j_agent_destroy_f)(jvm_agent_t *);
typedef int (*j_frame_iter_f)(jvm_agent_t *, prgregset_t, java_stack_f *,
    void *);

static j_agent_create_f j_agent_create;
static j_agent_destroy_f j_agent_destroy;
static j_frame_iter_f j_frame_iter;

// Copied from pstack
static int
jvm_object_iter(void *cd, const prmap_t *pmp, const char *obj)
{
	char path[PATH_MAX];
	char *name;
	char *s1, *s2;
	struct ps_prochandle *Pr = cd;

	if ((name = strstr(obj, "/libjvm.so")) == NULL)
		name = strstr(obj, "/libjvm_g.so");

	if (name) {
		(void) strcpy(path, obj);
		if (Pstatus(Pr)->pr_dmodel != PR_MODEL_NATIVE) {
			s1 = name;
			s2 = path + (s1 - obj);
			(void) strcpy(s2, "/64");
			s2 += 3;
			(void) strcpy(s2, s1);
		}

		s1 = strstr(obj, ".so");
		s2 = strstr(path, ".so");
		(void) strcpy(s2, "_db");
		s2 += 3;
		(void) strcpy(s2, s1);

		if ((libjvm = dlopen(path, RTLD_LAZY|RTLD_GLOBAL)) != NULL)
			return (1);
	}

	return (0);
}

// Copied from pstack
static jvm_agent_t *
load_libjvm(struct ps_prochandle *Pr)
{
	jvm_agent_t *ret;

	/*
	 * Iterate through all the loaded objects in the target, looking
	 * for libjvm.so.  If we find libjvm.so we'll try to load the
	 * corresponding libjvm_db.so that lives in the same directory.
	 *
	 * At first glance it seems like we'd want to use
	 * Pobject_iter_resolved() here since we'd want to make sure that
	 * we have the full path to the libjvm.so.  But really, we don't
	 * want that since we're going to be dlopen()ing a library and
	 * executing code from that path, and therefore we don't want to
	 * load any library code that could be from a zone since it could
	 * have been replaced with a trojan.  Hence, we use Pobject_iter().
	 * So if we're debugging java processes in a zone from the global
	 * zone, and we want to get proper java stack stack frames, then
	 * the same jvm that is running within the zone needs to be
	 * installed in the global zone.
	 */
	(void) Pobject_iter(Pr, jvm_object_iter, Pr);

	if (libjvm) {
		j_agent_create = (j_agent_create_f)
		    dlsym(libjvm, "Jagent_create");
		j_agent_destroy = (j_agent_destroy_f)
		    dlsym(libjvm, "Jagent_destroy");
		j_frame_iter = (j_frame_iter_f)
		    dlsym(libjvm, "Jframe_iter");

		if (j_agent_create == NULL || j_agent_destroy == NULL ||
		    j_frame_iter == NULL ||
		    (ret = j_agent_create(Pr, JVM_DB_VERSION)) == NULL) {
			return (NULL);
		}

		return (ret);
	}

	return (NULL);
}


int frame_cb(void *cld, prgregset_t _regs, const char *name, int _bci, int line, void *_handle) {
    bool *found_frame = cld;
    *found_frame = true;
    printf("%s\n", name);
}

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "usage: %s <core>\n", argv[0]);
        exit(1);
    }

    int err;
    struct ps_prochandle *prochandle = Pgrab_core(argv[1], NULL, PGRAB_RDONLY, &err);
    if (prochandle == NULL) {
        fprintf(stderr, "failed to open %s: %s\n", argv[1], Pgrab_error(err));
        exit(1);
    }

    jvm_agent_t *agent = load_libjvm(prochandle);
    if (agent == NULL) {
        fprintf(stderr, "failed to create java agent\n");
        exit(1);
    }

    char *line = NULL;
    size_t cap = 0;
    ssize_t len;
    while ((len = getline(&line, &cap, stdin)) > 0) {
        
        unsigned long long pc;
        if (sscanf(line, "%llx", &pc) != 1) {
            fprintf(stderr, "unexpected input '%s'. expected a program counter, e.g. '0xfffffd7fed879fd4'\n", line);
            exit(1);
        }

        // Have to prime with registers so it can walk, but we don't have anything except the PC.
        // It will only be able to find a single frame, but this is OK because we are going to
        // call it once for each frames ourselves.
        prgregset_t regs = { 0 };
        regs[R_PC] = pc;
        bool found_frame = false;

        j_frame_iter(agent, regs, frame_cb, &found_frame);
        if (!found_frame) {
            // Didn't find a function corresponding to this PC, just pass the
            // raw address through.
            printf("%llx (no symbol found)\n", pc);
        }

        free(line);
        line = NULL;
    }

    return 0;
}
