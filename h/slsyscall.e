#ifndef SLSYSCALL_E
#define SLSYSCALL_E
/**
 * common extern function for slsyscall1.c and slsyscall2.c
 */
#include "types.h"
extern devreg_t * sldev_reg_loc(int devnum);
#endif