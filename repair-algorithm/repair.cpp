#include "Repair.h"
using namespace std;

void RepairAlgorithm::addOccurrence(unsigned long long key, unsigned version, int idx)
{
//	cerr << "addOccurrence(" << getKeyAsString(key) << ", " << version << ", " << idx << ")" << endl;
//	cerr << "Key as pair: " << getKeyAsString(key) << endl;
	if (hashTable.count(key) > 0) // We've seen this pair before
	{
//		cerr << "Count: " << hashTable[key]->getSize() << endl;
		hashTable[key]->addOccurrence(version, idx);
	}
	else // First time we've seen this pair
	{
//		cerr << "Count: 0" << endl;
		HeapEntry* entry = myHeap.insert(key);

		// Create a hash table entry, and initialize it with its heap entry pointer
		// This creates the first occurrence (see the constructor)
		hashTable[key] = new HashTableEntry(entry, version, idx);
	}
}

void RepairAlgorithm::removeOccurrence(unsigned long long key, unsigned v, int idx)
{
//	cerr << "removeOccurrence(" << getKeyAsString(key) << ", " << v << ", " << idx << ")" << endl;

	if (myHeap.empty()) {
		return;
	}

	if (hashTable.count(key) < 1) {
		cerr << "Key: " << key << " not found in hashTable" << endl;
		system("pause");
		return;
	}

	// Assertions
	checkVersionAndIdx(v, idx);
	assert(hashTable[key] != NULL);

	// cerr << "removeOccurrence(" << key << ", " << v << ", " << idx << ")" << endl;
	// cerr << "hashTable[key]: " << hashTable[key] << endl;
	// cerr << "hashTable[key]->getSize(): " << hashTable[key]->getSize() << endl;
	// system("pause");

	// Remove this occurrence at this key
	hashTable[key]->removeOccurrence(v, idx);
	
	// If we've removed all the occurrences at this key, remove the heap entry as well
	if (hashTable[key]->getSize() < 1) {
		// TODOs
		// 1) Is idxInHeap always defined in this way?
		// 2) Will there always be a swap?
		int idxInHeap = hashTable[key]->getHeapEntryPointer()->getIndex();

		// We can already set our heap entry pointer to null because we'll be deleting by idxInHeap
		hashTable[key]->setHeapEntryPointer(NULL);

		// To understand this, look at the implementation of myHeap.deleteAtIdx(idxInHeap)
		// If indexOfEntryThatGotSwapped is -1, that means there was no swap
		int indexOfEntryThatGotSwapped = myHeap.deleteAtIndex(idxInHeap);
		if (indexOfEntryThatGotSwapped != -1) {
			unsigned long long keyOfEntryThatGotSwapped = myHeap.getAtIndex(indexOfEntryThatGotSwapped)->getKey();

			// When we delete from the heap, we use a swap with the last element
			// Well, hashTable[keyOfLastElement] was pointing to it, so that's not very nice
			// Our delete function returns to us the new location of the swapped last element specifically so we can use it here like so
			assert(hashTable.count(keyOfEntryThatGotSwapped) > 0);
			assert(hashTable[keyOfEntryThatGotSwapped] != NULL);
			hashTable[keyOfEntryThatGotSwapped]->setHeapEntryPointer(myHeap.getAtIndex(indexOfEntryThatGotSwapped));
		}

		hashTable.erase(key);
	}
}

void RepairAlgorithm::checkVersionAndIdx(unsigned v, int idx)
{
	assert(v >= 0 && v < this->versions.size());
	assert(idx >= 0 && idx < versions[v].size());
}

int RepairAlgorithm::scanLeft(unsigned v, int idx)
{
	checkVersionAndIdx(v, idx);
	while (idx > 0) {
		if (versions[v][--idx] != 0) return idx;
	}
	return -1;
}

int RepairAlgorithm::scanRight(unsigned v, int idx)
{
	checkVersionAndIdx(v, idx);
	while (idx < versions[v].size() - 1) {
		if (versions[v][++idx] != 0) return idx;
	}
	return -1;
}

unsigned long long RepairAlgorithm::getKeyAtIdx(unsigned v, int idx)
{
	checkVersionAndIdx(v, idx);
	if (versions[v][idx] == 0) {
		return 0;
	}
	int rightIdx = scanRight(v, idx);
	if (rightIdx == -1)
		return 0;

	if (versions[v][idx] == 0 || versions[v][rightIdx] == 0) {
		return 0;
	}

	return combineToUInt64(versions[v][idx], versions[v][rightIdx]);
}

