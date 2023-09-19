#include <string.h>
#include "../../h/const.h"
#include "../../h/types.h"
#include "../../h/vpop.h"

#include "../../h/traps.e"
#include "../../h/page2.h"
#include "../../h/page2.e"
#include "../../h/support.h"
#include "../../h/moretypes.h"
#include "../../h/slsyscall.e"
#include "../../h/slsyscall2.e"
#include "../../h/support2.e"

/* DIRTY */
/* #define MAXFRAMES 20 */
#define MAXFRAMES 10	/* hoca */

register int r2 asm("%d2");
register int r3 asm("%d3");
register int r4 asm("%d4");
static int r4addr;

extern int end(), sem_mm, p_alive;
/* extern int startd0(), endd0(), startd1(), edata(), startb0(), endb0(), startb1(); */
/* static char buf[2048]; */

/* int sem_pf=1; */
static int sem_pf=1;	/* hoca */
int pf_ctr, pf_start;
int Tsysframe[5];
int Tmmframe[5];
int Scronframe, Spagedframe, Sdiskframe;
int Tsysstack[5];
int Tmmstack[5];
int Scronstack, Spagedstack, Sdiskstack;
/*
 * new from hoca final phase
 */
/* QUESTION
 * mutex?
 */
static int sem_floppy;
/*
 * DISCUSS
 * might be true
 */
static frameinfo_t frameinfo[MAXFRAMES];
/* other */
/* free frame list
 * ENULL as sentinel value for nonavailable
 */
static int freeframes[MAXFRAMES];
/* ============================== */
/**
 * DIRTY
 */
/* static void */
/* pbd(void) */
/* { */
/* 	int ptr = 0;	/\* next starting point *\/ */
/* 	ptr += sprintf(buf, "===nucleus data===\n"); */
/* 	ptr += sprintf(buf+ptr, "startd0: 0x%x\n", (int) startd0); */
/* 	ptr += sprintf(buf+ptr, "endd0: 0x%x\n",  (int) endd0); */
/* 	ptr += sprintf(buf+ptr, "===sl data===\n"); */
/* 	ptr += sprintf(buf+ptr, "startd1: 0x%x\n",  (int) startd1); */
/* 	ptr += sprintf(buf+ptr, "edata: 0x%x\n",  (int) edata); */
/* 	ptr += sprintf(buf+ptr, "===nucleus BSS===\n"); */
/* 	ptr += sprintf(buf+ptr, "startb0: 0x%x\n",  (int) startb0); */
/* 	ptr += sprintf(buf+ptr, "endb0: 0x%x\n",  (int) endb0); */
/* 	ptr += sprintf(buf+ptr, "===sl BSS===\n"); */
/* 	ptr += sprintf(buf+ptr, "startb1: 0x%x\n",  (int) startb1); */
/* 	ptr += sprintf(buf+ptr, "end: 0x%x\n",  (int) end); */
/* } */
/* ============================== */


/* ==============================
 * utility
 */
static int
frame_list_pop(int freeframes[], int len)
{
	int res;
	if (freeframes[0] != ENULL)
	{
		/* valid free frame number */
		if (!(freeframes[0] >= pf_start && freeframes[0] < pf_start + MAXFRAMES))
		{
			panic("page2.c:frame_list_pop: invalid free frame num");
			return;
		}
		res = freeframes[0];
		memmove(freeframes, &freeframes[1], sizeof freeframes[0] * (len-1));
		/* new tail of `freeframes` always ENULL */
		freeframes[len-1] = ENULL;
		return res;
	}
	/* no valid page frame */
	panic("page2.frame_list_pop: no free frame available");
	return ENULL;
}


static int
frame_list_push(int item, int freeframes[], int len)
{
	/* find tail */
	int i;
	for (i = 0; i < len; i++)
	{
		if (freeframes[i] == ENULL)
			break;
	}
	if (i == len)
	{
		panic("page2.frame_list_push: cannot find free frame list's tail");
		return 1;
	}
	freeframes[i] = item;
	return 0;
}


static tracksectuple_t
desect_tracsec(int tracksecnum)
{
	tracksectuple_t res;
	res.tracknum = tracksecnum >> 3;
	res.sectornum = tracksecnum & 7;
	return res;
}


