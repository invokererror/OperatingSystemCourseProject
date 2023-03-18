/*
 * This code is my own work, it was written without consulting code written by other students (Yiwei Zhu)
 */

/*
 * This file fulfills all functionality required for Active Semaphore List module
 */

#include "../h/const.h"
#include "../h/types.h"
#include "../h/procq.h"
#include "../h/procq.e"
#include "../h/asl.h"
#include "../h/asl.e"
#include "../h/util.e"
#include "../util/util.c"
#include "../h/misc.h"


/* module-local variables */
aslhead_t semd_h;	/* active semaphore list's head */
semd_t semdTable[MAXPROC];
semd_t *semdFree_h;	/* -> available semd */


/**
 * Initialize the semaphore descriptor free list
 */
void
initSemd(void)
{
	int i;
	for (i = 0;
		 i <= MAXPROC-2;
		 i++)
	{
		semdTable[i].s_prev = (semd_t*) ENULL;
		semdTable[i].s_next = &semdTable[i+1];
		semdTable[i].s_semAdd = (int*) ENULL;
		semdTable[i].s_link.index = ENULL;
		semdTable[i].s_link.next = (proc_t*) ENULL;
	}
	/* set last node */
	semdTable[MAXPROC-1].s_next = (semd_t*) ENULL;
	semdTable[MAXPROC-1].s_prev = (semd_t*) ENULL;
	semdTable[MAXPROC-1].s_semAdd = (int*) ENULL;
	semdTable[MAXPROC-1].s_link.index = ENULL;
	semdTable[MAXPROC-1].s_link.next = (proc_t*) ENULL;
	/* set `semdFree_h` */
	semdFree_h = &semdTable[0];
	/* init ASL */
	semd_h.head = (semd_t*) ENULL;
	semd_h.aslSize = 0;
}


/**
 * Return FALSE if the ASL is empty or TRUE if not empty
 * Used to determine if there are any semaphores on ASL
 */
int
headASL(void)
{
	return (semd_h.head == (semd_t*) ENULL) || (semd_h.aslSize == 0) ? FALSE : TRUE;
}


/**
 * Search in ASL for `semAdd`.
 * Returns semd_t * where ret->s_semAdd == semAdd, or ENULL if not found or ASL empty
 */
static semd_t *
searchSemAdd(int *semAdd)
{
	if (headASL() == FALSE)
	{
		/* ASL empty */
		return (semd_t*) ENULL;
	}
	semd_t *p;
	bool_t looped = FALSE;
	for (p = semd_h.head; p->s_semAdd < semAdd; p = p->s_next)
	{
		if (p == semd_h.head)
		{
			if (looped)
				break;
			looped = TRUE;
		}
	}
	if (p->s_semAdd >= semAdd)
		return p->s_semAdd == semAdd ? p : (semd_t*) ENULL;
	/* looped and not found */
	return (semd_t*) ENULL;
		
}


/**
 * Return a pointer to the process table entry that is at the head of the process queue associated with semaphore `semAdd`
 * If the list is empty, return ENULL
 */
proc_t *
headBlocked(int *semAdd)
{
	semd_t *semd;
	if (headASL() == FALSE)
		return (proc_t*) ENULL;
	if ((semd = searchSemAdd(semAdd)) == (semd_t*) ENULL)
	{
		/* `semAdd` not in ASL */
		return (proc_t*) ENULL;
	}
	/* `semd->s_semAdd == semAdd` */
	if (semd->s_semAdd != semAdd)
	{
		panic("Incorrect implementation of searchSemAdd!");
		return (proc_t*) ENULL;
	}
	proc_t *q_tail = semd->s_link.next;
	int q_tail_ind = semd->s_link.index;
	if (q_tail == (proc_t*) ENULL)
		return (proc_t*) ENULL;
	return q_tail->p_link[q_tail_ind].next;
}


/**
 * Abandoned
 * Binary search on ASL to locate `semAdd`.
 * Return its position or -1 if not found.
 */
