#include "md5.h"
#include <cstdio>
#include <cstring>
#include <cerrno>
#include <unistd.h>
#include <cstdlib>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <map>
#include <string>
#include <vector>

struct FileReference
{
	const char *dir;
	char *file;
	FileReference *next;
};

std::map<off_t,FileReference*> SizeMap;
std::vector<FileReference*> SizeDups;

void ScanDirectory(const char *path);

int main(int argc, char **argv)
{
	if (argc < 2)
	{
		printf("Usage: %s <directory>\n", argv[0]);
		return EXIT_FAILURE;
	}
	
	for (int i = 1; i < argc; i++)
		ScanDirectory(argv[i]);
	
	printf("Initial scanning complete on %d files\n", SizeMap.size());
	printf("Running deep scan on %d sets of files\n", SizeDups.size());
	
	SizeMap.clear();
	
	MD5 md5;
	unsigned char hbuf[512 * 1024];
	ssize_t rd;
	std::map<std::string,FileReference*> HashList;
	for (std::vector<FileReference*>::iterator it = SizeDups.begin(); it != SizeDups.end(); ++it)
	{
		for (FileReference *p = *it, *np; p; p = np)
		{
			np = p->next;
			
			int ffd = open((std::string(p->dir) + "/" + p->file).c_str(), O_RDONLY);
			if (ffd < 0)
			{
				printf("Unable to open file '%s/%s': %s\n", p->dir, p->file, strerror(errno));
				continue;
			}
			
			md5.Init();
			while ((rd = read(ffd, hbuf, sizeof(hbuf))) > 0)
				md5.Append(hbuf, rd);

			close(ffd);
			
			if (rd < 0)
			{
				printf("Unable to read file '%s/%s': %s\n", p->dir, p->file, strerror(errno));
				continue;
			}
			
			std::string hash((const char *) md5.Finish(), 16);
			std::map<std::string,FileReference*>::iterator hit = HashList.find(hash);
			if (hit == HashList.end())
			{
				p->next = NULL;
				HashList.insert(std::make_pair(hash, p));
			}
			else
			{
				FileReference *i = hit->second;
				while (i->next != NULL)
					i = i->next;
				i->next = p;
				p->next = NULL;
			}
		}
		
		printf("\n");
		char obuf[33];
		for (std::map<std::string,FileReference*>::iterator ht = HashList.begin(); ht != HashList.end(); ++ht)
		{
			if (!ht->second->next)
				continue;
			
			md5.Hex((const unsigned char *) ht->first.data(), 16, obuf);
			printf("%s\n", obuf);
			for (FileReference *p = ht->second; p; p = p->next)
				printf("\t%s/%s\n", p->dir, p->file);
			printf("\n");
		}
		HashList.clear();
	}
	
	return EXIT_SUCCESS;
}

void ScanDirectory(const char *path)
{
	DIR *d = opendir(path);
	if (!d)
	{
		printf("opendir error (%s): %s\n", path, strerror(errno));
		return;
	}
	
	char *tmp = new char[strlen(path) + 1];
	strcpy(tmp, path);
	path = tmp;
	
	struct dirent *de;
	struct stat st;
	int dfd = dirfd(d);
	while ((de = readdir(d)) != NULL)
	{
		if (de->d_name[0] == '.' && (!de->d_name[1] || (de->d_name[1] == '.' && !de->d_name[2])))
			continue;
		
		if (fstatat(dfd, de->d_name, &st, 0) < 0)
		{
			printf("stat failure (%s/%s): %s\n", path, de->d_name, strerror(errno));
			continue;
		}
		
		if (S_ISREG(st.st_mode))
		{
			FileReference *ref = new FileReference();
			ref->dir = path;
			ref->file = new char[de->d_reclen + 1];
			strcpy(ref->file, de->d_name);
			ref->next = NULL;
			
			std::map<off_t,FileReference*>::iterator it = SizeMap.find(st.st_size);
			if (it == SizeMap.end())
			{
				SizeMap.insert(std::make_pair(st.st_size, ref));
			}
			else if (!it->second->next)
			{
				it->second->next = ref;
				SizeDups.push_back(it->second);
			}
			else
			{
				FileReference *i = it->second;
				while (i->next != NULL)
					i = i->next;
				i->next = ref;
			}
		}
		else if (S_ISDIR(st.st_mode))
		{
			ScanDirectory((std::string(path) + "/" + de->d_name).c_str());
		}
		else
		{
			printf("Skipped (non-file): %s/%s\n", path, de->d_name);
		}
	}
	
	closedir(d);
}
