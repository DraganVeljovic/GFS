#include "file.h"
#include "kernelFile.h"

using namespace std;

File::File (Partitions::PartitionInfo* pInfo, char* fname, unsigned long entryNumber,unsigned long firstIndexCluster, 
		unsigned long size, unsigned long currentDataCluster,unsigned long lastIndexCluster, int fullDataCluster,
		unsigned long readCluster, unsigned long oldCursor)
{
	myImpl = new KernelFile(pInfo,fname,entryNumber,firstIndexCluster,size,currentDataCluster,lastIndexCluster, 
		fullDataCluster,readCluster,oldCursor);
}


File::~File()
{
	delete myImpl;
}

char File::write(BytesCnt toBeWritten, char *buffer)
{
	return myImpl->write(toBeWritten,buffer);
}

BytesCnt File::read(BytesCnt toBeRead, char *buffer)
{
	return myImpl->read(toBeRead,buffer);
}

char File::eof()
{
	return myImpl->eof();
}

BytesCnt File::filePos()
{
	return myImpl->filePos();
}

char File::seek(BytesCnt pos)
{
	return myImpl->seek(pos);
}

BytesCnt File::getFileSize()
{
	return myImpl->getFileSize();
}

char File::truncate()
{
	return myImpl->truncate();
}