/* int */
/* searchSemAdd(int *semAdd) */
/* { */
	/* cause weird SIGILL */
	/* int left_ind = 0, right_ind = semd_h.aslSize-1; */
	/* int mid; */
	/* while (left_ind <= right_ind) */
	/* { */
	/* 	mid = (left_ind + right_ind) / 2; */
	/* 	if (*semAdd == semd_h.semAddVec[mid]) */
	/* 	{ */
	/* 		return mid; */
	/* 	} */
	/* 	if (*semAdd < semd_h.semAddVec[mid]) */
	/* 	{ */
	/* 		right_ind = mid-1; */
	/* 		continue; */
	/* 	} */
	/* 	/\* *semAdd > semd_h.semAddVec[mid] *\/ */
	/* 	left_ind = mid+1; */
	/* 	continue; */
	/* } */
	/* return -1; */

	/* try linear search */
	/* int i; */
	/* for (i = 0; i < SEMMAX; i++) */
	/* { */
	/* 	if (*semAdd == semd_h.semAddVec[i]) */
	/* 	{ */
	/* 		return i; */
	/* 	} */
	/* } */
	/* return -1; */
/* 	return -1; */
/* } */

/**
 * insert a semaphore descriptor to ASL
 */
static void
insertSemD(semd_t *semd)
{
	if (headASL() == FALSE)
	{
		/* nothing in ASL */
		semd_h.head = semd;
		semd_h.aslSize++;
		/* semd self-pointing */
		semd->s_prev = semd;
		semd->s_next = semd;
		return;
	}
	/* walk down till a semd to insert before it */
	semd_t *cur;
	bool_t looped = FALSE;
	for (cur = semd_h.head; cur->s_semAdd < semd->s_semAdd; cur = cur->s_next)
	{
		if (cur == semd_h.head)
		{
			if (looped)
				break;
			looped = TRUE;
		}
	}
	if (cur == semd_h.head && looped)
	{
		/* all semd in ASL < `semd` */
		/* insert `semd` at tail of ASL */
		cur->s_prev->s_next = semd;
		semd->s_prev = cur->s_prev;
		cur->s_prev = semd;
		semd->s_next = cur;
	}
	else
	{
		/* insert `semd` before cur, after cur->s_prev */
		if (cur->s_prev == (semd_t*) ENULL)
		{
			panic("Each semd in ASL should have been inited!");
			return;
		}
		cur->s_prev->s_next = semd;
		semd->s_prev = cur->s_prev;
		semd->s_next = cur;
		cur->s_prev = semd;
		/* update semd_h to new head? */
		if (cur == semd_h.head)
		{
			/* semd now new head of ASL */
			semd_h.head = semd;
		}
	}
	if (semd_h.aslSize >= MAXPROC)
	{
		panic("Reached MAXPROC for inserting new semd");
		return;
	}
	semd_h.aslSize++;
}


/**
 * reset `semd` and give back to `semdFree_h`
 * precondition: semd previously is in ASL
 */
static void
freeSemD(semd_t *semd)
{
	/* reset semd attributes */
	semd->s_next = (semd_t*) ENULL;
	semd->s_prev = (semd_t*) ENULL;
	semd->s_semAdd = (int*) ENULL;
	semd->s_link.index = ENULL;
	semd->s_link.next = (proc_t*) ENULL;
	/* give back to `semdFree_h` */
	if (semdFree_h != (semd_t*) ENULL)
	{
		semdFree_h->s_prev = semd;
	}
	semd->s_next = semdFree_h;
	semdFree_h = semd;
}


/**
 * if ASL becomes empty, update `semd_h`
 * precondition: can find `semd` in ASL
 */
static int
removeSemD(semd_t *semd)
{
	semd_t *p;
	int looped = FALSE;
	for (p = semd_h.head; 1; p = p->s_next)
	{
		if (p == semd_h.head)
		{
			if (looped)
				break;
			looped = TRUE;
		}
		if (p == semd)
		{
			if (semd->s_next == semd)
			{
				/* ASL has only one element */
				semd_h.head = (semd_t*) ENULL;
				semd_h.aslSize = 0;
			} else
			{
				/* ASL has >= 2 elements */
				semd->s_prev->s_next = semd->s_next;
				semd->s_next->s_prev = semd->s_prev;
				semd_h.aslSize--;
				if (semd == semd_h.head)
				{
					semd_h.head = semd_h.head->s_next;
				}
			}
			freeSemD(semd);
			/* assumption: no duplicate semd_t in ASL */
			break;
		}
	}
	return 0;
}


