#include <string.h>
#include "../../h/const.h"
#include "../../h/moreconst.h"
#include "../../h/vpop.h"
#include "../../h/misc.h"
#include "../../h/support.h"
#include "../../h/support2.h"
#include "../../h/slsyscall2.h"
#include "../../h/page2.e"
#include "../../h/support.e"
#include "../../h/slsyscall1.e"

/* ==============================
 * extern declaration
 */
diskrequest_t diskq[MAXTPROC];
int sem_disk;
int sem_virtualvp;
virt_phys_sem_t vplist[MAXTPROC];

/* ==============================
 * private functions
 */
/**
 * SIDE-EFFECT
 * mark segtable[index] as not present
 */
static void
sd_off(sd_t *segtable, int index)
{
	segtable[index].sd_p = 0;
	segtable[index].sd_prot = 0;
	segtable[index].sd_len = 0;
	segtable[index].sd_pta = (pd_t*) ENULL;
}


/**
 * SIDE-EFFECT
 * mark segtable[index] as present
 */
static void
sd_on(sd_t *segtable, int index, int prot, int page_len, pd_t *pagetable)
{
	segtable[index].sd_p = 1;
	segtable[index].sd_prot = prot;
	segtable[index].sd_len = page_len;
	segtable[index].sd_pta = pagetable;
}


/**
 * SIDE-EFFECT
 * mark pagetable[index] as not present
 */
static void
pd_off(pd_t *pagetable, int index)
{
	pagetable[index].pd_p = 0;
	pagetable[index].pd_m = 0;
	pagetable[index].pd_r = 1;	/* virgin page trick */
	/* pagetable[index].pd_frame = ENULL; */
	pagetable[index].pd_frame = index;	/* per Edger */
}


/**
 * SIDE-EFFECT
 * mark pagetable[index] as present
 */
static void
pd_on(pd_t *pagetable, int index, int page_frame_num)
{
	pagetable[index].pd_p = 1;
	pagetable[index].pd_m = 0;
	pagetable[index].pd_r = 0;
	pagetable[index].pd_frame = page_frame_num;
}


static void
set_sysproc_pt(int which_sysproc, const int EVT_PAGEEND, const int TRAPINT_PAGESTART, const int TRAPINT_PAGEEND, const int DEVREG_PAGESTART, const int DEVREG_PAGEEND)
{
	pd_t *pt;
	int i;
	switch (which_sysproc)
	{
	case CRON:
		pt = pagetable_cron;
		break;
	case PAGED:
		pt = pagetable_paged;
		break;
	case DISK:
		pt = pagetable_disk;
		break;
	default:
		panic("support.set_sysproc_pt: invalid sysproc type");
		return;
	}
	/* EVT no */
	for (i = 0; i <= EVT_PAGEEND; i++)
	{
		pd_off(pt, i);
	}
	/* seg0 page2 yes */
	pd_on(pt, 2, 2);
	/* trapint area no */
	for (i = TRAPINT_PAGESTART; i <= TRAPINT_PAGEEND; i++)
	{
		pd_off(pt, i);
	}
	/* dev reg no */
	for (i = DEVREG_PAGESTART; i <= DEVREG_PAGEEND; i++)
	{
		if (pt == pagetable_paged)
		{
			/* pagedaemon needs to access floppy0 */
			pd_on(pt, i, i);
		} else if (pt == pagetable_disk)
		{
			/* diskdaemon needs to access disk0 */
			pd_on(pt, i, i);
		} else {
			pd_off(pt, i);
		}
	}
	/* nucleus text no */
	for (i = (int)start / PAGESIZE; i <= (int)endt0 / PAGESIZE; i++)
	{
		pd_off(pt, i);
	}
	/* sl text yes */
	for (i = (int)startt1 / PAGESIZE; i <= (int)etext / PAGESIZE; i++)
	{
		pd_on(pt, i, i);
	}
	/* nucleus data no */
	for (i = (int)startd0 / PAGESIZE; i <= (int)endd0 / PAGESIZE; i++)
	{
		pd_off(pt, i);
	}
	/* sl data yes */
	for (i = (int)startd1 / PAGESIZE; i <= (int)edata / PAGESIZE; i++)
	{
		pd_on(pt, i, i);
	}
	/* nucleus bss no */
	for (i = (int)startb0 / PAGESIZE; i <= (int)endb0 / PAGESIZE; i++)
	{
		pd_off(pt, i);
	}
	/* sl bss yes */
	for (i = (int)startb1 / PAGESIZE; i <= (int)end / PAGESIZE; i++)
	{
		pd_on(pt, i, i);
	}
	/* stack */
	for (i = (int)end / PAGESIZE + 1; i < pf_start; i++)
	{
		/* have access to its own stack */
		if (pt == pagetable_cron)
		{
			if (i == Scronstack / PAGESIZE)
			{
				/* scronstack yes */
				pd_on(pagetable_cron, i, i);
			} else
			{
				pd_off(pagetable_cron, i);
			}
		} else if (pt == pagetable_paged)
		{
			if (i == Spagedframe)
			{
				/* spagedstack yes */
				pd_on(pagetable_paged, i, i);
			} else
			{
				pd_off(pagetable_paged, i);
			}
		} else if (pt == pagetable_disk)
		{
			if (i == Sdiskframe)
			{
				/* sdiskstack yes */
				pd_on(pagetable_disk, i, i);
			} else
			{
				pd_off(pagetable_disk, i);
			}
		} else
		{
			panic("support.set_sysproc_pt: invalid `pt`");
			return;
		}
	}
	/* rest of RIPT all off */
	for (i = pf_start; i < 256; i++)
	{
		pd_off(pt, i);
	}
	/* tprocess shared seg2 page table */
	for (i = 0; i < 32; i++)
	{
		pd_off(pagetable_tseg2, i);
	}
}


