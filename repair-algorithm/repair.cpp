#include "Repair.h"
using namespace std;

void doubleLinkOccurrences(Occurrence* prev, Occurrence* curr)
{
	//Set the preceding pointer of the current element
	if (curr)
		curr->setPrev(prev);

	//Set the succeeding pointer of the previous element
	if (prev)
		prev->setNext(curr);
}

void doubleLinkNeighbors(Occurrence* prec, Occurrence* curr)
{
	//Set the preceding pointer of the current element
	if (curr)
		curr->setPrec(prec);

	//Set the succeeding pointer of the previous element
	if (prec)
		prec->setSucc(curr);
}

void addOrUpdatePair(RandomHeap& myHeap, unordered_map<unsigned long long, HashTableEntry*>& hashTable, 
	unsigned long long key, unsigned leftPosition, Occurrence* prec, Occurrence* succ)
{
	if (key == 0)
		return;

	HeapEntry* hp;

	if (hashTable.count(key))
	{
		hashTable[key]->addOccurrence(new Occurrence(key, leftPosition));
	}
	else //First time we've seen this pair
	{
		//Create a heap entry, and initialize the count to 1
		hp = new HeapEntry(key, 1, &myHeap);

		//Create a hash table entry, and initialize it with its heap entry pointer
		hashTable[key] = new HashTableEntry(hp, prec, succ, leftPosition); //This creates the first occurrence (see the constructor)

		//The order of these calls matters: do this first and the hash table entry won't know the index
		myHeap.insert(hp);
	}
}

void extractPairs(const vector<vector<unsigned> >& versions, RandomHeap& myHeap, 
	unordered_map<unsigned long long, HashTableEntry*>& hashTable, vector<VersionDataItem>& versionData,
	RepairTree& repairTree)
{
	vector<unsigned> wordIDs;
	for (size_t v = 0; v < versions.size(); v++)
	{
		unsigned long long currPair;

		wordIDs = versions[v];

		// The previous entry in the HT (used to set preceding and succeeding pointers)
		Occurrence* prevOccurrence(NULL);

		RepairTreeNode* prevTreeNode(NULL);

		// Go through the string and get all overlapping pairs, and process them
		for (size_t i = 0; i < wordIDs.size() - 1; i++)
		{
			// Building level 1 of the repair tree
			// prevTreeNode is to maintain neighbor associations
			prevTreeNode = repairTree.addNodes(wordIDs[i], NULL, prevTreeNode, v, i);

			// Save some metadata for each version
			if (i == 0)
			{
				versionData.push_back(VersionDataItem(prevTreeNode, v, wordIDs.size()));
			}

			// Squeeze the pair of two unsigned numbers together for storage
			currPair = combineToUInt64((unsigned long long)wordIDs[i], (unsigned long long)wordIDs[i+1]);

			// Add the pair to our structures
			addOrUpdatePair(myHeap, hashTable, currPair, i);

			// The first occurrence was the last one added because we add to the head
			Occurrence* lastAddedOccurrence = hashTable[currPair]->getHeadOccurrence();

			// Checks for existence of prev, and links them to each other
			doubleLinkNeighbors(prevOccurrence, lastAddedOccurrence);

			// Update the previous occurrence variable
			prevOccurrence = lastAddedOccurrence;
		}
		// The loop goes to size - 1, so we need to take care of the last wordID
		prevTreeNode = repairTree.addNodes(wordIDs.back(), NULL, prevTreeNode, versions.size() - 1, wordIDs.size() - 1);
	}
}

void removeFromHeap(RandomHeap& myHeap, HeapEntry* hp)
{
	if (hp && !myHeap.empty())
	{
		myHeap.deleteRandom(hp->getIndex());
	}
}

