#ifndef ASL_E
#define ASL_E

#include "../h/procq.h"
extern void insertBlocked(int *semAdd, proc_t *p);
extern proc_t * removeBlocked(int *semAdd);
extern proc_t * outBlocked(proc_t *p);
extern proc_t * headBlocked(int *semAdd);
extern void initSemd(void);
extern void headASL(void);
#endif