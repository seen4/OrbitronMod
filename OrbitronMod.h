#pragma once
#include "packages/minhook/include/MinHook.h"


#if (defined _M_X64) && (defined __x86_64__)
	#error This project can only be compiled under x86 mode.
#endif

#if (defined _M_IX86) || (defined __i386__)
	#pragma comment (lib,"packages/minhook/lib/libMinHook.x86.lib")
#endif