static int
encapsulate_tracsec(tracksectuple_t tracsec)
{
	return (tracsec.tracknum << 3) | tracsec.sectornum;
}


/**
 * only applicable to floppy0!
 */
static void
incr_tracsec(tracksectuple_t *tracsec)
{
	if (tracsec->sectornum < UPSECTOR_FLOPPY)
	{
		(tracsec->sectornum)++;
	} else
	{
		tracsec->sectornum = 0;
		if (tracsec->tracknum < UPTRACK_FLOPPY)
		{
			(tracsec->tracknum)++;
		} else
		{
			panic("page2.incr_tracsec: exhausted floppy0!");
			return;
		}
	}
}


void
pageinit(void)
{
	int r4addr;
  int endframe;

  /* check if you have space for 35 page frames, the system
     has 128K */
  endframe=(int)end / PAGESIZE;
  /* if (endframe > 220 ) { /\* 110 K *\/ */
  /* pbd(); */
  if (endframe
	  + 1	/* one blank page */
	  + MAXTPROC	/* Tsysstack[] */
	  + MAXTPROC	/* Tmmstack[] */
	  + 1	/* Scronstack */
	  + 1	/* Spagedstack */
	  + 1	/* Sdiskstack */
	  + MAXFRAMES	/* 10 free frames */
	  + 2	/* 2 nucleus stacks */
	  > 255 ) { /* 256 page frames */
    HALT();
  }

  Tsysframe[0] = endframe + 2;
  Tsysframe[1] = endframe + 3;
  Tsysframe[2] = endframe + 4;
  Tsysframe[3] = endframe + 5;
  Tsysframe[4] = endframe + 6;
  Tmmframe[0]  = endframe + 7;
  Tmmframe[1]  = endframe + 8;
  Tmmframe[2]  = endframe + 9;
  Scronframe   = endframe + 10;
  Tmmframe[3]  = endframe + 11;
  Spagedframe  = endframe + 12;
  Tmmframe[4]  = endframe + 13;
  Sdiskframe   = endframe + 14;

  Tsysstack[0] = (endframe + 3)*512 - 2;
  Tsysstack[1] = (endframe + 4)*512 - 2;
  Tsysstack[2] = (endframe + 5)*512 - 2;
  Tsysstack[3] = (endframe + 6)*512 - 2;
  Tsysstack[4] = (endframe + 7)*512 - 2;
  Tmmstack[0]  = (endframe + 8)*512 - 2;
  Tmmstack[1]  = (endframe + 9)*512 - 2;
  Tmmstack[2]  = (endframe + 10)*512 - 2;
  Scronstack   = (endframe + 11)*512 - 2;
  Tmmstack[3]  = (endframe + 12)*512 - 2;
  Spagedstack  = (endframe + 13)*512 - 2;
  Tmmstack[4]  = (endframe + 14)*512 - 2;
  Sdiskstack   = (endframe + 15)*512 - 2;
  pf_start    = (endframe + 17);
 
/*
  pf_start = (int)end / PAGESIZE + 1; 
*/

  sem_pf = MAXFRAMES;
  pf_ctr = 0;
  /*
   * new
   */
  /* other vars to init */
  sem_floppy = 1;	/* mutex */
  sem_mm = 1;	/* mutex */
  last_clock_hand = 0;
  freesector_counter.tracknum = 0;
  freesector_counter.sectornum = 0;
  int i;
  for (i = 0; i < MAXFRAMES; i++)
  {
	  frameinfo[i].is_present = FALSE;
	  frameinfo[i].termnum = ENULL;
	  frameinfo[i].pagenum = ENULL;
	  frameinfo[i].segnum = ENULL;
	  frameinfo[i].tracknum = ENULL;
	  frameinfo[i].sectornum = ENULL;
  }
  for (i = 0; i < MAXFRAMES; i++)
  {
	  freeframes[i] = pf_start + i;
  }

  state_t daemon_st;
  /* create pagedaemon */
  daemon_st.s_sp = Spagedstack;
  daemon_st.s_pc = (int) pagedaemon;
  daemon_st.s_sr.ps_t = 0;
  daemon_st.s_sr.ps_s = 1;	/* priv */
  daemon_st.s_sr.ps_m = 1;	/* mm on */
  daemon_st.s_sr.ps_int = 0;	/* interruptable */
  daemon_st.s_sr.ps_x = 0;
  daemon_st.s_sr.ps_n = 0;
  daemon_st.s_sr.ps_z = 0;
  daemon_st.s_sr.ps_o = 0;
  daemon_st.s_sr.ps_c = 0;
  daemon_st.s_crp = segtable_paged;
  r4addr = (int) &daemon_st;
  r4 = r4addr;
  DO_CREATEPROC();
  if (r2 != 0)
  {
	  panic("page2.c.pageinit: nucleus error on SYS1");
	  return;
  }
  /* DIRTY */
  /* create diskdaemon */
  daemon_st.s_sp = Sdiskstack;
  daemon_st.s_pc = (int) diskdaemon;
  daemon_st.s_sr.ps_t = 0;
  daemon_st.s_sr.ps_s = 1;	/* priv */
  daemon_st.s_sr.ps_m = 1;	/* mm on */
  daemon_st.s_sr.ps_int = 0;	/* interruptable */
  daemon_st.s_sr.ps_x = 0;
  daemon_st.s_sr.ps_n = 0;
  daemon_st.s_sr.ps_z = 0;
  daemon_st.s_sr.ps_o = 0;
  daemon_st.s_sr.ps_c = 0;
  daemon_st.s_crp = segtable_disk;
  r4addr = (int) &daemon_st;
  r4 = r4addr;
  DO_CREATEPROC();
  if (r2 != 0)
  {
	  panic("page2.c.pageinit: nucleus error on SYS1");
	  return;
  }
}


