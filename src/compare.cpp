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
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

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
void FastDup::Compare(FileReference *first, DupeSetCallback callback)
{
	// Buffer for quick creation of the file's full path
	char fnbuf[PATH_MAX];
	
	int fcount = 0;
	for (FileReference *p = first; p; p = p->next)
		++fcount;
	
	/* FDs */
	int ffd[fcount];
	/* File reference map, used afterwards to map back to the real file */
	FileReference *frmap[fcount];
	/* Data buffers */
	char *rrdbuf = new char[fcount * 65535];
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
		
		PathMerge(fnbuf, sizeof(fnbuf), p->dir, p->file);
		
		rdbuf[i] = rrdbuf + (65535 * i);
		
		if ((ffd[i] = open(fnbuf, O_RDONLY)) < 0)
		{
			printf("Unable to open file '%s': %s\n", fnbuf, strerror(errno));
			exit(EXIT_FAILURE);
		}
	}
	
	// Loop over blocks of the files, which will be compared
	for (;;)
	{
		for (i = 0; i < fcount; i++)
		{
			if (omit[i])
				continue;
			
			rdbp = read(ffd[i], rdbuf[i], 65535);
			
			if (rdbp < 0)
			{
				printf("%d: Read error: %s\n", i, strerror(errno));
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
				
				int flagpos = int(((fcount-1)*i)-(i*(i/2.0-0.5))+(j-i)-1);
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
					
					int kflagpos = int(((fcount-1)*k)-(k*(k/2.0-0.5))+(j-k)-1);
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
			if (!omit[j] && matchflag[int(((fcount-1)*i)-(i*(i/2.0-0.5))+(j-i)-1)])
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
			callback(re, relen);
		}
	}
	
	return;
}
