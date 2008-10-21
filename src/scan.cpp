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

#include "main.h"
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

static char errbuf[1024];
/* Used for interactive display */
static double scanstart = 0, lasttime = 0;

void FastDup::AddDirectoryTree(const char *path, ErrorCallback cberr)
{
	scanstart = SSTime();
	this->ScanDirectory(path, strlen(path), NULL, cberr);
}

void FastDup::ScanDirectory(const char *basepath, int bplen, const char *name, ErrorCallback cberror)
{
	/* Calculate length of this path (basepath + name) */
	int pathlen = bplen;
	if (name)
		pathlen += strlen(name);
	
	bool addedfile = false;
	
	/* Merge basepath and name to form the path of this directory.
	 * This value is stored in FileReferences to avoid having to
	 * allocate it once per file (wasteful of memory). */
	char *path = new char[pathlen + 2];
	strncpy(path, basepath, bplen);
	if (name)
		strcpy(path + bplen, name);
	if (path[pathlen - 1] != '/')
		path[pathlen++] = '/';
	path[pathlen] = 0;
	
	if (Interactive)
	{
		double now = SSTime();
		if (now - lasttime >= 0.1)
		{
			/* Restore saved position, then overwrite with the new information */
			printf("\E[u%lu files in %.3f seconds", FileCount, now - scanstart);
			fflush(stdout);
			lasttime = now;
		}
	}
	
	DIR *d = opendir(path);
	if (!d)
	{
		snprintf(errbuf, sizeof(errbuf), "Unable to open directory: %s", strerror(errno));
		cberror(path, errbuf);
		delete []path;
		return;
	}
	
	struct dirent *de;
	struct stat st;
	int dfd = dirfd(d);
	while ((de = readdir(d)) != NULL)
	{
		/* Eliminate . and .. */
		if (de->d_name[0] == '.' && (!de->d_name[1] || (de->d_name[1] == '.' && !de->d_name[2])))
			continue;
		
		/* fstatat() avoids lookups and permissions checks, since we already have a dirfd */
		if (fstatat(dfd, de->d_name, &st, AT_SYMLINK_NOFOLLOW) < 0)
		{
			snprintf(errbuf, sizeof(errbuf), "Unable to read file information: %s", strerror(errno));
			cberror(PathMerge(path, de->d_name).c_str(), errbuf);
			continue;
		}
		
 process_dir_item:
		if (S_ISLNK(st.st_mode))
		{
			/* We need to check if this link leads to a path under any tree we're scanning,
			 * to prevent false results (the same file/files would show twice) and link
			 * recursion. This gets complicated because links may be relative to the 
			 * directory they are in - so, we have to get the link, make it absolute, and 
			 * clean any special segments (.., etc) out for it to be safely compared to
			 * our paths.
			 */
			char lbuf[PATH_MAX + 1];
			char clbuf[PATH_MAX + 1];
			
			int lblen = readlinkat(dfd, de->d_name, lbuf, PATH_MAX);
			if (lblen <= 0)
			{
				snprintf(errbuf, sizeof(errbuf), "Unable to read link information: %s", strerror(errno));
				cberror(PathMerge(path, de->d_name).c_str(), errbuf);
				continue;
			}
			lbuf[lblen] = 0;
			
			if (lbuf[0] != '/')
			{
				// Relative path, prepend this directory
				memmove(lbuf + pathlen, lbuf, lblen + 1);
				memcpy(lbuf, path, pathlen);
				lblen += pathlen;
			}
			
			if (!PathResolve(clbuf, PATH_MAX + 1, lbuf))
			{
				snprintf(errbuf, sizeof(errbuf), "Unable to resolve invalid link path");
				cberror(lbuf, errbuf);
				continue;
			}
			
			int i;
			for (i = 0; i < treecount; i++)
			{
				int len = strlen(scantrees[i]);
				if (scantrees[i][len - 1] == '/')
					len--;
				
				if (strncmp(scantrees[i], clbuf, (len < lblen) ? len : lblen) == 0)
					break;
			}
			
			if (i < treecount)
				continue;
			
			/* Link resolved; stat the destination and reprocess with that */
			if (lstat(clbuf, &st) < 0)
			{
				snprintf(errbuf, sizeof(errbuf), "Unable to read file information for link destination: %s", strerror(errno));
				cberror(clbuf, errbuf);
				continue;
			}
			
			goto process_dir_item;
		}
		
		if (S_ISREG(st.st_mode))
		{
			if (!st.st_size)
				continue;
			
			FileCount++;
			FileSizeTotal += st.st_size;
			addedfile = true;
			
			/* Create FileReference */
			FileReference *ref = new FileReference();
			ref->dir = path;
			ref->file = new char[strlen(de->d_name) + 1];
			strcpy(ref->file, de->d_name);
			ref->next = NULL;
			
			/* Add FileReference to our list; if a file with this size is known,
			 * the reference is appended to the linked list. */
			std::map<off_t,FileReference*>::iterator it = FileSzMap.find(st.st_size);
			if (it == FileSzMap.end())
			{
				FileSzMap.insert(std::make_pair(st.st_size, ref));
			}
			else if (!it->second->next)
			{
				it->second->next = ref;
				CandidateSetCount++;
			}
			else
			{
				FileReference *i = it->second;
				while (i->next != NULL)
					i = i->next;
				i->next = ref;
			}
		}
		else if (S_ISDIR(st.st_mode))
		{
			this->ScanDirectory(path, pathlen, de->d_name, cberror);
		}
	}
	
	closedir(d);
	if (!addedfile)
		delete []path;
}

void FastDup::EndScanning()
{
	/* This technique was created by the developers of InspIRCd 
	 * (http://www.inspircd.org) to allow deleting items from a STL
	 * container while iterating over it. It has been tested on many
	 * STL implementations without flaw. */
	for (SizeRefMap::iterator it = FileSzMap.begin(), safeit; it != FileSzMap.end();)
	{
		if (!it->second->next)
		{
			safeit = it;
			++it;
			delete []safeit->second->file;
			delete safeit->second;
			FileSzMap.erase(safeit);
		}
		else
			++it;
	}
}