void
p1(void)
{
	register r2 asm("%d2");
	register r4 asm("%d4");
	int r4addr;
	state_t p1a_st;
	STST(&p1a_st);
	pageinit();
	/* ==============================
	 * other
	 * init var
	 */
	sem_mm = 1;	/* QUESTION: seems to be mutex */
	/* FUTURE */
	int ti;
	for (ti = 0; ti < MAXTPROC; ti++)
	{
		vplist[ti].phys_sem = 0;
		vplist[ti].virt_sem = ENULL;
	}
	sem_virtualvp = 1;	/* mutex */
	sem_disk = 0;
	for (ti = 0; ti < MAXTPROC; ti++)
	{
		diskwait[ti] = 0;
	}
	for (ti = 0; ti < MAXTPROC; ti++)
	{
		diskq[ti].tracknum = ENULL;
		diskq[ti].sectornum = ENULL;
		diskq[ti].physbuf = (char*) ENULL;
		diskq[ti].put_or_get = ENULL;
		diskq[ti].io_sta = ENULL;
		diskq[ti].io_len = ENULL;
	}
	/* ============================== */
	/* init seg and page tables */
	const int EVT_PAGEEND = (BEGINTRAP / PAGESIZE) - 1;
	const int TRAPINT_PAGESTART = (BEGINTRAP / PAGESIZE);
	const int TRAPINT_PAGEEND = (BEGINDEVREG / PAGESIZE) - 1;
	const int DEVREG_PAGESTART = BEGINDEVREG / PAGESIZE;
	const int DEVREG_PAGEEND = DEVREG_PAGESTART;	/* occupy one page */
	int i;
	for (i = 0; i < MAXTPROC; i++)
	{
		/* terminal i */
		tbigstruct_t *tstruct = &t_shared_struct[i];
		/*
		 * seg tables
		 */
		/* seg table: nonpriv tproc */
		/* for each segment */
		int j;
		for (j = 0; j < 32; j++)
		{
			if (j == 0)
			{
				/* seg0
				 * marked as off
				 */
				sd_off(tstruct->segtable_nonpriv, j);
			} else if (j == 1 || j == 2)
			{
				/* seg1 or 2 */
				sd_on(tstruct->segtable_nonpriv, j, 7, 31,
					  (j == 1) ? tstruct->pagetable_seg1 : pagetable_tseg2);
			} else
			{
				/* seg3 to 31 */
				sd_off(tstruct->segtable_nonpriv, j);
			}
		}
		/* seg table: priv tproc */
		for (j = 0; j < 32; j++)
		{
			sd_t *segentry = &tstruct->segtable_priv[j];
			if (j == 0)
			{
				/* QUESTION: write access cuz stack in there */
				/* seg0 */
				sd_on(tstruct->segtable_priv, j, 7, 255, tstruct->pagetable_seg0);
			} else if (j == 1 || j == 2)
			{
				/* seg1 or 2 */
				sd_on(tstruct->segtable_priv, j, 7, 31,
					  (j == 1) ? tstruct->pagetable_seg1 : pagetable_tseg2);
			} else
			{
				/* seg3 to 31 */
				sd_off(tstruct->segtable_priv, j);
			}
		}
		/*
		 * page tables
		 */
		/* seg1 page table */
		for (j = 0; j < 32; j++)
		{
			/* mark as virgin page */
			pd_off(tstruct->pagetable_seg1, j);
		}
		/* seg0 RIPT */
		int page_frame_num = 0;
		/* EVT no */
		for (; page_frame_num <= EVT_PAGEEND; page_frame_num++)
		{
			pd_off(tstruct->pagetable_seg0, page_frame_num);
			
		}
		/* seg0 page 2 yes */
		pd_on(tstruct->pagetable_seg0, 2, 2);
		/* trapint area no */
		for (page_frame_num = TRAPINT_PAGESTART; page_frame_num <= TRAPINT_PAGEEND; page_frame_num++)
		{
			pd_off(tstruct->pagetable_seg0, page_frame_num);
		}
		/* device reg yes */
		for (page_frame_num = DEVREG_PAGESTART; page_frame_num <= DEVREG_PAGEEND; page_frame_num++)
		{
			pd_on(tstruct->pagetable_seg0, page_frame_num, page_frame_num);
		}
		/* nucleus text no */
		for (page_frame_num = (int)start / PAGESIZE; page_frame_num <= (int)endt0 / PAGESIZE; page_frame_num++)
		{
			/* pd_off(tstruct->pagetable_seg0, page_frame_num); */
			/* DIRTY
			 * for executing panic if things go wrong
			 */
			pd_on(tstruct->pagetable_seg0, page_frame_num, page_frame_num);
		}
		/* support text yes */
		for (page_frame_num = (int)startt1 / PAGESIZE; page_frame_num <= (int)etext / PAGESIZE; page_frame_num++)
		{
			pd_on(tstruct->pagetable_seg0, page_frame_num, page_frame_num);
		}
		/* nucleus data no */
		for (page_frame_num = (int)startd0 / PAGESIZE; page_frame_num <= (int)endd0 / PAGESIZE; page_frame_num++)
		{
			/* pd_off(tstruct->pagetable_seg0, page_frame_num); */
			/* DIRTY
			 */
			pd_on(tstruct->pagetable_seg0, page_frame_num, page_frame_num);
		}
		/* support data yes */
		for (page_frame_num = (int)startd1 / PAGESIZE; page_frame_num <= (int)edata / PAGESIZE; page_frame_num++)
		{
			pd_on(tstruct->pagetable_seg0, page_frame_num, page_frame_num);
		}
		/* nucleus bss no */
		for (page_frame_num = (int)startb0 / PAGESIZE; page_frame_num <= (int)endb0 / PAGESIZE; page_frame_num++)
		{
			/* pd_off(tstruct->pagetable_seg0, page_frame_num); */
			/* DIRTY
			 */
			pd_on(tstruct->pagetable_seg0, page_frame_num, page_frame_num);
		}
		/* support bss yes */
		for (page_frame_num = (int)startb1 / PAGESIZE; page_frame_num <= (int)end / PAGESIZE; page_frame_num++)
		{
			pd_on(tstruct->pagetable_seg0, page_frame_num, page_frame_num);
		}
		/* stack */
		for (page_frame_num = (int)end / PAGESIZE + 1; page_frame_num < pf_start; page_frame_num++)
		{
			if (page_frame_num == Tsysstack[i] / PAGESIZE || page_frame_num == Tmmstack[i] / PAGESIZE)
			{
				/* Tsysstack[i] and Tmmstack[i] yes */
				/* QUESTION: right? */
				pd_on(tstruct->pagetable_seg0, page_frame_num, page_frame_num);
			} else
			{
				/* other stack no */
				pd_off(tstruct->pagetable_seg0, page_frame_num);
			}
		}
		/* p1a/cron stack no
		 * already done
		 */
		/* nucleus stack no */
		/* ASSUMPTION: ~~free space~~, nucleus stack and the rest all no */
		/* TODO: call getfreeframe() for bootcode page before marking all free space page (frame) as nonpresent, otherwise cannot touch free pool */
		for (page_frame_num = pf_start; page_frame_num <= 253; page_frame_num++)
		{
			/* pagein() needs to write to frame */
			pd_on(tstruct->pagetable_seg0, page_frame_num, page_frame_num);
		}
		for (page_frame_num = 254; page_frame_num < 256; page_frame_num++)
		{
			pd_off(tstruct->pagetable_seg0, page_frame_num);
		}
	}
	/* other sys process's seg/page table */
	/* cron */
	/* seg table */
	sd_on(segtable_cron, 0, 7, 255, pagetable_cron);
	/* seg 1 to 31 not present */
	for (i = 1; i < 32; i++)
	{
		sd_off(segtable_cron, i);
	}
	/* page table */
	set_sysproc_pt(CRON, EVT_PAGEEND, TRAPINT_PAGESTART, TRAPINT_PAGEEND, DEVREG_PAGESTART, DEVREG_PAGEEND);
	/* === part2 === */
	/* paged */
	/* seg table */
	sd_on(segtable_paged, 0, 7, 255, pagetable_paged);
	/* seg 1 to 31 not present */
	for (i = 1; i < 32; i++)
	{
		sd_off(segtable_paged, i);
	}
	set_sysproc_pt(PAGED, EVT_PAGEEND, TRAPINT_PAGESTART, TRAPINT_PAGEEND, DEVREG_PAGESTART, DEVREG_PAGEEND);
	/*
	 * page daemon needs to have access to all free frames, to page in/out
	 */
	for (i = pf_start; i <= 253; i++)
	{
		pd_on(pagetable_paged, i, i);
	}
	/* diskdaemon */
	/* seg table */
	sd_on(segtable_disk, 0, 7, 255, pagetable_disk);
	/* seg 1 to 31 not present */
	for (i = 1; i < 32; i++)
	{
		sd_off(segtable_disk, i);
	}
	set_sysproc_pt(DISK, EVT_PAGEEND, TRAPINT_PAGESTART, TRAPINT_PAGEEND, DEVREG_PAGESTART, DEVREG_PAGEEND);
	/* === */
	/* boot strap loader object code */
	int bootcode[] = {	
		0x41f90008,
		0x00002608,
		0x4e454a82,
		0x6c000008,
		0x4ef90008,
		0x0000d1c2,
		0x787fb882,
		0x6d000008,
		0x10bc000a,
		0x52486000,
		0xffde4e71
	};
	for (i = 0; i < MAXTPROC; i++)
	{
		int freepf = getfreeframe(i, 31, 1);	/* FUTURE */
		/* page frame number n -> page frame boundary starts at n * PAGESIZE */
		bcopy(bootcode, freepf*PAGESIZE, 44);
		/* tproc seg1 page 31 */
		pd_on(t_shared_struct[i].pagetable_seg1, 31, freepf);
	}
	/* pass control to p1a() */
	p1a_st.s_sp = Scronstack;
	p1a_st.s_pc = (int) p1a;
	p1a_st.s_sr.ps_s = 1;
	p1a_st.s_sr.ps_m = 1;	/* mm on */
	p1a_st.s_sr.ps_int = 0;	/* interrupt on */
	p1a_st.s_crp = segtable_cron;
	LDST(&p1a_st);
}


