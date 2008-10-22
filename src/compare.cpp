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
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <math.h>

/* Deep comparison is the clever technique upon which the entire
 * concept of fastdup is based.
 *
 * The normal method of comparing files is to hash them and compare
 * their hashes - this has the benefit of being O(n) (whereas
 * straight comparisons are O(n^n)), but is somewhat CPU intensive
 * (though usually still disk bound) and not infallable (collisions).
 * The real problem with hashing when used on large filesets is that,
 * while it can run in O(n) time, it cannot run in anything less.
 * That is, every file must be hashed entirely. When dealing with
 * large files or large numbers of files, this can be come seriously
 * slow.
 *
 * Deep comparison compares files byte by byte, but does so very
 * intelligently. We've already got files in sets by filesize, which
 * dramatically reduces the number of comparisons necessary. The
 * idea is to compare every file to every other file in blocks, but
 * with the use of a number of tricks to reduce the number of
 * comparisons actually necessary. As a result, this method will work
 * fastest on files that are not duplicates (which will be eliminated
 * as soon as they cannot match anything else), which are far more
 * common in most datasets than non-duplicates. See the code for
 * details on how exactly this is optimized down from needing full
 * O(n^n) comparisons on the contents of every file - there are many
 * methods, some of which are quite intricate.
 */

#define BLOCKSIZE 65536

