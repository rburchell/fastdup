#include "main.h"

std::map<off_t,FileReference*> SizeMap;
std::vector<FileReference*> SizeDups;
int DupeCount = 0, DupeSetCount = 0, FileCount = 0;

int treecount = 0;
char **scantrees = NULL;

int main(int argc, char **argv)
{
	if (argc < 2)
	{
		printf("Usage: %s <directory>\n", argv[0]);
		return EXIT_FAILURE;
	}
	
	{
		char cwd[PATH_MAX];
		char tmp[PATH_MAX];
		if (!getcwd(cwd, PATH_MAX))
		{
			printf("Unable to get current directory: %s\n", strerror(errno));
			return EXIT_FAILURE;
		}
		
		treecount = argc - 1;
		scantrees = new char*[treecount];
		for (int i = 1; i < argc; i++)
		{
			if (argv[i][0] != '/')
			{
				PathMerge(tmp, PATH_MAX, cwd, argv[i]);
				int len = strlen(tmp) + 2;
				scantrees[i - 1] = new char[len];
				PathResolve(scantrees[i - 1], len, tmp);
			}
			else
			{
				int len = strlen(argv[i]);
				scantrees[i - 1] = new char[len + 2];
				PathResolve(scantrees[i - 1], len + 2, argv[i]);
			}
			
			if (!DirectoryExists(scantrees[i - 1]))
			{
				printf("Error: %s does not exist or is not a directory\n", argv[i]);
				return EXIT_FAILURE;
			}
		}
	}
	
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
	
	SizeMap.clear();
	printf("Comparing %d set%s of files...\n\n", SizeDups.size(), (SizeDups.size() != 1) ? "s" : "");
	
	for (std::vector<FileReference*>::iterator it = SizeDups.begin(); it != SizeDups.end(); ++it)
	{
		DeepCompare(*it);
	}
	double endtm = SSTime();
	
	printf("Found %d duplicate%s of %d file%s. Scanned %d file%s in %.3f seconds.\n", DupeCount - DupeSetCount, (DupeCount - DupeSetCount != 1) ? "s" : "", DupeSetCount,
		(DupeSetCount != 1) ? "s" : "", FileCount, (FileCount != 1) ? "s" : "", endtm - starttm);
	
	return EXIT_SUCCESS;
}
