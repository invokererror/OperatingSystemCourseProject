/*
 * This code is my own work, it was written without consulting code written by other students (Yiwei Zhu)
 */

/*
 * This file fulfills all functionality required by process queue module
 */

#include "../h/const.h"
#include "../h/types.h"
#include "../h/procq.h"
#include "../h/misc.h"
#include "../h/procq.e"
#include "../h/util.e"
#include <stdlib.h>
/* #include <assert.h> */

proc_link procFree_h;
proc_t procTable[MAXPROC];


void
initProc(void)
{
	int i;
	for (i = 0; i <= MAXPROC-2; i++)
	{
		procTable[i].p_link[0].next = &procTable[i+1];
		procTable[i].p_link[0].index = 0;
		/* all other procTable[i].p_link[k] set to invalid. No NULL allowed! */
		int k;
		for (k = 1; k < SEMMAX; k++)
		{
			procTable[i].p_link[k].next = (proc_t*) ENULL;
			procTable[i].p_link[k].index = -1;
		}
		procTable[i].qcount = 0;
		int j;
		for (j = 0; j < SEMMAX; j++)
		{
			procTable[i].semvec[j] = (int*) ENULL;
		}
		/* phase 2 addition */
		procTable[i].parent = (proc_t*) ENULL;
		procTable[i].sibling = (proc_t*) ENULL;
		procTable[i].child = (proc_t*) ENULL;
		procTable[i].cpu_time = 0L;
		procTable[i].tdck = 0L;
		procTable[i].sys_old = (state_t*) ENULL;
		procTable[i].sys_new = (state_t*) ENULL;
		procTable[i].prog_old = (state_t*) ENULL;
		procTable[i].prog_new = (state_t*) ENULL;
		procTable[i].mm_old = (state_t*) ENULL;
		procTable[i].mm_new = (state_t*) ENULL;
		/* phase 3 addition */
		procTable[i].io_res.io_sta = ENULL;
		procTable[i].io_res.io_len = ENULL;
	}
	procTable[MAXPROC-1].p_link[0].next = (proc_t *) ENULL;
	procTable[MAXPROC-1].p_link[0].index = 0;
	/* all other procTable[i].p_link[k] set to invalid. No NULL allowed! */
	int k;
	for (k = 1; k < SEMMAX; k++)
	{
		procTable[MAXPROC-1].p_link[k].next = (proc_t*) ENULL;
		procTable[MAXPROC-1].p_link[k].index = -1;
	}
	procTable[MAXPROC-1].qcount = 0;
	int j;
	for (j = 0; j < SEMMAX; j++)
	{
		procTable[MAXPROC-1].semvec[j] = (int*) ENULL;
	}
	/* phase 2 addition */
	procTable[MAXPROC-1].parent = (proc_t*) ENULL;
	procTable[MAXPROC-1].sibling = (proc_t*) ENULL;
	procTable[MAXPROC-1].child = (proc_t*) ENULL;
	procTable[MAXPROC-1].cpu_time = 0L;
	procTable[MAXPROC-1].tdck = 0L;
	procTable[MAXPROC-1].sys_old = (state_t*) ENULL;
	procTable[MAXPROC-1].sys_new = (state_t*) ENULL;
	procTable[MAXPROC-1].prog_old = (state_t*) ENULL;
	procTable[MAXPROC-1].prog_new = (state_t*) ENULL;
	procTable[MAXPROC-1].mm_old = (state_t*) ENULL;
	procTable[MAXPROC-1].mm_new = (state_t*) ENULL;
	/* phase 3 addition */
	procTable[MAXPROC-1].io_res.io_len = ENULL;
	procTable[MAXPROC-1].io_res.io_sta = ENULL;
	/* init `procFree_h` */
	procFree_h.next = &procTable[0];
	procFree_h.index = 0;
}


proc_t *
allocProc(void)
{
	if (procFree_h.next == (proc_t*) ENULL)
	{
		return (proc_t *) ENULL;
	}

	proc_t * res = procFree_h.next;
	procFree_h.next = procFree_h.next->p_link[procFree_h.index].next;
	procFree_h.index = 0;
	return res;
}


