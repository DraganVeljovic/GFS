#include "bank.h"
#include "processInfo.h"
#include "fileInfo.h"
#include "kernelFS.h"
#include "semOp.h"
#include <iostream>
using namespace std;

list<ProcessInfo> Bank::processes;

HANDLE Bank::mutexProcesses = CreateSemaphore(NULL,1,32,NULL);
HANDLE Bank::access = CreateSemaphore(NULL,1,32,NULL);
HANDLE Bank::checkSem = CreateSemaphore(NULL,1,32,NULL);


int Bank::check(DWORD processId, char *fname)
{
	wait(checkSem);

	list<FileInfo> copyListOfFiles;

	list<FileInfo>::iterator fileIt = KernelFS::listOfFiles->begin();

	int* fileOpened = NULL;

	DWORD* openingProcess = NULL;

	int loopCondition = 1;


	while (fileIt != KernelFS::listOfFiles->end())
	{
		if ((*fileIt).equalByName(fname)) 
		{	
			if ((*fileIt).opened)
			{
				signal(checkSem);

				return 0;
			}

			fileOpened = &((*fileIt).opened);
			openingProcess = &((*fileIt).processId);

			copyListOfFiles.push_back(FileInfo(fname));
			copyListOfFiles.back().opened = 1;
			copyListOfFiles.back().processId = processId;
		}
		else
		{

			copyListOfFiles.push_back(FileInfo((*fileIt).fileName));
			copyListOfFiles.back().opened = (*fileIt).opened;
			copyListOfFiles.back().processId = (*fileIt).processId;
		}

		fileIt++;
	}

	list<ProcessInfo>::iterator processIt = processes.begin();

	for (; processIt != processes.end(); processIt++)
	{
		(*processIt).completed = 0;
	}

	while (1) 
	{
		processIt = processes.begin();

		int found = 0;
		int allCompleted = 1;

		for (; processIt != processes.end(); processIt++)
		{
			if ((*processIt).completed) continue;

			((*processIt)).completed = 1;

			allCompleted = 0;

			for (fileIt = copyListOfFiles.begin(); fileIt != copyListOfFiles.end() && loopCondition; fileIt++)
			{
				list<FileInfo>::iterator compFileIt = (*processIt).declaredFiles.begin();

				for (; compFileIt != (*processIt).declaredFiles.end(); compFileIt++)
				{
					if ((*fileIt).equal(&(*compFileIt)))
					{
						if ((*fileIt).opened)
							if ((*processIt).processID == (*fileIt).processId) 
							{	
								(*processIt).completed &= 1;
								break;
							}
							else 
							{
								(*processIt).completed &= 0;
								loopCondition = 0;
								break;
							}
						else 
						{
							(*processIt).completed &= 1;
							break;
						}
					}
				}

			}
			
			if ((*processIt).completed == 1) 
			{	
				
				for (fileIt = copyListOfFiles.begin(); fileIt != copyListOfFiles.end(); fileIt++)
				{
					list<FileInfo>::iterator compFileIt = (*processIt).declaredFiles.begin();

					for (; compFileIt != (*processIt).declaredFiles.end(); compFileIt++)
					{
						if ((*fileIt).equal(&(*compFileIt)))
						{
							(*fileIt).opened = 0;
							break;
						}
					}
				}

				found = 1;

			}

			loopCondition = 1;

		}

		if (allCompleted) 
		{
			copyListOfFiles.clear();

			*fileOpened = 1;
			*openingProcess = processId;

			signal(checkSem);

			return 1;
		}

		if (!found)
		{
			copyListOfFiles.clear();

			signal(checkSem);

			return 0;
		}

	}
}

