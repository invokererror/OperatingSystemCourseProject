#ifndef TRAP_H
#define TRAP_H

#include "procq.h"
#include "types.h"
void trapinit(void);
static void trapsyshandler(void);
static void trapmmhandler(void);
static void trapproghandler(void);
/* other private functions */
static int post_traphandler(void);
static void manual_resume(proc_t *running_proc);
static void before_trap_handler(int handler_type);
static state_t * proc_sys_old(proc_t *p);
#endif
