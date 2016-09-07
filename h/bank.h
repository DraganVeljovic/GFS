#ifndef BANK_H
#define BANK_H

#include "partitions.h"
#include <list>
#include <windows.h>
using namespace std;

class ProcessInfo;

class Bank {
public:

	static list<ProcessInfo> processes;

	static HANDLE mutexProcesses;

	static HANDLE access;

	static HANDLE checkSem;

	static int check (DWORD processId, char* fname);

	static void tryOpen (DWORD processId, Partitions::PartitionInfo* pInfo,char* fname);

	static void retry (char* fname);

	static char declare(DWORD processId, char* fname, int mode);

	static int checkDeclare (DWORD processId, char* fname);

};

#endif