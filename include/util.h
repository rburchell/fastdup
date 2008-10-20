#ifndef UTIL_H
#define UTIL_H

/* FastDup (http://dev.dereferenced.net/fastdup/)
 * Copyright 2008 - John Brooks <special@dereferenced.net>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * A copy of the GNU General Public License is available in the
 * LICENSE file distributed with this program.
 */

#include <string>
#include <sys/types.h>

bool stricompare(const std::string &s1, const std::string &s2);
const char *PathMerge(char *buf, size_t size, const char *p1, const char *p2);
std::string PathMerge(const std::string &p1, const std::string &p2);
size_t PathResolve(char *buf, size_t bufsz, const char *path);

bool FileExists(const char *path);
bool DirectoryExists(const char *path);
std::string ByteSizes(off_t size);

bool PromptChoice(const char *prompt, bool fallback);

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