void RepairAlgorithm::extractPairs()
{
	for (size_t v = 0; v < versions.size(); v++)
	{
		unsigned long long currPair;

		// Go through the string and get all overlapping pairs, and process them
		for (size_t i = 0; i < versions[v].size() - 1; i++)
		{
			// Squeeze the pair of two unsigned numbers together for storage
			currPair = combineToUInt64((unsigned long long)versions[v][i], (unsigned long long)versions[v][i+1]);

			// Add this occurrence of the pair to our structures
			this->addOccurrence(currPair, v, i);
		}
	}
//	cerr << "Number of versions: " << versions.size() << endl;
//	cerr << "Number of distinct pairs: " << hashTable.size() << endl;
}

/*
	While the heap is not empty, get the max and process it (that is, replace all occurrences and modify all prec and succ pointers)
	The max will keep getting removed, as well as the occurrences it touches
	Two new occurrences will be added (resulting from the replacement)
	So 3 occurrences are removed and 2 are added during each iteration
		Don't forget to link the new occurrences together

	Example of one iteration: abcd -> axd (replacing bc with symbol x)
		New occurrences to add:		ax, xd
		Old occurrences to remove:	ab, bc, cd

	We used to rely on the linked lists in the end to do the partitioning
	Now we're going to use associations
	-> Now we're going to build some trees inside here -yk, 2/24/13

*/
void RepairAlgorithm::doRepair(unsigned repairStoppingPoint)
{
//	HashTableEntry* ht;
//	for (auto it = hashTable.begin(); it != hashTable.end(); it++)
//	{
//		unsigned long long k = it->first;
//		ht = it->second;
//		cerr << getKeyAsString(k) << ": " << ht << endl;
//	}
//	exit(1);

	while (!myHeap.empty())
	{
		// Print the vectors
//		for (unsigned v = 0; v < versions.size(); v++) {
//			// Print each vector in one line
//			cerr << "Version " << v << ": ";
//			for (unsigned i = 0; i < versions[v].size(); i++) {
//				cerr << versions[v][i] << " ";
//			}
//			cerr << endl;
//		}
//		system("pause");

		unsigned symbol;
		
		// Get the max from the heap
		HeapEntry* hp = myHeap.getMax();

		assert(hp != NULL);

		// The pair of ints represented as one 64 bit int
		unsigned long long key = hp->getKey();

		assert(hashTable.count(key));

		// Get the hash table entry (so all occurrences and so on)
		HashTableEntry* max = hashTable[key];

		assert(max != NULL);

		size_t numOccurrences = max->getSize();

		// TODO think about this number
		// Thought about it: it should be well below the number of versions
		// Imagine a fragment that occurs in numVersions - 2 of the versions. That's a good fragment, let's keep it. Maybe repairStoppingPoint := numVersions / 2
		if (numOccurrences < repairStoppingPoint)
			return;

		// Will use this as the new symbol (say we're replacing abcd with axd, this is x)
		symbol = nextWordID();

		unsigned totalCountOfCurrPair = 0;

		// If we ever have 3 of the same symbol in a row, an interesting bug happens
		// BUG DESCRIPTION: given 3 of the same symbol in a row, we have 2 consecutive equivalent occurrences of the same pair
		// When we do the removes and adds for one of them, the other one becomes invalid
		// Without checking for this case, we proceed to do the removes and adds for the now invalid pair, causing a runtime error
		// So, if there are two adjacent indexes (abs(idx - prevIdx) < 2) then we just skip replacement for that occurrence, as it is invalid
		int prevIdx;

		// For all versions
		for (size_t v = 0; v < versions.size(); v++)
		{
			if (!max->hasLocationsAtVersion(v)) {
				continue;
			}

			prevIdx = -1;
			auto indexes = max->getLocationsAtVersion(v);

			cerr << "Replacement: [" << symbol << " -> " << getKeyAsString(key) << "]" << endl;

			// For all locations of the pair in the current version
			for (auto it = indexes.begin(); it != indexes.end(); ++it)
			{
				int idx = *it;

				// This isn't good enough.
				// First of all the indexes are unordered, so you might get 5, then 7, then 6, which would screw things up
				// Second, even if they were, what about 5 or 10 such occurrences? We would skip replacing all but one of them, which is also wrong
				if (prevIdx >= 0) {
					if (abs(idx - prevIdx) < 2) {
						prevIdx = idx;
						continue;
					}
				}

				// EXAMPLE: idx = 3, so currKey = (5,6)
				// 0 1 2 3 4 5 6 7 8
				// 1 3 0 5 0 0 6 2 2

				// Find the key to the left of this one and remove that occurrence of it from our structures
				// 1 3 0 5 0 0 6 2 2
				// We want leftKey = (3,5)
				// leftIdx = scanLeft(v, idx) // should be 1
				// leftKey = getKeyAtIdx(leftIdx)
				// hashTable[leftKey]->removeOccurrence(v, leftIdx)

				int leftIdx = scanLeft(v, idx);
				if (leftIdx != -1) {
					unsigned long long leftKey = getKeyAtIdx(v, leftIdx);
					if (leftKey != 0) {
						assert(hashTable.count(leftKey));
						assert(hashTable[leftKey] != NULL);
						removeOccurrence(leftKey, v, leftIdx);
					}
				}

				// Find the key to the right of this one and remove that occurrence of it from our structures
				// 1 3 0 5 0 0 6 2 2
				// We want leftKey = (6,2)
				// rightIdx = scanRight(v, idx) // should be 6
				// rightKey = getKeyAtIdx(rightIdx)
				// hashTable[rightKey]->removeOccurrence(v, rightIdx)

				int rightIdx = scanRight(v, idx); // should be 6
				if (rightIdx != -1) {
					unsigned long long rightKey = getKeyAtIdx(v, rightIdx);
					if (rightKey != 0) {
						assert(hashTable.count(rightKey));
						assert(hashTable[rightKey] != NULL);
						removeOccurrence(rightKey, v, rightIdx);
					}
				}

				// We have the current key, remove this occurrence of it from our structures
				// 1 3 0 5 0 0 6 2 2 key = (5,6) and idx = 3
				if (key != 0) {
					assert(hashTable.count(key));
					assert(hashTable[key] != NULL);
					removeOccurrence(key, v, idx);
				}


				// Store the association and which version it occurs in
				if (totalCountOfCurrPair == 0)
					associations.push_back(Association(symbol, versions[v][idx], versions[v][rightIdx], numOccurrences, v));
				else
					associations.back().addVersion(v);



				// Now the replacement: 7 -> (5,6)
				// We modify the actual array of word Ids
				// 1 3 0 7 0 0 0 2 2
				versions[v][idx] = symbol;
				if (rightIdx != -1) {
					versions[v][rightIdx] = 0;
				}

				// Now add the 2 new pairs
				// (3,7) at idx 1 and (7,2) at idx 3
				if (leftIdx != -1) {
					unsigned long long newLeftKey = getKeyAtIdx(v, leftIdx);
					if (newLeftKey != 0)
					{
						this->addOccurrence(newLeftKey, v, leftIdx);
					}
				}
				if (rightIdx != -1) {
					unsigned long long newRightKey = getKeyAtIdx(v, idx);
					if (newRightKey != 0)
					{
						this->addOccurrence(newRightKey, v, idx);
					}
				}

				totalCountOfCurrPair++;

				prevIdx = idx;
			}
		}
	}
}