/**
 * refer ch6 pp. 40
 */
static void
p1a(void)
{
	register int r2 asm("%d2");
	register int r4 asm("%d4");
	state_t cron_st;
	STST(&cron_st);
	state_t tproc_startst;
	tproc_startst.s_pc = (int) tprocess;
	tproc_startst.s_sr.ps_t = 0;
	tproc_startst.s_sr.ps_s = 1;	/* privileged */
	tproc_startst.s_sr.ps_m = 1;	/* mm on */
	tproc_startst.s_sr.ps_int = 0;	/* interruptable */
	tproc_startst.s_sr.ps_x = 0;
	tproc_startst.s_sr.ps_n = 0;
	tproc_startst.s_sr.ps_z = 0;
	tproc_startst.s_sr.ps_o = 0;
	tproc_startst.s_sr.ps_c = 0;
	int i;
	for (i = 0; i < MAXTPROC; i++)
	{
		/* create tproc */
		tproc_startst.s_crp = t_shared_struct[i].segtable_priv;
		tproc_startst.s_sp = Tsysstack[i];
		/* tproc_startst.s_sp = &Tsysstack[i]; */
		/* tproc_startst.s_sp = Tsysstack + i; */
		tproc_startst.s_r[4] = i;	/* d4 contains terminal number */
		/* QUESTION
		 * p1a mm on, r4 is logical
		 * nucleus see physical
		 * no problem cuz p1a seg0 RIPT identity map?
		 */ 
		int r4addr = (int) &tproc_startst;
		r4 = r4addr;
		DO_CREATEPROC();
		if (r2 != 0)
		{
			panic("support.p1a: nucleus error on SYS1");
			return;
		}
	}
	/* become cron process */
	cron_st.s_pc = (int) cron;
	cron_st.s_sp = Scronstack;
	LDST(&cron_st);
}


