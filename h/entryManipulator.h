#ifndef ENTRYANALYZER_H
#define ENTRYANALYZER_H

struct Entry;

class Partitions;

class EntryManipulator {
public:

	static int isFree (Entry* entry);

	static void fillEntry (Entry* entry, char* fname, unsigned long firstIndexCluster);

	static void fillEntry (Entry* entrySrc, Entry* entryDst);

	static void releaseEntry (Entry* entry);

	static int checkName (Entry* entry, char* fname);

	static int compareEntries (Entry* e1, Entry* e2);

};

#endif