/**
 * required whether virgin page or invalid page
 * does not call `getfreesector`
 * already in `sem_mm`
 */
int
getfreeframe(int procnum, int page, int seg)
{
  vpop vpopget[2];

  vpopget[0].op=LOCK;
  vpopget[0].sem=&sem_pf;
  if (sem_pf <= 0)
  {
	  /* in critical section, but about to be blocked
	   * V(sem_mm)
	   */
	  vpopget[1].op = UNLOCK;
	  vpopget[1].sem = &sem_mm;
	  r3 = 2;
	  r4 = (int) vpopget;
	  DO_SEMOP();
	  /* woke up */
	  /* need to reacquire `sem_mm` */
	  vpopget[0].op = LOCK;
	  vpopget[0].sem = &sem_mm;
	  r4addr = (int) vpopget;
	  r4 = r4addr;
	  r3 = 1;
	  DO_SEMOP();	/* P(sem_mm) */
  } else
  {
	r3=1;
	r4 = (int)vpopget;
	DO_SEMOP(); /* P(sem_pf) */
  }

  /* take free frame off `freeframes` */
  int freeframe_num = frame_list_pop(freeframes, MAXFRAMES);
  /* update frameinfo[] */
  if (!(freeframe_num >= pf_start && freeframe_num < pf_start + MAXFRAMES))
  {
	  panic("page2.getfreeframe: nonsense free frame num");
	  return ENULL;
  }
  frameinfo[freeframe_num-pf_start].is_present = TRUE;
  frameinfo[freeframe_num-pf_start].termnum = procnum;
  frameinfo[freeframe_num-pf_start].pagenum = page;
  frameinfo[freeframe_num-pf_start].segnum = seg;
  /* getfreeframe() does not store track/sector num. setting to invalid track/sector num in case of virgin page
   * rely on pagein() to recover those 2 fields in case of invalid page
   */
  frameinfo[freeframe_num-pf_start].tracknum = ENULL;
  frameinfo[freeframe_num-pf_start].sectornum = ENULL;
 
  return (freeframe_num);
} 


/**
 * already confirms invalid page
 * already in `sem_mm`
 */ 
