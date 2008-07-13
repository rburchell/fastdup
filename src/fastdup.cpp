#include "main.h"

std::map<off_t,FileReference*> SizeMap;
std::vector<FileReference*> SizeDups;

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
				int len = strlen(tmp) + 1;
				scantrees[i - 1] = new char[len];
				PathResolve(scantrees[i - 1], len, tmp);
			}
			else
				scantrees[i - 1] = argv[i];
		}
	}
	
	/* Initial scan - this step will recurse through the directory tree(s)
	 * and find each file we will be working with. These files are mapped
	 * by their size as this process runs through, so we will be provided
	 * with a list of files with the same sizes at the end. Those files
	 * are what we select for the deep comparison, which is where the magic
	 * really shows ;) */
	printf("Scanning... this may take some time\n");
	for (int i = 1; i < argc; i++)
		ScanDirectory(argv[i], strlen(argv[i]), NULL);
	
	if (SizeMap.empty())
	{
		printf("\nNo files found!\n");
		return EXIT_SUCCESS;
	}
	
	printf("\nInitial scanning complete on %d files\n", SizeMap.size());
	printf("Running deep scan on %d sets of files\n\n", SizeDups.size());
	
	SizeMap.clear();
	
	for (std::vector<FileReference*>::iterator it = SizeDups.begin(); it != SizeDups.end(); ++it)
	{
		DeepCompare(*it);
	}
	
	return EXIT_SUCCESS;
}
