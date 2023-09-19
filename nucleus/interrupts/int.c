/*
 * Honor code
 * This code is my own work, it was written without consulting code written by other students (Yiwei Zhu)
 */

#include "../../h/const.h"
#include "../../h/moreconst.h"
#include "../../h/types.h"
#include "../../h/vpop.h"
#include "../../h/util.h"
#include "../../h/procq.e"
#include "../../h/asl.e"
#include "../../h/traps.e"
#include "../../h/int.e"
#include "../../h/int.h"


/* === utility functions === */
/**
 * Returns pointer to device register in memory
 */
devreg_t * dev_reg_loc(int devnum)
{
	return (devreg_t*) (BEGINDEVREG + 0x10 * devnum);
}


static int dev_type(int devnum)
{
	if (0 <= devnum && devnum <= 4)
	{
		/* terminal */
		return TERMINAL;
	}
	if (5 <= devnum && devnum <= 6)
	{
		return PRINTER;
	}
	if (7 <= devnum && devnum <= 10)
	{
		return DISK;
	}
	if (11 <= devnum && devnum <= 14)
	{
		return FLOPPY;
	}
	panic("int.dev_type: invalid devnum");
	return -1;
}


static int
abs_dev_num(int devtype, int devnum)
{
	switch (devtype)
	{
	case TERMINAL:
		return devnum + TERM0;
		break;
	case PRINTER:
		return devnum + PRINT0;
		break;
	case DISK:
		return devnum + DISK0;
		break;
	case FLOPPY:
		return devnum + FLOPPY0;
		break;
	default:
		panic("int.abs_dev_num: improper devnum");
		return -1;
	}
}


static state_t * interrupt_vector_area(int devtype)
{
	switch (devtype)
	{
	case TERMINAL:
		return (state_t*) BEGININTR;
		break;
	case PRINTER:
		return (state_t*) 0xa60;
		break;
	case DISK:
		return (state_t*) 0xaf8;
		break;
	case FLOPPY:
		return (state_t*) 0xb90;
		break;
	default:
		panic("int.interrupt_vector_area: improper devtype");
		return (state_t*) ENULL;
	}
}


/**
 * now is a pointer to a stack-frame (auto) variable that is as accurate as possible
 */
static void
update_cputime(proc_t *p, long *now)
{
	if (p->tdck == 0L)
	{
		panic("int.update_cputime: nonsensical use");
		return;
	}
	if (*now < p->tdck)
	{
		panic("int.update_cputime: do not think this could happen");
		return;
	}
	p->cpu_time += *now - p->tdck;
	p->tdck = 0;
}


/**
 * nucleus-module function. used by trap.main (and possibly others)
 * assumption: passed completion status in process does not contain stale value. e.g. inted_p->io_res.io_len should be fresh, and -1 means no meaning for length register
 */
void waitforio_write_to_proct(proc_t *p)
{
	/* write the "returned" value to its p_s */
	if (p->io_res.io_sta == ENULL)
	{
		panic("int.waitforio_write_to_proct: precondition failed");
		return;
	}
	p->p_s.s_r[3] = p->io_res.io_sta;
	p->p_s.s_r[2] = p->io_res.io_len;
}


static void
before_int_handler(int interrupt_type)
{
	/* first things first, update interrupted process cpu time */
	long now;
	STCK(&now);
	proc_t *inted_proc = headQueue(rq_tl);
	if (inted_proc != (proc_t*) ENULL)	/* interrupt could happen with empty RQ */
	{
		update_cputime(inted_proc, &now);
		/* update proc_t.p_s: state_t */
		switch (interrupt_type)
		{
			state_t *inted_proc_st = (state_t*) ENULL;
		case CLOCK:
			inted_proc_st = (state_t*) 0xc28;
			inted_proc->p_s = *inted_proc_st;
			break;
		case TERMINAL:
		case PRINTER:
		case DISK:
		case FLOPPY:
			inted_proc_st = interrupt_vector_area(interrupt_type);
			inted_proc->p_s = *inted_proc_st;
			break;
		default:
			panic("int.before_int_handler: invalid interrupt type");
			return;
		}
	}
}


/**
 * precondition: RQ head is not empty, such that old interrupt area stored by STLD is sensical
 */
