#ifndef SLSYSCALL2_H
#define SLSYSCALL2_H
#include "moreconst.h"
#include "moretypes.h"

#define DISKPUT 0
#define DISKGET 1
/*
 * elevator direction
 */
#define SCANUP 0
#define SCANDOWN 1

#define UPTRACK 99

typedef struct virt_phys_sem {
	int phys_sem;
	int virt_sem;
} virt_phys_sem_t;
static int diskwait[MAXTPROC];
/* disk queue */
typedef struct diskrequest {
	int tracknum;
	int sectornum;
	char *physbuf;
	int put_or_get;
	/* passed from diskdaemon
	 */
	int io_sta;
	int io_len;
} diskrequest_t;
static int elevdirection;
static int cur_track_loc;
#endif
