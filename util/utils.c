#ifndef UTILS_C
#define UTILS_C
#include "utils.e"
#include "../h/procq.h"
#include "../h/const.h"

int
insertSemvec(proc_t *p, int *semAdd)
{
	int i;
	for (i = 0; i < SEMMAX; i++)
	{
		int *sem_addr_i = p->semvec[i];
		if (sem_addr_i == (int*) ENULL)
		{
			/* insert */
			p->semvec[i] = semAdd;
			break;
		}
	}
	if (i >= SEMMAX)
	{
		/* no available semvec[SEMMAX] */
		panic("No available proc_t->semvec[SEMMAX]!");
		return 1;
	}
	return 0;
}


/**
 * Assumption: `semAdd` in `p->semvec`!
 */
int
removeSemvec(proc_t *p, int *semAdd)
{
	int i;
	for (i = 0; i < SEMMAX; i++)
	{
		if (p->semvec[i] != semAdd)
			continue;
		p->semvec[i] = (int*) ENULL;
		return 0;
	}
	/* cannot find semvec! */
	panic("removeSemvec: cannot find semAdd in semvec!");
	return 1;
}
#endif