void removeOccurrence(RandomHeap& myHeap, unordered_map<unsigned long long, 
	HashTableEntry*>& hashTable, Occurrence* oc)
{
	if (!oc)
	{
		return;
	}
	unsigned long long key = oc->getPair();
	if (hashTable.count(key))
	{
		HeapEntry* hp = hashTable[key]->getHeapPointer();
		hashTable[key]->removeOccurrence(oc);
		if (hashTable[key]->getSize() < 1)
		{
			removeFromHeap(myHeap, hp);
			hashTable.erase(key);
		}
	}
}

unsigned long long getNewRightKey(unsigned symbol, Occurrence* succ)
{
	if (!succ) return 0;
	unsigned symbolToTheRight = succ->getRight();
	return combineToUInt64(symbol, symbolToTheRight);
}

unsigned long long getNewLeftKey(unsigned symbol, Occurrence* prec)
{
	if (!prec) return 0;
	unsigned symbolToTheLeft = prec->getLeft();
	return combineToUInt64(symbolToTheLeft, symbol);
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
void doRepair(RandomHeap& myHeap, unordered_map<unsigned long long, HashTableEntry*>& hashTable, 
	vector<Association>& associations, unsigned repairStoppingPoint, vector<VersionDataItem>& versionData,
	RepairTree& repairTree)
{
	while (!myHeap.empty())
	{
		unsigned symbol;
		unsigned symbolToTheLeft;
		unsigned symbolToTheRight;
		
		// Get the max from the heap
		HeapEntry hp = myHeap.getMax();

		// The string of 2 chars, used to key into the hashmap
		unsigned long long key = hp.getKey();

		// Get the hash table entry (so all occurrences and so on)
		HashTableEntry* max = hashTable[key];
		size_t numOccurrences = max->getSize();

		// TODO think about this number
		// Thought about it: it should be well below the number of versions
		// Imagine a fragment that occurs in numVersions - 2 of the versions. That's a good fragment, let's keep it. Maybe repairStoppingPoint := numVersions / 2
		if (numOccurrences < repairStoppingPoint)
			return;

		Occurrence* curr;
		Occurrence* prec;
		Occurrence* succ;

		// Will use this as the new symbol (say we're replacing abcd with axd, this is x)
		symbol = nextID();

		curr = max->getHeadOccurrence();

		// Build up the Repair tree
		// Add all the nodes for this symbol -> left, right
		repairTree.addNodes(symbol, curr, NULL);

		// For all occurrences of this entry, do the replacement and modify the corresponding entries
		for (size_t i = 0; i < numOccurrences; i++)
		{
			Occurrence* newLeftOcc(NULL);
			Occurrence* newRightOcc(NULL);
			
			// Get the occurrence and its neighbors
			curr = max->getHeadOccurrence();

			// If curr is null, we have a problem. This should never happen.
			if (!curr)
				break;

			prec = curr->getPrec();
			succ = curr->getSucc();

			// Store the association before you do anything else
			if (i == 0)
				associations.push_back(Association(symbol, curr->getLeft(), curr->getRight(), numOccurrences));
			
			// Now go through all the edge cases (because of the links we have to make, there are a lot)
			bool onLeftEdge(false);
			bool onRightEdge(false);
			bool nearLeftEdge(false);
			bool nearRightEdge(false);

			unsigned long long newLeftKey;
			unsigned long long newRightKey;

			// Use these bools instead of following the pointers repeatedly
			if (!prec)
				onLeftEdge = true;
			else
				if (!prec->getPrec())
					nearLeftEdge = true;
			
			if (!succ)
				onRightEdge = true;
			else
				if (!succ->getSucc())
					nearRightEdge = true;

			newLeftKey = getNewLeftKey(symbol, prec);
			newRightKey = getNewRightKey(symbol, succ);

			unsigned oldLeftIndex, oldRightIndex;
			if (onLeftEdge)
				oldLeftIndex = 0;
			else
				oldLeftIndex = prec->getLeftPositionInSequence();

			oldRightIndex = curr->getLeftPositionInSequence();

			// Just creates the occurrence in the hash table and heap, doesn't link it to its neighbors
			// Passing along the index from the pair we're replacing
			// You get holes eventually (which you want) because 3 pairs get replaced by 2 every time
			addOrUpdatePair(myHeap, hashTable, newLeftKey, oldLeftIndex);
			addOrUpdatePair(myHeap, hashTable, newRightKey, oldRightIndex);

			if (!nearLeftEdge && !onLeftEdge)
			{
				// Have 2 neighbors to the left
				newLeftOcc = hashTable[newLeftKey]->getHeadOccurrence();
				doubleLinkNeighbors(prec->getPrec(), newLeftOcc);
			}
			if (!nearRightEdge && !onRightEdge)
			{
				// Have 2 neighbors to the right
				newRightOcc = hashTable[newRightKey]->getHeadOccurrence();
				doubleLinkNeighbors(newRightOcc, succ->getSucc());
			}
			if (!onRightEdge && !onLeftEdge)
			{
				// A neighbor on each side, link them
				newLeftOcc = hashTable[newLeftKey]->getHeadOccurrence();
				newRightOcc = hashTable[newRightKey]->getHeadOccurrence();
				doubleLinkNeighbors(newLeftOcc, newRightOcc);
			}

			//cerr << "Removing curr: " << curr->getLeft() << "," << curr->getRight() << endl;
			removeOccurrence(myHeap, hashTable, curr);
			
			if (!onRightEdge)
			{
				//cerr << "Removing succ: " << succ->getLeft() << "," << succ->getRight() << endl;
				removeOccurrence(myHeap, hashTable, succ);
			}
			if (!onLeftEdge)
			{
				//cerr << "Removing prec: " << prec->getLeft() << "," << prec->getRight() << endl;
				removeOccurrence(myHeap, hashTable, prec);
			}
		}
	}
}

void cleanup(unordered_map<unsigned long long, HashTableEntry*>& hashTable)
{
	for (unordered_map<unsigned long long, HashTableEntry*>::iterator it = hashTable.begin(); it != hashTable.end(); it++)
	{
		delete it->second;
		it->second = NULL;
	}
}

/*************************************************************************************************/
/*
New Tree Building Code: takes the resulting vector<Association> from repair 
and builds a tree for each version. This is currently pseudocode, soon to be real!
*/


int binarySearch(vector<Association>& associations, unsigned startingLoc, unsigned target)
{
	// TODO
}

RepairTreeNode* buildTree(unsigned loc, unsigned versionNum, vector<Association>& associations)
{
	RepairTreeNode* root = NULL;
	associations[loc].getVersions().erase(versionNum); // Make sure getVersions returns by ref, otherwise this does nothing
	unsigned left = associations[loc].getLeft();
	unsigned right = associations[loc].getRight();

	lLoc = binarySearch(associations, loc, left);
	rLoc = binarySearch(associations, loc, right);

	if (lLoc == -1) root->addLeftChild(new RepairTreeNode(left));
	else root->addLeftChild(buildTree(lLoc, versionNum, associations));

	if (rLoc == -1) root->addRightChild(new RepairTreeNode(right));
	else root->addRightChild(buildTree(rLoc, versionNum, associations));

	return root;
}

int getNextRootLoc(unsigned loc, vector<Association>& associations)
{
	multiset<unsigned> versions = associations[loc].getVersions();
	while (versions.size() <= 0)
	{
		--loc;
		if (loc < 0)
		{
			return -1;
		}
		versions = associations[loc].getVersions();
	}
	return (int)loc;
}

vector<RepairTreeNode*> getTrees(vector<Association>& associations)
{
	unsigned loc = associations.size() - 1;
	RepairTreeNode* root = NULL;
	unsigned versionNum = 0;
	vector<RepairTreeNode*> trees = vector<RepairTreeNode*>();

	while (loc = getNextRootLoc(loc, associations))
	{
		while (v = associations[loc].getVersions().begin()) // TODO getVersions() returns multiset<unsigned>
		{
			trees[v] = buildTree(loc, versionNum);
			--loc; // pre is more efficient
		}
	}
}


/*************************************************************************************************/