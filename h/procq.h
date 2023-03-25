#ifndef PROCQ_H
#define PROCQ_H
#include "types.h"
#include "const.h"
/* link descriptor type */
typedef struct proc_link {
	int index;
	struct proc_t *next;
} proc_link;

/* process table entry type */
typedef struct proc_t {
	proc_link p_link[SEMMAX];	/* pointers to next entries */
	state_t p_s;	/* processor state of the process */
	int qcount;	/* number of queues containing this entry */
	int *semvec[SEMMAX];	/* vector of active semaphores for this entry */
	/* plus other entries to be added by you later */
	/* === phase 2 === */
	/* process tree - 3-pointer approach */
	struct proc_t *parent;
	struct proc_t *sibling;	/* forms linked list of children of its parent */
	struct proc_t *child;	/* leftmost */
	/* CPU time */
	long cpu_time;	/* cumulated CPU time by process */
	long tdck;	/* Time of Day ClocK. timestamp of last started */
	/* 6 state_t* old/new area */
	state_t *sys_old, *sys_new;
	state_t *prog_old, *prog_new;
	state_t *mm_old, *mm_new;
	/* === phase 3 === */
	iores_t io_res;
} proc_t;

#endif
