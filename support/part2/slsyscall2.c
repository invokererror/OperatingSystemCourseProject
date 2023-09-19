#include <string.h>
#include "../../h/const.h"
#include "../../h/types.h"
#include "../../h/vpop.h"
#include "../../h/support.h"
#include "../../h/support2.h"
#include "../../h/slsyscall2.h"
#include "../../h/slsyscall.e"
#include "../../h/slsyscall2.e"
#include "../../h/support2.e"

register r2 asm("%d2");
register r3 asm("%d3");
register r4 asm("%d4");
static int r4addr;

static int
cmp(const void *a, const void *b)
{
	int argt1 = *(const int*)a;
    int argt2 = *(const int*)b;
 
	if (argt1 == ENULL)
		return -1;
	if (argt2 == ENULL)
		return 1;
	switch (elevdirection)
	{
	case SCANUP:
		if (diskq[argt1].tracknum < diskq[argt2].tracknum)
			return -1;
		if (diskq[argt1].tracknum > diskq[argt2].tracknum)
			return 1;
		return 0;
		break;
	case SCANDOWN:
		if (diskq[argt1].tracknum > diskq[argt2].tracknum)
			return -1;
		if (diskq[argt1].tracknum < diskq[argt2].tracknum)
			return 1;
		return 0;
		break;
	default:
		panic("slsyscall2.cmp: fuck");
		break;
	}
	return -2;
}


void
virtualv(int semaddr)
{
	int i;
	vpop vp[2];
	
	vp[0].op = LOCK;
	vp[0].sem = &sem_virtualvp;
	r4addr = (int) vp;
	r4 = r4addr;
	r3 = 1;
	DO_SEMOP();	/* P(sem_virtualvp) */
	/* ==============================
	 * critical section: vplist
	 */
	for (i = 0; i < MAXTPROC; i++)
	{
		if (vplist[i].virt_sem == semaddr)
		{
			/* found physical sem associated */
			if (vplist[i].phys_sem >= 0)
			{
				panic("slsyscall2.virtualv: phys sem must < 0");
				return;
			}
			vp[0].op = UNLOCK;
			vp[0].sem = &vplist[i].phys_sem;
			r4addr = (int) vp;
			r4 = r4addr;
			r3 = 1;
			DO_SEMOP();	/* V physical sem */
			if (vplist[i].phys_sem == 0)
			{
				/* no longer needed */
				vplist[i].virt_sem = ENULL;
			}
			break;
		}
	}
	if (i == MAXTPROC)
	{
		/* no physical sem associated */
		(*((int*) semaddr))++;
	}
	vp[0].op = UNLOCK;
	vp[0].sem = &sem_virtualvp;
	r4addr = (int) vp;
	r4 = r4addr;
	r3 = 1;
	DO_SEMOP();	/* V(sem_virtualvp) */
	/* ============================== */
}


void
virtualp(int semaddr)
{
	int i;
	vpop vp[2];
	
	if ((*((int*)semaddr)) == 0)
	{
		/* if no physical sem, allocate one */
		vp[0].op = LOCK;
		vp[0].sem = &sem_virtualvp;
		r4addr = (int) vp;
		r4 = r4addr;
		r3 = 1;
		DO_SEMOP();	/* P(sem_virtualvp) */
		/* ==============================
		 * critical section: vplist
		 */
		for (i = 0; i < MAXTPROC; i++)
		{
			if (vplist[i].virt_sem == semaddr)
			{
				/* found physical sem associated */
				vp[0].op = UNLOCK;
				vp[0].sem = &sem_virtualvp;
				vp[1].op = LOCK;
				vp[1].sem = &vplist[i].phys_sem;
				r4addr = (int) vp;
				r4 = r4addr;
				r3 = 2;
				DO_SEMOP();	/* P(sem) and exit critical section */
				/* ============================== */
				break;
			}
		}
		if (i == MAXTPROC)
		{
			/* no physical sem associated */
			/* allocate one */
			for (i = 0; i < MAXTPROC; i++)
			{
				if (vplist[i].virt_sem == ENULL)
				{
					/* empty slot */
					vplist[i].virt_sem = semaddr;
					vplist[i].phys_sem = 0;
					break;
				}
			}
			if (i == MAXTPROC)
			{
				panic("slsyscall2.virtualp: all 5 physical sem used, no available");
				return;
			}
			/* P physical sem */
			vp[0].op = UNLOCK;
			vp[0].sem = &sem_virtualvp;
			vp[1].op = LOCK;
			vp[1].sem = &vplist[i].phys_sem;
			r4addr = (int) vp;
			r4 = r4addr;
			r3 = 2;
			DO_SEMOP();	/* P(sem) and exit critical section */
			/* ============================== */
		}
	} else if ((*((int*)semaddr)) > 0)
	{
		/* just decr */
		(*((int*)semaddr))--;
	} else
	{
		panic("slsyscall2.virtualp: passed semaddr < 0. dunno what to do");
		return;
	}
}


