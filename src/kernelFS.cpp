#include "kernelFS.h"
#include "file.h"
#include "part.h"
#include "partitions.h"
#include "EntryManipulator.h"
#include "semOp.h"
#include "fileInfo.h"
#include "processInfo.h"
#include "bank.h"
#include <cstdlib>
#include <cstring>
#include <iostream>
using namespace std;


#pragma warning ( disable : 4018 )

HANDLE KernelFS::mutexList = CreateSemaphore(NULL, 1, 32,NULL);// Brani listu

// Lista svih fajlova
list<FileInfo>* KernelFS::listOfFiles = new list<FileInfo>();

int KernelFS::fileEqualByName (KernelFile* file, char* path) 
{
	if (!strcmp(file->fileName,path)) return 1;
	return 0;
}

int KernelFS::fileEqualByEntry (KernelFile* file, unsigned long entryNumber)
{
	if (file->entryNumber == entryNumber) return 1;
	return 0;
}

// consumeCluster i returnCluster nebezbedbe fje
unsigned long KernelFS::consumeCluster (Partitions::PartitionInfo* pInfo) 
{
	if (!pInfo->moreFreeSpace()) return 0;

	pInfo->numberOfFreeClusters--;

	char* buffer = new char[2048];
	char* rootDir = new char[2048];

	pInfo->partition->readCluster(0,rootDir);

	unsigned long returnValue = ((unsigned long*)rootDir)[1];

	pInfo->partition->readCluster(returnValue, buffer);
	
	((unsigned long*)rootDir)[1] = ((unsigned long*)buffer)[0];
	((unsigned long*)buffer)[0] = 0;

	pInfo->partition->writeCluster(returnValue,buffer);

	pInfo->partition->writeCluster(0,rootDir);

	delete [] buffer;
	delete [] rootDir;

	return returnValue;
}

void KernelFS::returnCluster (Partitions::PartitionInfo* pInfo, unsigned long clusterNo) 
{
	pInfo->numberOfFreeClusters++;

	char* buffer = new char[2048];
	char* rootDir = new char[2048];

	pInfo->partition->readCluster(0,rootDir);
	pInfo->partition->readCluster(clusterNo,buffer);

	((unsigned long*)buffer)[0] = ((unsigned long*)rootDir)[1];

	for (int i = 4; i < 2048; i++)
		buffer[i] = 0;

	pInfo->partition->writeCluster(clusterNo,buffer);

	((unsigned long*)rootDir)[1] = clusterNo;

	pInfo->partition->writeCluster(0,rootDir); 

	delete [] buffer;
	delete [] rootDir;
}

KernelFS::KernelFS ()
{

}


char KernelFS::mount (Partition* partition)
{
	return Partitions::mount(partition);
}

char KernelFS::unmount (char part)
{
	Partitions::PartitionInfo* pInfo = Partitions::getInfo(part);
	if (pInfo == NULL) 
	{
		return '0';
	}

	wait(pInfo->rootAccess);

	if (pInfo->needUnmount) 
	{
		signal(pInfo->rootAccess);

		return '0';
	}

	if (pInfo->numberOfAccess)
	{
		pInfo->allowAccess = 0;
		pInfo->needUnmount = 1;

		signal(pInfo->rootAccess);

		wait(pInfo->unmountSem);
	
		wait(pInfo->rootAccess);

		pInfo->needUnmount = 0;
	}

	signal(pInfo->rootAccess);

	return Partitions::unmount(part);
}