static void
tprocess(void)
{
	register int r2 asm("%d2");
	register int r3 asm("%d3");
	register int r4 asm("%d4");
	int termnum = r4;
	state_t nonpriv_st;
	STST(&nonpriv_st);
	int r3addr, r4addr;
	/* sys5 area */
	/* QUESTION: prog new area? */
	t_shared_struct[termnum].slprog_new.s_r[4] = termnum;	/* d4 term num */
	t_shared_struct[termnum].slprog_new.s_sp = Tsysstack[termnum];	/* same as sys stack */
	t_shared_struct[termnum].slprog_new.s_pc = (int) slproghandler;
	t_shared_struct[termnum].slprog_new.s_sr.ps_t = 0;
	t_shared_struct[termnum].slprog_new.s_sr.ps_s = 1;
	t_shared_struct[termnum].slprog_new.s_sr.ps_m = 1;
	t_shared_struct[termnum].slprog_new.s_sr.ps_int = 0;
	t_shared_struct[termnum].slprog_new.s_sr.ps_x = 0;
	t_shared_struct[termnum].slprog_new.s_sr.ps_n = 0;
	t_shared_struct[termnum].slprog_new.s_sr.ps_z = 0;
	t_shared_struct[termnum].slprog_new.s_sr.ps_o = 0;
	t_shared_struct[termnum].slprog_new.s_sr.ps_c = 0;
	t_shared_struct[termnum].slprog_new.s_crp = t_shared_struct[termnum].segtable_priv;
	r3addr = (int) &t_shared_struct[termnum].slprog_old;
	r4addr = (int) &t_shared_struct[termnum].slprog_new;
	r4 = r4addr;
	r3 = r3addr;
	r2 = PROGTRAP;	/* prog old/new */
	DO_SPECTRAPVEC();	/* SYS5 */
	t_shared_struct[termnum].slmm_new.s_r[4] = termnum;	/* d4 term num */
	t_shared_struct[termnum].slmm_new.s_sp = Tmmstack[termnum];
	t_shared_struct[termnum].slmm_new.s_pc = (int) slmmhandler;
	t_shared_struct[termnum].slmm_new.s_sr.ps_t = 0;
	t_shared_struct[termnum].slmm_new.s_sr.ps_s = 1;
	t_shared_struct[termnum].slmm_new.s_sr.ps_m = 1;
	t_shared_struct[termnum].slmm_new.s_sr.ps_int = 0;
	t_shared_struct[termnum].slmm_new.s_sr.ps_x = 0;
	t_shared_struct[termnum].slmm_new.s_sr.ps_n = 0;
	t_shared_struct[termnum].slmm_new.s_sr.ps_z = 0;
	t_shared_struct[termnum].slmm_new.s_sr.ps_o = 0;
	t_shared_struct[termnum].slmm_new.s_sr.ps_c = 0;
	t_shared_struct[termnum].slmm_new.s_crp = t_shared_struct[termnum].segtable_priv;
	r3addr = (int) &t_shared_struct[termnum].slmm_old;
	r4addr = (int) &t_shared_struct[termnum].slmm_new;
	r4 = r4addr;
	r3 = r3addr;
	r2 = MMTRAP;	/* mm old/new */
	DO_SPECTRAPVEC();
	t_shared_struct[termnum].slsys_new.s_r[4] = termnum;	/* d4 term num */
	t_shared_struct[termnum].slsys_new.s_sp = Tsysstack[termnum];	/* same as sys stack */
	t_shared_struct[termnum].slsys_new.s_pc = (int) slsyshandler;
	t_shared_struct[termnum].slsys_new.s_sr.ps_t = 0;
	t_shared_struct[termnum].slsys_new.s_sr.ps_s = 1;
	t_shared_struct[termnum].slsys_new.s_sr.ps_m = 1;
	t_shared_struct[termnum].slsys_new.s_sr.ps_int = 0;
	t_shared_struct[termnum].slsys_new.s_sr.ps_x = 0;
	t_shared_struct[termnum].slsys_new.s_sr.ps_n = 0;
	t_shared_struct[termnum].slsys_new.s_sr.ps_z = 0;
	t_shared_struct[termnum].slsys_new.s_sr.ps_o = 0;
	t_shared_struct[termnum].slsys_new.s_sr.ps_c = 0;
	t_shared_struct[termnum].slsys_new.s_crp = t_shared_struct[termnum].segtable_priv;
	r3addr = (int) &t_shared_struct[termnum].slsys_old;
	r4addr = (int) &t_shared_struct[termnum].slsys_new;
	r4 = r4addr;
	r3 = r3addr;
	r2 = SYSTRAP;	/* sys old/new */
	DO_SPECTRAPVEC();
	/* ==============================
	 * other. non-documented
	 */
	/* init shared structure before becoming nonpriv */
	memset(t_shared_struct[termnum].termbuf, '\0', TERMBUF_SIZE);
	t_shared_struct[termnum].sem_cron = 0;
	t_shared_struct[termnum].wakeup_time = 0L;
	t_shared_struct[termnum].dead = FALSE;
	/* ============================== */
	
	/* become nonpriv */
	/*
	 * QUESTION: where is nonpriv tproc stack?
	 * guess bootcode sets it
	 */
	nonpriv_st.s_sp = 0x80000 + 32*PAGESIZE - 2;	/* support1.pdf pp. 4 */
	nonpriv_st.s_pc = 0x80000 + 31*PAGESIZE;
	nonpriv_st.s_crp = t_shared_struct[termnum].segtable_nonpriv;
	nonpriv_st.s_sr.ps_s = 0;	/* nonprivileged */
	LDST(&nonpriv_st);
}