void
pagein(int term, int page, int seg, int pf)
{
	vpop vp[1];
	if (!(term >= 0 && term < MAXTPROC) ||
		!(page >= 0 && page <= 31) ||
		!(seg >= 1 && seg <= 2) ||
		!(pf >= 0 && pf <= 255 && pf >= pf_start && pf <= 255-2))
	{
		panic("page2.pagein: invalid arg");
		return;
	}

	switch (seg)
	{
		tracksectuple_t tracsec;
		devreg_t *devreg_floppy0;
		int io_sta;
	case 1:
		tracsec = desect_tracsec(t_shared_struct[term].pagetable_seg1[page].pd_frame);
		vp[0].op = LOCK;
		vp[0].sem = &sem_floppy;
		r4addr = (int) vp;
		r4 = r4addr;
		r3 = 1;
		DO_SEMOP();
		/* ==============================
		 * critical section: floppy
		 */
		/* get from floppy0 */
		devreg_floppy0 = sldev_reg_loc(FLOPPY0);
		devreg_floppy0->d_track = tracsec.tracknum;
		/* starts IOSEEK */
		devreg_floppy0->d_op = IOSEEK;
		r4 = FLOPPY0;
		DO_WAITIO();
		/* woke up */
		io_sta = r3;
		switch (io_sta)
		{
		case NORMAL:
			break;
		default:
			panic("page2.pagein: abnormal status from floppy0");
			return;
		}
		devreg_floppy0->d_sect = tracsec.sectornum;
		devreg_floppy0->d_badd = t_shared_struct[term].termbuf;
		/* read swapped page from floppy0 */
		devreg_floppy0->d_op = IOREAD;
		r4 = FLOPPY0;
		DO_WAITIO();
		/* woke up */
		io_sta = r3;
		switch (io_sta)
		{
		case NORMAL:
			break;
		default:
			panic("page2.pagein: abnormal status from floppy0");
			return;
		}
		vp[0].op = UNLOCK;
		vp[0].sem = &sem_floppy;
		r4addr = (int) vp;
		r4 = r4addr;
		r3 = 1;
		DO_SEMOP();
		/* ============================== */

		/* copy termbuf to page frame */
		memcpy(pf*PAGESIZE, t_shared_struct[term].termbuf, SECTORSIZE);
		/* memcpy(t_shared_struct[term].pagetable_seg0[pf], t_shared_struct[term].termbuf, SECTORSIZE); */
		/* update frameinfo[] */
		if (!(frameinfo[pf-pf_start].tracknum == ENULL && frameinfo[pf-pf_start].sectornum == ENULL))
		{
			panic("page2.pagein: frameinfo[] track/sector num supposed to be set ENULL by getfreeframe?!");
			return;
		}
		frameinfo[pf-pf_start].tracknum = tracsec.tracknum;
		frameinfo[pf-pf_start].sectornum = tracsec.sectornum;
		break;
	case 2:
		/* already in sem_mm */
		if (!(pagetable_tseg2[page].pd_p == 0 && pagetable_tseg2[page].pd_r == 0))
		{
			panic("page2.pagein: really?");
			return;
		}
		tracsec = desect_tracsec(pagetable_tseg2[page].pd_frame);
		/* must not hold sem_mm while doing IO */
		/* QUESTION: do not care */
		vp[0].op = LOCK;
		vp[0].sem = &sem_floppy;
		r4addr = (int) vp;
		r4 = r4addr;
		r3 = 1;
		DO_SEMOP();	 /* P(sem_floppy) */
		/* ==============================
		 * critical section: floppy
		 */
		/* get from floppy0 */
		devreg_floppy0 = sldev_reg_loc(FLOPPY0);
		devreg_floppy0->d_track = tracsec.tracknum;
		devreg_floppy0->d_op = IOSEEK;
		r4 = FLOPPY0;
		DO_WAITIO();
		/* woke up */
		io_sta = r3;
		switch (io_sta)
		{
		case NORMAL:
			break;
		default:
			panic("page2.pagein: abnormal status from floppy0");
			return;
		}
		devreg_floppy0->d_sect = tracsec.sectornum;
		devreg_floppy0->d_badd = t_shared_struct[term].termbuf;
		/* read swapped page from floppy0 */
		devreg_floppy0->d_op = IOREAD;
		r4 = FLOPPY0;
		DO_WAITIO();
		/* woke up */
		io_sta = r3;
		switch (io_sta)
		{
		case NORMAL:
			break;
		default:
			panic("page2.pagein: abnormal status from floppy0");
			return;
		}
		vp[0].op = UNLOCK;
		vp[0].sem = &sem_floppy;
		r4addr = (int) vp;
		r4 = r4addr;
		r3 = 1;
		DO_SEMOP();	/* V(sem_floppy) */
		/* ============================== */
		/* still in `sem_mm` */
		/* copy termbuf to page frame */
		memcpy(pf*PAGESIZE, t_shared_struct[term].termbuf, SECTORSIZE);
		/* memcpy(t_shared_struct[term].pagetable_seg0[pf], t_shared_struct[term].termbuf, SECTORSIZE); */
		/* update frameinfo[] */
		if (!(frameinfo[pf-pf_start].tracknum == ENULL && frameinfo[pf-pf_start].sectornum == ENULL))
		{
			panic("page2.pagein(seg=2): frameinfo[] track/sector num supposed to be set ENULL by getfreeframe?!");
			return;
		}
		frameinfo[pf-pf_start].tracknum = tracsec.tracknum;
		frameinfo[pf-pf_start].sectornum = tracsec.sectornum;
		break;
	default:
		panic("page2.pagein: invalid seg");
		return;
	}
}


