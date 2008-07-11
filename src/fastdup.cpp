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
#include <sys/time.h>

struct FileReference
{
	const char *dir;
	char *file;
	FileReference *next;
};

std::map<off_t,FileReference*> SizeMap;
std::vector<FileReference*> SizeDups;

void ScanDirectory(const char *path);

double SSTime()
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return (double)tv.tv_sec + (tv.tv_usec / (double)1000000);
}

double TestCompareByte(int argc, char **argv)
{
	int fcount = argc;
	
	int ffd[fcount];
	
	for (int i = 0; i < argc; i++)
	{
		ffd[i] = open(argv[i], O_RDONLY);
		if (ffd[i] < 0)
		{
			printf("Unable to open '%s': %s\n", argv[i], strerror(errno));
			exit(EXIT_FAILURE);
		}
	}
	
	double stm = SSTime();
	char rdbuf[fcount][65535];
	ssize_t rdbp = 0;
	bool matchflag[(fcount*(fcount-1))/2];
	bool omit[fcount];
	int omitted = 0;
	int skipcount[fcount];
	
	memset(matchflag, true, sizeof(matchflag));
	memset(omit, 0, sizeof(omit));
	memset(skipcount, 0, sizeof(skipcount));
	
	for (;;)
	{
		for (int i = 0; i < fcount; i++)
		{
			if (omit[i])
				continue;
			
			skipcount[i] = 0;
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
		
		for (int i = 0; i < fcount; i++)
		{
			if (omit[i])
				continue;
			
			for (int j = i + 1; j < fcount; j++)
			{
				if (omit[j])
					continue;
				
				int flagpos = (int)(((fcount-1)*i)-(i*(i/2-0.5))+(j-i)-1);
				if (!matchflag[flagpos] || memcmp(rdbuf[i], rdbuf[j], rdbp) == 0)
					continue;
				
				matchflag[flagpos] = 0;
				skipcount[i]++;
				if (++skipcount[j] == (fcount - 1))
				{
					omit[j] = true;
					omitted++;
				}
			}
			
			if (skipcount[i] == (fcount - 1))
			{
				omit[i] = true;
				if (++omitted == fcount)
					goto endscan;
			}
		}
	}
	
 endscan:
	int matches = 0;
	for (int i = 0; i < fcount; i++)
	{
		if (omit[i])
			continue;
		
		for (int j = i + 1; j < fcount; j++)
		{
			if (omit[j])
				continue;
			
			if (matchflag[(int)(((fcount-1)*i)-(i*(i/2-0.5))+(j-i)-1)])
				matches++;
		}
	}
	
	double etm = SSTime();
	for (int i = 0; i < fcount; i++)
		close(ffd[i]);
	return etm - stm;
}

double TestCompareHash(int argc, char **argv)
{
	int fcount = argc;
	int ffd[fcount];
	
	for (int i = 0; i < argc; i++)
	{
		ffd[i] = open(argv[i], O_RDONLY);
		if (ffd[i] < 0)
		{
			printf("Unable to open '%s': %s\n", argv[i], strerror(errno));
			exit(EXIT_FAILURE);
		}
	}
	
	double stm = SSTime();
	char rdbuf[65535];
	unsigned char hashlist[fcount][16];
	ssize_t rdbp = 0;
	MD5 md5;
	
	for (int i = 0; i < fcount; i++)
	{
		md5.Init();
		
		while ((rdbp = read(ffd[i], rdbuf, 65535)) > 0)
			md5.Append((unsigned char *) rdbuf, rdbp);
		
		if (rdbp < 0)
		{
			printf("%d: Read error: %s\n", i, strerror(errno));
			exit(EXIT_FAILURE);
		}
		
		md5.Finish(hashlist[i]);
	}
	
	int matches = 0;
	
	for (int i = 0; i < fcount; i++)
	{
		for (int j = i + 1; j < fcount; j++)
		{
			if (memcmp(hashlist[i], hashlist[j], 16) == 0)
				matches++;
		}
	}
	
	double etm = SSTime();
	for (int i = 0; i < fcount; i++)
		close(ffd[i]);
	return etm - stm;
}

int main(int argc, char **argv)
{
	if (argc < 2)
	{
		printf("Usage: %s <directory>\n", argv[0]);
		return EXIT_FAILURE;
	}
	
	if (strcmp(argv[1], "-t") == 0)
	{
		if (argc < 5)
			return EXIT_FAILURE;
		
		int tests = atoi(argv[2]);
		if (!tests)
			return EXIT_FAILURE;
		
		double re_h[tests];
		double re_b[tests];

		printf("Running %d tests of byte comparison, after discarding two\n", tests);
		TestCompareByte(argc - 3, argv + 3);
		TestCompareByte(argc - 3, argv + 3);
		for (int i = 0; i < tests; i++)
			re_b[i] = TestCompareByte(argc - 3, argv + 3);
		
		printf("Running %d tests of hash comparison, after discarding two\n", tests);
		TestCompareHash(argc - 3, argv + 3);
		TestCompareHash(argc - 3, argv + 3);
		for (int i = 0; i < tests; i++)
			re_h[i] = TestCompareHash(argc - 3, argv + 3);
		
		printf("\n");
		
		double avg_h = 0, max_h = 0, min_h = 0;
		double avg_b = 0, max_b = 0, min_b = 0;
		
		for (int i = 0; i < tests; i++)
		{
			avg_h += re_h[i];
			if (!i || re_h[i] > max_h)
				max_h = re_h[i];
			if (!i || re_h[i] < min_h)
				min_h = re_h[i];
			
			avg_b += re_b[i];
			if (!i || re_b[i] > max_b)
				max_b = re_b[i];
			if (!i || re_b[i] < min_b)
				min_b = re_b[i];
		}
		
		avg_h = avg_h / tests;
		avg_b = avg_b / tests;
		
		printf("Byte:\n\tmin %f %+f\n\tmax %f %+f\n\tavg %f %+f\n", min_b, min_b - min_h, max_b, max_b - max_h, avg_b, avg_b - avg_h);
		printf("Hash:\n\tmin %f %+f\n\tmax %f %+f\n\tavg %f %+f\n", min_h, min_h - min_b, max_h, max_h - max_b, avg_h, avg_h - avg_b);
		printf("\n");
		
		return EXIT_SUCCESS;
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
