#ifndef INT_H
#define INT_H

#include "misc.h"
/* pseudo clock */
static long pclock;
/* pseudo clock semaphore */
static int pclocksem;
/* array of device semaphore, indexed via device number */
static int devsem[15];	/* 15 devices */
/*
 * array of stored completion status per device, by nucleus
 * used when device interrupt happen before SYS8
 */
static iores_t dev_compl_st[15];
/*
 * for determining whether interrupt happens before (weird) or after (normal) waitforio
 * int could be bool_t
 */
static bool_t waitforio_signal_received[15];

/* === private function === */
static void intterminalhandler(void);
static void intprinterhandler(void);
static void intfloppyhandler(void);
static void intdiskhandler(void);
static void inthandler(int devtype, int devnum);
static void intclockhandler(void);
static void intsemop(int *semAdd, int semop);
static void sleep(void);
/* === utility function === */
static int dev_type(int devnum);
static int abs_dev_num(int devtype, int devnum);
static state_t * interrupt_vector_area(int devtype);
static void update_cputime(proc_t *p, long *now);
static void before_int_handler(int interrupt_type);
static void manual_resume_from_interrupt(int devtype);

/* /\* device register *\/ */
/* typedef struct dev_t { */
/* 	int op;	/\* operation code *\/ */
/* 	int st;	/\* status code *\/ */
/* 	int devnum;	/\* device number *\/ */
/* } dev_t; */
/* /\* inheritance via aggregation *\/ */
/* typedef struct diskdev_t { */
/* 	dev_t abs; */
/* 	int *addr;	/\* disk address, track no. *\/ */
/* 	int *bufaddr;	/\* buffer address *\/ */
/* } diskdev_t; */
/* /\* floppy same as disk *\/ */
/* typedef struct diskdev_t floppydev_t; */
/* typedef struct printerdev_t { */
/* 	dev_t abs; */
/* 	int length; */
/* 	int *bufaddr;	/\* buffer address *\/ */
/* } printerdev_t ; */
/* /\* terminal same as printer *\/ */
/* typedef struct termdev_t { */
/* 	printerdev_t super; */
/* 	int last_seek;	/\* last read location in termini *\/ */
/* } termdev_t; */
#endif