void
pagedaemon(void)
{
	int num_of_free_frame;
	int i;
	vpop vp[1];
	int r4addr;
	bool_t need_pageout = FALSE;
	while (1)
	{
		/* need to suicide? */
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
			/* DISCUSS */
			/* help diskdaemon suicide */
			while (sem_disk < 0)
			{
				/* diskdaemon blocked */
				vp[0].op = UNLOCK;
				vp[0].sem = &sem_disk;
				r4addr = (int) vp;
				r4 = r4addr;
				r3 = 1;
				DO_SEMOP();	/* V(sem_disk) */
			}
			DO_TERMINATEPROC();
			panic("page2.pagedaemon: cannot suicide");
			return;
		}
		
		need_pageout = FALSE;
		vp[0].op = LOCK;
		vp[0].sem = &sem_mm;
		r4addr = (int) vp;
		r4 = r4addr;
		r3 = 1;
		DO_SEMOP();	/* P(sem_mm) */
		/* get number of free frames */
		num_of_free_frame = 0;
		for (i = 0; i < MAXFRAMES; i++)
		{
			if (freeframes[i] >= pf_start && freeframes[i] < pf_start + MAXFRAMES)
			{
				num_of_free_frame++;
				continue;
			} else
			{
				if (freeframes[i] != ENULL)
				{
					panic("page2.pagedaemon: some non-ENULL but also non-frame number in `freeframes[]`");
					return;
				}
				break;
			}
		}
		if (num_of_free_frame > MAXFRAMES)
		{
			panic("page2.pagedaemon: freeframe[] out of bound");
			return;
		}
		if (num_of_free_frame <= LOWWATERMARK)
		{
			/* start work */
			/* select used page frame */
			int infloop = 0;
			for (i = last_clock_hand; ;infloop++)
			{
				if (infloop >= 3 * MAXFRAMES)
				{
					panic("page2.pagedaemon: should able to find page to swap out but no");
					return;
				}
				if (frameinfo[i].is_present)
				{
				    switch (frameinfo[i].segnum)
					{
					case 1:
						if (t_shared_struct[frameinfo[i].termnum].pagetable_seg1[frameinfo[i].pagenum].pd_r == 1)
						{
							/* second chance */
							t_shared_struct[frameinfo[i].termnum].pagetable_seg1[frameinfo[i].pagenum].pd_r = 0;
						} else
						{
							/* no chance */
							need_pageout = TRUE;
							tracksectuple_t tracsec;
							if (frameinfo[i].tracknum == ENULL && frameinfo[i].sectornum == ENULL)
							{
								/* virgin page, never paged out */
								/* need to call `getfreesector` */
								tracsec = desect_tracsec(getfreesector());
							} else
							{
								/* set by `pagein` => paged out in the past
								 * => no need to call `getfreesector()`
								 */
								tracsec.tracknum = frameinfo[i].tracknum;
								tracsec.sectornum = frameinfo[i].sectornum;
							}
							/* memcpy(t_shared_struct[frameinfo[i].termnum].termbuf, t_shared_struct[frameinfo[i].termnum].pagetable_seg1[frameinfo[i].pagenum].pd_frame * PAGESIZE, PAGESIZE); */

							vad_t pfaddr;
							pfaddr.v_seg = 0;
							pfaddr.v_page = t_shared_struct[frameinfo[i].termnum].pagetable_seg1[frameinfo[i].pagenum].pd_frame;
							pfaddr.v_offset = 0;
							bcopy(t_shared_struct[frameinfo[i].termnum].pagetable_seg1[frameinfo[i].pagenum].pd_frame * PAGESIZE,\
								  t_shared_struct[frameinfo[i].termnum].termbuf,\
								  PAGESIZE);
							/* bcopy(*(char**)&pfaddr,\ */
							/* 	  t_shared_struct[frameinfo[i].termnum].termbuf,\ */
							/* 	  PAGESIZE); */
							vp[0].op = LOCK;
							vp[0].sem = &sem_floppy;
							r4addr = (int) vp;
							r4 = r4addr;
							r3 = 1;
							DO_SEMOP();
							/* ==============================
								* critical section: floppy0
								*/
							/* swap to floppy0 device */
							devreg_t *devreg_floppy0 = sldev_reg_loc(FLOPPY0);
							devreg_floppy0->d_track = tracsec.tracknum;
							devreg_floppy0->d_op = IOSEEK;
							r4 = FLOPPY0;
							DO_WAITIO();
							/* woke up */
							int io_sta = r3;
							if (io_sta != NORMAL)
							{
								panic("page2.pagedaemon: floppy0 abnormal response");
								return;
							}
							devreg_floppy0->d_sect = tracsec.sectornum;
							devreg_floppy0->d_badd = t_shared_struct[frameinfo[i].termnum].termbuf;
							devreg_floppy0->d_op = IOWRITE;
							r4 = FLOPPY0;
							DO_WAITIO();
							/* woke up */
							io_sta = r3;
							if (io_sta != NORMAL)
							{
								panic("page2.pagedaemon: abnormal floppy0 while pageing out");
								return;
							}
							vp[0].op = UNLOCK;
							vp[0].sem = &sem_floppy;
							r4addr = (int) vp;
							r4 = r4addr;
							r3 = 1;
							DO_SEMOP();
							/* ============================== */
							/* update page table */
							int frame_swapped = t_shared_struct[frameinfo[i].termnum].pagetable_seg1[frameinfo[i].pagenum].pd_frame;
							if (frame_swapped != i + pf_start)
							{
								panic("page2.pagedaemon: two ways of getting frame number inconsistent?!");
								return;
							}
							t_shared_struct[frameinfo[i].termnum].pagetable_seg1[frameinfo[i].pagenum].pd_p = 0;	/* no longer glorified */
							t_shared_struct[frameinfo[i].termnum].pagetable_seg1[frameinfo[i].pagenum].pd_m = 0;
							t_shared_struct[frameinfo[i].termnum].pagetable_seg1[frameinfo[i].pagenum].pd_r = 0;	/* becomes invalid */
							t_shared_struct[frameinfo[i].termnum].pagetable_seg1[frameinfo[i].pagenum].pd_frame = encapsulate_tracsec(tracsec);
							/* update frameinfo[] */
							frameinfo[i].is_present = FALSE;
							frameinfo[i].termnum = ENULL;
							frameinfo[i].segnum = ENULL;
							frameinfo[i].pagenum = ENULL;
							frameinfo[i].tracknum = ENULL;
							frameinfo[i].sectornum = ENULL;
							/* update free frame list */
							frame_list_push(frame_swapped, freeframes, MAXFRAMES);
							vp[0].op = UNLOCK;
							vp[0].sem = &sem_pf;
							r4addr = (int) vp;
							r4 = r4addr;
							r3 = 1;
							DO_SEMOP();	/* V(sem_pf) */
						}
						break;
					case 2:
						/* QUESTION: allow taking off shared seg2 page? */
						/* no, do not want! */
						break;
					default:
						panic("page2.pagedaemon: unexpected segnum");
						return;
					}
					if (need_pageout)
					{
						/* carried page out */
						last_clock_hand = (i == MAXFRAMES-1) ? 0 : i+1;
						break;
					}
				}
				/* next loop */
				if (i == MAXFRAMES-1)
				{
					/* circular */
					i = 0;
				} else
				{
					i++;
				}
			}
			/* page daemon swapped out some page */
			/* QUESTION: one page at a time */
			vp[0].op = UNLOCK;
			vp[0].sem = &sem_mm;
			r4addr = (int) vp;
			r4 = r4addr;
			r3 = 1;
			DO_SEMOP();	/* V(sem_mm) */
			DO_WAITCLOCK();
		} else
		{
			/* no work required */
			vp[0].op = UNLOCK;
			vp[0].sem = &sem_mm;
			r4addr = (int) vp;
			r4 = r4addr;
			r3 = 1;
			DO_SEMOP();	/* V(sem_mm) */
			DO_WAITCLOCK();
		}
	}
}