char KernelFS::format (char part)
{
	Partitions::PartitionInfo* pInfo = Partitions::getInfo(part);
	if (pInfo == NULL) 
	{
		return '0';
	}

	wait(pInfo->rootAccess);

	Partition* partition = pInfo->partition;

	if (partition != NULL) 
	{
		char* buffer = new char[2048];

		if (!pInfo->allowAccess)
		{
			delete [] buffer;

			signal(pInfo->rootAccess);

			return '0';
		}

		if (pInfo->numberOfAccess)
		{
			pInfo->allowAccess = 0;
			pInfo->needFormat = 1;
			
			signal(pInfo->rootAccess);

			wait(pInfo->formatSem);

			wait(pInfo->rootAccess);

		}

		
		for (unsigned long i=0; i<partition->getNumOfClusters(); i++)
		{
			partition->readCluster(i,buffer);

			if (i == (partition->getNumOfClusters()-1))
			{
				((unsigned long*)buffer)[0] = 0;
				for (int j=4; j<2048; j++)
					buffer[j] = 0;

				partition->writeCluster(i,buffer);

				break;
			}

			if (!i)
			{
				((unsigned long*)buffer)[0] = 0;
				((unsigned long*)buffer)[1] = 1;

				for (int j=8; j<2048; j++) 
				{
					buffer[j] = 0;
				}
			}
			else
			{
				((unsigned long*)buffer)[0] = i+1;
				((unsigned long*)buffer)[1] = i-1;
				for (int j=4; j<2048; j++)
					buffer[j] = 0;
			}
			
			partition->writeCluster(i,buffer);

		}

		delete [] buffer;

		pInfo->numberOfFreeClusters = partition->getNumOfClusters();

		if (!pInfo->needUnmount) pInfo->allowAccess = 1;
		else signal(pInfo->unmountSem);
				
		signal(pInfo->rootAccess);		
		
		return '1';
	}

	signal(pInfo->rootAccess);

	return '0';	
}

File* KernelFS::open(char* fname)
{
	char part = fname[0];

	Partitions::PartitionInfo* pInfo = Partitions::getInfo(part);
	if (pInfo == NULL) 
	{
		return NULL;
	}

	wait(pInfo->rootAccess);

	if (!pInfo->allowAccess)
	{
		signal(pInfo->rootAccess);
	
		return NULL;
	}

	signal(pInfo->rootAccess);

	if (Bank::checkDeclare(::GetCurrentThreadId(),fname) == 0) 
	{
		return NULL;
	}
	Bank::tryOpen(::GetCurrentThreadId(),pInfo,fname);

	wait(pInfo->rootAccess);

	Partition* partition = pInfo->partition;

	if (partition != NULL)
	{
		char* buffer = new char[2048];
		char* rootDir = new char[2048];

		partition->readCluster(0,rootDir);
		partition->readCluster(0,buffer);

		unsigned long more_ = 1;
		unsigned long entryNoTemp = 0;

		int goOn = 1;

		while (more_ && goOn)
		{
			more_ = ((unsigned long*)buffer)[0];

			buffer += 8;

			for (int i = 0; i < 102; i++)
			{
				Entry* entryInDir = &((Entry*)buffer)[i];
				if (!EntryManipulator::isFree(entryInDir))
				{
					if (EntryManipulator::checkName(entryInDir,fname))
					{
						unsigned long currentDataCluster;

						unsigned long lastIndexCluster;

						unsigned long oldCursor;

						unsigned long readCluster;

						int fullDataCluster;

						wait(mutexList);

						for (list<FileInfo>::iterator it = listOfFiles->begin();
							it != listOfFiles->end(); it++)
						{
							if ((*it).equalByName(fname)) 
							{
								(*it).opened = 1;
								(*it).processId = ::GetCurrentThreadId();
								
								currentDataCluster = (*it).currentDataCluster;
								lastIndexCluster = (*it).lastIndexCluster;
								fullDataCluster = (*it).fullDataCluster;
								readCluster = (*it).readCluster;
								oldCursor = (*it).oldCursor;

								break;
							}
						}
						
						unsigned long fic = entryInDir->firstIndexCluster;
						unsigned long s = entryInDir->size;
						
						buffer -= 8;

						delete [] buffer;

						delete [] rootDir;

						signal(mutexList);

						signal(pInfo->rootAccess);

						return new File(pInfo,fname,entryNoTemp + i,fic,s, currentDataCluster, 
							lastIndexCluster, fullDataCluster, readCluster, oldCursor);
					}
				}
				else 
				{
					goOn = 0;
					break;
				}
			}


			entryNoTemp += 102;

			buffer -= 8;

			if (more_) pInfo->partition->readCluster(more_,buffer);

		} 

		unsigned long rootIndexCluster = 0;
		unsigned long entryInRootIndexCluster = (pInfo->lastOccupiedEntry + 1) % 102;

		unsigned long more = ((unsigned long*)rootDir)[0];

		while (more)
		{
			rootIndexCluster = more;
			partition->readCluster(more, buffer);
			more = ((unsigned long*)buffer)[0];
		}

		if ((pInfo->lastOccupiedEntry % 102) == 101)
		{
			int nextDirCluster = consumeCluster(pInfo);
			if (!nextDirCluster) 
			{
				signal(pInfo->rootAccess);

				return NULL;
			
			}

			((unsigned long*)buffer)[0] = nextDirCluster;

			pInfo->partition->writeCluster(rootIndexCluster, buffer);

			pInfo->partition->readCluster(nextDirCluster, buffer);
			((unsigned long*)buffer)[1] = rootIndexCluster;

			buffer += 8;

			Entry* entry = &(((Entry*)buffer)[0]);

			EntryManipulator::fillEntry(entry, fname, 0);

			buffer -= 8;

			pInfo->partition->writeCluster(nextDirCluster, buffer);
		}
		else
		{
			buffer += 8;
			
			Entry* entry = &(((Entry*)buffer)[entryInRootIndexCluster]);

			EntryManipulator::fillEntry(entry, fname, 0);

			buffer -= 8;

			pInfo->partition->writeCluster(rootIndexCluster,buffer);
		}

		pInfo->lastOccupiedEntry++;

		delete [] buffer;
		delete [] rootDir;		

		wait(mutexList);

		for (list<FileInfo>::iterator it = listOfFiles->begin();
			it != listOfFiles->end(); it++)
		{
			if ((*it).equalByName(fname)) 
			{
				(*it).opened = 1;
				(*it).processId = ::GetCurrentThreadId();

				break;
			}
		}

		signal(mutexList);

		signal(pInfo->rootAccess);

		return new File(pInfo,fname,pInfo->lastOccupiedEntry,0,0,0,0,0,0,0); 

	}

	signal(pInfo->rootAccess);
	
	return NULL;
}

