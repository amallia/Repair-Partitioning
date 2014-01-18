#ifndef REPAIR_H
#define REPAIR_H

#include <iostream>
#include <fstream>
#include <vector>
#include <unordered_map>
#include <iterator>
#include <sstream>
#include <locale>
#include <string>
#include "Occurrence.h"
#include "HashTableEntry.h"
#include "../util/md5.h"
#include "../indexed-heap/HeapEntry.h"
#include "../indexed-heap/IndexedHeap.h"
#include "../partitioning/Partitioning.h"
#include "../util/FileUtils.h"
#include "RepairTreeNode.h"
#include "Util.h"
#include <assert.h>


typedef std::unordered_map<unsigned long long, HashTableEntry*> RepairHashTable;

class RepairAlgorithm
{
private:
	/***** Private Vars *****/

	// The entry at i is the number of fragments for version i
	unsigned* versionPartitionSizes;
	
	// The offsets that define fragments, for all versions [v0:f0 v0:f1 v0:f2 v1:f0 v1:f1 v2:f0 v2:f1 ...]
	unsigned* offsets;

	// Pass this to the partitioning alg, it prevents us from going too far down a repair tree
	unsigned numLevelsDown;
	
	// The minimum allowed size for a fragment, used during partitioning
	unsigned minFragSize;

	// How much should be favor fragmenting into small fragments. Should we keep them bigger, go for more occurrences, etc.
	double fragmentationCoefficient;

	// Each inner vector: The wordIDs for that version, outer vector: all the versions
	std::vector<std::vector<unsigned> > versions;

	// One of the main structures used in repair, allows us to choose entries by priority, in our case numOccurrences
	IndexedHeap myHeap;
	
	// The other main structure used in repair, allows us to access entries by key, which is in our case the pair of symbols
	RepairHashTable hashTable;
	
	// The result of repair, a list of associations in the form (roughly) symbol -> (left, right)
	std::vector<Association> associations;
	

	/***** Repair Core Algorithm *****/
	void addOccurrence(unsigned long long key, unsigned version, int idx);

	void removeOccurrence(unsigned long long key, unsigned v, int idx);

	void extractPairs();

	void removeOccurrence(Occurrence* oc);

	int scanLeft(unsigned v, int idx);

	int scanRight(unsigned v, int idx);

	void checkVersionAndIdx(unsigned v, int idx);

	unsigned long long getKeyAtIdx(unsigned v, int idx);

	void doRepair(unsigned repairStoppingPoint);

	
	/***** Tree building, offsets, and partitioning ******/
	void deleteTree(RepairTreeNode* node);

	RepairTreeNode* buildTree(int loc, unsigned versionNum);

	int getNextRootLoc(int loc);

	unsigned calcOffsets(RepairTreeNode* node);

public:

	RepairAlgorithm(std::vector<std::vector<unsigned> > versions,
		unsigned numLevelsDown = 1, unsigned minFragSize = 2, double fragmentationCoefficient = 1.0) : 
	versions(versions), numLevelsDown(numLevelsDown), minFragSize(minFragSize), fragmentationCoefficient(fragmentationCoefficient)
	{
		// Allocate the heap, hash table, array of associations, and list of pointers to neighbor structures	
		myHeap = IndexedHeap();
		
		hashTable = RepairHashTable();
		
		associations = std::vector<Association>();
		
	 	unsigned maxArraySize = versions.size() * MAX_NUM_FRAGMENTS_PER_VERSION;

		this->offsets = new unsigned[maxArraySize];

		this->versionPartitionSizes = new unsigned[versions.size()];
	}

	unsigned* getVersionPartitionSizes();

	unsigned* getOffsetsAllVersions();

	std::vector<Association> getAssociations(unsigned repairStoppingPoint = 0)
	{
		// Run through the string and grab all the initial pairs
		// Add them to all the structures
		extractPairs();

		// Replace pairs with symbols until done
		// (either some early stop condition or one symbol left)
		doRepair(repairStoppingPoint);

		// Return the payload
		return this->associations;		
	}

	void cleanup();
};


// The whole point of this is to be able to order the lists of offsets by version number
// After repair, we go through the resulting associations and build trees for each version
// That iteration doesn't go in order of versionNum
// Our output must be ordered by version number, so we do the following
// TODO finish this explanation
class PartitionList
{
private:
	std::vector<unsigned> offsets;
	unsigned versionNum;
public:
	PartitionList(unsigned versionNum) : versionNum(versionNum) {}
	PartitionList() {}

	void push(unsigned offset)
	{
		offsets.push_back(offset);
	}

	int size() const
	{
		return offsets.size();
	}

	unsigned get(unsigned index) const
	{
		return offsets[index];
	}

	unsigned getVersionNum() const
	{
		return versionNum;
	}

	~PartitionList()
	{
		offsets.clear();
	}
};

class SortPartitionsByVersionNumComparator
{
public:
	bool operator() (const PartitionList lhs, const PartitionList rhs) const
	{
		return lhs.getVersionNum() < rhs.getVersionNum();
	}
};

typedef std::set<PartitionList, SortPartitionsByVersionNumComparator> SortedPartitionsByVersionNum;
#endif
