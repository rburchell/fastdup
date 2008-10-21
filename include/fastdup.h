#ifndef FASTDUP_H
#define FASTDUP_H

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

#include <map>

class DirReference;
class FileReference;

class FastDup
{
 public:
	typedef void (*DupeSetCallback)(FileReference *files[], unsigned long count, off_t filesize);
	typedef bool (*ErrorCallback)(const char *file, const char *error);
	
 private:
	typedef std::map<off_t,FileReference*> SizeRefMap;
	
	SizeRefMap FileSzMap;
	
	/* scan.cpp */
	void ScanDirectory(const char *basepath, int bplen, const char *name, ErrorCallback cberr);
	/* compare.cpp */
	void Compare(FileReference *first, off_t filesize, DupeSetCallback callback);
	
 public:
	unsigned long FileCount, CandidateSetCount, DupeFileCount, DupeSetCount;
	off_t FileSizeTotal;
	
	FastDup();
	~FastDup();
	
	/* scan.cpp */
	void AddDirectoryTree(const char *path, ErrorCallback cberr);
	void EndScanning();
	
	unsigned long Run(DupeSetCallback dupecb);
	
	void Cleanup();
};

class DirReference
{
 private:
	unsigned refs;
 public:
	char *path;
	
	DirReference(char *v)
		: refs(0), path(v)
	{
	}
	
	DirReference(const char *base, int baselen, const char *seg)
		: refs(0)
	{
		if (!seg)
		{
			path = new char[baselen + 2];
			memcpy(path, base, baselen + 1);
			
			if (path[baselen - 1] != '/')
			{
				path[baselen++] = '/';
				path[baselen] = 0;
			}
		}
		else
		{
			int len = baselen + strlen(seg);
			path = new char[len + 3];
			
			PathMerge(path, len + 3, base, seg);
			if (path[len - 1] != '/')
			{
				path[len++] = '/';
				path[len] = 0;
			}
		}
	}
	
	~DirReference()
	{
		if (path)
			delete []path;
	}
	
	void AddRef()
	{
		++refs;
	}
	
	void RemoveRef()
	{
		if (--refs < 1)
			delete this;
	}
	
	unsigned RefCount()
	{
		return refs;
	}
};

class FileReference
{
 private:
	FileReference(const FileReference &r)
	{
	}
 public:
	DirReference *dir;
	char *file;
	FileReference *next;
	
	FileReference(DirReference *dr, const char *fn)
		: dir(dr), next(NULL)
	{
		dir->AddRef();
		
		int len = strlen(fn);
		file = new char[len + 1];
		memcpy(file, fn, len + 1);
	}
	
	~FileReference()
	{
		dir->RemoveRef();
		delete []file;
	}
	
	const char *FullPath()
	{
		static char fnbuf[PATH_MAX];
		PathMerge(fnbuf, sizeof(fnbuf), dir->path, file);
		return fnbuf;
	}
};

#endif
