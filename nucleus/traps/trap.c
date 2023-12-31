/*
 * This code is my own work, it was written without consulting code written by other students (Yiwei Zhu)
 */

#include "../../h/trap.h"
#include "../../h/traps.e"
#include "../../h/syscall.e"
#include "../../h/int.e"
#include "../../h/util.h"

/****************************************
 * private functions
 ****************************************
 */
/**
 * post-procedure for all syscalls
 */
static int
post_traphandler(void)
{
	/* call main.schedule() to resume back to user space */
	schedule();
	return 0;
}

static void
manual_resume(proc_t *running_proc)
{
	/* update tdck */
	long now;
	STCK(&now);
	if (running_proc->tdck != 0)
	{
		panic("trap.manual_resume: updating non-0 tdck");
		return;
	}
	running_proc->tdck = now;
	/* kick off */
	/*
	 * DISCUSS:
	 * the logic might not be as wished
	 */
	if (running_proc->sys_old == (state_t*) ENULL)
	{
		LDST((state_t*) 0x930);
	} else
	{
		LDST(running_proc->sys_old);
	}
}


static void
before_trap_handler(int handler_type)
{
	/* first things first, update interrupted process cpu time */
	long now;
	STCK(&now);
	proc_t *inted_proc = headQueue(rq_tl);
	if (inted_proc == (proc_t*) ENULL)
	{
		panic("trap.before_trap_handler: empty RQ. could due to deadlock");
		return;
	}
	inted_proc->cpu_time += now - inted_proc->tdck;
	inted_proc->tdck = 0;
	/* update proc_t.p_s: state_t */
	state_t *inted_proc_st = (state_t*) ENULL;
	switch (handler_type)
	{
	case PROGTRAP:
		inted_proc_st = (state_t*) BEGINTRAP;
		break;
	case MMTRAP:
		inted_proc_st = (state_t*) (BEGINTRAP + 76*2);
		break;
	case SYSTRAP:
		inted_proc_st = (state_t*) (BEGINTRAP + 76*4);
		break;
	default:
		panic("trap.before_trap_handler:invalid trap");
		return;
	}
	inted_proc->p_s = *inted_proc_st;
}


/**
 * return sys trap old area location from `p`
 * default if no SYS5
 */
static state_t * proc_sys_old(proc_t *p)
{
	state_t *old = (state_t*) 0x930;
	if (p->sys_old != (state_t*) ENULL)
	{
		old = p->sys_old;
	}
	return old;
}


void
trapinit(void)
{
	/* load entries in EVT */
	*(int*) 0x008 = (int) STLDMM;
	*(int*) 0x00c = (int) STLDADDRESS;
	*(int*) 0x010 = (int) STLDILLEGAL;
	*(int*) 0x014 = (int) STLDZERO;
	*(int*) 0x020 = (int) STLDPRIVILEGE;
	*(int*) 0x08c = (int) STLDSYS;
	*(int*) 0x94 = (int) STLDSYS9;
	*(int*) 0x98 = (int) STLDSYS10;
	*(int*) 0x9c = (int) STLDSYS11;
	*(int*) 0xa0 = (int) STLDSYS12;
	*(int*) 0xa4 = (int) STLDSYS13;
	*(int*) 0xa8 = (int) STLDSYS14;
	*(int*) 0xac = (int) STLDSYS15;
	*(int*) 0xb0 = (int) STLDSYS16;
	*(int*) 0xb4 = (int) STLDSYS17;
	/* set new areas for traps */
	state_t st;
	int i;
	for (i = 0; i < 17; i++)
	{
		st.s_r[i] = 0;
	}
	st.s_sr.ps_t = 0;
	st.s_sr.ps_s = 1;
	st.s_sr.ps_m = 0;
	st.s_sr.ps_int = 7;	/* no interrupt? */
	st.s_sr.ps_x = 0;
	st.s_sr.ps_n = 0;
	st.s_sr.ps_z = 0;
	st.s_sr.ps_o = 0;
	st.s_sr.ps_c = 0;
	st.s_sp = MEMORY_BOUND;
	/* PROG */
	st.s_pc = (int) trapproghandler;
	*(state_t*) (0x800 + 76) = st;
	/* MM */
	st.s_pc = (int) trapmmhandler;
	*(state_t*) (0x898 + 76) = st;
	/* SYS */
	st.s_pc = (int) trapsyshandler;
	*(state_t*) (0x930 + 76) = st;
}


