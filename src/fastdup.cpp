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

extern double scanstart;

FastDup::FastDup()
	: FileCount(0), CandidateSetCount(0), DupeFileCount(0), DupeSetCount(0), FileSizeTotal(0)
{
}

FastDup::~FastDup()
{
	this->Cleanup();
}

void FastDup::AddDirectoryTree(const char *path)
{
	std::string v = path;
	char tmp[PATH_MAX+1];
	
	if (path[0] != '/')
	{
		char cwd[PATH_MAX + 1];
		if (!getcwd(cwd, PATH_MAX + 1))
			throw std::runtime_error("Unable to get current directory");
		
		v = PathMerge(cwd, v);
	}
	
	PathResolve(tmp, sizeof(tmp), v.c_str());
	
	if (!DirectoryExists(tmp))
		throw std::runtime_error("Path does not exist or is not a directory");
	
	DirList.push_back(tmp);
}

void FastDup::DoScanning(ErrorCallback errcb)
{
	scanstart = SSTime();
	
	for (std::vector<std::string>::iterator it = DirList.begin(); it != DirList.end(); ++it)
		this->ScanDirectory(it->c_str(), it->length(), NULL, errcb);
	
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
			delete safeit->second;
			FileSzMap.erase(safeit);
		}
		else
			++it;
	}
}

unsigned long FastDup::DoCompare(DupeSetCallback dupecb)
{
	DupeFileCount = DupeSetCount = 0;
	
	for (SizeRefMap::iterator i = FileSzMap.begin(); i != FileSzMap.end(); ++i)
	{
		this->Compare(i->second, i->first, dupecb);
	}
	
	return DupeSetCount;
}

void FastDup::Cleanup()
{
	for (SizeRefMap::iterator it = FileSzMap.begin(); it != FileSzMap.end(); ++it)
	{
		for (FileReference *p = it->second, *np; p; p = np)
		{
			np = p->next;
			delete p;
		}
	}
	
	FileSzMap.clear();
	FileCount = CandidateSetCount = DupeFileCount = DupeSetCount = 0;
	FileSizeTotal = 0;
}