static void
slmmhandler(void)
{
	register int r3 asm("%d3");
	register int r4 asm("%d4");
	int termnum = r4;
	vpop vp[1];
	int r4addr;
	/* check validity of page fault */
	/* QUESTION: mm_typ? */
	int mm_type = t_shared_struct[termnum].slmm_old.s_tmp.tmp_mm.mm_typ;
	if (mm_type != PAGEMISS)
	{
		/*
		 * error: support.slmmhandler: unrecoverable mm trap type
		 */
		/* terminate */
		terminate(termnum);	/* issues SYS2 call */
		panic("support.slmmhandler: not killed");
		return;
	}
	/* seg0 page fault -> panic */
	int mm_seg = t_shared_struct[termnum].slmm_old.s_tmp.tmp_mm.mm_seg;
	int mm_page = t_shared_struct[termnum].slmm_old.s_tmp.tmp_mm.mm_pg;
	if (mm_seg == 0)
	{
		/*
		 * error: support.slmmhandler: page fault on seg0!
		 * kill the proc
		 */
		terminate(termnum);
		panic("support.slmmhandler: not killed");
		return;
	}
	vp[0].op = LOCK;
	vp[0].sem = &sem_mm;
	r4addr = (int) vp;
	r4 = r4addr;
	r3 = 1;
	DO_SEMOP();	/* P(sem_mm) */
	/* ==============================
	 * critical section
	 * getfreeframe + pagein
	 * do not want to consider race vs pagedaemon, etc.
	 */
	int freepf = getfreeframe(termnum, mm_page, mm_seg);
	/* FUTURE: pagein */
	switch (mm_seg)
	{
	case 1:
		if (t_shared_struct[termnum].pagetable_seg1[mm_page].pd_r == 0)
		{
			/* invalid page */
			pagein(termnum, mm_page, mm_seg, freepf);
		}
		break;
	case 2:
		if (pagetable_tseg2[mm_page].pd_r == 0)
		{
			/* invalid page */
			pagein(ENULL, mm_page, mm_seg, freepf);
		}
		break;
	default:
		panic("support.slmmhandler: unexpected mm_seg");
		return;
	}
	switch (mm_seg)
	{
	case 1:
		pd_on(t_shared_struct[termnum].pagetable_seg1, mm_page, freepf);
		break;
	case 2:
		/* FUTURE: seg2 sem_mm mutex */
		/* already in sem_mm */
		/* still nonpresent page? evil concurrency */
		if (pagetable_tseg2[mm_page].pd_p == 0)
		{
			pd_on(pagetable_tseg2, mm_page, freepf);
		}
		break;
	default:
		panic("support.slmmhandler: unexpected seg");
		break;
	}
	vp[0].op = UNLOCK;
	vp[0].sem = &sem_mm;
	r4addr = (int) vp;
	r4 = r4addr;
	r3 = 1;
	DO_SEMOP();
	/* ============================== */
	LDST(&t_shared_struct[termnum].slmm_old);
}


