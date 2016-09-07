#ifndef ELEMENT_H
#define ELEMENT_H

#include "part.h"
#include <cstdlib>
#include <windows.h>

using namespace std;


class Partitions {
public:

	static char letters[];
	static int* occupiedLetters; // 0 - slovo je slobodno, 1 - zauzeto

	static HANDLE letterAccess;

	class PartitionInfo{
	public:
		Partition* partition;
		char partitionName;
		int free;
		int numberOfAccess;
		int allowAccess;
		int lastOccupiedEntry;
		unsigned long firstFreeCluster;
		unsigned long numberOfFreeClusters;

		int needFormat;
		int needUnmount;

		HANDLE rootAccess;
		HANDLE formatSem;
		HANDLE unmountSem;

		PartitionInfo ()
		{
			partition = NULL;
			partitionName = '0';
			free = 1;
			numberOfAccess = 0;
			allowAccess = 1;
			lastOccupiedEntry = -1;

			rootAccess = CreateSemaphore(NULL,1,32,NULL);
			formatSem = CreateSemaphore(NULL,0,32,NULL);
			unmountSem = CreateSemaphore(NULL,0,32,NULL);

			numberOfFreeClusters = 0;

			needFormat = 0;
			needUnmount = 0;
		}

		~PartitionInfo ()
		{
			CloseHandle(rootAccess);
			CloseHandle(formatSem);
			CloseHandle(unmountSem);
		}

		int moreFreeSpace ()
		{
			if (numberOfFreeClusters > 0) return 1;
			else return 0;
		}

	};

	Partitions () {}

	static PartitionInfo* partitions;

	static PartitionInfo* getInfo (char c);

	static char mount (Partition* p);

	static char unmount (char c);

	static int moreFreeSpace (char c);

};



#endif