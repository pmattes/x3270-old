/* Hard-coded conf.h for wc3270 */

#define LIBX3270DIR "."	/*wrong*/
#if defined(_WIN32)
#include <sys/time.h>
#if 0
extern int gettimeofday(struct timeval *tv, void *ignored);
#endif
#endif
#define X3270_TN3270E	1
#define X3270_TRACE	1
#define X3270_FT	1
#define X3270_ANSI	1
#define X3270_PRINTER	1
