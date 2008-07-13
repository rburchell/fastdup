#include "main.h"
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

void ScanDirectory(const char *basepath, int bplen, const char *name)
{
	int pathlen = bplen + 1;
	if (name)
		pathlen += strlen(name);
	
	char *path = new char[pathlen + 1];
	strncpy(path, basepath, bplen);
	if (name)
		strcpy(path + bplen, name);
	path[pathlen - 1] = '/';
	path[pathlen] = 0;
	
	DIR *d = opendir(path);
	if (!d)
	{
		printf("opendir error (%s): %s\n", path, strerror(errno));
		delete []path;
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
				continue;
			}
			
			goto process_dir_item;
		}
		
		if (S_ISREG(st.st_mode))
		{
			if (!st.st_size)
				continue;
			
			FileReference *ref = new FileReference();
			ref->dir = path;
			// is reclen right?
			ref->file = new char[de->d_reclen + 1];
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


