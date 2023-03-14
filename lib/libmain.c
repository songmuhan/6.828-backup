// Called from entry.S to get us going.
// entry.S already took care of defining envs, pages, uvpd, and uvpt.

#include <inc/lib.h>

extern void umain(int argc, char **argv);

const volatile struct Env *thisenv;
const char *binaryname = "<unknown>";

void
libmain(int argc, char **argv)
{
	// set thisenv to point at our Env structure in envs[].
	// LAB 3: Your code here.
//    cprintf(" ******** libmain: envs %p, envid %p\n",envs, sys_getenvid());
	thisenv = &envs[ENVX(sys_getenvid())];
//    cprintf(" *** libmain: thisenv %p\n",thisenv);


	// save the name of the program so that panic() can use it
	if (argc > 0)
		binaryname = argv[0];

	// call user main routine
	umain(argc, argv);
//    panic("here");

	// exit gracefully
	exit();
}