void
insertProc(proc_link *tp, proc_t *p)
{
	if (p->qcount >= SEMMAX)
	{
		/* panic */
		panic("insertProc: process resides on too many queues");
		return;
	}
	/* uninitialized tp->index=-1, tp->next = 0xffffff */
	if (tp->next == (proc_t*) ENULL)
	{
		tp->next = p;
		/* find available slot in `p->p_link[]` */
		size_t vacant_ind;
		for (vacant_ind = 0; vacant_ind < SEMMAX; vacant_ind++)
		{
			if (p->p_link[vacant_ind].next == (proc_t*) ENULL)
			{
				/* smallest slot available */
				break;
			}
		}
		if (vacant_ind >= SEMMAX)
		{
			panic("insertProc: AssertionFails: cannot find vacant slot");
			return;
		}
		tp->index = vacant_ind;
		p->qcount++;
		/* one element `tp` => point to itself */
		p->p_link[vacant_ind].next = p;
		p->p_link[vacant_ind].index = vacant_ind;
		return;
	}
	/* `tp` queue has >= 1 elements */
	proc_t * q_tail = tp->next;
	int tl_ind = tp->index;
	proc_t * q_head = q_tail->p_link[tl_ind].next;
	int head_ind = q_tail->p_link[tl_ind].index;
	/* previous tail now point to `p` */
	q_tail->p_link[tl_ind].next = p;
	/* use smallest n in p->p_link[n] where n is available */
	size_t vacant_ind;
	for (vacant_ind = 0; vacant_ind < SEMMAX; vacant_ind++)
	{
		if (p->p_link[vacant_ind].next == (proc_t*) ENULL)
			break;
	}
	if (vacant_ind >= SEMMAX)
	{
		panic("insertProc: AssertionFails: cannot find vacant slot");
		return;
	}
	q_tail->p_link[tl_ind].index = vacant_ind;
	/* `p` point to head */
	p->p_link[vacant_ind].next = q_head;
	p->p_link[vacant_ind].index = head_ind;
	/* update `tp` to new tail (inserted `p`) */
	tp->next = p;
	tp->index = vacant_ind;
	/* update `p` struct */
	p->qcount++;
}


proc_t *
removeProc(proc_link *tp)
{
	if (tp->next == (proc_t*) ENULL)
	{
		return (proc_t *) ENULL;
	}
	if (tp->next->p_link[tp->index].next == tp->next &&
		tp->next->p_link[tp->index].index == tp->index)
	{
		/* `tp` points to itself => 1 element in queue */
		proc_t *res = tp->next;
		/* update struct of removed process */
		res->qcount--;
		res->p_link[tp->index].next = (proc_t*) ENULL;
		res->p_link[tp->index].index = ENULL;
		/* reset `tp` */
		tp->next = (proc_t*) ENULL;
		tp->index = ENULL;
		return res;
	}
	/* `tp` contains >= 2 elements */
	/* queue removes from head */
	proc_t *tail = tp->next;
	int tl_ind = tp->index;
	proc_t *head = tail->p_link[tl_ind].next;
	int hd_ind = tail->p_link[tl_ind].index;
	/* tail now points to head->next */
	tail->p_link[tl_ind].next = head->p_link[hd_ind].next;
	tail->p_link[tl_ind].index = head->p_link[hd_ind].index;
	/* update `head` struct */
	head->p_link[hd_ind].next = (proc_t*) ENULL;
	head->p_link[hd_ind].index = ENULL;
	head->qcount--;
	
	return head;
}


