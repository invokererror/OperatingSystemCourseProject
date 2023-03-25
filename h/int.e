#ifndef INT_E
#define INT_E
#include "types.h"
#include "procq.h"
extern void waitforpclock(state_t *old);
extern void waitforio(state_t *old);
extern void intinit(void);
extern void intdeadlock(void);
extern void intschedule(void);
/* nucleus module functions */
extern void waitforio_write_to_proct(proc_t *inted_p);
#endif