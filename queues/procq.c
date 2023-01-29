/**
 */

#include "../h/const.h"
#include "../h/types.h"
#include "../h/procq.h"
#include "../h/misc.h"
#include "../h/procq.e"
#include "../h/util.e"
#include "../util/util.c"
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
	}
	procTable[MAXPROC-1].p_link[0].next = (proc_t *) ENULL;
	procTable[MAXPROC-1].p_link[0].index = 0;
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
			if (p->p_link[vacant_ind].next == (proc_t*) ENULL ||
				p->p_link[vacant_ind].next == NULL)
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
		if (p->p_link[vacant_ind].next == (proc_t*) ENULL ||
			p->p_link[vacant_ind].next == NULL)
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
	if (tp == NULL || tp->next == (proc_t*) ENULL)
	{
		return (proc_t *) ENULL;
	}
	/* `tp` points to itself => 1 element in queue */
	if (tp->next->p_link[tp->index].next == tp->next &&
		tp->next->p_link[tp->index].index == tp->index)
	{
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
			 prev = NULL, prev_ind = -1;
		 q != (proc_t*) ENULL && q != NULL &&
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
			if (prev == NULL)
			{
				/* prev is NULL => `q` is tail of queue */
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
				for (second_tl = q->p_link[q_ind].next,
						 second_tl_ind = q->p_link[q_ind].index,
						 old_second_tl_ind = -1;
					 second_tl->p_link[second_tl_ind].next != q;
					 second_tl_ind = second_tl->p_link[second_tl_ind].index,
						 second_tl = second_tl->p_link[old_second_tl_ind].next,
						 old_second_tl_ind = second_tl_ind
						 
					)
					;
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
	/* size_t i; */
	/* for (i = 0; i < SEMMAX; i++) */
	/* { */
	/* 	assert(p->p_link[i].next == (proc_t*) ENULL); */
	/* } */
	/* push to `procFree_h` stack */
	p->p_link[0].next = procFree_h.next;
	p->p_link[0].index = 0;
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