/**
 * Return TRUE if there is already `p` in queue led by `tp`, otherwise FALSE
 */
static int
pAlreadyInProcq(proc_link *tp, proc_t *p)
{
	if (tp->next == (proc_t*) ENULL)
	{
		/* invalid `tp` */
		return FALSE;
	}
	proc_t *proc;
	int proc_ind, prev_proc_ind;
	bool_t is_tail_visited = FALSE;
	for (proc = tp->next, proc_ind = tp->index, prev_proc_ind = ENULL;
		 proc != p;
		 prev_proc_ind = proc_ind,
			 proc_ind = proc->p_link[proc_ind].index,
			 proc = proc->p_link[prev_proc_ind].next)
	{
		if (proc == tp->next)
		{
			if (is_tail_visited)
			{
				/* iterated to a loop */
				break;
			}
			is_tail_visited = TRUE;
		}
	}
	return proc == p ? TRUE : FALSE;
}


/**
 * Insert the process table entry pointed to by p at the tail of the process queue associated with the semaphore whose address is semAdd. If the semaphore is currently not active (there is no descriptor for it in the ASL), allocate a new descriptor from the free list, insert it in the ASL (at the appropriate position), and initialize all of the fields. If a new semaphore descriptor needs to be allocated and the free list is empty, return TRUE. In all other cases return FALSE.
 */
int
insertBlocked(int *semAdd, proc_t *p)
{
	semd_t *semd;
	if ((semd = searchSemAdd(semAdd)) == (semd_t*) ENULL)
	{
		/* `semAdd` not in ASL */
		/* allocate new semd from free list */
		semd_t *newSemD = semdFree_h;
		if (newSemD == (semd_t*) ENULL)
		{
			/* no semd available */
			return TRUE;
		}
		semdFree_h = semdFree_h->s_next;
		/* update `newSemD` */
		newSemD->s_semAdd = semAdd;
		/* insert `newSemD` to ASL */
		insertSemD(newSemD);
		/* associate `newSemD` with a process queue with sole element as `p` */
		/* find an available slot in p->p_link */
		int empty_slot;
		for (empty_slot = 0; empty_slot < SEMMAX; empty_slot++)
		{
			if (p->p_link[empty_slot].next == (proc_t*) ENULL)
				break;
		}
		if (empty_slot >= SEMMAX)
		{
			panic("All p->p_link[] is non-empty. Cannot find an available slot!");
			return TRUE;
		}
		/* make `p` self-pointing */
		p->p_link[empty_slot].index = empty_slot;
		p->p_link[empty_slot].next = p;
		newSemD->s_link.next = p->p_link[empty_slot].next;
		newSemD->s_link.index = p->p_link[empty_slot].index;
		/* update semvec in `p` */
		for (empty_slot = 0;
			 empty_slot < SEMMAX;
			 empty_slot++)
		{
			if (p->semvec[empty_slot] == (int*) ENULL)
				break;
		}
		if (empty_slot >= SEMMAX)
		{
			panic("No available slot for p->semvec[]!");
			return TRUE;
		}
		p->semvec[empty_slot] = newSemD->s_semAdd;
		/* update qcount */
		p->qcount++;
		return FALSE;
	}
	/* `semAdd` is in ASL */
	/* `p` already in process queue associated with semd? */
	if (pAlreadyInProcq(&semd->s_link, p))
	{
		/* no duplicate `p` in procq */
		/* do nothing */
		return FALSE;
	}
	insertProc(&semd->s_link, p);
	/* update `p` */
	int empty_slot;
	for (empty_slot = 0;
		 empty_slot < SEMMAX;
		 empty_slot++)
	{
		if (p->semvec[empty_slot] == (int*) ENULL)
			break;
	}
	if (empty_slot >= SEMMAX)
	{
		panic("No available slot for p->semvec[]!");
		return TRUE;
	}
	p->semvec[empty_slot] = semd->s_semAdd;
	
	return FALSE;
}