void
diskput(int termnum, char *virtbuf, int tracknum, int sectornum)
{
	vpop vp[1];
	/* put on diskq */
	if (diskq[termnum].tracknum != ENULL ||
		diskq[termnum].sectornum != ENULL)
	{
		panic("slsyscall2.diskput: how can already have request?");
		return;
	}
	diskq[termnum].tracknum = tracknum;
	diskq[termnum].sectornum = sectornum;
	diskq[termnum].physbuf = t_shared_struct[termnum].termbuf;
	bcopy(virtbuf, diskq[termnum].physbuf, SECTORSIZE);
	diskq[termnum].put_or_get = DISKPUT;
	/* V(sem_disk) */
	vp[0].op = UNLOCK;
	vp[0].sem = &sem_disk;
	r4addr = (int) vp;
	r4 = r4addr;
	r3 = 1;
	DO_SEMOP();	/* V(sem_disk) */
	/* P(diskwait[termnum]) */
	vp[0].op = LOCK;
	vp[0].sem = &diskwait[termnum];
	r4addr = (int) vp;
	r4 = r4addr;
	r3 = 1;
	DO_SEMOP();	/* P(diskwait[termnum]) */
	/* woke up by diskdaemon */
	/* request completed */
	if (diskq[termnum].io_sta != NORMAL)
	{
		panic("slsyscall2.diskput: diskdaemon returns abnormal IO status");
		return;
	}
	/* "return" */
	t_shared_struct[termnum].slsys_old.s_r[2] = (diskq[termnum].io_sta == NORMAL) ? SECTORSIZE : -diskq[termnum].io_sta;
	/* reset diskq[termnum] */
	diskq[termnum].tracknum = ENULL;
	diskq[termnum].sectornum = ENULL;
	LDST(&t_shared_struct[termnum].slsys_old);
}


void
diskget(int termnum, char *virtbuf, int tracknum, int sectornum)
{
	vpop vp[1];
	/* put on diskq */
	if (diskq[termnum].tracknum != ENULL ||
		diskq[termnum].sectornum != ENULL)
	{
		panic("slsyscall2.diskget: how can already have request?");
		return;
	}
	diskq[termnum].tracknum = tracknum;
	diskq[termnum].sectornum = sectornum;
	diskq[termnum].physbuf = t_shared_struct[termnum].termbuf;
	diskq[termnum].put_or_get = DISKGET;
	/* V(sem_disk) */
	vp[0].op = UNLOCK;
	vp[0].sem = &sem_disk;
	r4addr = (int) vp;
	r4 = r4addr;
	r3 = 1;
	DO_SEMOP();	/* V(sem_disk) */
	/* P(diskwait[termnum]) */
	vp[0].op = LOCK;
	vp[0].sem = &diskwait[termnum];
	r4addr = (int) vp;
	r4 = r4addr;
	r3 = 1;
	DO_SEMOP();	/* P(diskwait[termnum]) */
	/* woke up by diskdaemon */
	/* request completed */
	if (diskq[termnum].io_sta != NORMAL)
	{
		/* test code doing shit */
		/* cannot suicide, must "return" */
	}
	if (diskq[termnum].io_sta == NORMAL)
	{
		memcpy(virtbuf, t_shared_struct[termnum].termbuf, SECTORSIZE);
	}
	/* "return" */
	t_shared_struct[termnum].slsys_old.s_r[2] = (diskq[termnum].io_sta == NORMAL) ? SECTORSIZE : -diskq[termnum].io_sta;
	/* reset diskq[termnum] */
	diskq[termnum].tracknum = ENULL;
	diskq[termnum].sectornum = ENULL;
	LDST(&t_shared_struct[termnum].slsys_old);
}