char KernelFS::doesExist(char* fname) 
{
	char part = fname[0];

	Partitions::PartitionInfo* pInfo = Partitions::getInfo(part);
	if (pInfo == NULL) 
	{
		return '0';
	}

	wait(pInfo->rootAccess);

	char* buffer = new char[2048];

	pInfo->partition->readCluster(0,buffer);
	
	unsigned long more = 1;

	while (more)
	{		
		more = ((unsigned long*)buffer)[0];

		buffer += 8;

		for (int i = 0; i < 102; i++)
		{
			Entry* entryInDir = &((Entry*)buffer)[i];
			if (!EntryManipulator::isFree(entryInDir))
			{
				if (EntryManipulator::checkName(entryInDir,fname))
				{
					delete [] buffer;
					signal(pInfo->rootAccess);

					return '1';
				}
			}
		}

		buffer -= 8;

		if (more) pInfo->partition->readCluster(more,buffer);

	} 

	delete [] buffer;

	signal(pInfo->rootAccess);

	return '0';
}

char KernelFS::deleteFile(char* fname) 
{
	char returnValue = '0';

	char part = fname[0];

	Partitions::PartitionInfo* pInfo = Partitions::getInfo(part);
	if (pInfo == NULL) 
	{
		return returnValue;
	}

	wait(pInfo->rootAccess);

	if (Bank::checkDeclare(::GetCurrentThreadId(),fname) == 0)
	{
		signal(pInfo->rootAccess);

		return returnValue;
	}

	wait(mutexList);

	for (list<FileInfo>::iterator it = listOfFiles->begin();
		it != listOfFiles->end(); it++)
	{
		if ((*it).equalByName(fname)) 
		{
			if (((*it).opened == 1) || ((*it).numberOfDeclarations > 1))
			{
				signal(mutexList);
				signal(pInfo->rootAccess);

				return '0';
			}
		}
	}

	signal(mutexList);

	signal(pInfo->rootAccess);

	declare(fname,0);

	wait(pInfo->rootAccess);

	wait(Bank::mutexProcesses);

	for (list<ProcessInfo>::iterator pit = Bank::processes.begin();
		pit != Bank::processes.end(); pit++)
	{
		if ((*pit).processID == ::GetCurrentThreadId())
		{
			(*pit).declaredFiles.remove(FileInfo(fname));
		}
	}

	signal(Bank::mutexProcesses);

	char* buffer = new char[2048];

	unsigned long srcClusterNo = 0;
	int saveSrcCluster = 1;

	unsigned long dstClusterNo = 0;

	pInfo->partition->readCluster(0,buffer);
	
	unsigned long more = 1;

	unsigned long exchangeEntryNo = 2;
	Entry* exchangeEntry = NULL;
	Entry* lastOccupiedEntry = NULL;

	while (more)
	{		
		more = ((unsigned long*)buffer)[0];

		buffer += 8;

		for (int i = 0; i < 102; i++)
		{
			Entry* entryInDir = &((Entry*)buffer)[i];
			if (!EntryManipulator::isFree(entryInDir))
			{
				lastOccupiedEntry = entryInDir;

				if (EntryManipulator::checkName(entryInDir,fname))
				{
					if (entryInDir->firstIndexCluster != 0)
					{
						char* tempBuffer = new char[2048];

						pInfo->partition->readCluster(entryInDir->firstIndexCluster,tempBuffer);

						unsigned long data_more = 1;

						while (data_more)
						{
							data_more = ((unsigned long*)tempBuffer)[0];

							tempBuffer += 4;

							for (int i = 0; i < 511; i++)
							{
								if (((unsigned long*)tempBuffer)[i] != 0)
								{
									KernelFS::returnCluster(pInfo,((unsigned long*)tempBuffer)[i]);
								}
								else break;
							}

							tempBuffer -= 4;

							if (data_more) pInfo->partition->readCluster(data_more,tempBuffer);
						}

						pInfo->partition->readCluster(entryInDir->firstIndexCluster,tempBuffer);

						unsigned long returnIndexClusters = ((unsigned long*)tempBuffer)[0];

						while (returnIndexClusters != 0)
						{
							pInfo->partition->readCluster(returnIndexClusters,tempBuffer);
							KernelFS::returnCluster(pInfo,returnIndexClusters);
							returnIndexClusters = ((unsigned long*)tempBuffer)[0];							
						}

						KernelFS::returnCluster(pInfo,entryInDir->firstIndexCluster);

						entryInDir->firstIndexCluster = 0;

						delete [] tempBuffer;
					}

					exchangeEntryNo = i;

					exchangeEntry = entryInDir;

					saveSrcCluster = 0;

					returnValue = '1';
				}
			}
			else
			{
				buffer -= 8;

				if (srcClusterNo == dstClusterNo)
				{
					if (i == 1 && srcClusterNo != 0) 
					{
						unsigned long previousRootCluster = ((unsigned long*)buffer)[1];

						pInfo->partition->readCluster(previousRootCluster,buffer);

						unsigned long clusterToDelete = ((unsigned long*)buffer)[0];

						KernelFS::returnCluster(pInfo,clusterToDelete);

						((unsigned long*)buffer)[0] = 0;

						pInfo->partition->writeCluster(previousRootCluster,buffer);
					}
					else
					{
						EntryManipulator::fillEntry(lastOccupiedEntry,exchangeEntry);
						EntryManipulator::releaseEntry(lastOccupiedEntry); // dupli posao ali ne treba da smeta

						pInfo->partition->writeCluster(srcClusterNo,buffer);
					}

					buffer += 8;

					pInfo->lastOccupiedEntry--;

					break;
				}
				else
				{ 

					char* tempBuffer = new char[2048];

					pInfo->partition->readCluster(srcClusterNo,tempBuffer);

					tempBuffer += 8;

					Entry* entrySrc = &((Entry*)tempBuffer)[exchangeEntryNo];

					EntryManipulator::fillEntry(entrySrc,lastOccupiedEntry);
					EntryManipulator::releaseEntry(lastOccupiedEntry);

					tempBuffer -= 8;

					if (i == 1)
					{
						unsigned long previousRootCluster = ((unsigned long*)buffer)[1];

						pInfo->partition->readCluster(previousRootCluster,buffer);

						unsigned long clusterToDelete = ((unsigned long*)buffer)[0];

						KernelFS::returnCluster(pInfo,clusterToDelete);

						((unsigned long*)buffer)[0] = 0;

						pInfo->partition->writeCluster(previousRootCluster,buffer);
					}
					else pInfo->partition->writeCluster(dstClusterNo,buffer);

					pInfo->partition->writeCluster(srcClusterNo,tempBuffer);

					delete [] tempBuffer;

					buffer += 8;

					pInfo->lastOccupiedEntry--;

					break;
				}
			}
				
		}

		buffer -= 8;

		if (more) 
		{
			pInfo->partition->readCluster(more,buffer);
			if (saveSrcCluster) srcClusterNo = more;
		    dstClusterNo = more;
		}

	} 

	delete [] buffer;

	if (returnValue == '1') 
	{
		wait(mutexList);

		listOfFiles->remove(FileInfo(fname));

		signal(mutexList);
	}

	signal(pInfo->rootAccess);

	return returnValue;

}