static void
manual_resume_from_interrupt(int devtype)
{
	state_t *old = interrupt_vector_area(devtype);
	proc_t *inted_p = headQueue(rq_tl);
	if (inted_p == (proc_t*) ENULL)
	{
		panic("int.manual_resume: precondition failed: empty RQ");
		return;
	}
	/* old area stores `inted_p` state just before interrupt */
	/* update proc_t.tdck */
	if (inted_p->tdck != 0L)
	{
		panic("int.manual_resume: updating non-0 tdck");
		return;
	}
	long now;
	STCK(&now);
	inted_p->tdck = now;
	/* kick off */
	LDST(old);
}


void
intinit(void)
{
	/* EVT */
	*(int*) 0x100 = (int) STLDTERM0;
	*(int*) 0x104 = (int) STLDTERM1;
	*(int*) 0x108 = (int) STLDTERM2;
	*(int*) 0x10c = (int) STLDTERM3;
	*(int*) 0x110 = (int) STLDTERM4;
	*(int*) 0x114 = (int) STLDPRINT0;
	*(int*) 0x11c = (int) STLDDISK0;
	*(int*) 0x12c = (int) STLDFLOPPY0;
	*(int*) 0x140 = (int) STLDCLOCK;
	/* new area */
	state_t st;
	int i;
	for (i = 0; i < 17; i++)
	{
		st.s_r[i] = 0;
	}
	st.s_sr.ps_t = 0;
	st.s_sr.ps_s = 1;	/* supervisor mode */
	st.s_sr.ps_m = 0;
	st.s_sr.ps_int = 7;	/* still uninterruptable */
	st.s_sr.ps_x = 0;
	st.s_sr.ps_n = 0;
	st.s_sr.ps_z = 0;
	st.s_sr.ps_o = 0;
	st.s_sr.ps_c = 0;
	st.s_sp = MEMORY_BOUND;
	/* TERMINAL */
	st.s_pc = (int) intterminalhandler;
	*(state_t*) (BEGININTR + 76) = st;
	/* PRINTER */
	st.s_pc = (int) intprinterhandler;
	*(state_t*) (BEGININTR + 76*3) = st;
	/* DISK */
	st.s_pc = (int) intdiskhandler;
	*(state_t*) (BEGININTR + 76*5) = st;
	/* FLOPPY */
	st.s_pc = (int) intfloppyhandler;
	*(state_t*) (BEGININTR + 76*7) = st;
	/* CLOCK */
	st.s_pc = (int) intclockhandler;
	*(state_t*) (BEGININTR + 76*9) = st;
	/* other module variable init */
	pclock = 0;	/* count from 0 to 100_000, with some error */
	pclocksem = 0;
	for (i = 0; i < 15; i++)
	{
		dev_compl_st[i].io_len = ENULL;
		dev_compl_st[i].io_sta = ENULL;
	}
	for (i = 0; i < 15; i++)
	{
		waitforio_signal_received[i] = FALSE;
	}
}


void
waitforpclock(state_t *old)
{
	intsemop(&pclocksem, LOCK);
}


void
waitforio(state_t *old)
{
	/* fetch dev reg from old area */
	/* device num. The device ordered by trapped process to run */
	int devnum = old->s_r[4];	/* from d4 */
	int devtype = dev_type(devnum);
	proc_t *inted_p = headQueue(rq_tl);
	/* unsigned dev_type = inted_p->p_s.s_tmp.tmp_int.in_dev; */
	/* unsigned dev_num = inted_p->p_s.s_tmp.tmp_int.in_dno; */
	/* assumption: process executing this function not interruptable => `old` remain intact */
	if (dev_compl_st[devnum].io_sta != ENULL)
	{
		/* nucleus stored something => interrupt occurred before SYS8 */
		(devsem[devnum])--;
		/* pass nucleus-stored completion status to process */
		inted_p->io_res.io_sta = dev_compl_st[devnum].io_sta;
		inted_p->io_res.io_len = (devtype == TERMINAL || devtype == PRINTER)
			? dev_compl_st[devnum].io_len	/* also length register */
			: ENULL;	/* meaningless */
		/* saved completion status passed to process -> the info becomes obsolete */
		/* reset ASAP */
		dev_compl_st[devnum].io_sta = ENULL;
		dev_compl_st[devnum].io_len = ENULL;
		/* write the "returned" value to process's p_s */
		waitforio_write_to_proct(inted_p);
		/*
		 * DISCUSS: do not see the point of keeping io_res inside process
		 * already, old area has the "returned" info
		 */
		inted_p->io_res.io_sta = ENULL;
		inted_p->io_res.io_len = ENULL;
	} else
	{
		/* interrupt not yet occur. normal case */
		/* signal waitforio */
		waitforio_signal_received[devnum] = TRUE;
		if (devsem[devnum] > 0)
		{
			panic("int.waitforio: P on device semaphore will not block the process");
			return;
		}
		intsemop(&devsem[devnum], LOCK);	/* be blocked */
	}
}