// Release memory from all structures
void RepairAlgorithm::cleanup()
{
	for (auto it = hashTable.begin(); it != hashTable.end(); it++) {
		delete (*it).second;
	}

	for (size_t i = 0; i < versions.size(); ++i)
	{
		versions[i].clear();
	}
	versions.clear();

	this->associations.clear();
	resetcurrentWordID();
	resetFragID();
}

/*************************************************************************************************/
/*
New Tree Building Code: takes the resulting vector<Association> from repair 
and builds a tree for each version
*/
/*************************************************************************************************/

RepairTreeNode* RepairAlgorithm::buildTree(int loc, unsigned versionNum)
{
	// Allocate the current node and set its symbol
	RepairTreeNode* root = new RepairTreeNode(associations[loc].getSymbol());

	// Keep track of which versions we've processed in order to choose a root (see getNextRootLoc)
	associations[loc].removeFromVersions(versionNum);
	
	unsigned left = associations[loc].getLeft();
	unsigned right = associations[loc].getRight();

	int lLoc = binarySearch(left, associations, 0, loc);
	int rLoc = binarySearch(right, associations, 0, loc);

	if (lLoc == -1) root->setLeftChild(new RepairTreeNode(left));
	else root->setLeftChild(buildTree(lLoc, versionNum));

	if (rLoc == -1) root->setRightChild(new RepairTreeNode(right));
	else root->setRightChild(buildTree(rLoc, versionNum));

	return root;
}

