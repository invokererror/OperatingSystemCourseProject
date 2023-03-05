/*
 * This code is from README or README.instructions
 */

#ifndef PANIC_C
#define PANIC_C
char myerrbuf[128];

panic(s)
register char *s;
{
	register char *i=myerrbuf;

	while ((*i++ = *s++) != '\0')
		;

	asm("  trap    #0");  /* stop the simulator */
	/* HALT(); */
}
#endif
