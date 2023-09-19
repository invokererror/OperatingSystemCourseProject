/*
 * This code is my own work, it was written without consulting code written by other students (Yiwei Zhu)
 */
#include "../../h/syscall.e"
#include "../../h/types.h"
#include "../../h/procq.h"
#include "../../h/vpop.h"
#include "../../h/traps.e"
#include "../../h/utils.e"
#include "../../util/utils.c"


int
creatproc(state_t *old)
{
	proc_t *new_proc = allocProc();
	if (new_proc == (proc_t*) ENULL)
	{
		old->s_r[2] = -1;	/* d2 = -1; */
		return 1;
	}
	new_proc->p_s = *(state_t*) old->s_r[4];	/* from d4 */
	/* process tree */
	proc_t *running_proc = headQueue(rq_tl);
	new_proc->parent = running_proc;
	new_proc->sibling = (proc_t*) ENULL;
	new_proc->child = (proc_t*) ENULL;
	if (running_proc->child != (proc_t*) ENULL)
	{
		/* insert to sibling head */
		new_proc->sibling = running_proc->child;
		running_proc->child = new_proc;
	} else
	{
		running_proc->child = new_proc;
	}
	/* put on RQ */
	insertProc(&rq_tl, new_proc);
	old->s_r[2] = 0;	/* d2 = 0; */
	return 0;
}


/**
 * recursively terminate process and descendents, post-order
 * looks like in-order
 *
 * `pproc`: parent process
 */
int
killproc_aux(proc_t *pproc)
{
	if (pproc == (proc_t*) ENULL)
	{
		return 0;
	}
	proc_t *pproc_sib = pproc->sibling;
	proc_t *pproc_chd = pproc->child;
	
	killproc_aux(pproc_chd);
	
	/* kill pproc */
	/* try outProc from RQ? */
	if (outProc(&rq_tl, pproc) == (proc_t*) ENULL)
	{
		/* not found in RQ */
		/* blocked by some sems */
		/* update those sems */
		/* DISCUSS: add following code to outBlocked function? */
		int i;
		for (i = 0; i < SEMMAX; i++)
		{
			int *sem_addr = pproc->semvec[i];
			if (sem_addr != (int*) ENULL)
			{
				(*sem_addr)++;
			}
		}
		/* remove `pproc` from semqueue for all its blocked sems */
		/* assumption: outBlocked works for multiple blocked sems per process */
		if (outBlocked(pproc) == (proc_t*) ENULL)
		{
			/* not success */
			panic("process not on RQ AND not blocked by any sem!");
			return 1;
		}
	} else
	{
		/* in RQ */
		/* removed the process from RQ */
		/* do nothing */
	}
	/* update process tree */
	/* safe net. should be fine without */
	pproc->child = (proc_t*) ENULL;
	pproc->sibling = (proc_t*) ENULL;
	pproc->parent = (proc_t*) ENULL;
	/* finally kill */
	freeProc(pproc);

	killproc_aux(pproc_sib);

	return 0;
}


/**
 * the function that actually kills a process
 */
int
killproc_real(proc_t *p)
{
	proc_t *p_parent = p->parent;
	/* dirty trick: set p->sib to ENULL to facilitate recursion */
	proc_t *p_sib = p->sibling;
	p->sibling = (proc_t*) ENULL;
	killproc_aux(p);
	/* update process tree */
	if (p_parent == (proc_t*) ENULL)
	{
		/* p1 is killed */
		/* no process tree structure update */
		return 0;
	}
	if (p_parent->child == (proc_t*) ENULL)
	{
		panic("syscall:killproc_real: wrong proc tree");
		return 1;
	}
	if (p_parent->child == p)
	{
		p_parent->child = p_sib;
	} else
	{
		/* change sibling link adjacent to `p` */
		proc_t *p_prev;
		for (p_prev = p_parent->child;
			 p_prev->sibling != p;
			 p_prev = p_prev->sibling)
			;
		p_prev->sibling = p_sib;
	}
	return 0;
}


int
killproc(state_t *old)
{
	proc_t *running_proc = headQueue(rq_tl);
	return killproc_real(running_proc);
}