// wait na mutexList
// wait na mutexProcesses
// moguc wait na waitTillSafe za odgovarajuci proces
void Bank::tryOpen(DWORD processId, Partitions::PartitionInfo* pInfo, char *fname)
{
	wait(access);

	wait(KernelFS::mutexList);
	wait(Bank::mutexProcesses);

	if (Bank::check(processId,fname)) 
	{
		signal(Bank::mutexProcesses);
		signal(KernelFS::mutexList);

		signal(access);

		return;
	}

	for (list<FileInfo>::iterator it = KernelFS::listOfFiles->begin(); it != KernelFS::listOfFiles->end(); it++)
	{
		if ((*it).equalByName(fname)) 
		{ 
			(*it).waittingProcesses.push_back(processId);

			break;
		}
	}


	for (list<ProcessInfo>::iterator it = Bank::processes.begin(); 
		it != Bank::processes.end(); it++)
	{
		if ((*it).processID == processId)
		{
			signal(Bank::mutexProcesses);
			signal(KernelFS::mutexList);

			signal(access);

			wait((*it).waitTillSafe);
			
			return;
		}
	}

	signal(access);
	
}

// wait na mutexList
// wait na mutexProcesses
void Bank::retry(char *fname)
{
	wait(access);

	wait(KernelFS::mutexList);
	wait(Bank::mutexProcesses);

	list<FileInfo>::iterator it = KernelFS::listOfFiles->begin();

	int next = 1;
	int fnameFirst = 1;

	while(it != KernelFS::listOfFiles->end())
	{
		if ((*it).equalByName(fname))
		{
			if (fnameFirst)
			{
				fnameFirst = 0;

				if (!(*it).waittingProcesses.empty())
				{
					for (list<unsigned int>::iterator dwit = (*it).waittingProcesses.begin(); 
						dwit != (*it).waittingProcesses.end(); dwit++)
					{
						unsigned int processId = (*dwit); 

						if (processId == ::GetCurrentThreadId()) continue;

						if (Bank::check(processId,fname))
						{
							for (list<ProcessInfo>::iterator pit = Bank::processes.begin();
								pit != Bank::processes.end(); pit++)
							{
								if ((*pit).processID == processId)
								{
									(*it).waittingProcesses.remove(processId);

									signal(Bank::mutexProcesses);
									signal(KernelFS::mutexList);

									signal(access);

									signal((*pit).waitTillSafe);
									
									return;
								}
							}
						}
					}

					next = 0;
					it = KernelFS::listOfFiles->begin();
				}
				else
				{
					next = 0;
					it = KernelFS::listOfFiles->begin();
				}
			}
			else it++;	
		}
		else
		{
			if (next) it++;
			else
			{
				if (!(*it).opened)
				{
					if ((*it).waittingProcesses.empty()) it++;
					else
					{
						for (list<unsigned int>::iterator dwit = (*it).waittingProcesses.begin(); 
							dwit != (*it).waittingProcesses.end(); dwit++)
						{
							unsigned int processId = (*dwit);

							if (processId == ::GetCurrentThreadId()) continue;

							if (Bank::check(processId,(*it).fileName))
							{
								for (list<ProcessInfo>::iterator pit = Bank::processes.begin();
									pit != Bank::processes.end(); pit++)
								{
									if ((*pit).processID == processId)
									{
										(*it).waittingProcesses.remove(processId);

										signal(Bank::mutexProcesses);
										signal(KernelFS::mutexList);

										signal(access);

										signal((*pit).waitTillSafe);
										
										return;
									}
								}
							}
						}

						it++;
					}
				}
				else it++;
			}
		}
	}
	

	signal(Bank::mutexProcesses);
	signal(KernelFS::mutexList);

	signal(access);
}