void FastDup::Compare(FileReference *first, off_t filesize, DupeSetCallback callback)
{
	int blockcount = ceil(filesize / BLOCKSIZE);

	int fcount = 0;
	for (FileReference *p = first; p; p = p->next)
		++fcount;
	
	bool progress = (Interactive && (filesize*fcount >= 3*1048576));
	int ipint = 0;
	if (progress)
	{
		ipint = ceil(blockcount/100);
		printf("Comparing %d files... \E[s0%%", fcount);
		fflush(stdout);
	}
	
	/* FDs */
	int ffd[fcount];
	/* File reference map, used afterwards to map back to the real file */
	FileReference *frmap[fcount];
	/* Data buffers */
	char *rrdbuf = new char[fcount * BLOCKSIZE];
	char *rdbuf[fcount];
	ssize_t rdbp = 0;
	/* Matchflag is 1 for each file pair that may still match, 0
	 * for pairs that cannot match, or 2 indicating that the current
	 * block of both files is known to match (which will be reset
	 * to 1 before the next block).
	 *
	 * To save space, a somewhat complex equation is used to calculate
	 * the positions of the flag for a given pair of files (i and j).
	 * The result of this equation is to map them so that, if there are
	 * 3 files total, [0] is 0 & 1, [1] is 0 & 2, [2] is 1 & 2, and
	 * no space is wasted.
	 *
	 * To calculate the position for files i and j with a maximum of f
	 * files, assuming that j > i, use:
	 *     int(((f-1)*i)-(i*(i/2.0-0.5))+(j-i)-1);
	 */
	char matchflag[(fcount*(fcount-1))/2];
#define FLAGPOS(i,j) int(((fcount-1)*(i))-((i)*((i)/2.0-0.5))+((j)-(i))-1)
	/* Holds the result for the matching of the current file (i) against
	 * all other TESTED files, by the index of the second file (j). Note
	 * that many parts of this may be left untouched due to other
	 * optimizations preventing a comparison. Use only when that
	 * information is known. Values are identical to the return of
	 * memcmp; -1 for i < j, 0 for i == j, 1 for i > j.
	 *
	 * Used to avoid comparisons by finding when two other files cannot
	 * match this block, or must match this block (i.e. i > j and i < k,
	 * or i == j and i == k).
	 */
	int mresult[fcount];
	/* Omit is true for files that have no possible matches left. These
	 * are not read or processed at all. */
	bool omit[fcount];
	/* Number of files omitted; used to end early if we run out of possible
	 * matches */
	int omitted = 0;
	/* Used to calculate omit, by keeping track of the number of comparisons
	 * with other files that have been skipped against this file. When this
	 * reaches fcount for a given file index, that file may be omitted. */
	int skipcount[fcount];
	
	memset(matchflag, 1, sizeof(matchflag));
	memset(omit, false, sizeof(omit));
	memset(skipcount, 0, sizeof(skipcount));
	
	int i = 0, j = 0;
	for (FileReference *p = first; p; p = p->next, ++i)
	{
		frmap[i] = p;
		const char *fn = p->FullPath();
		
		rdbuf[i] = rrdbuf + (BLOCKSIZE * i);
		
		if ((ffd[i] = open(fn, O_RDONLY)) < 0)
		{
			printf("Unable to open file '%s': %s\n", fn, strerror(errno));
			// Note: if handled in any other way, clean up open FDs and rrdbuf
			exit(EXIT_FAILURE);
		}
	}
	
	// Loop over blocks of the files, which will be compared
	int ti = 0;
	for (int block = 0;; ++block)
	{
		if (progress && (++ti == ipint))
		{
			ti = 0;
			fprintf(stdout, "\E[u%d%%", int((double(block+1)/blockcount)*100));
			fflush(stdout);
		}
		
		for (i = 0; i < fcount; i++)
		{
			if (omit[i])
				continue;
			
			rdbp = read(ffd[i], rdbuf[i], BLOCKSIZE);
			
			if (rdbp < 0)
			{
				printf("%d: Read error: %s\n", i, strerror(errno));
				// Note: if handled in any other way, cleanup
				exit(EXIT_FAILURE);
			}
			else if (!rdbp)
			{
				// All files are assumed to be equal in size
				break;
			}
		}
		
		if (!rdbp)
			break;
		
		for (i = 0; i < fcount; i++)
		{
			if (omit[i])
				continue;
			
			for (j = i + 1; j < fcount; j++)
			{
				if (omit[j])
					continue;
				
				int flagpos = FLAGPOS(i, j);
				if (!matchflag[flagpos])
					continue;
				else if (matchflag[flagpos] == 2)
				{
					matchflag[flagpos] = 1;
					mresult[j] = 0;
				}
				else
					mresult[j] = memcmp(rdbuf[i], rdbuf[j], rdbp);
				
				for (int k = j - 1; k > i; --k)
				{
					if (omit[k])
						continue;
					
					int kflagpos = FLAGPOS(k, j);
					if (!matchflag[kflagpos])
						continue;
					
					if (mresult[k] != mresult[j])
					{
						matchflag[kflagpos] = 0;
						++skipcount[j];
						if (++skipcount[k] == fcount - 1)
						{
							omit[k] = true;
							close(ffd[k]);
							ffd[k] = -1;
							omitted++;
						}
					}
					else if (mresult[k] == 0 && mresult[j] == 0)
					{
						/* These will be equal, so we can avoid comparing them; signal this by setting
						 * matchflag to 2 for this block (it is one-shot, and will be reset before the
						 * next block). This is a huge saver when there is more than one duplicate of
						 * the same file. */
						matchflag[kflagpos] = 2;
					}
				}
				
				if (mresult[j] != 0)
				{
					matchflag[flagpos] = 0;
					skipcount[i]++;
					if (++skipcount[j] == fcount - 1)
					{
						omit[j] = true;
						close(ffd[j]);
						ffd[j] = -1;
						omitted++;
					}
				}
			}
			
			if (skipcount[i] == fcount - 1)
			{
				omit[i] = true;
				close(ffd[i]);
				ffd[i] = -1;
				if (++omitted == fcount)
					goto endscan;
			}
		}
	}
	
 endscan:
 	delete []rrdbuf;
 	
 	/* Cleanup and gather/process results */
 	
 	if (progress)
 		fputs("\E[0G\E[K", stdout);
 	
 	FileReference *re[fcount];
 	unsigned long relen = 0;
 	
	for (i = 0; i < fcount; i++)
	{
		if (omit[i])
			continue;
		close(ffd[i]);
		
		relen = 0;
		for (int j = i + 1; j < fcount; j++)
		{
			if (!omit[j] && matchflag[FLAGPOS(i,j)])
			{
				if (!relen)
					re[relen++] = frmap[i];
				re[relen++] = frmap[j];
				
				/* Prevent this from showing up again in our results */
				omit[j] = true;
				close(ffd[j]);
			}
		}
		
		if (relen)
		{
			DupeSetCount++;
			DupeFileCount += relen;
			callback(re, relen, filesize);
		}
	}
	
	return;
}
#undef FLAGPOS
