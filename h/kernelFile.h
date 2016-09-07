#ifndef KERNELFILE_H
#define KERNELFILE_H

#include "file.h"
#include "partitions.h"
#include <windows.h>

typedef unsigned long BytesCnt;

class Partitions;

class KernelFile { 
public: 
	char write (BytesCnt toBeWritten, char* buffer); 

	BytesCnt read (BytesCnt, char* buffer); 

	char seek (BytesCnt);

	BytesCnt filePos();

	char eof (); 

	BytesCnt getFileSize (); 

	char truncate ();

	~KernelFile(); 

	KernelFile (Partitions::PartitionInfo* pInfo, char* fname, unsigned long entryNumber,unsigned long firstIndexCluster, 
		unsigned long size, unsigned long currentDataCluster,unsigned long lastIndexCluster, int fullDataCluster, 
		unsigned long readCluster, unsigned long oldCursor);

	char writeOnDisk();

	int callIfDirty ();

	BytesCnt cursor;

	char* cache;

	char* readCache;

	int fullDataCluster;

	Partitions::PartitionInfo* pInfo;

	char* fileName;

	unsigned long entryNumber;

	unsigned long firstIndexCluster;

	unsigned long lastIndexCluster;

	unsigned long currentDataCluster;

	unsigned long readCluster;

	unsigned long oldCursor;

	BytesCnt size;

	int dirty;

	int cacheFull;

	HANDLE access;
	
};

#endif