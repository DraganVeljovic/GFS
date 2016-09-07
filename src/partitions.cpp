#include "partitions.h"
#include "semOp.h"
#include <iostream>
using namespace std;


char Partitions::letters[] = {'A','B','C','D','E','F','G','H','I','J','K','L','M','N','O','P','Q','R','S','T','U','V','W','X','Y','Z'};

Partitions::PartitionInfo* Partitions::partitions = new Partitions::PartitionInfo[26];

HANDLE Partitions::letterAccess = CreateSemaphore(NULL,1,32,NULL);


char Partitions::mount (Partition* p)
{
	wait(letterAccess);

	for (int i=0; i<26; i++)
	{
		if (partitions[i].free)
		{
			wait(partitions[i].rootAccess);

			partitions[i].partition = p;
			partitions[i].partitionName = letters[i];
			partitions[i].free = 0;
			partitions[i].allowAccess = 1;
			partitions[i].needFormat = 0;
			partitions[i].needUnmount = 0;
			partitions[i].numberOfAccess = 0;

			signal(partitions[i].rootAccess);

			signal(letterAccess);

			return partitions[i].partitionName;
		}
	}

	signal(letterAccess);

	return '0';
}

char Partitions::unmount (char c)
{
	wait(letterAccess);

	for (int i=0; i<26; i++)
	{
		if (partitions[i].partitionName == c && !partitions[i].free) 
		{
			wait(partitions[i].rootAccess);

			partitions[i].partition = NULL;
			partitions[i].partitionName = '0';
			partitions[i].free = 1;

			signal(partitions[i].rootAccess);

			signal(letterAccess);

			return '1';
		}
	}

	signal(letterAccess);

	return '0';
}

Partitions::PartitionInfo* Partitions::getInfo (char c)
{
	wait(letterAccess);

	for (int i=0; i<26; i++)
	{
		if (partitions[i].partitionName == c && !partitions[i].free) 
		{
			signal(letterAccess);

			return &partitions[i];
		}
	}

	signal(letterAccess);

	return NULL;
}

int Partitions::moreFreeSpace (char c) 
{
	PartitionInfo* pInfo = getInfo(c);

	wait(pInfo->rootAccess);

	if (pInfo == NULL) 
	{
		signal(pInfo->rootAccess);
		return -1;
	}

	if (pInfo->numberOfFreeClusters > 0) 
	{
		signal(pInfo->rootAccess);
		return 1;
	}
	else 
	{
		signal(pInfo->rootAccess);
		return 0;
	}
}


