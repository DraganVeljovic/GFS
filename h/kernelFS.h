#ifndef KERNELFS_H
#define KERNELFS_H

#include "part.h"
#include "fs.h"
#include "kernelFile.h"
#include "partitions.h"
#include <list>
#include <windows.h>
#include <stdlib.h>


class FileInfo;
class EntryManipulator;
class Partitions;

class KernelFS {
public:

	static list<FileInfo>* listOfFiles;

	static HANDLE mutexList;

	static int fileEqualByName (KernelFile* file, char* name); 
	static int fileEqualByEntry (KernelFile* file, unsigned long entryNumber); 

	static unsigned long consumeCluster (Partitions::PartitionInfo* pInfo);

	static void returnCluster(Partitions::PartitionInfo* pInfo, unsigned long clusterNo);

	KernelFS ();

	static char mount(Partition* partition); //montira particiju
										// vraca dodeljeno slovo
										// ili 0 u slucaju neuspeha

	static char unmount(char part); //demontira particiju oznacenu datim
							// slovom vraca 0 u slucaju neuspeha ili 1 u slucaju uspeha
	static char format(char part); // formatira particiju sa datim slovom;
							// vraca 0 u slucaju neuspeha ili 1 u slucaju uspeha

	static int readRootDir(char part, EntryNum n, Directory &d);
							//prvim argumentom se zadaje particija, drugim redni broj
							//validnog ulaza od kog se po?inje ?itanje,
							//treci argument je adresa na kojoj se smesta procitani niz ulaza

	static char doesExist(char* fname); //argument je naziv fajla zadat
										//apsolutnom putanjom

	static char declare(char* fname, int mode);
							//prvi argument je naziv fajlova zadat apsolutnom putanjom,
							//drugi mod:
							// 1 – najava koriscenja, 0 – odjava koriscenja zadatog fajla

	static File* open(char* fname); 

	static char deleteFile(char* fname);
};

#endif