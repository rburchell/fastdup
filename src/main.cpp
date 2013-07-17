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
#include <getopt.h>
#include <limits.h>

bool Interactive = false;

static bool FileErrors = false;
static off_t FileSzWasted = 0;

static void ShowHelp(const char *bin);
static void DuplicateSet(FileReference *files[], unsigned long fcount, off_t filesize);
static bool ScanTreeError(const char *path, const char *error);

static int ReadOptions(int argc, char **argv, DupOptions &dopt)
{
	Interactive = isatty(fileno(stdout));
	
	char opt;
	while ((opt = getopt(argc, argv, "ibhc:")) >= 0)
	{
		switch (opt)
		{
			case 'i':
				Interactive = true;
				break;
			case 'b':
				Interactive = false;
				break;
			case 'c':
				if ((*optarg == '>' || *optarg == '+') && (dopt.sz_min = ParseHumanSize(optarg+1)));
				else if ((*optarg == '<' || *optarg == '-') && (dopt.sz_max = ParseHumanSize(optarg+1)));
				else if ((*optarg == '=') && (dopt.sz_eq = ParseHumanSize(optarg+1)));
				else
				{
					fprintf(stderr, "Error: Invalid argument '%s' to option -l\n", optarg);
					exit(EXIT_FAILURE);
				}
				break;
			case 'h':
			default:
				ShowHelp(argv[0]);
				exit(EXIT_FAILURE);
				break;
		}
	}
	
	if (optind >= argc)
	{
		ShowHelp(argv[0]);
		exit(EXIT_FAILURE);
	}
	
	return optind;
}

int main(int argc, char **argv)
{
	FastDup dupi;
	
	int pi = ReadOptions(argc, argv, dupi.opt);
	
	for (int i = pi; i < argc; ++i)
		dupi.AddDirectoryTree(argv[i]);
	
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
	dupi.DoScanning(ScanTreeError);
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
	
	printf("Comparing %lu set%s of files...\n\n", dupi.CandidateSetCount, (dupi.CandidateSetCount != 1) ? "s" : "");
	
	dupi.DoCompare(DuplicateSet);
	endtm = SSTime();
	
	printf("Found %lu duplicate%s of %lu file%s (%sB wasted)\n", dupi.DupeFileCount - dupi.DupeSetCount, (dupi.DupeFileCount - dupi.DupeSetCount != 1) ? "s" : "", dupi.DupeSetCount,
		(dupi.DupeSetCount != 1) ? "s" : "", ByteSizes(FileSzWasted).c_str());
	printf("Scanned %lu file%s (%sB) in %.3f seconds\n", dupi.FileCount, (dupi.FileCount != 1) ? "s" : "", ByteSizes(dupi.FileSizeTotal).c_str(), endtm - starttm);
	
	dupi.Cleanup();
	
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
	FileSzWasted += filesize * (fcount-1);
	
	printf("%lu files (%sB/ea)\n", fcount, ByteSizes(filesize).c_str());

	if (Interactive)
	{
		printf("\t<blank>\tAll\n");
	}

	for (unsigned long i = 0; i < fcount; ++i)
	{
		if (Interactive)
			printf("\t[%lu]\t%s\n", i, files[i]->FullPath());
		else
			printf("\t%s\n", files[i]->FullPath());
	}

	if (Interactive)
	{
ask:
		printf("Which file to keep?\n");
		std::string str;
		std::getline(std::cin, str);

		if (str.empty())
		{
			std::cout << "Keeping all" << std::endl;
		}
		else
		{
			char *serr;
			unsigned long fkeep = strtoul(str.c_str(), &serr, 10); // Which file to keep
			if (*serr != '\0' || fkeep >= fcount)
			{
				std::cerr << str << " is not valid input" << std::endl;
				goto ask;
			}

			std::cout << "Keeping " << fkeep << std::endl;
			for (unsigned long i = 0; i < fcount; ++i)
			{
				if (i == fkeep)
					continue;

				files[i]->Unlink();
			}
		}
	}
	
	printf("\n");
}

static void ShowHelp(const char *bin)
{
	printf(
		"fastdup " FASTDUP_VERSION " - http://dev.dereferenced.net/fastdup/\n\n"
		"Usage: %s [options] directory [directory..]\n"
		"Options:\n"
		"    -c [+-=]1[gmkb]             File conditions; size is greater (+), less (-), or\n"
		"                                    equal (=)\n"
		"    -i                          Enable interactive prompts (default on terminals)\n"
		"    -b                          Disable interactive prompts (batch mode)\n"
		"    -h                          Show help and options\n"
		"\n", bin
	);
}

