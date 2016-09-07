#include "fileInfo.h"
#include "processInfo.h"
#include <cstring>

FileInfo::FileInfo(char* fileName) : opened(0), numberOfDeclarations(0), fullDataCluster(0),
	lastIndexCluster(0), currentDataCluster(0), oldCursor(0), readCluster(0)
{
	this->fileName = new char[strlen(fileName) + 1];

	for (unsigned long i = 0; i < strlen(fileName); i++)
	{
		this->fileName[i] = fileName[i];
	}

	this->fileName[strlen(fileName)] = '\0';

}

int FileInfo::equalByName(char* fileName)
{

	for (unsigned int i = 0; i < strlen(fileName); i++)
	{
		if (this->fileName[i] != fileName[i]) return 0;
	}

	return 1;
}

int FileInfo::equal(FileInfo *fInfo)
{
	return equalByName(fInfo->fileName);
}

FileInfo::FileInfo(const FileInfo& fileInfo)
{
	this->fileName = new char[strlen(fileInfo.fileName) + 1];

	for (unsigned long i = 0; i < strlen(fileInfo.fileName); i++)
	{
		this->fileName[i] = fileInfo.fileName[i];
	}

	this->fileName[strlen(fileInfo.fileName)] = '\0';
}

FileInfo::~FileInfo ()
{
	delete fileName;
}
