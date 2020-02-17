/* This file is used by the fuzzer in order to prevent executing shell commands.
 */
#define _GNU_SOURCE
#include <time.h>

struct tm *(*orig_localtime)(const time_t *timep);

int
execl(const char *pathname, const char *arg, ...) {
	return 0;
}
