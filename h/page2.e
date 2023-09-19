#ifndef PAGE2_E
#define PAGE2_E
extern int getfreeframe(int procnum, int page, int seg);
extern void pagein(int procnum, int page, int seg, int frame);
extern void pagedaemon(void);
extern int getfreesector(void);
/* help me */
extern void pageinit(void);
extern void putframe(int term);
#endif
