#ifndef SUPPORT2_E
#define SUPPORT2_E
#include "slsyscall2.h"
#include "moreconst.h"
extern diskrequest_t diskq[MAXTPROC];
extern int sem_disk;
extern int sem_virtualvp;
extern virt_phys_sem_t vplist[MAXTPROC];
#endif