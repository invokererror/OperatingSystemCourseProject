#ifndef SUPPORT_H
#define SUPPORT_H
/* big struct for tprocess */
#include "types.h"
#include "moreconst.h"
#include "misc.h"

typedef struct tbigstruct_t {
	/*
	 * 2 seg tables
	 */
	sd_t segtable_nonpriv[32];
	sd_t segtable_priv[32];
	/* seg1 page table */
	pd_t pagetable_seg1[32];
	/* seg0 RIPT */
	pd_t pagetable_seg0[256];
	/* terminal buffer */
	char termbuf[TERMBUF_SIZE];	/* need +1 for '\0' */
	/*
	 * NEW / OLD area for each SYS5
	 */
	state_t slsys_old, slsys_new;
	state_t slmm_old, slmm_new;
	state_t slprog_old, slprog_new;
	/* ==============================
	 * other. non-documented
	 */
	int sem_cron;
	long wakeup_time;
	bool_t dead;
} tbigstruct_t;

/* ==============================
 * vars
 */
tbigstruct_t t_shared_struct[MAXTPROC];
/* seg/page table for other system processes */
sd_t segtable_cron[32];
sd_t segtable_paged[32];
sd_t segtable_disk[32];
pd_t pagetable_cron[256];
pd_t pagetable_paged[256];
pd_t pagetable_disk[256];
/* tprocess shared page table on seg2 */
pd_t pagetable_tseg2[32];

/* ====================
 * function definition
 */
static void p1a(void);
static void tprocess(void);
static void slmmhandler(void);
static void slsyshandler(void);
static void slproghandler(void);
static void cron(void);
/* ==============================
 * other private functions
 */
static void sd_off(sd_t *segtable, int index);
static void sd_on(sd_t *segtable, int index, int prot, int page_len, pd_t *pagetable);
static void pd_off(pd_t *pagetable, int index);
static void pd_on(pd_t *pagetable, int index, int page_frame_num);
#endif
