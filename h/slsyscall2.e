#ifndef SLSYSCALL2_E
#define SLSYSCALL2_E
void virtualv(int semaddr);
void virtualp(int semaddr);
void diskput(int termnum, char *termbuf, int tracknum, int sectornum);
void diskget(int termnum, char *virtbuf, int tracknum, int sectornum);
void diskdaemon(void);
#endif