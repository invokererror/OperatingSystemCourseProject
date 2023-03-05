/*
 * This code is my own work, it was written without consulting code written by other students (Yiwei Zhu)
 */
#include "../../h/main.h"
#include "../../h/main.e"
#include "../../h/traps.e"
#include "../../h/traps.h"
#include "../../h/procq.h"
#include "../../h/procq.e"


/* nucleus stack size is 2 * PAGESIZE */
#define NUCLEUS_STACK_SIZE_PAGES 2
/* global machine registers */
/*
 * interesting. Ken Mandelberg suggests against this.
 */
register int d2 asm("%sp");

void
main(void)
{
	/* immediately STST */
	STST((state_t*) BEGINTRAP);
	state_t p1_st;
	/* copy later */
	p1_st = *(state_t*) BEGINTRAP;
	/* calls init() */
	init();

	/* set up processor state for p1() */
	/* all other registers same as nucleus */
	p1_st.s_pc = (int) p1;
	/* set `sp` (a7), immediately above nucleus */
	p1_st.s_sp = MEMORY_BOUND - NUCLEUS_STACK_SIZE_PAGES * PAGESIZE;
	/* turn off interrupt */
	p1_st.s_sr.ps_int = 7;
	/* set supervisor mode */
	p1_st.s_sr.ps_s = 1;
	/* set mm off */
	p1_st.s_sr.ps_m = 0;
	
	/* craft p1 proc_t */
	proc_t *p1_proc = allocProc();
	int i;
	for (i = 0; i < SEMMAX; i++)
	{
		p1_proc->p_link[i].index = ENULL;
		p1_proc->p_link[i].next = (proc_t*) ENULL;
	}
	p1_proc->p_s = p1_st;
	p1_proc->qcount = 0;
	for (i = 0; i < SEMMAX; i++)
	{
		p1_proc->semvec[i] = (int*) ENULL;
	}
	/* phase 2 stuff */
	p1_proc->parent = (proc_t*) ENULL;
	p1_proc->sibling = (proc_t*) ENULL;
	p1_proc->child = (proc_t*) ENULL;
	p1_proc->cpu_time = 0;
	p1_proc->tdck = 0;
	p1_proc->prog_old = (state_t*) BEGINTRAP;
	p1_proc->prog_new = (state_t*) (BEGINTRAP + 76);
	p1_proc->mm_old = (state_t*) (BEGINTRAP + 76*2);
	p1_proc->mm_new = (state_t*) (BEGINTRAP + 76*3);
	p1_proc->sys_old = (state_t*) (BEGINTRAP + 76*4);
	p1_proc->sys_new = (state_t*) (BEGINTRAP + 76*5);
	/* add p1() to RQ */
	insertProc(&rq_tl, p1_proc);
	/* calls schedule() */
	schedule();
}

void
static init(void)
{
	/* determines physical memory bound */
	MEMORY_BOUND = d2;
	rq_tl.index = ENULL;
	rq_tl.next = (proc_t*) ENULL;
	/* init routines */
	initProc();
	initSemd();
	trapinit();
	intinit();
}

void
schedule(void)
{
	if (rq_tl.next == (proc_t*) ENULL)
	{
		/* RQ is empty */
		intdeadlock();
	}
	intschedule();
	/* load process state at head of RQ */
	proc_t *first_proc = headQueue(rq_tl);
	/* update proc_t->tdck */
	if (first_proc->tdck != 0)
	{
		panic("main.schedule: forgot to reset tdck!");
		return;
	}
	long now;
	STCK(&now);
	first_proc->tdck = now;
	LDST(&first_proc->p_s);
}