/**
 * This function is called when the RQ is empty.
 */
void
intdeadlock(void)
{
	if (headASL() == FALSE)
	{
		/* no process blocked */
		/*
		 * DISCUSS: phase 3 print issues SYS8 which seems nonsense
		 */
		/* print("nucleus: normal termination"); */
		panic("nucleus: normal termination");
/*
  asm("trap #4");
*/
		HALT();
		return;
	}
	/* some process blocked */
	if (headBlocked(&pclocksem) != (proc_t*) ENULL)
	{
		/* blocked by pclock */
		intschedule();
		sleep();
		return;
	}
	int i;
	for (i = 0; i < 15; i++)
	{
		if (headBlocked(&devsem[i]) != (proc_t*) ENULL)
		{
			/* blocked by devnum i */
			sleep();
			return;
		}
	}
	/* blocked, but neither pclock nor I/O */
	/*
	 * per spec it says "print" deadlock message, but print in phase 3 implemented as issuing SYS8, resulting in no error message
	 */
	/* print("nucleus: deadlock"); */
	panic("nucleus: deadlock");
	HALT();
	return;
}


void
intschedule(void)
{
	long quantum = 5000L;
	LDIT(&quantum);
}


static void
intterminalhandler(void)
{
	state_t *old = (state_t*) BEGININTR;
	int devtype = old->s_tmp.tmp_int.in_dev;
	int devnum = old->s_tmp.tmp_int.in_dno;
	inthandler(devtype, devnum);
}


static void
intprinterhandler(void)
{
	state_t *old = (state_t*) 0xa60;
	/* nonsense, devtype received is 0 where should be 1 */
	/* int devtype = old->s_tmp.tmp_int.in_dev; */
	int devnum = old->s_tmp.tmp_int.in_dno;
	/* /\* get devnum from d4 *\/ */
	/* int devnum = old->s_r[4]; */
	/* int devtype = dev_type(devnum); */
	/* craft my own devtype */
	int devtype = PRINTER;
	inthandler(devtype, devnum);
}


static void
intdiskhandler(void)
{
	state_t *old = (state_t*) 0xaf8;
/* int devtype = old->s_tmp.tmp_int.in_dev; */
	int devtype = DISK;
	int devnum = old->s_tmp.tmp_int.in_dno;
	inthandler(devtype, devnum);
}


static void
intfloppyhandler(void)
{
	state_t *old = (state_t*) 0xb90;
	/* int devtype = old->s_tmp.tmp_int.in_dev; */
	int devtype = FLOPPY;
	int devnum = old->s_tmp.tmp_int.in_dno;
	inthandler(devtype, devnum);
}


