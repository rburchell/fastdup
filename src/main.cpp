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
#include <getopt.h>

bool Interactive = false;
int treecount = 0;
char **scantrees = NULL;

static bool FileErrors = false;
static off_t FileSzWasted = 0;

static void ShowHelp(const char *bin);
static void DuplicateSet(FileReference *files[], unsigned long fcount, off_t filesize);
static bool ScanTreeError(const char *path, const char *error);

static bool ReadOptions(int argc, char **argv)
{
	Interactive = isatty(fileno(stdout));
	
	char opt;
	while ((opt = getopt(argc, argv, "ibh")) >= 0)
	{
		switch (opt)
		{
			case 'i':
				Interactive = true;
				break;
			case 'b':
				Interactive = false;
				break;
			case 'h':
			default:
				ShowHelp(argv[0]);
				return false;
		}
	}
	
	if (optind >= argc)
	{
		ShowHelp(argv[0]);
		return false;
	}
	
	char cwd[PATH_MAX];
	char tmp[PATH_MAX];
	if (!getcwd(cwd, PATH_MAX))
	{
		printf("Unable to get current directory: %s\n", strerror(errno));
		return false;
	}
	
	treecount = argc - optind;
	scantrees = new char*[treecount];
	for (int i = optind, j = 0; i < argc; i++, j++)
	{
		if (argv[i][0] != '/')
		{
			PathMerge(tmp, PATH_MAX, cwd, argv[i]);
			int len = strlen(tmp) + 2;
			scantrees[j] = new char[len];
			PathResolve(scantrees[j], len, tmp);
		}
		else
		{
			int len = strlen(argv[i]);
			scantrees[j] = new char[len + 2];
			PathResolve(scantrees[j], len + 2, argv[i]);
		}
		
		if (!DirectoryExists(scantrees[j]))
		{
			printf("Error: %s does not exist or is not a directory\n", argv[i]);
			return false;
		}
	}
	
	return true;
}

int main(int argc, char **argv)
{
	if (!ReadOptions(argc, argv))
		return EXIT_FAILURE;
	
	FastDup dupi;
	
	/* Initial scan - this step will recurse through the directory tree(s)
	 * and find each file we will be working with. These files are mapped
	 * by their size as this process runs through, so we will be provided
	 * with a list of files with the same sizes at the end. Those files
	 * are what we select for the deep comparison, which is where the magic
	 * really shows ;) */
	if (Interactive)
	{
		printf("Scanning for files... \E[s");
		fflush(stdout);
	}
	else
		printf("Scanning for files...\n");
	
	double starttm = SSTime();
	for (int i = 0; i < treecount; i++)
		dupi.AddDirectoryTree(scantrees[i], ScanTreeError);
	
	double endtm = SSTime();
	if (Interactive)
		printf("\E[u%lu files in %.3f seconds\n", dupi.FileCount, endtm - starttm);
	
	if (!dupi.FileCount)
	{
		printf("\nNo files found!\n");
		return EXIT_SUCCESS;
	}
	
	if (FileErrors && Interactive)
	{
		if (!PromptChoice("\nUnable to scan some files. Do you want to continue [y/n]? ", false))
			return EXIT_FAILURE;
	}
	
	dupi.EndScanning();
	printf("Comparing %lu set%s of files...\n\n", dupi.CandidateSetCount, (dupi.CandidateSetCount != 1) ? "s" : "");
	
	dupi.Run(DuplicateSet);
	endtm = SSTime();
	
	printf("Found %lu duplicate%s of %lu file%s (%sB wasted)\n", dupi.DupeFileCount - dupi.DupeSetCount, (dupi.DupeFileCount - dupi.DupeSetCount != 1) ? "s" : "", dupi.DupeSetCount,
		(dupi.DupeSetCount != 1) ? "s" : "", ByteSizes(FileSzWasted).c_str());
	printf("Scanned %lu file%s (%sB) in %.3f seconds\n", dupi.FileCount, (dupi.FileCount != 1) ? "s" : "", ByteSizes(dupi.FileSizeTotal).c_str(), endtm - starttm);
	
	return EXIT_SUCCESS;
}

bool ScanTreeError(const char *path, const char *error)
{
	if (Interactive)
		fprintf(stderr, "\E[0GError (%s): %s\nScanning for files... \E[s", path, error);
	else
		fprintf(stderr, "Error (%s): %s\n", path, error);
	FileErrors = true;
	return true;
}

void DuplicateSet(FileReference *files[], unsigned long fcount, off_t filesize)
{
	char fnbuf[PATH_MAX];
	
	FileSzWasted += filesize * (fcount-1);
	
	printf("%lu files (%sB/ea)\n", fcount, ByteSizes(filesize).c_str());
	for (unsigned long i = 0; i < fcount; ++i)
	{
		PathMerge(fnbuf, sizeof(fnbuf), files[i]->dir, files[i]->file);
		printf("\t%s\n", fnbuf);
	}
	
	printf("\n");
}

static void ShowHelp(const char *bin)
{
	printf(
		"fastdup " FASTDUP_VERSION " - http://dev.dereferenced.net/fastdup/\n\n"
		"Usage: %s [options] directory [directory..]\n"
		"Options:\n"
		"    -i                          Enable interactive prompts (default on terminals)\n"
		"    -b                          Disable interactive prompts (batch mode)\n"
		"    -h                          Show help and options\n"
		"\n", bin
	);
}