int
getfreesector(void)
{
	/* easy version */
	int ret = encapsulate_tracsec(freesector_counter);
	incr_tracsec(&freesector_counter);
	return ret;
}


void
putframe(int term)
{
	vpop vp[2];
	int r4addr;
	if (headQueue(rq_tl) == (proc_t*) ENULL)
	{
		panic("page2.putframe: rq empty yet called by terminate()");
		return;
	}
	int i;
	for (i = 0; i < 32; i++)
	{
		/* seg1 */
		switch (t_shared_struct[term].pagetable_seg1[i].pd_p)
		{
			int pf;
		case 0:
			/* virgin page, or invalid page */
			switch (t_shared_struct[term].pagetable_seg1[i].pd_r)
			{
			case 0:	/* invalid page */
				/* recycle track/sector from pd_frame */
				/* OHHHH we do not need recycle since we never run out of floppy0 sectors!
				 */
				/* const int pf = t_shared_struct[term].pagetable_seg1[i].pd_frame; */
				/* tracksectuple_t tracsec = desect_tracsec(pf); */
				/* update frameinfo[]
				 * no in this case no frame associated with invalid page
				 */
				/* update free frame list
				 * no, already invalid
				 */
				break;
			case 1:	/* virgin page */
				/* nothing to recycle */
				break;
			default:
				panic("page2.putframe: nonsense pd_r bit");
				return;
			}
			break;
		case 1:
			/* an in-use page, recycling frame */
			pf = t_shared_struct[term].pagetable_seg1[i].pd_frame;
			vp[0].op = LOCK;
			vp[0].sem = &sem_mm;
			r4addr = (int) vp;
			r4 = r4addr;
			r3 = 1;
			DO_SEMOP();	/* P(sem_mm) */
			/* ==============================
			 * critical section
			 */
			/* update frameinfo[] */
			if (!(pf >= pf_start && pf < pf_start + MAXFRAMES))
			{
				panic("page2.putframe: how many panics have i wrote?");
				return;
			}
			frameinfo[pf-pf_start].is_present = FALSE;
			frameinfo[pf-pf_start].termnum = ENULL;
			frameinfo[pf-pf_start].segnum = ENULL;
			frameinfo[pf-pf_start].pagenum = ENULL;
			frameinfo[pf-pf_start].tracknum = ENULL;
			frameinfo[pf-pf_start].sectornum = ENULL;
			/* update free frame list */
			frame_list_push(pf, freeframes, MAXFRAMES);
			vp[0].op = UNLOCK;
			vp[0].sem = &sem_mm;
			/* recycled a frame => V(sem_pf) */
			vp[1].op = UNLOCK;
			vp[1].sem = &sem_pf;
			r4addr = (int) vp;
			r4 = r4addr;
			r3 = 2;
			DO_SEMOP();	/* V(sem_mm) */
			/* ============================== */
			/* OHHHH we do not recycle sector */
			break;
		default:
			panic("page2.putframe: nonsense pd_p bit");
			return;
		}
		/* seg2 */
		/* QUESTION:
		 * did not allow page daemon to swap out shared seg2 page
		 * => if no one using this page => can safely recycle! ...
		 *
		 * no, how can you ever recycle a shared page? how do you know others never going to reference it?
		 */
	}
}


