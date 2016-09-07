#include "kernelFile.h"
#include "kernelFS.h"
#include "semOp.h"
#include "part.h"
#include "entryManipulator.h"
#include "bank.h"
#include "fileInfo.h"
#include <list>
#include <cstring>
#include <iostream>

using namespace std;

#pragma warning ( disable : 4996)

KernelFile::KernelFile (Partitions::PartitionInfo* pInfo, char* fname, unsigned long entryNumber,unsigned long firstIndexCluster, 
		unsigned long size, unsigned long currentDataCluster,unsigned long lastIndexCluster, int fullDataCluster, 
		unsigned long readCluster, unsigned long oldCursor)
: cursor(0), dirty(0), cacheFull(0)
{

	this->pInfo = pInfo;
	this->entryNumber = entryNumber;
	this->firstIndexCluster = firstIndexCluster;
	this->size = size;

	this->currentDataCluster = currentDataCluster;
	this->lastIndexCluster = lastIndexCluster;
	this->fullDataCluster = fullDataCluster;
	this->readCluster = readCluster;
	this->oldCursor = oldCursor;

	fileName = new char[strlen(fname)+1];
	for(unsigned int i=0; i<strlen(fname); i++) 
		fileName[i] = fname[i];
	fileName[strlen(fname)] = '\0';

	access = CreateSemaphore(NULL,1,32,NULL);

	cache = new char[2048];
	readCache = new char[2048];

	if (currentDataCluster) this->pInfo->partition->readCluster(currentDataCluster,cache);
	if (readCluster) this->pInfo->partition->readCluster(readCluster,readCache);
}

KernelFile::~KernelFile()
{
	wait(access);

	wait(pInfo->rootAccess);

	if (dirty)
	{
		callIfDirty ();
	}

	char* buffer = new char[2048];

	pInfo->partition->readCluster(0,buffer);
	
	unsigned long rootIndexCluster = entryNumber / 102;
	unsigned long entryInRootIndexCluster = 0;
	if (rootIndexCluster > 0) entryInRootIndexCluster = entryNumber % 102;
	else entryInRootIndexCluster = entryNumber;

	if (rootIndexCluster > 0) 
	{
		for (unsigned long i = 0; i < rootIndexCluster; i++)
		{
			pInfo->partition->readCluster(((unsigned long*)buffer)[0],buffer);
		}
	}

	buffer += 8;

	Entry* rootEntry = &((Entry*)buffer)[entryInRootIndexCluster];

	rootEntry->size = size;
	
	rootEntry->firstIndexCluster = firstIndexCluster;

	buffer -= 8;

	pInfo->partition->writeCluster(rootIndexCluster,buffer);

	delete [] buffer;


	wait(KernelFS::mutexList);

	for (list<FileInfo>::iterator it = KernelFS::listOfFiles->begin();
		it != KernelFS::listOfFiles->end(); it++)
	{
		if ((*it).equalByName(fileName))
		{
			(*it).opened = 0;
			(*it).processId = 0;

			(*it).currentDataCluster = currentDataCluster;
			(*it).fullDataCluster = fullDataCluster;
			(*it).lastIndexCluster = lastIndexCluster;
			(*it).readCluster = readCluster;
			(*it).oldCursor = oldCursor;

			break;
		}
	}

	signal(KernelFS::mutexList);

	delete [] cache;

	delete [] readCache;

	signal(access);

	CloseHandle(access);

	Bank::retry(this->fileName);

	signal(pInfo->rootAccess);
}

