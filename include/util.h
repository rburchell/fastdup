#ifndef UTIL_H
#define UTIL_H

#include <string>
#include <sys/types.h>

bool stricompare(const std::string &s1, const std::string &s2);
const char *PathMerge(char *buf, size_t size, const char *p1, const char *p2);
std::string PathMerge(const std::string &p1, const std::string &p2);

bool FileExists(const char *path);
bool DirectoryExists(const char *path);
std::string ByteSizes(off_t size);

size_t strlcpy(char *dst, const char *src, size_t siz);
size_t strlcat(char *dst, const char *src, size_t siz);

#include <sys/time.h>

inline double SSTime()
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return (double)tv.tv_sec + (tv.tv_usec / (double)1000000);
}

#endif