static void
slsyshandler(void)
{
	register int r4 asm("%d4");
	int termnum = r4;
	int sysnum = t_shared_struct[termnum].slsys_old.s_tmp.tmp_sys.sys_no;
	switch (sysnum)
	{
		/* FUTURE: slsyscall2.c */
		vad_t argd4_va;
		int argd4;
		int argd3;
		int argd2;
	case 9:
		readfromterminal(termnum);
		break;
	case 10:
		writetoterminal(termnum);
		break;
	case 11:
		argd4 = t_shared_struct[termnum].slsys_old.s_r[4];
		argd4_va = *(vad_t*) &argd4;
		if (argd4_va.v_seg != 2)
		{
			/* kill */
			DO_TERMINATEPROC();
			panic("support.slsyshandler: SYS11 failed to suicide");
			return;
		}
		virtualv(argd4);
		LDST(&t_shared_struct[termnum].slsys_old);
		break;
	case 12:
		argd4 = t_shared_struct[termnum].slsys_old.s_r[4];
		argd4_va = *(vad_t*) &argd4;
		if (argd4_va.v_seg != 2)
		{
			/* kill */
			terminate(termnum);
			panic("support.slsyshandler: SYS12 failed to suicide");
			return;
		}
		virtualp(argd4);
		LDST(&t_shared_struct[termnum].slsys_old);
		break;
		
	case 13:
		delay(termnum);
		break;
	case 14:
		argd3 = t_shared_struct[termnum].slsys_old.s_r[3];	/* va */
		argd4 = t_shared_struct[termnum].slsys_old.s_r[4];	/* tracknum */
		argd2 = t_shared_struct[termnum].slsys_old.s_r[2];	/* sectornum */
		diskput(termnum, (char*) argd3, argd4, argd2);
		break;
	case 15:
		argd3 = t_shared_struct[termnum].slsys_old.s_r[3];	/* va */
		argd4 = t_shared_struct[termnum].slsys_old.s_r[4];	/* tracknum */
		argd2 = t_shared_struct[termnum].slsys_old.s_r[2];	/* sectornum */
		/* HACK */
		if (argd3 == 0x0)
		{
			terminate(termnum);
		}
		diskget(termnum, (char*) argd3, argd4, argd2);
		break;
	case 16:
		gettimeofday(termnum);
		break;
	case 17:
		terminate(termnum);
		break;
	default:
		panic("support.slsyshandler: unexpected sys trap num");
		break;
	}
}


