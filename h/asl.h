#ifndef ASL_H
#define ASL_H
/* semaphore descriptor type */
typedef struct semd_t {
	struct semd_t *s_next,	/* next element on the queue */
	*s_prev;	/* previous element on the queue */
	int *s_semAdd;	/* pointer to the semaphore proper */
	proc_link s_link;	/* pointer/index to the tail of the queue of processes blocked on this semaphore */
} semd_t;

typedef struct aslhead_t {
	struct semd_t *head;
	/* int semAddVec[SEMMAX]; */
	int aslSize;
} aslhead_t;

/* private functions */
static semd_t * searchSemAdd(int *semAdd);
static void insertSemD(semd_t *semd);
static int removeSemD(semd_t *semd);
static int pAlreadyInProcq(proc_link *tp, proc_t *p);
static void freeSemD(semd_t* semd);
#endif
