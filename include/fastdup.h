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

#endif