static void
slproghandler(void)
{
	register int r4 asm("%d4");
	int termnum = r4;
	terminate(termnum);
}


static void
cron(void)
{
	register int r3 asm("%d3");
	register int r4 asm("%d4");
	int r4val;
	while (1)
	{
		/* QUESTION: way of determining whether all tproc is dead */
		int termi;
		bool_t alldead = TRUE;
		for (termi = 0; termi < MAXTPROC; termi++)
		{
			if (!t_shared_struct[termi].dead)
			{
				alldead = FALSE;
				break;
			}
		}
		if (alldead)
		{
			/* no tproc running -> shut down */
			DO_TERMINATEPROC();
			return;
		}
		bool_t work = FALSE;
		long now;
		for (termi = 0; termi < MAXTPROC; termi++)
		{
			if (t_shared_struct[termi].sem_cron < 0)
			{
				/* some tproc delaying */
				STCK(&now);
				if (now >= t_shared_struct[termi].wakeup_time)
				{
					work = TRUE;
					/* release tproc */
					vpop vp[1];
					vp[0].op = UNLOCK;
					vp[0].sem = &t_shared_struct[termi].sem_cron;
					r4val = (int) vp;
					r4 = r4val;
					r3 = 1;
					DO_SEMOP();	/* V(sem_cron) */
				}
			}
		}
		if (!work)
		{
			/* block on pclock */
			DO_WAITCLOCK();
		}
	}
}
