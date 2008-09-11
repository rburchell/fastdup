#include "main.h"
#include <getopt.h>

std::map<off_t,FileReference*> SizeMap;
std::vector<FileReference*> SizeDups;
int DupeCount = 0, DupeSetCount = 0, FileCount = 0;
bool FileErrors = false;
bool Interactive = false;

int treecount = 0;
char **scantrees = NULL;

bool ReadOptions(int argc, char **argv)
{
	Interactive = isatty(fileno(stdout));
	
	char opt;
	while ((opt = getopt(argc, argv, "ib")) >= 0)
	{
		switch (opt)
		{
			case 'i':
				Interactive = true;
				break;
			case 'b':
				Interactive = false;
				break;
			default:
				return false;
		}
	}
	
	if (optind >= argc)
	{
		printf("Usage: %s [options] directory [directory..]\n", argv[0]);
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
	
	/* Initial scan - this step will recurse through the directory tree(s)
	 * and find each file we will be working with. These files are mapped
	 * by their size as this process runs through, so we will be provided
	 * with a list of files with the same sizes at the end. Those files
	 * are what we select for the deep comparison, which is where the magic
	 * really shows ;) */
	printf("Scanning for files...\n");
	double starttm = SSTime();
	for (int i = 0; i < treecount; i++)
		ScanDirectory(scantrees[i], strlen(scantrees[i]), NULL);
	
	if (SizeMap.empty())
	{
		printf("\nNo files found!\n");
		return EXIT_SUCCESS;
	}
	
	if (FileErrors && Interactive)
	{
		if (!PromptChoice("\nUnable to scan some files. Do you want to continue [y/n]? ", false))
			return EXIT_FAILURE;
	}
	
	SizeMap.clear();
	printf("Comparing %lu set%s of files...\n\n", SizeDups.size(), (SizeDups.size() != 1) ? "s" : "");
	
	for (std::vector<FileReference*>::iterator it = SizeDups.begin(); it != SizeDups.end(); ++it)
	{
		DeepCompare(*it);
	}
	double endtm = SSTime();
	
	printf("Found %d duplicate%s of %d file%s. Scanned %d file%s in %.3f seconds.\n", DupeCount - DupeSetCount, (DupeCount - DupeSetCount != 1) ? "s" : "", DupeSetCount,
		(DupeSetCount != 1) ? "s" : "", FileCount, (FileCount != 1) ? "s" : "", endtm - starttm);
	
	return EXIT_SUCCESS;
}
