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

FastDup::FastDup()
	: FileCount(0), CandidateSetCount(0), DupeFileCount(0), DupeSetCount(0), FileSizeTotal(0)
{
}

FastDup::~FastDup()
{
}

unsigned long FastDup::Run(DupeSetCallback dupecb)
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
