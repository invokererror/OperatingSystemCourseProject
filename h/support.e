#ifndef SUPPORT_E
#define SUPPORT_E
extern void p1(void);
/*
 * seg0 page labels
 */
/* nucleus text */
extern int start(void);
extern int endt0(void);
/* sl text */
extern int startt1(void);
extern int etext(void);
/* nucleus data */
extern int startd0(void);
extern int endd0(void);
/* sl data */
extern int startd1(void);
extern int edata(void);
/* nucleus BSS */
extern int startb0(void);
extern int endb0(void);
/* sl BSS */
extern int startb1(void);
extern int end(void);
/* stack */
extern int Tsysstack[];
extern int Tmmstack[];
extern int Scronstack;
/* === part2 === */
extern int Spagedframe;
extern int Sdiskframe;
/* === */
/* free pool */
extern int pf_start;
#endif