char KernelFile::write (BytesCnt toBeWritten, char* outputBuffer) 
{
	wait(access);

	wait(pInfo->rootAccess);

	if (!toBeWritten)
	{
		signal(access);
		signal(pInfo->rootAccess);

		return '1';
	}

	dirty = 1;

	unsigned long dataClusterEntry = cursor / 2048;
	unsigned long dataClusterPosition = cursor % 2048;

	char* buffer = new char[2048];

	if (firstIndexCluster == 0)
	{
		firstIndexCluster = KernelFS::consumeCluster(pInfo);
		if (!firstIndexCluster)
		{
			delete [] buffer;

			signal(pInfo->rootAccess);
			signal(access);

			return '0';
		}

		pInfo->partition->readCluster(0,buffer);

		unsigned long rootIndexCluster = entryNumber / 102;
		unsigned long entryInRootIndexCluster = 0;
		if (rootIndexCluster > 0) entryInRootIndexCluster = entryNumber % 102;
		else entryInRootIndexCluster = entryNumber;

		if (rootIndexCluster > 0) 
		{
			for (unsigned long i = 0; i < rootIndexCluster; i++)
			{
				pInfo->partition->readCluster(((unsigned long*)buffer)[0],buffer);
			}
		}

		buffer += 8;

		Entry* rootEntry = &((Entry*)buffer)[entryInRootIndexCluster];

		rootEntry->firstIndexCluster = firstIndexCluster;

		buffer -= 8;

		pInfo->partition->writeCluster(rootIndexCluster,buffer);

		pInfo->partition->readCluster(firstIndexCluster,buffer);

		buffer += 4;

		currentDataCluster = KernelFS::consumeCluster(pInfo);
		
		if (!currentDataCluster)
		{
			buffer -= 4;
			delete buffer;

			signal(pInfo->rootAccess);
			signal(access);

			return '0';
		}

		((unsigned long*)buffer)[0] = currentDataCluster;

		buffer -= 4;

		pInfo->partition->writeCluster(firstIndexCluster,buffer);

		lastIndexCluster = firstIndexCluster;

	}
	else if (oldCursor != cursor)
	{

		pInfo->partition->readCluster(firstIndexCluster,buffer);

		while(dataClusterEntry > 510)
		{
			dataClusterEntry -= 511;
			pInfo->partition->readCluster(((unsigned long*)buffer)[0],buffer);
		}

		unsigned long newDataCluster = ((unsigned long*)buffer)[dataClusterEntry + 1];

		if (newDataCluster != currentDataCluster)
		{
			if (currentDataCluster) pInfo->partition->writeCluster(currentDataCluster,cache);
			pInfo->partition->readCluster(newDataCluster,cache);
			currentDataCluster = newDataCluster;
		}

	}

	for (unsigned long i = 0; i < toBeWritten; i++)
	{
		cache[(dataClusterPosition + i) % 2048] = outputBuffer[i];

		if (((dataClusterPosition + i + 1) % 2048 == 0))
		{
			cacheFull = 1;

			if (writeOnDisk() == '0') 
			{
				delete [] buffer;

				signal(pInfo->rootAccess);
				signal(access);

				return '0';
			}
		}
	}

	delete [] buffer;

	cursor += toBeWritten;

	oldCursor = cursor;

	size += (cursor > size) ? (cursor - size) : 0;

	signal(pInfo->rootAccess);

	signal(access);

	return '1';
}

