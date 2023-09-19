#ifndef SLSYSCALL1_E
#define SLSYSCALL1_E
#include "types.h"
extern void readfromterminal(int termnum);
extern void writetoterminal(int termnum);
extern void delay(int termnum);
extern void gettimeofday(int termnum);
extern void terminate(int termnum);
#endif