static void
trapsyshandler(void)
{
	before_trap_handler(SYSTRAP);
	proc_t *caller_proc = headQueue(rq_tl);
	/* 9 traps */
	/* SYS: 0x930 */
	state_t *st_old = (state_t*) 0x930;	/* might actually intended regardless of SYS5'd process */
	/* syscall number. from SP? in old save area state_t */
	int syscall_num = st_old->s_tmp.tmp_sys.sys_no;
	if (st_old->s_sr.ps_s == 0)
	{
		/* syscall from user process */
		if (syscall_num < 1 || syscall_num > 8)
		{
			/* not sys1 to 8 */
			/* pass */
		} else
		{
			/* cause privileged instruction program trap */
			/* DISCUSS: is this right? */
			if (caller_proc->prog_new != (state_t*) ENULL)
			{
				/* called SYS5 */
				caller_proc->prog_old->s_tmp.tmp_pr.pr_typ = PRIVILEGE;
				LDST(caller_proc->prog_new);
			} else
			{
				/* DISCUSS: what to do here? */
				panic("trap.trapsyshandler: guess should do something");
				return;
			}
		}
	}
	if (syscall_num == 1)
	{
		creatproc(st_old);
		/*
		 * instead of schedule() like others, resume to user space manually
		 */
		manual_resume(caller_proc);
		return;
	} else if (syscall_num == 2)
	{
		killproc(st_old);
	} else if (syscall_num == 3)
	{
		semop(st_old);
	} else if (syscall_num == 4)
	{
		notused(st_old);
	} else if (syscall_num == 5)
	{
		trapstate(st_old);
	} else if (syscall_num == 6)
	{
		getcputime(st_old);
		/*
		 * instead of schedule() like others, resume to user space manually
		 */
		manual_resume(caller_proc);
		return;
	} else if (syscall_num == 7)
	{
		waitforpclock(st_old);
	} else if (syscall_num == 8)
	{
		waitforio(st_old);
		if (caller_proc->io_res.io_sta == ENULL)
		{
			/* nothing after `waitforio` -> interrupt not happened yet => normal case */
			/* then should have been blocked and call schedule() */
			schedule();
		} else
		{
			/*
			 * has something after `waitforio`
			 * => weird case where `waitforio` pass from nucleus-saved compl_st
			 * no blocking, no (need) schedule()
			 */
			manual_resume(caller_proc);
		}
		return;
	} else
	{
		trapsysdefault(st_old);
	}
	/* after syscall, resume to user space */
	post_traphandler();
}


void
trapmmhandler(void)
{
	before_trap_handler(MMTRAP);
	proc_t *inted_proc = headQueue(rq_tl);
	long now;
	if (inted_proc->mm_old != (state_t*) ENULL)
	{
		/* called SYS5 */
		/* copy MMTRAP_OLDAREA of physical memory to this area */
		*inted_proc->mm_old = *(state_t*) (BEGINTRAP + 76*2);
		 /* before pass up, set tdck, since same process continuing */
		if (inted_proc->tdck != 0L)
		{
			panic("trap.trapmmhandler: caller proc's tdck not reset");
			return 1;
		}
		STCK(&now);
		inted_proc->tdck = now;
		LDST(inted_proc->mm_new);
	} else
	{
		/* no passup vector */
		/* terminate */
		killproc_real(inted_proc);
		post_traphandler();
	}
}


void
trapproghandler(void)
{
	before_trap_handler(PROGTRAP);
	proc_t *inted_proc = headQueue(rq_tl);
	long now;
	if (inted_proc->prog_old != (state_t*) ENULL)
	{
		/* called SYS5 */
		/* copy PROGTRAP_OLDAREA of physical memory to this area */
		*inted_proc->prog_old = *(state_t*) BEGINTRAP;
		/* before pass up, set tdck, since same process continuing */
		if (inted_proc->tdck != 0L)
		{
			panic("trap.trapproghandler: caller proc's tdck not reset");
			return 1;
		}
		STCK(&now);
		inted_proc->tdck = now;

		LDST(inted_proc->prog_new);
	} else
	{
		/* no passup vector */
		/* terminate */
		killproc_real(inted_proc);
		post_traphandler();
	}
	
}