int KernelFS::readRootDir(char part, EntryNum n, Directory &d)
{
	int returnValue = 0;

	char* buffer = new char[2048];

	Partitions::PartitionInfo* pInfo = Partitions::getInfo(part);
	if (pInfo == NULL) 
	{
		return returnValue;
	}

	wait(pInfo->rootAccess);

	if (pInfo->lastOccupiedEntry < n)
	{
		delete [] buffer;
		signal(pInfo->rootAccess);

		return returnValue;
	}

	unsigned long rootIndexCluster = n / 102;
	unsigned long entryInRootIndexCluster = 0;
	if (rootIndexCluster > 0) entryInRootIndexCluster = n % 102;
	else entryInRootIndexCluster = n;

	pInfo->partition->readCluster(0,buffer);

	if (rootIndexCluster > 0) 
	{
		for (unsigned long i = 0; i < rootIndexCluster; i++)			
		{
			pInfo->partition->readCluster(((unsigned long*)buffer)[0],buffer);
		}
	}

	buffer += 8;

	for (int i = 0; i < 65; i++)
	{
		Entry* entry = &((Entry*)buffer)[entryInRootIndexCluster];
		if (EntryManipulator::isFree(entry)) break;

		if (i == 64) 
		{
			returnValue = 65;
			break;
		}
		
		EntryManipulator::fillEntry(entry,&d[i]);

		entryInRootIndexCluster++;
		returnValue++;

		if (entryInRootIndexCluster == 102)
		{
			buffer -= 8;

			unsigned long nextRootIndexCluster = ((unsigned long*)buffer)[0];

			if (nextRootIndexCluster == 0)
			{
				delete [] buffer;
				signal(pInfo->rootAccess);

				return returnValue;
			}

			pInfo->partition->readCluster(nextRootIndexCluster,buffer);

			buffer += 8;

			entryInRootIndexCluster = 0;
		}

	}

	buffer -= 8;

	delete [] buffer;

	signal(pInfo->rootAccess);

	return returnValue;
}

char KernelFS::declare(char* fname, int mode)
{
	Partitions::PartitionInfo* pInfo = Partitions::getInfo(fname[0]);

	if (pInfo == NULL) 
	{
		return '0';
	}

	wait(pInfo->rootAccess);

	if (!pInfo->allowAccess && mode)
	{
		signal(pInfo->rootAccess);

		return '0';
	}

	char returnValue = Bank::declare(::GetCurrentThreadId(),fname,mode);
	if (returnValue == '0') 
	{
		signal(pInfo->rootAccess);

		return returnValue;
	}
	

	if (mode) pInfo->numberOfAccess++;
	else pInfo->numberOfAccess--;

	if (pInfo->numberOfAccess == 0)
	{
		if (pInfo->needFormat)
		{
			pInfo->needFormat = 0;

			signal(pInfo->formatSem);
		}
		else if (pInfo->needUnmount)
		{
			pInfo->needUnmount = 0;

			signal(pInfo->unmountSem);
		}
	}

	signal(pInfo->rootAccess);

	return returnValue;
}


