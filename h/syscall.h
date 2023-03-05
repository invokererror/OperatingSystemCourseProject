#ifndef SYSCALL_H
#define SYSCALL_H
#include "types.h"

int creatproc(state_t *old);
int killproc(state_t *old);
int semop(state_t *old);
int notused(state_t *old);
int trapstate(state_t *old);
int getcputime(state_t *old);
int trapsysdefault(state_t *old);
#endif
