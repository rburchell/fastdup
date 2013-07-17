/* FastDup (http://dev.dereferenced.net/fastdup/)
 * Copyright 2013 - Robin Burchell <robin+git@viroteck.net>
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

#ifdef __APPLE__
# define NO_FSTATAT
# define NO_READLINKAT
#endif

static char errbuf[1024];
/* Used for interactive display */
double scanstart = 0, lasttime = 0;

void FastDup::ScanDirectory(const char *basepath, int bplen, const char *name, ErrorCallback cberror)
{
	/* Used in FileReferences to save memory by only storing the path once */
	DirReference *dirref = new DirReference(basepath, bplen, name);
	int pathlen = strlen(dirref->path);
	
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

	DIR *d = opendir(dirref->path);
	if (!d)
	{
		snprintf(errbuf, sizeof(errbuf), "Unable to open directory: %s", strerror(errno));
		cberror(dirref->path, errbuf);
		delete dirref;
		return;
	}

#ifdef NO_FSTATAT
	/* Lack of fstatat() requires us to actually change the working
	 * directory (using fchdir) in order to stat a file.
	 *
	 * We look up the current working directory and return to it later to not
	 * modify state.
	 */
	char returnpath[PATH_MAX + 1];
	char *cwd = getcwd(returnpath, PATH_MAX);
#endif

	struct dirent *de;
	struct stat st;
	int dfd = dirfd(d);

#ifdef NO_FSTATAT
	if (fchdir(dfd) < 0)
	{
		snprintf(errbuf, sizeof(errbuf), "Unable to fchdir: %s", strerror(errno));
		cberror(PathMerge(dirref->path, de->d_name).c_str(), errbuf);
		return;
	}
#endif

	while ((de = readdir(d)) != NULL)
	{
		/* Eliminate . and .. */
		if (de->d_name[0] == '.' && (!de->d_name[1] || (de->d_name[1] == '.' && !de->d_name[2])))
			continue;
		
#ifndef NO_FSTATAT
		/* fstatat() avoids lookups and permissions checks, since we already have a dirfd */
		if (fstatat(dfd, de->d_name, &st, AT_SYMLINK_NOFOLLOW) < 0)
#else
		if (lstat(de->d_name, &st) < 0)
#endif
		{
			snprintf(errbuf, sizeof(errbuf), "Unable to read file information: %s", strerror(errno));
			cberror(PathMerge(dirref->path, de->d_name).c_str(), errbuf);
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

#ifndef NO_READLINKAT
			int lblen = readlinkat(dfd, de->d_name, lbuf, PATH_MAX);
#else
			int lblen = readlink(de->d_name, lbuf, PATH_MAX);
#endif
			if (lblen <= 0)
			{
				snprintf(errbuf, sizeof(errbuf), "Unable to read link information: %s", strerror(errno));
				cberror(PathMerge(dirref->path, de->d_name).c_str(), errbuf);
				continue;
			}
			lbuf[lblen] = 0;
			
			if (lbuf[0] != '/')
			{
				// Relative path, prepend this directory
				memmove(lbuf + pathlen, lbuf, lblen + 1);
				memcpy(lbuf, dirref->path, pathlen);
				lblen += pathlen;
			}
			
			if (!PathResolve(clbuf, PATH_MAX + 1, lbuf))
			{
				snprintf(errbuf, sizeof(errbuf), "Unable to resolve invalid link path");
				cberror(lbuf, errbuf);
				continue;
			}
			
			/* If the destination of this link is within a path we will scan, don't follow it
			 * to avoid false positives. */
			bool invalid = false;
			for (std::vector<std::string>::iterator it = DirList.begin(); it != DirList.end(); ++it)
			{
				if (strncmp(it->c_str(), clbuf, ((*it)[it->length()-1] == '/') ? it->length()-1 : it->length()) == 0)
				{
					invalid = true;
					break;
				}
			}
			
			if (invalid)
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
			
			if (opt.sz_eq && (st.st_size != opt.sz_eq))
				continue;
			else if (opt.sz_min && (st.st_size < opt.sz_min))
				continue;
			else if (opt.sz_max && (st.st_size > opt.sz_max))
				continue;
			
			FileCount++;
			FileSizeTotal += st.st_size;
			
			/* Create FileReference */
			FileReference *ref = new FileReference(dirref, de->d_name);
			
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
			this->ScanDirectory(dirref->path, pathlen, de->d_name, cberror);
		}
	}
	
	closedir(d);
	if (!dirref->RefCount())
		delete dirref;
#ifdef NO_FSTATAT
	chdir(cwd);
#endif
}