/**
 * Search the ASL for a descriptor of this semaphore. If none is found, return ENULL. Otherwise, remove the first process table entry from the process queue of the appropriate semaphore descriptor and return a pointer to it. If the process queue for this semaphore becomes empty, remove the descriptor from the ASL and insert it in the free list of semaphore descriptors.
 */
proc_t *
removeBlocked(int *semAdd)
{
	semd_t *semd;
	if (headASL() == FALSE)
	{
		/* ASL is empty */
		return (proc_t*) ENULL;
	}
	if ((semd = searchSemAdd(semAdd)) == (semd_t*) ENULL)
	{
		/* `semAdd` not in ASL */
		return (proc_t*) ENULL;
	}
	proc_t *res = removeProc(&semd->s_link);
	/* set semvec in proc */
	int i;
	for (i = 0; i < SEMMAX; i++)
	{
		if (res->semvec[i] == semAdd)
			break;
	}
	if (i >= SEMMAX)
	{
		panic("No `semAdd` registered in res->semvec!");
		return (proc_t*) ENULL;
	}
	res->semvec[i] = (int*) ENULL;
	/* procq becomes empty? */
	if (semd->s_link.next == (proc_t*) ENULL)
	{
		/* process queue blocked by `semAdd` now empty */
		/* remove `semd` from ASL */
		if (semd->s_prev == (semd_t*) ENULL || semd->s_next == (semd_t*) ENULL)
		{
			panic("Trying to remove a semd from an empty ASL!");
			return (proc_t*) ENULL;
		}
		semd->s_prev->s_next = semd->s_next;
		semd->s_next->s_prev = semd->s_prev;
		/* update `semd_h` */
		if (semd == semd_h.head)
		{
			semd_h.head = semd->s_next;
		}
		semd_h.aslSize--;
		if (semd_h.aslSize < 0)
		{
			panic("`semd_h.aslSize` now negative!");
			return (proc_t*) ENULL;
		}
		freeSemD(semd);
	}
	return res;
}


/**
 * Remove the process table entry pointed to by p from the queues associated with the appropriate semaphore on the ASL. If the desired entry does not appear in any of the process queues (an error condition), return ENULL. Otherwise, return p.
 */
proc_t *
outBlocked(proc_t *p)
{
	semd_t *semd;
	bool_t appears = FALSE;
	bool_t looped = FALSE;
	for (semd = semd_h.head; 1; semd = semd->s_next)
	{
		if (semd == semd_h.head)
		{
			if (looped)
				break;
			looped = TRUE;
		}
		if (pAlreadyInProcq(&semd->s_link, p))
		{
			appears = TRUE;
			/* removes `p` from blocked semq */
			outProc(&semd->s_link, p);
			/* update sem related attribute in `p` */
			int i;
			for (i = 0; i < SEMMAX; i++)
			{
				if (p->semvec[i] == semd->s_semAdd)
				{
					p->semvec[i] = (int*) ENULL;
					/* assumption: no duplicate semvec */
					break;
				}
			}
			if (i >= SEMMAX)
			{
				panic("asl.outBlocked: cannot find semvec where should");
				return (proc_t*) ENULL;
			}
		}
	} 
	if (appears == FALSE)
		return (proc_t*) ENULL;
	/* outBlocked could result in some semd no longer active */
	/* update ASL */
	looped = FALSE;
	/* potentially removing semd from ASL while looping ASL, careful looping */
	/* find tail of ASL */
	/* impl assumption: ASL is a loop */
	semd_t *asl_tl;
	for (asl_tl = semd_h.head; asl_tl->s_next != semd_h.head; asl_tl = asl_tl->s_next)
		;
	/* CAREFUL looping */
	semd_t* asl_next;
	for (semd = semd_h.head, asl_next = semd->s_next;
		 ;
		 semd = asl_next, asl_next = asl_next->s_next)
	{
		if (semd->s_link.next == (proc_t*) ENULL)
		{
			/* `semd` no longer active */
			removeSemD(semd);
		}
		if (semd == asl_tl)
		{
			/* when we hit tail, we executed body and exit */
			break;
		}
	}
	return p;
}