void
diskdaemon(void)
{
	vpop vp[2];
	cur_track_loc = 0;
	elevdirection = SCANUP;
	/* accepted requests via SCAN alg
	 * e.g. t0,t1,t3,t4
	 */
	int requestq[MAXTPROC];
	/* sorted via distance from accepted requests
	 * e.g. term 4,1,3,0,2
	 */
	int request_index[MAXTPROC];
	int i;
	while (1)
	{
		bool_t alldead = TRUE;
		for (i = 0; i < MAXTPROC; i++)
		{
			if (!t_shared_struct[i].dead)
			{
				alldead = FALSE;
				break;
			}
		}
		if (alldead)
		{
			DO_TERMINATEPROC();
			panic("slsyscall2.diskdaemon: cannot suicide");
			return;
		}
		vp[0].op = LOCK;
		vp[0].sem = &sem_disk;
		r4addr = (int) vp;
		r4 = r4addr;
		r3 = 1;
		DO_SEMOP();	/* P(sem_disk) */
		/* filter in accepted requests */
		int tl = 0;
		for (i = 0; i < MAXTPROC; i++)
		{
			switch (elevdirection)
			{
			case SCANUP:
				if (diskq[i].tracknum >= cur_track_loc)
				{
					/* accept term i */
					requestq[tl] = i;
					tl++;
				}
				break;
			case SCANDOWN:
				if (diskq[i].tracknum <= cur_track_loc && diskq[i].tracknum != ENULL)
				{
					/* accept term i */
					requestq[tl] = i;
					tl++;
				}
				break;
			default:
				panic("slsyscall2.diskdaemon: fuck");
				return;
			}
		}
		/* remaining ENULL */
		for (; tl < MAXTPROC; tl++)
		{
			requestq[tl] = ENULL;
		}
		/* sort accepted requests */
		memcpy(request_index, requestq, sizeof(int)*MAXTPROC);
		qsort(request_index, MAXTPROC, sizeof(int), cmp);
		/* perform request */
		bool_t performed_request = FALSE;
		int request_performed_counter = 0;
		for (i = 0; i < MAXTPROC; i++)
		{
			int io_sta, io_len;
			if (request_index[i] == ENULL)
				continue;	/* placeholder */
			performed_request = TRUE;
			const int term = request_index[i];
			devreg_t *disk0 = sldev_reg_loc(DISK0);
			if (cur_track_loc != diskq[term].tracknum)
			{
				/* need to IOSEEK */
				disk0->d_track = diskq[term].tracknum;
				disk0->d_op = IOSEEK;
				r4 = DISK0;
				DO_WAITIO();
				/* woke up */
				io_sta = r3;
				if (io_sta != NORMAL)
				{
					/* test code doing shit */
					/* skip I/O */
				}
			}
			if (io_sta == NORMAL)
			{
				disk0->d_sect = diskq[term].sectornum;
				disk0->d_badd = diskq[term].physbuf;
				disk0->d_op = (diskq[term].put_or_get == DISKPUT) ? IOWRITE : IOREAD;
				r4 = DISK0;
				DO_WAITIO();
				/* woke up */
				io_len = r2;
				io_sta = r3;
				if (io_sta != NORMAL)
				{
					panic("slsyscall2.disdaemon: abnormal disk0 IOREAD/WRITE");
					return;
				}
			}
			/* return dev status to `diskwait[]` */
			diskq[term].io_sta = io_sta;
			diskq[term].io_len = io_len;
			request_performed_counter++;
			/* optional more P(sem_disk) for multi-requests */
			if (request_performed_counter > 1)
			{
				if (sem_disk <= 0)
				{
					panic("slsyscall2.diskdaemon: multi-request performed yet not enough sem_disk");
					return;
				}
				/* P(sem) where sem > 0 <=> sem-- */
				sem_disk--;
			}
			/* release term */
			vp[0].op = UNLOCK;
			vp[0].sem = &diskwait[term];
			r4addr = (int) vp;
			r4 = r4addr;
			r3 = 1;
			DO_SEMOP();	/* V(diskwait[term]) */
			/* next loop */
			if (diskq[term].tracknum >= 0 && diskq[term].tracknum <= UPTRACK)
			{
				/* only update if legal I/O */
				cur_track_loc = diskq[term].tracknum;
			}
		}
		/* next loop */
		if (!performed_request)
		{
			/* could be that disk head at extreme end of track */
			/* turn around */
			elevdirection = (elevdirection == SCANUP) ? SCANDOWN : SCANUP;
			/* no request fulfilled => yield back `sem_disk` from P(sem_disk) at beginning */
			sem_disk++;
		}
	}
}
