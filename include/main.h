#ifndef MAIN_H
#define MAIN_H

#include <cstdio>
#include <cstring>
#include <cerrno>
#include <unistd.h>
#include <cstdlib>
#include "util.h"
#include <map>
#include <vector>

struct FileReference
{
	const char *dir;
	char *file;
	FileReference *next;
};

extern std::map<off_t,FileReference*> SizeMap;
extern std::vector<FileReference*> SizeDups;
extern int DupeCount, DupeSetCount, FileCount;
extern bool FileErrors, Interactive;

extern int treecount;
extern char **scantrees;

void ScanDirectory(const char *basepath, int bplen, const char *name);
void DeepCompare(FileReference *first);

#endif
