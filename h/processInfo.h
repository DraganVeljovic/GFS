#ifndef PROCESSINFO_H
#define PROCESSINFO_H

#include <list>
#include <windows.h>

class FileInfo;

class ProcessInfo {
public:

	DWORD processID;

	list<FileInfo> declaredFiles;

	int completed;

	HANDLE waitTillSafe;

	ProcessInfo (DWORD processID)
	{
		this->processID = processID;
		waitTillSafe = CreateSemaphore(NULL,0,32,NULL);
		completed = 0;
	}

	ProcessInfo (const ProcessInfo& processInfo)
	{
		this->processID = processInfo.processID;
		this->waitTillSafe = CreateSemaphore(NULL,0,32,NULL);
		this->completed = processInfo.completed;
	}

	~ProcessInfo ()
	{
		CloseHandle(waitTillSafe);
	}

	friend bool operator== (const ProcessInfo& p1, const ProcessInfo& p2)
	{
		if (p1.processID == p2.processID) return true;
		return false;
	}

};

#endif