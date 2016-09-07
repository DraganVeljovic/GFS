#include "fs.h"
#include "kernelFS.h"

using namespace std;

FS::~FS()
{

}

char FS::mount(Partition* partition)
{
	return KernelFS::mount(partition);
}

char FS::unmount(char part)
{
	return KernelFS::unmount(part);
}

char FS::format(char part)
{
	return KernelFS::format(part);
}

int FS::readRootDir(char part, EntryNum n, Directory &d)
{
	return KernelFS::readRootDir(part,n,d);
}

char FS::doesExist(char* fname)
{
	return KernelFS::doesExist(fname);
}

	
File* FS::open(char* fname)
{
	return KernelFS::open(fname);
}

char FS::deleteFile(char* fname)
{
	return KernelFS::deleteFile(fname);
}

char FS::declare(char *fname, int mode)
{
	return KernelFS::declare(fname,mode);
}
