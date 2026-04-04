#ifndef _TESTS_SUPPORT_DEBUG_H
#define _TESTS_SUPPORT_DEBUG_H

#ifdef DEBUG
#include <signal.h>  // raise(SIGTRAP)

#define breakpoint raise(SIGTRAP);

#else

#define breakpoint

#endif

#define MAYBE_UNUSED __attribute__((unused))

#endif  // _TESTS_SUPPORT_DEBUG_H
