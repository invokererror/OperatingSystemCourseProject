#ifndef MORECONST_H
#define MORECONST_H

#define CLOCKINTERVAL 100000L
/* #define MAXTPROC 2 */
/* DIRTY */
#define MAXTPROC 4
#define UPAMOUNT 128
/* extra one to ensure null-terminated */
/* #define TERMBUF_SIZE (UPAMOUNT + 1) */
#define TERMBUF_SIZE SECTORSIZE
#define SECTORSIZE 512
/* constants to allow SYS calls */
#define	DO_CREATEPROC		SYS1	/* create process */
#define	DO_TERMINATEPROC	SYS2	/* terminate process */
#define	DO_SEMOP			SYS3	/* V or P a semaphore */
#define	DO_SPECTRAPVEC		SYS5	/* specify trap vectors for passup */
#define	DO_GETCPUTIME		SYS6	/* get cpu time used to date */
#define	DO_WAITCLOCK		SYS7	/* delay on the clock semaphore */
#define	DO_WAITIO		SYS8	/* delay on a io semaphore */

#define	DO_PASSERN		passeren	/* P a semaphore */
#define	DO_VERHOGEN		verhogen	/* V a semaphore */
#endif
