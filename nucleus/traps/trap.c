/*
 * This code is my own work, it was written without consulting code written by other students (Yiwei Zhu)
 */

#include "../../h/trap.h"
#include "../../h/traps.e"
#include "../../h/syscall.h"
#include "../../h/int.e"
#include "../../h/util.h"

/****************************************
 * private functions
 ****************************************
 */
/**
 * post-procedure for all syscalls
 */
int
post_syscall(void)
{
	/* call main.schedule() to resume back to user space */
	schedule();
	return 0;
}

void
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
	*(int*) 0x100 = (int) STLDTERM0;
	*(int*) 0x104 = (int) STLDTERM1;
	*(int*) 0x108 = (int) STLDTERM2;
	*(int*) 0x10c = (int) STLDTERM3;
	*(int*) 0x110 = (int) STLDTERM4;
	*(int*) 0x114 = (int) STLDPRINT0;
	*(int*) 0x11c = (int) STLDDISK0;
	*(int*) 0x12c = (int) STLDFLOPPY0;
	*(int*) 0x140 = (int) STLDCLOCK;
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


void static
trapsyshandler(void)
{
	/* 9 traps */
	/* first things first, update caller process cpu time */
	long now;
	STCK(&now);
	proc_t *caller_proc = headQueue(rq_tl);
	caller_proc->cpu_time += now - caller_proc->tdck;
	caller_proc->tdck = 0;
	/* SYS: 0x930 */
	state_t *st_old = (state_t*) 0x930;
	/* syscall number. from SP? in old save area state_t */
	int syscall_num = st_old->s_tmp.tmp_sys.sys_no;
	if (syscall_num == 1)
	{
		creatproc(st_old);
		/*
		 * instead of schedule() like others, resume to user space manually
		 */
		manual_resume(caller_proc);
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
	} else if (syscall_num == 7)
	{
		waitforpclock(st_old);
	} else if (syscall_num == 8)
	{
		waitforio(st_old);
	} else
	{
		trapsysdefault(st_old);
	}
	/* after syscall, resume to user space */
	post_syscall();
}


void
trapmmhandler(void)
{
	
}


void
trapproghandler(void)
{
	
}