char KernelFile::writeOnDisk ()
{
	char* buffer = new char[2048];

	if (fullDataCluster)
	{
		currentDataCluster = KernelFS::consumeCluster(pInfo);
		if (!currentDataCluster)
		{
			delete [] buffer;

			return '0';
		}

		pInfo->partition->readCluster(lastIndexCluster,buffer);

		buffer += 4;

		int allocateNextIndexCluster = 0;

		for (int i = 0; i < 511; i++)
		{
			if (((unsigned long*)buffer)[i] == 0)
			{
				((unsigned long*)buffer)[i] = currentDataCluster;
				break;
			}
			if (i == 510) allocateNextIndexCluster = 1;
		}

		if (allocateNextIndexCluster)
		{
			unsigned long nextIndexCluster = KernelFS::consumeCluster(pInfo);
			if (!nextIndexCluster)
			{
				buffer -= 4;
				delete [] buffer;

				return '0';
			}			

			buffer -= 4;

			((unsigned long*)buffer)[0] = nextIndexCluster;

			pInfo->partition->writeCluster(lastIndexCluster,buffer);

			pInfo->partition->readCluster(nextIndexCluster,buffer);

			lastIndexCluster = nextIndexCluster;

			buffer += 4;

			((unsigned long*)buffer)[0] = currentDataCluster;

			allocateNextIndexCluster = 0;

		}

		buffer -= 4;

		pInfo->partition->writeCluster(lastIndexCluster,buffer);

		fullDataCluster = 0;
	}

	pInfo->partition->writeCluster(currentDataCluster,cache);

	if (cacheFull)
	{
		fullDataCluster = 1;
		cacheFull = 0;
	}

	delete [] buffer;

	return '1';
}

BytesCnt KernelFile::filePos() 
{
	wait(access);
	signal(access);

	return cursor;
}

char KernelFile::eof() 
{
	wait(access);

	if (cursor == size)
	{
		signal(access);
		return 1;
	}
	else
	{
		signal(access);
		return 0;
	}
}

BytesCnt KernelFile::getFileSize()
{
	wait(access);
	signal(access);

	return size;
}

char KernelFile::seek(BytesCnt pos)
{
	wait(access);

	if (pos > size || pos < 0) 
	{
		signal(access);
		return '0';
	}

	cursor = pos;

	signal(access);

	return '1';
}

BytesCnt KernelFile::read(BytesCnt toBeRead, char* inputBuffer)
{
	wait(access);
	wait(pInfo->rootAccess);

	char* indexBuffer = new char[2048];

	if (dirty)
	{
		if (!callIfDirty ())
		{
			delete [] indexBuffer;

			signal(pInfo->rootAccess);
			signal(access);

			return '0';
		}
	}

	if (cursor + toBeRead > size) toBeRead = size - cursor;

	unsigned long dataClusterEntry = cursor / 2048;
	unsigned long dataClusterPosition = cursor % 2048;

	pInfo->partition->readCluster(firstIndexCluster,indexBuffer); 

	while(dataClusterEntry > 510)
	{
		dataClusterEntry -= 511;
		pInfo->partition->readCluster(((unsigned long*)indexBuffer)[0],indexBuffer);
	}

	unsigned long newReadCluster = ((unsigned long*)indexBuffer)[dataClusterEntry + 1];

	if (newReadCluster != readCluster)
	{
		pInfo->partition->readCluster(newReadCluster,readCache);
		readCluster = newReadCluster;
	}
	
	for (BytesCnt i = 0; i < toBeRead; i++)
	{
		inputBuffer[i] = readCache[(dataClusterPosition + i) % 2048];

		if (((dataClusterPosition + i + 1) % 2048) == 0)
		{

			if (dataClusterEntry == 510) 
			{
				pInfo->partition->readCluster(((unsigned long*)indexBuffer)[0],indexBuffer);

				dataClusterEntry = 0;
			}
			else dataClusterEntry++;

			readCluster = ((unsigned long*)indexBuffer)[dataClusterEntry + 1];

			pInfo->partition->readCluster(readCluster,readCache);

			dataClusterPosition = 0;
			
		}
	}

	delete [] indexBuffer;

	cursor = cursor + toBeRead;

	signal(pInfo->rootAccess);

	signal(access);

	return toBeRead;
}

