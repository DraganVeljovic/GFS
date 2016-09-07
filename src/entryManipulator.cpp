#include "entryManipulator.h"
#include "fs.h"
#include <cstring>

#include <iostream>
using namespace std;

int EntryManipulator::isFree(Entry *entry)
{
	for (int i = 0; i < 8; i++)
		if (entry->name[i] != 0) return 0;

	for (int i = 0; i < 3; i++)
		if (entry->ext[i] != 0) return 0;

	if (entry->reserved != 0) return 0;

	if (entry->firstIndexCluster != 0) return 0;

	if (entry->size != 0) return 0;

	return 1;
}

void EntryManipulator::fillEntry (Entry* entry, char* fname, unsigned long firstIndexCluster)
{
	fname += 3;

	int i = 0;

	while (*fname != '.') {
		entry->name[i] = *fname;
		i++;
		fname++;
	}

	for (i; i < 8; i++) {
		entry->name[i] = ' ';
	}

	i = 0;
	
	for (i; i < 3; i++) {
		entry->ext[i] = fname[i+1];
	}

	entry->firstIndexCluster = firstIndexCluster;

	entry->reserved = 0;

	entry->size = 0;
}

#pragma warning ( disable : 4996)

void EntryManipulator::fillEntry(Entry* entrySrc, Entry* entryDst)
{
	for (int i=0; i<8; i++)
	{
		entryDst->name[i] = entrySrc->name[i];
		if (i<3) entryDst->ext[i] = entrySrc->ext[i];
	}

	entryDst->reserved = entrySrc->reserved;

	entryDst->firstIndexCluster = entrySrc->firstIndexCluster;

	entryDst->size = entrySrc->size;

}

void EntryManipulator::releaseEntry(Entry* entry)
{
	for (int i = 0; i < 8; i++)
	{
		entry->name[i] = 0;
		if (i < 3) entry->ext[i] = 0;
	}
	
	entry->reserved = 0;

	entry->firstIndexCluster = 0;

	entry->size = 0;
	
}

int EntryManipulator::checkName(Entry* entry, char* fname)
{
	fname += 3;

	int i = 0;

	while (*fname != '.') {
		
		if (entry->name[i] != fname[0]) 
		{
			return 0;
		}
		i++; 

		fname++;
	}
	
	return 1;
}

int EntryManipulator::compareEntries(Entry *e1, Entry *e2)
{
	for (int i = 0; i < 8; i++)
		if (e1->name[i] != e2->name[i]) return 0;

	for (int i = 0; i < 3; i++)
		if (e1->ext[i] != e2->ext[i]) return 0;

	if (e1->reserved != e2->reserved) return 0;

	if (e1->firstIndexCluster != e2->firstIndexCluster) return 0;

	if (e1->size != e2->size) return 0;

	return 1;
}