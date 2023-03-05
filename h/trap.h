#ifndef TRAP_H
#define TRAP_H

#include "procq.h"
void trapinit(void);
void static trapsyshandler(void);
void static trapmmhandler(void);
void static trapproghandler(void);
/* other private functions */
int post_syscall(void);
void manual_resume(proc_t *running_proc);
#endif