char KernelFile::truncate ()
{
	wait(access);
	wait(pInfo->rootAccess);

	if (dirty)
	{
		if (!callIfDirty ())
		{
			signal(pInfo->rootAccess);
			signal(access);

			return '0';
		}
	}

	char* buffer = new char[2048];

	pInfo->partition->readCluster(firstIndexCluster,buffer);

	unsigned long dataClusterEntry = cursor / 2048;
	unsigned long dataClusterPosition = cursor % 2048;

	unsigned long indexCluster = firstIndexCluster;
	unsigned long oldIndexCluster = 0;

	while (dataClusterEntry > 510)
	{
		oldIndexCluster = indexCluster;
		dataClusterEntry -= 511;
		indexCluster = ((unsigned long*)buffer)[0];
		pInfo->partition->readCluster(indexCluster,buffer);
	}

	buffer += 4;
	
	if (dataClusterPosition) 
	{
		currentDataCluster = ((unsigned long*)buffer)[dataClusterEntry];
		pInfo->partition->readCluster(currentDataCluster,cache);
		pInfo->partition->readCluster(currentDataCluster,readCache);
		lastIndexCluster = indexCluster;
		if (dataClusterEntry < 510) dataClusterEntry++;
		else 
		{
			dataClusterEntry = 0;
			
			buffer -= 4;

			indexCluster = ((unsigned long*)buffer)[0];

			if (indexCluster) lastIndexCluster = indexCluster;

			buffer += 4;
		}
	}
	else if (cursor != 0) 
	{
		if (!dataClusterEntry)
		{
			pInfo->partition->readCluster(oldIndexCluster,cache);
			currentDataCluster = ((unsigned long*)cache)[510];
		}
		else currentDataCluster = ((unsigned long*)buffer)[dataClusterEntry-1];

		lastIndexCluster = oldIndexCluster;

		pInfo->partition->readCluster(currentDataCluster,readCache);
		pInfo->partition->readCluster(currentDataCluster,cache);
	}

	oldIndexCluster = 0;
	int release = 0;

	while (indexCluster)
	{
		int nextDataClusterEntry = dataClusterEntry;
		while(nextDataClusterEntry < 511)
		{
			if (((unsigned long*)buffer)[nextDataClusterEntry] != 0)
			{
				KernelFS::returnCluster(pInfo,((unsigned long*)buffer)[nextDataClusterEntry]);
				((unsigned long*)buffer)[nextDataClusterEntry] = 0;
				nextDataClusterEntry++;
			}
			else break;
		}

		buffer -= 4;

		oldIndexCluster = indexCluster;
		indexCluster = ((unsigned long*)buffer)[0];	

		if (release) 
		{
			KernelFS::returnCluster(pInfo,oldIndexCluster);
			release = 0;
		}

		if (indexCluster) 
		{
			pInfo->partition->readCluster(indexCluster,buffer);

			buffer += 4;
			
			dataClusterEntry = 0;
			release = 1;
		}

	}

	if (cursor == 0)
	{
		KernelFS::returnCluster(pInfo,firstIndexCluster);
		
		firstIndexCluster = 0;

		currentDataCluster = 0;

		lastIndexCluster = 0;
	}

	size = cursor;

	pInfo->partition->readCluster(0,buffer);

	/*
	unsigned long more_ = 1;
	unsigned long old = 0;

	while (more_)
	{
		more_ = ((unsigned long*)buffer)[0];

		buffer += 8;

		for (int i = 0; i < 102; i++)
		{
			Entry* entryInDir = &((Entry*)buffer)[i];
			if (!EntryManipulator::isFree(entryInDir))
				if (EntryManipulator::checkName(entryInDir,fileName))
				{
					entryInDir->firstIndexCluster = firstIndexCluster;
					entryInDir->size = size;
					
					break;
				}

		}

		buffer -= 8;

		pInfo->partition->writeCluster(old,buffer);

		if (more_) 
		{
			old = more_;
			pInfo->partition->readCluster(more_,buffer);
		}

	} 
	*/

	delete [] buffer;

	signal(pInfo->rootAccess);

	signal(access);

	return '1';
}

int KernelFile::callIfDirty ()
{
	if (writeOnDisk() == '0')
	{
		return 0;
	}

	dirty = 0;

	return 1;
}