//wait na mutexProcesses 
//wait na mutexList
char Bank::declare(DWORD processId, char *fname, int mode)
{
	if ((!Bank::checkDeclare(processId,fname) && !mode) || 
		(Bank::checkDeclare(processId,fname) && mode))	return '0';

	wait(access);

	wait(Bank::mutexProcesses);
	wait(KernelFS::mutexList);

	int fileInTheList = 0;

	for (list<ProcessInfo>::iterator it = Bank::processes.begin();
		it != Bank::processes.end(); it++)
	{
		if ((*it).processID == processId)
		{
			if (mode) 
			{
				for (list<FileInfo>::iterator fit = KernelFS::listOfFiles->begin();
					fit != KernelFS::listOfFiles->end(); fit++)
				{
					if ((*fit).equalByName(fname))
					{
						fileInTheList = 1;
						(*fit).numberOfDeclarations++;
					}
				}

				if (!fileInTheList)
				{
					KernelFS::listOfFiles->push_back(FileInfo(fname));
					KernelFS::listOfFiles->back().opened = 0;
					KernelFS::listOfFiles->back().numberOfDeclarations = 1;
				}

				
				int found = 0;

				for (list<FileInfo>::iterator dit = (*it).declaredFiles.begin();
					dit != (*it).declaredFiles.end(); dit++)
				{
					if ((*dit).equalByName(fname)) found = 1;
				}

				if (!found) (*it).declaredFiles.push_back(FileInfo(fname));

			}
			else 
			{
				for (list<FileInfo>::iterator fit = KernelFS::listOfFiles->begin();
					fit != KernelFS::listOfFiles->end(); fit++)
				{
					if ((*fit).equalByName(fname))
					{
						if ((*fit).opened && ((*fit).processId == processId))
						{
							signal(KernelFS::mutexList);
							signal(Bank::mutexProcesses);

							signal(access);

							return '0';
						}

						(*fit).numberOfDeclarations--;

						break;
					}
				}

				(*it).declaredFiles.remove(FileInfo(fname));

				if ((*it).declaredFiles.empty()) Bank::processes.remove((*it));

			}
			
			signal(KernelFS::mutexList);
			signal(Bank::mutexProcesses);

			signal(access);

			return '1';
		}
	}

	if (!mode) 
	{
		signal(KernelFS::mutexList);
		signal(Bank::mutexProcesses);

		signal(access);

		return '0';
	}

	Bank::processes.push_back(ProcessInfo(processId));

	for (list<FileInfo>::iterator fit = KernelFS::listOfFiles->begin();
		fit != KernelFS::listOfFiles->end(); fit++)
	{
		if ((*fit).equalByName(fname))
		{
			fileInTheList = 1;

			(*fit).numberOfDeclarations++;
		}
	}

	if (!fileInTheList)
	{
		KernelFS::listOfFiles->push_back(FileInfo(fname));
		KernelFS::listOfFiles->back().opened = 0;
		KernelFS::listOfFiles->back().numberOfDeclarations = 1;
	}

	//wait(Bank::processes.back().mutexDeclared);

	/*
	int found = 0;

	for (list<FileInfo>::iterator dit = Bank::processes.back().declaredFiles.begin();
		dit != Bank::processes.back().declaredFiles.end(); dit++)
	{
		if ((*dit).equalByName(fname)) found = 1;
	}

	if (!found) Bank::processes.back().declaredFiles.push_back(FileInfo(fname));
	*/
	//signal(Bank::processes.back().mutexDeclared);

	Bank::processes.back().declaredFiles.push_back(FileInfo(fname));

	signal(KernelFS::mutexList);
	signal(Bank::mutexProcesses);

	signal(access);

	return '1';
}

//radi wait na Bank::mutexProcesses 
//za svaki proces wait na mutexDeclared
int Bank::checkDeclare (DWORD processId, char* fname)
{
	wait(access);

	wait(Bank::mutexProcesses);

	for (list<ProcessInfo>::iterator pit = Bank::processes.begin();
		pit != Bank::processes.end(); pit++)
	{
		if ((*pit).processID == processId)
		{
			for (list<FileInfo>::iterator fit = (*pit).declaredFiles.begin();
				fit != (*pit).declaredFiles.end(); fit++)
			{
				if ((*fit).equalByName(fname)) 
				{
					signal(Bank::mutexProcesses);

					signal(access);

					return 1;
				}
			}
		}
	}

	signal(Bank::mutexProcesses);

	signal(access);

	return 0;
}
