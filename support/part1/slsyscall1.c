#include <string.h>
#include "../../h/const.h"
#include "../../h/moreconst.h"
#include "../../h/vpop.h"
#include "../../h/slsyscall1.e"
#include "../../h/support.h"
#include "../../h/slsyscall1.h"
#include "../../h/page2.e"	/* page2.putframe */
#include "../../h/slsyscall.e"	/* common support level function */

/* ==============================
 * support level functions
 */
/**
 * same impl as in int module
 */
devreg_t *
sldev_reg_loc(int devnum)
{
	if (!((devnum >= 0) && (devnum <= 14)))
	{
		panic("slsyscall1.sldev_reg_loc: invalid devnum");
		return (devreg_t*) ENULL;
	}
	return (devreg_t*) (BEGINDEVREG + 0x10 * devnum);
}


/* ==============================
 * private functions
 */
static int
term_devnum(int termindex)
{
	return termindex;	/* happens to be identity map */
}


void
readfromterminal(int termnum)
{
	register int r2 asm("%d2");
	register int r3 asm("%d3");
	register int r4 asm("%d4");
	char *returnbuf_orig = (char*) t_shared_struct[termnum].slsys_old.s_r[3];	/* arg d3 */
	int termdevnum = term_devnum(termnum);
	devreg_t *termdevreg = sldev_reg_loc(termdevnum);
	/* use sl.termbuf for io */
	/* set up term's device registers */
	termdevreg->d_dadd = 0;
	termdevreg->d_badd = t_shared_struct[termnum].termbuf;
	termdevreg->d_stat = 0;
	termdevreg->d_op = IOREAD;		/* start i/o    */
	/* waitforio syscall */
	r4 = termdevnum;
	DO_WAITIO();
	/* woke up */
	int io_len = r2;
	int io_sta = r3;
	/* alter io_len before "return" */
	switch (io_sta)
	{
	case NORMAL:
		break;
	case ENDOFINPUT:
		io_len = -ENDOFINPUT;
		break;
	default:
		io_len = -io_sta;
		break;
	}
	/* transfer to virtual termbuf */
	bcopy(t_shared_struct[termnum].termbuf, returnbuf_orig, TERMBUF_SIZE);
	/* "return" */
	t_shared_struct[termnum].slsys_old.s_r[2] = io_len;	/* d2 */
	LDST(&t_shared_struct[termnum].slsys_old);
}


void
writetoterminal(int termnum)
{
	register r2 asm("%d2");
	register r3 asm("%d3");
	register r4 asm("%d4");
	char* writefrombuf = (char*) t_shared_struct[termnum].slsys_old.s_r[3];	/* arg d3 */
	int len = t_shared_struct[termnum].slsys_old.s_r[4];	/* arg d4 */
	if (len > UPAMOUNT)
	{
		panic("sl.writetoterminal: trolled by nonpriv tproc");
		return;
	}
	/* transfer to sl.termbuf */
	bcopy(writefrombuf, t_shared_struct[termnum].termbuf, len);
	/* ensure null-termination */
	t_shared_struct[termnum].termbuf[len] = '\0';
	int termdevnum = term_devnum(termnum);
	devreg_t *termdevreg = sldev_reg_loc(termdevnum);
	/* set up term's device registers */
	termdevreg->d_amnt = len;
	termdevreg->d_badd = t_shared_struct[termnum].termbuf;
	termdevreg->d_stat = 0;	/* reset */
	termdevreg->d_op = IOWRITE;		/* start i/o    */
	/* waitforio syscall */
	r4 = termdevnum;
	DO_WAITIO();
	/* woke up */
	int io_len = r2;
	int io_sta = r3;
	/* alter io_len before "return" */
	switch (io_sta)
	{
	case NORMAL:
		break;
	case ENDOFINPUT:
		io_len = -ENDOFINPUT;
		break;
	default:
		io_len = -io_sta;
		break;
	}
	/* "return" */
	t_shared_struct[termnum].slsys_old.s_r[2] = io_len;	/* return value d2 */
	LDST(&t_shared_struct[termnum].slsys_old);
}


void
delay(int termnum)
{
	register int r3 asm("%d3");
	register int r4 asm("%d4");
	int r4val;
	int delayperiod = t_shared_struct[termnum].slsys_old.s_r[4];	/* arg d4 */
	long now;
	STCK(&now);
	t_shared_struct[termnum].wakeup_time = now + delayperiod;
	vpop vp[1];
	vp[0].op = LOCK;
	vp[0].sem = &t_shared_struct[termnum].sem_cron;
	r4val = (int) vp;
	r4 = r4val;
	r3 = 1;
	DO_SEMOP();	/* P(sem_cron) */
	/* woken up, presumably by cron */
	/* "return" */
	LDST(&t_shared_struct[termnum].slsys_old);
}


void
gettimeofday(int termnum)
{
	long now;
	STCK(&now);
	/* "return" */
	t_shared_struct[termnum].slsys_old.s_r[2] = now;	/* arg d2 */
	LDST(&t_shared_struct[termnum].slsys_old);
}


void
terminate(int termnum)
{
	/* signal dead */
	t_shared_struct[termnum].dead = TRUE;
	/* recycle frame */
	putframe(termnum);
	DO_TERMINATEPROC();
}
