#ifndef FILE_H
#define FILE_H

#include "partitions.h"

typedef unsigned long BytesCnt;

class KernelFile; 
class Partition;

class File { 
public: 
	char write (BytesCnt, char* buffer); 

	BytesCnt read (BytesCnt, char* buffer); 

	char seek (BytesCnt);

	BytesCnt filePos();

	char eof (); 

	BytesCnt getFileSize (); 

	char truncate ();

	~File(); //zatvaranje fajla 

private: 
	friend class FS;
	friend class KernelFS; 
	File (Partitions::PartitionInfo* pInfo, char* fname, unsigned long entryNumber,unsigned long firstIndexCluster, 
		unsigned long size, unsigned long currentDataCluster,unsigned long lastIndexCluster, int fullDataCluster, 
		unsigned long readCluster, unsigned long oldCursor);

	KernelFile *myImpl; 
};

#endif