int RepairAlgorithm::getNextRootLoc(int loc)
{
	while (associations[loc].getVersions().size() <= 0)
	{
		--loc;
		if (loc < 0)
		{
			return -1;
		}
	}
	return loc;
}

unsigned* RepairAlgorithm::getVersionPartitionSizes() {
	return this->versionPartitionSizes;
}

unsigned* RepairAlgorithm::getOffsetsAllVersions()
{
	int loc = associations.size() - 1;
	RepairTreeNode* currRoot = NULL;
	int versionNum = 0;

	SortedPartitionsByVersionNum offsetMap = SortedPartitionsByVersionNum();
	PartitionList theList;

	RepairDocumentPartition partitionAlg = RepairDocumentPartition(this->associations, this->versions.size(),
		this->numLevelsDown, this->minFragSize, this->fragmentationCoefficient);

	vector<unsigned> bounds;
	while (true)
	{
		loc = getNextRootLoc(loc);
		if (loc == -1) break;

		while (true)
		{
			// TODO verify that this is the lowest version number in the list
			// Wait, why? I don't remember the logic...
			// Ok, versionNum is one of the versions in which this association occurred
			// We don't go in order of versionNum, which is worrying because we use it as an array index below
			// It is the lowest because we return begin() and the set is sorted by symbol
			// BULLSHIT: the lowest for that location, but not the lowest overall, IDIOT!
			versionNum = associations[loc].getVersionAtBegin();
			if (versionNum == -1) break;

			// Assert that versionNum is valid
			assert(versionNum < versions.size() && versionNum >= 0);
	
			currRoot = buildTree(loc, versionNum);

			// Let's see if this tree is reasonable
			// int countNodes = currRoot->getCountNodes();

			this->calcOffsets(currRoot);
			resetOffset();

			theList = PartitionList(versionNum);
			bounds = vector<unsigned>();

			partitionAlg.getPartitioningOneVersion(currRoot, this->numLevelsDown, bounds, this->minFragSize, versions[versionNum].size());

			for (size_t i = 0; i < bounds.size(); i++) {
				theList.push(bounds[i]);
			}

			// Now theList should be populated, just insert it into the sorted set
			offsetMap.insert(theList);

			// TODO Do we need to do anything with theList?

			// Deallocate all those tree nodes
			this->deleteTree(currRoot);
		}
		--loc;
	}

	unsigned totalOffsetInArray = 0;
	unsigned numVersions = 0;

	// Post processing to get offsetMap into offsets and versionPartitionSizes
	for (SortedPartitionsByVersionNum::iterator it = offsetMap.begin(); it != offsetMap.end(); it++)
	{
		// Reusing the same var from above, should be ok
		theList = *it;
		for (size_t j = 0; j < theList.size(); j++)
		{
			this->offsets[totalOffsetInArray + j] = theList.get(j);
		}
		this->versionPartitionSizes[numVersions++] = theList.size();
		totalOffsetInArray += theList.size();
	}

	// Clean up offsetMap
	offsetMap.clear();

	return this->offsets;
}

// Not tested, well sorta tested
void RepairAlgorithm::deleteTree(RepairTreeNode* node) {
	RepairTreeNode* leftChild = node->getLeftChild();
	RepairTreeNode* rightChild = node->getRightChild();
	if (leftChild) {
		deleteTree(leftChild);
	}
	if (rightChild) {
		deleteTree(rightChild);
	}
	delete node;
	node = NULL;
}

unsigned RepairAlgorithm::calcOffsets(RepairTreeNode* node)
{
	if (!node) return 0;

	// node is not a terminal
	if (node->getLeftChild())
	{
		RepairTreeNode* leftChild = node->getLeftChild();
		RepairTreeNode* rightChild = node->getRightChild();
		
		// The left ones must be set first, because the right ones depend on them
		unsigned leftOffset = calcOffsets(leftChild);
		calcOffsets(rightChild);
		
		node->setOffset(leftOffset);
		
		// By calling the recursive function first, we guarantee that these sizes are set
		unsigned leftSize = leftChild->getSize();
		unsigned rightSize = rightChild->getSize();
		node->setSize(leftSize + rightSize);
		
		return leftOffset;
	}

	// node is a terminal, so its size is 1 by definition
	node->setSize(1);

	// We're going in pre-order (right, that's what it's called?)
	unsigned offset = nextOffset();
	node->setOffset(offset);
	return offset;
}
