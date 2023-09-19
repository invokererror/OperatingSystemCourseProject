/*
 * This code is from README or README.instructions
 */

#ifndef PANIC_C
#define PANIC_C
static char myerrbuf[128];

void
panic(register char *s)
{
	register char *i=myerrbuf;

	while ((*i++ = *s++) != '\0')
		;

	asm("  trap    #0");  /* stop the simulator */
	/* HALT(); */
}
#endif
