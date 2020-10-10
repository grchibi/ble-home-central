/**
 * ttlib.h
 *
 *    2020/10/03
 */

#include <string>
#include <cstdio>
#include <vector>

#ifdef DEBUG_BUILD
#define DEBUG_PUTS(str) puts(str)
#define DEBUG_PRINTF(fmt, ...) printf(fmt, __VA_ARGS__);
#else
#define DEBUG_PUTS(str)
#define DEBUG_PRINTF(fmt, ...)
#endif


template <typename ... Args>
std::string format(const std::string& fmt, Args ... args)
{
	size_t len = std::snprintf(nullptr, 0, fmt.c_str(), args ...);
	std::vector<char> buf(len + 1);
	std::snprintf(&buf[0], len + 1, fmt.c_str(), args ...);
	return std::string(&buf[0], &buf[0] + len);
}


// end of ttlib.h
