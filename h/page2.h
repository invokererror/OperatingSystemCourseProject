#ifndef PAGE2_H
#define PAGE2_H
#include "moretypes.h"
#define LOWWATERMARK 2
#define HIGHWATERMARK 4
#define UPTRACK_FLOPPY 99
#define UPSECTOR_FLOPPY 7
/* last place of clock hand of second chance replacement algorithm */
static int last_clock_hand;
static tracksectuple_t freesector_counter;
/* static void pageinit(void); */
/* static void putframe(int procnum); */
/* utility */
static int frame_list_pop(int freeframes[], int len);
static int frame_list_push(int item, int freeframes[], int len);
static tracksectuple_t desect_tracsec(int tracksecnum);
static int encapsulate_tracsec(tracksectuple_t tracsec);
#endif
