#ifndef SYSCALL_E
#define SYSCALL_E
#include "types.h"

extern int creatproc(state_t *old);
extern int killproc(state_t *old);
extern int semop(state_t *old);
extern int notused(state_t *old);
extern int trapstate(state_t *old);
extern int getcputime(state_t *old);
extern int trapsysdefault(state_t *old);
#endif
