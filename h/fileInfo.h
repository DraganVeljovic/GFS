#ifndef FILEINFO_H
#define FILEINFO_H

#include <list>
#include <windows.h>
using namespace std;


class FileInfo {
public:

	char* fileName;

	int opened;

	int numberOfDeclarations;

	int fullDataCluster;

	unsigned long lastIndexCluster;

	unsigned long currentDataCluster;

	unsigned long oldCursor;

	unsigned long readCluster;

	DWORD processId; 

	list<unsigned int> waittingProcesses;

	FileInfo (char* fileName);

	~FileInfo ();

	FileInfo (const FileInfo& fileInfo);

	int equalByName (char* fileName);

	int equal (FileInfo* fInfo);

	friend bool operator== (const FileInfo& f1, const FileInfo& f2)
	{
		if (strlen(f1.fileName) != strlen(f2.fileName)) return false;

		for (unsigned int i = 0; i < strlen(f1.fileName); i++)
		{
			if (f1.fileName[i] != f2.fileName[i]) return false;
		}

		return true;
	}
	
};

#endif