static void
inthandler(int devtype, int devnum)
{
	before_int_handler(devtype);
	bool_t to_call_schedule = (headQueue(rq_tl) == (proc_t*) ENULL ? TRUE : FALSE);
	/* get absolute device number */
	int devnum_abs = abs_dev_num(devtype, devnum);
	devreg_t *dr = dev_reg_loc(devnum_abs);
	if (waitforio_signal_received[devnum_abs] == TRUE)
	{
		/* normal case: interrupt happen after SYS8 */
		/* then the waiting process must be head of devsem queue */
		if (devsem[devnum_abs] >= 0)
		{
			/* char msgbuf[50]; */
			/* sprintf(msgbuf, "int.inthandler: normal case but devnum %d sem value nonnegative", devnum_abs); */
			/* panic(msgbuf); */
			panic("???");
			return;
		}
		/* determining waiting proc **before** intsemop, otherwise lose track of waiting proc */
		proc_t *waiting_p = headBlocked(&devsem[devnum_abs]);
		if (waiting_p == (proc_t*) ENULL)
		{
			panic("int.inthandler: normal case but no process blocked by dev sem!");
			return;
		}
		/* V(devsem) */
		intsemop(&devsem[devnum_abs], UNLOCK);	/* unblock but not necessarily put on RQ */
		/* assumption: devreg in-memory is fresh */
		/* pass completion status directly to process, no need to save */
		if (waiting_p->io_res.io_sta != ENULL)
		{
			panic("int.inthandler: non-ENULL io_sta inside process before passing");
			return;
		}
		waiting_p->io_res.io_sta = dr->d_stat;
		waiting_p->io_res.io_len = (devtype == TERMINAL || devtype == PRINTER) ? dr->d_amnt : ENULL;
		/* reset */
		waitforio_signal_received[devnum_abs] = FALSE;
	} else
	{
		/* weird case: interrupt faster than SYS8 */
		/* incr devsem[devnum] to maintain seemingly normal sem value
		 * but i did not use sem value to decide whether normal case or weird case
		 */
		(devsem[devnum_abs])++;
		/* nucleus, save the completion status */
		dev_compl_st[devnum_abs].io_sta = dr->d_stat;
		dev_compl_st[devnum_abs].io_len = (devtype == TERMINAL || devtype == PRINTER) ? dr->d_amnt : ENULL;
		/* weird case, must have some process in RQ => definitely not call schedule() */
		if (headQueue(rq_tl) == (proc_t*) ENULL)
		{
			panic("int.inthandler: weird case assumption wrong");
			return;
		}
	}
	/* either case, RQ should not be empty */
	if (headQueue(rq_tl) == (proc_t*) ENULL)
	{
		panic("int.inthandler: empty RQ in some case");
	}
	/* resume to user space */
	if (to_call_schedule)
	{
		schedule();
	} else
	{
		manual_resume_from_interrupt(devtype);
	}
}


static void
intclockhandler(void)
{
	long now;
	STCK(&now);
	before_int_handler(CLOCK);
	if (rq_tl.next != (proc_t*) ENULL)
	{
		/* round robin */
		proc_t *timesup = removeProc(&rq_tl);
		insertProc(&rq_tl, timesup);
	}
	if (now - pclock >= CLOCKINTERVAL)
	{
		/* update pclock */
		if (pclocksem < 0)
		{
			/* some process blocked by pclock */
			intsemop(&pclocksem, UNLOCK);
		}
		/* tick */
		/* reset pclock */
		pclock = now;
	}
	schedule();
}


static void
intsemop(int *semAdd, int semop)
{
	switch (semop)
	{
	case LOCK:
		(*semAdd)--;
		if (*semAdd < 0)
		{
			/* block */
			proc_t *inted_p = headQueue(rq_tl);	/* this function called as proxy from nucleus => still true */
			if (inted_p == (proc_t*) ENULL)
			{
				panic("int.intsemop: empty RQ");
				return;
			}
			if (removeProc(&rq_tl) != inted_p)
			{
				panic("int.intsemop: assumption that inted_p is head of RQ is wrong??");
				return;
			}
			insertBlocked(semAdd, inted_p);
		}
		break;
	case UNLOCK:
		(*semAdd)++;
		proc_t *maybe_blocked;
		if ((maybe_blocked = removeBlocked(semAdd)) != (proc_t*) ENULL)
		{
			/* indeed some process blocked */
			if (maybe_blocked->qcount == 0)
			{
				/* no other sem blocking */
				/* put on RQ */
				insertProc(&rq_tl, maybe_blocked);
			}
		}
		break;
	default:
		panic("int.intsemop: invalid semop");
		return;
	}
}


/**
 * This function is called when the RQ is empty.
 */
static void
sleep(void)
{
	if (rq_tl.next != (proc_t*) ENULL)
	{
		panic("int.sleep: precondition unsatisfied");
		return;
	}
	asm("stop #0x2000");
}