proc_t *
outProc(proc_link *tp, proc_t *p)
{
	/* loop through process queue */
	proc_t *q, *prev;
	int q_ind, prev_ind;
	bool_t visited_tail = FALSE;
	for (q = tp->next, q_ind = tp->index,
			 prev = (proc_t*) ENULL, prev_ind = -1;
		 q != (proc_t*) ENULL &&
			 !(q == tp->next && visited_tail);
		 prev = q, prev_ind = q_ind,
			 q = q->p_link[q_ind].next,
			 q_ind = prev->p_link[q_ind].index)
	{
		if (q == tp->next)
		{
			visited_tail = TRUE;
		}
		if (q == p)
		{
			/* found the process in queue */
			if (prev == (proc_t*) ENULL)
			{
				/* prev is ENULL => `q` is tail of queue */
				if (q->p_link[q_ind].next == q &&
					q->p_link[q_ind].index == q_ind)
				{
					/* `q` is only element */
					/* update `q` struct */
					q->p_link[q_ind].next = (proc_t*) ENULL;
					q->p_link[q_ind].index = ENULL;
					q->qcount--;
					/* update `tp` */
					tp->next = (proc_t*) ENULL;
					tp->index = ENULL;
					return p;
				}
				/* queue has >= 2 elements */
				/* find second tail pointing to `q` */
				proc_t *second_tl;
				int second_tl_ind;
				int old_second_tl_ind;
				/* use while-loop for gdb */
				second_tl = q->p_link[q_ind].next;
				second_tl_ind = q->p_link[q_ind].index;
				old_second_tl_ind = -1;
				while (second_tl->p_link[second_tl_ind].next != q)
				{
					old_second_tl_ind = second_tl_ind;
					second_tl_ind = second_tl->p_link[second_tl_ind].index;
					second_tl = second_tl->p_link[old_second_tl_ind].next;
				}
				/* second tail points to head */
				second_tl->p_link[second_tl_ind].next = q->p_link[q_ind].next;
				second_tl->p_link[second_tl_ind].index = q->p_link[q_ind].index;
				/* update `q` struct */
				q->p_link[q_ind].next = (proc_t*) ENULL;
				q->p_link[q_ind].index = ENULL;
				/* update `tp` */
				tp->next = second_tl;
				tp->index = second_tl_ind;
				return p;
			}

			/* `q` is not tail => queue >= 2 elements */
			if (q == tp->next->p_link[tp->index].next)
			{
				/* `q` is head */
				/* delegate to removeProc */
				return removeProc(tp);
			}
			/* `q` is neither tail nor head */
			/* `prev` points to q->next */
			prev->p_link[prev_ind].next = q->p_link[q_ind].next;
			prev->p_link[prev_ind].index = q->p_link[q_ind].index;
			/* update `q` struct */
			q->p_link[q_ind].next = (proc_t*) ENULL;
			q->p_link[q_ind].index = ENULL;
			q->qcount--;
			
			return p;
		}
	}
	return (proc_t*) ENULL;
}

void
freeProc(proc_t *p)
{
	/* `p` should not reside in any process queue */
	/* push to `procFree_h` stack */
	p->p_link[0].next = procFree_h.next;
	p->p_link[0].index = 0;
	/* reset other p attribute */
	p->parent = (proc_t*) ENULL;
	p->sibling = (proc_t*) ENULL;
	p->child = (proc_t*) ENULL;
	p->cpu_time = 0;
	p->tdck = 0;
	p->sys_old = (state_t*) ENULL;
	p->sys_new = (state_t*) ENULL;
	p->prog_old = (state_t*) ENULL;
	p->prog_new = (state_t*) ENULL;
	p->mm_old = (state_t*) ENULL;
	p->mm_new = (state_t*) ENULL;
	/* phase 3 */
	p->io_res.io_len = ENULL;
	p->io_res.io_sta = ENULL;
	/* more reset from phase 3 deadlock apocalypse */
	int i;
	for (i = 1; i <= SEMMAX-1; i++)
	{
		p->p_link[i].next = (proc_t*) ENULL;
		p->p_link[i].index = ENULL;
	}
	/* no freeing proc_t.state_t in fear */
	p->qcount = 0;
	for (i = 0; i < SEMMAX; i++)
	{
		p->semvec[i] = (int*) ENULL;
	}
	/* update `procFree_h` */
	procFree_h.next = p;
	procFree_h.index = 0;
}


proc_t *
headQueue(proc_link tp)
{
	if (tp.next == (proc_t*) ENULL)
	{
		/* queue is empty */
		return (proc_t*) ENULL;
	}
	return tp.next->p_link[tp.index].next;
}
