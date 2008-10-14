#ifndef FASTDUP_H
#define FASTDUP_H

#include <map>

class FastDup
{
 public:
	typedef void (*DupeSetCallback)(FileReference *files[], unsigned long count, off_t filesize);
	typedef bool (*ErrorCallback)(const char *file, const char *error);
	
 private:
	typedef std::map<off_t,FileReference*> SizeRefMap;
	
	SizeRefMap FileSzMap;
	
	/* scan.cpp */
	void ScanDirectory(const char *basepath, int bplen, const char *name, ErrorCallback cberr);
	/* compare.cpp */
	void Compare(FileReference *first, off_t filesize, DupeSetCallback callback);
	
 public:
	unsigned long FileCount, CandidateSetCount, DupeFileCount, DupeSetCount;
	off_t FileSizeTotal;
	
	FastDup();
	~FastDup();
	
	/* scan.cpp */
	void AddDirectoryTree(const char *path, ErrorCallback cberr);
	void EndScanning();
	
	unsigned long Run(DupeSetCallback dupecb);
};

#endif
