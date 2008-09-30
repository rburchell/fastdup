/* Copyright (c) 2008, John Brooks <special@dereferenced.net>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * The name of dereferenced.net, FastDup, or its contributors may not be
 *       used to endorse or promote products derived from this software without
 *       specific prior written permission, with the exception of simple
 *       attribution.
 *     * Redistribution, in whole or in part, with or without modification, for
 *       or as part of a commercial product or venture is prohibited without
 *       explicit written consent from the author.
 *
 * THIS SOFTWARE IS PROVIDED BY JOHN BROOKS ''AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "main.h"
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

void ScanDirectory(const char *basepath, int bplen, const char *name)
{
	int pathlen = bplen;
	if (name)
		pathlen += strlen(name);
	
	char *path = new char[pathlen + 2];
	strncpy(path, basepath, bplen);
	if (name)
		strcpy(path + bplen, name);
	if (path[pathlen - 1] != '/')
		path[pathlen++] = '/';
	path[pathlen] = 0;
	
	DIR *d = opendir(path);
	if (!d)
	{
		printf("opendir error (%s): %s\n", path, strerror(errno));
		delete []path;
		FileErrors = true;
		return;
	}
	
	struct dirent *de;
	struct stat st;
	int dfd = dirfd(d);
	while ((de = readdir(d)) != NULL)
	{
		if (de->d_name[0] == '.' && (!de->d_name[1] || (de->d_name[1] == '.' && !de->d_name[2])))
			continue;
		
		if (fstatat(dfd, de->d_name, &st, AT_SYMLINK_NOFOLLOW) < 0)
		{
			printf("stat failure (%s%s): %s\n", path, de->d_name, strerror(errno));
			FileErrors = true;
			continue;
		}
		
 process_dir_item:
		if (S_ISLNK(st.st_mode))
		{
			/* We need to check if this link leads to a path under any tree we're scanning,
			 * to prevent false results (the same file/files would show twice) and link
			 * recusion. This gets complicated because links may be relative to the 
			 * directory they are in - so, we have to get the link, make it absolute, and 
			 * clean any special segments (.., etc) out for it to be safely compared to
			 * our paths.
			 */
			char lbuf[PATH_MAX + 1];
			char clbuf[PATH_MAX + 1];
			
			int lblen = readlinkat(dfd, de->d_name, lbuf, PATH_MAX);
			if (lblen <= 0)
			{
				printf("readlink failure (%s%s): %s\n", path, de->d_name, strerror(errno));
				FileErrors = true;
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
				printf("readlink failure(%s%s): invalid path\n", path, de->d_name);
				FileErrors = true;
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
			
			if (lstat(clbuf, &st) < 0)
			{
				printf("stat failure (%s): %s\n", clbuf, strerror(errno));
				FileErrors = true;
				continue;
			}
			
			goto process_dir_item;
		}
		
		if (S_ISREG(st.st_mode))
		{
			if (!st.st_size)
				continue;
			
			FileCount++;
			ScannedSize += st.st_size;
			
			FileReference *ref = new FileReference();
			ref->dir = path;
			ref->file = new char[strlen(de->d_name) + 1];
			strcpy(ref->file, de->d_name);
			ref->next = NULL;
			
			std::map<off_t,FileReference*>::iterator it = SizeMap.find(st.st_size);
			if (it == SizeMap.end())
			{
				SizeMap.insert(std::make_pair(st.st_size, ref));
			}
			else if (!it->second->next)
			{
				it->second->next = ref;
				SizeDups.push_back(it->second);
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
			ScanDirectory(path, pathlen, de->d_name);
		}
	}
	
	closedir(d);
}
