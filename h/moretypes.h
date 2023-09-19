#ifndef MORETYPES_H
#define MORETYPES_H
#include "misc.h"
/* 3-valued logic */
/* typedef enum tvl { */
/* 	UNKNOWN, */
/* 	FALSE, */
/* 	TRUE */
/* } tvl_t; */
/* field type for frameinfo[] */
typedef struct frameinfo {
	bool_t is_present;	/* if used in memory in some page by some process */
	int termnum;
	int pagenum;
	int segnum;
	int tracknum;
	int sectornum;
} frameinfo_t;
/* namedtuple(track, sector) */
typedef struct tracksectuple {
	int tracknum;
	int sectornum;
} tracksectuple_t;
#endif