int
semop(state_t *old)
{
	/* get d3, d4 from old area */
	const int d3 = old->s_r[3];
	const int d4 = old->s_r[4];
	proc_t *caller_process = headQueue(rq_tl);
	/* for each vpop */
	int i;
	for (i = 0; i < d3; i++)
	{
		vpop *vp = &(((vpop*) d4)[i]);
		if (vp->op == LOCK)
		{
			(*vp->sem)--;
			/* if sem now blockes calling process, get it off (head of) RQ */
			if ((*vp->sem) < 0)
			{
				/* P causes blocking */
				/* get off RQ if on RQ */
				proc_t *useless = headQueue(rq_tl);
				if (useless == caller_process)
				{
					/* `caller_process` on RQ */
					if (removeProc(&rq_tl) != caller_process)
					{
						panic("moving wrong process from RQ");
						return 1;
					}
					/* update proc_t->state_t */
					caller_process->p_s = *old;
					/* update cpu_time */
					long now;
					STCK(&now);
					caller_process->cpu_time += now - caller_process->tdck;
					caller_process->tdck = 0;
					/* new head */
				}
				/* update proc_t */
				/* caller_process->qcount++; */
				/* insertSemvec(caller_process, vp->sem); */
				/* `caller_process` onto sem queue */
				insertBlocked(vp->sem, caller_process);
			}
		} else if (vp->op == UNLOCK)
		{
			(*vp->sem)++;
			/* pop head process from sem queue */
			proc_t *sem_q_hd = removeBlocked(vp->sem);
			/* update proc_t */
			/*
			 * CAVEAT: might need to change ->p_link!
			 * do nothing here
			 */
			if (sem_q_hd == (proc_t*) ENULL)
			{
				/* ok, removeBlocked treated removing no process blocked from sem as error. */
				/* but here it is not necessarily error */
				continue;
			}
			/* sem_q_hd->qcount--; */
			/* removeSemvec(sem_q_hd, vp->sem); */
			/* put onto RQ if no sem blocking */
			/*
			 * stronger test
			 * assumption: not on RQ
			 * qcount == 0 <==> semvec all -1
			 */
			int i;
			#ifndef NDEBUG
			/* potential side-effect if outProc is wrong */
			if (outProc(&rq_tl, sem_q_hd) != (proc_t*) ENULL)
			{
				panic("syscall.VERHOGEN: removed process from semq on RQ?!");
			}
			#endif
			int all_or_nothing = 0;
			all_or_nothing += (sem_q_hd->qcount == 0);
			int semvec_all_enull = 1;
			for (i = 0; i < SEMMAX; i++)
			{
				if (sem_q_hd->semvec[i] != (int*) ENULL)
				{
					semvec_all_enull = 0;
					break;
				}
			}
			all_or_nothing += semvec_all_enull;
			if (all_or_nothing != 0 && all_or_nothing != 2)
			{
				panic("qcount and semvec async");
			}
			if (sem_q_hd->qcount == 0)
			{
				/* put onto RQ */
				insertProc(&rq_tl, sem_q_hd);
			}
			
		} else
		{
			panic("switch vp->op error");
			return 1;
		}
	}
	/* resume to user space */
	schedule();
	return 0;
}


int
notused(state_t *old)
{
	HALT();
	return 1;
}


int
trapstate(state_t *old)
{
	proc_t *caller_proc = headQueue(rq_tl);
	/* repeated SYS5 call? */
	/* partial checking */
	if (caller_proc->sys_new != (state_t*) ENULL)
	{
		/* has called SYS5 before */
		/* terminate process */
		killproc_real(caller_proc);
		return 1;
	}
	/* main */
	int trap_type = old->s_r[2];	/* d2 */
	state_t *old_area = (state_t*) old->s_r[3];	/* d3 */
	state_t *new_area = (state_t*) old->s_r[4];	/* d4 */
	switch (trap_type)
	{
	case PROGTRAP:
		caller_proc->prog_old = old_area;
		caller_proc->prog_new = new_area;
		break;
	case MMTRAP:
		caller_proc->mm_old = old_area;
		caller_proc->mm_new = new_area;
		break;
	case SYSTRAP:
		caller_proc->sys_old = old_area;
		caller_proc->sys_new = new_area;
		break;
	default:
		panic("syscall.trapstate:nonsense trap type passed");
		return 1;
		break;
	}
	return 0;
}


int
getcputime(state_t *old)
{
	proc_t *running_proc = headQueue(rq_tl);
	old->s_r[2] = (int) running_proc->cpu_time;	/* d2 = ... */
	return 0;
}


int
trapsysdefault(state_t *old)
{
	/*
	 * pass up.
	 * if SYS5'ed, then LDST sys_new
	 */
	proc_t *caller_proc = headQueue(rq_tl);
	long now;
	if (caller_proc->sys_new != (state_t*) ENULL)
	{
		/* has SYS5'ed */
		/* store to sys_old from SYSTRAP_OLDAREA in physical memory */
		*caller_proc->sys_old = *(state_t*) (BEGINTRAP + 76*4);
		 /* before pass up, set tdck, since same process continuing */
		if (caller_proc->tdck != 0L)
		{
			panic("syscall.trapsysdefault: caller proc's tdck not reset");
			return 1;
		}
		STCK(&now);
		caller_proc->tdck = now;
		/* pass up */
		LDST(caller_proc->sys_new);
	} else
	{
		/* terminate */
		killproc_real(caller_proc);
	}
	return 0;
}
