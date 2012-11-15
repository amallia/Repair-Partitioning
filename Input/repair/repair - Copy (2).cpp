#include <iostream>
#include <fstream>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <iterator>
#include <sstream>
#include <locale>
#include <dirent.h>
#include "HeapEntry.h"
#include "RandomHeap.h"
#include "Tokenizer.h"
#include "Profiler.h"
using namespace std;

unsigned currentID(0);
unsigned nextID()
{
	return ++currentID;
}

unsigned long long combineToUInt64(unsigned long long left, unsigned long long right)
{
	return (left << 32) | right;
}

unsigned getLeft(unsigned long long key)
{
	return key >> 32;
}

unsigned getRight(unsigned long long key)
{
	return (key << 32) >> 32;
}

//Used to store associations from one symbol to two others
struct Association
{
	friend ostream& operator<<(ostream& os, const Association& a)
	{
		return os << a.symbol << " -> " << "(" << a.left << ", " << a.right << "), " << "freq: " << a.freq << endl;
	}

	unsigned symbol;
	unsigned left;
	unsigned right;
	unsigned freq;
	unsigned startPos; //TODO implement
	unsigned endPos; //TODO implement
	Association(unsigned symbol, unsigned left, unsigned right, unsigned freq) : symbol(symbol), left(left), right(right), freq(freq) {}
};

//ObjectPool<HeapEntry> heapPool = ObjectPool<HeapEntry>(1000);

//An Occurrence, which interacts with other occurrences when it gets replaced by a symbol
class Occurrence
{
private:
	//linked list of occurrences needs each occurrence to be able to point to the next one (see HashTableEntry)
	Occurrence* next;
	Occurrence* prev;

	//To modify each other's left and right
	Occurrence* prec;
	Occurrence* succ;
	unsigned left;
	unsigned right;
	unsigned leftPositionInSequence;
public:
	Occurrence() : prec(NULL), succ(NULL), next(NULL), prev(NULL) {}

	// Occurrence(unsigned left, unsigned right) : prec(NULL), succ(NULL), left(left), right(right), next(NULL), prev(NULL) {}

	Occurrence(unsigned long long key) : prec(NULL), succ(NULL), left(key >> 32), right((key << 32) >> 32), next(NULL), prev(NULL) {}

	Occurrence(unsigned long long key, unsigned leftPositionInSequence) : prec(NULL), succ(NULL), left(key >> 32), right((key << 32) >> 32), leftPositionInSequence(leftPositionInSequence), next(NULL), prev(NULL) {}

	Occurrence* getNext()	{return next;}
	Occurrence* getPrev()	{return prev;}
	Occurrence* getPrec()	{return prec;}
	Occurrence* getSucc()	{return succ;}

	unsigned getLeft()	const {return left;}
	unsigned getRight()	const {return right;}

	unsigned getLeftPositionInSequence()	const {return leftPositionInSequence;}

	void setNext(Occurrence* next)	{this->next = next;}
	void setPrev(Occurrence* prev)	{this->prev = prev;}
	void setPrec(Occurrence* prec)	{this->prec = prec;}
	void setSucc(Occurrence* succ)	{this->succ = succ;}

	unsigned long long getPair()
	{
		return combineToUInt64(left, right);
	}
};
//ObjectPool<Occurrence> occurrencePool = ObjectPool<Occurrence>(5000);


struct NeighborListItem
{
	Occurrence* leftMostOcc;
	unsigned index;
	NeighborListItem(Occurrence* leftMostOcc, unsigned index) : leftMostOcc(leftMostOcc), index(index) {}
};

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

class HashTableEntry
{
	HeapEntry* heapPointer;
	Occurrence* occurrences;
	size_t size;
public:
	HashTableEntry(HeapEntry* hp, Occurrence* prec, Occurrence* succ, unsigned leftPosition) : heapPointer(hp), size(1)
	{
		unsigned long long key = hp->getKey();

		occurrences = new Occurrence(key, leftPosition); //The head of the linked list (Occurrences have a next pointer)

		doubleLinkNeighbors(prec, occurrences);
		doubleLinkNeighbors(occurrences, succ);
	}
	HashTableEntry(HeapEntry* hp, Occurrence* oc) : heapPointer(hp), size(1)
	{
		occurrences = oc;
	}

	void increment()
	{
		size++;
		heapPointer->increment();
	}
	void decrement()
	{
		size--;
		heapPointer->decrement();
	}
	void removeOccurrence(Occurrence* target)
	{
		if (!target || !heapPointer)
			return;

		Occurrence* next = target->getNext();
		Occurrence* prev = target->getPrev();

		doubleLinkOccurrences(prev, next);

		decrement();
		if (size < 1)
		{
			if (occurrences == target)
			{
				delete occurrences;
				occurrences = NULL;
				return;
			}
			cerr << "we didn't find the target, wtf?" << endl;
			return;
		}

		if (occurrences == target)
		{
			occurrences = next;
		}
		delete target;
	}
	void addOccurrence(Occurrence* oc)
	{
		if (!oc || !occurrences)
			return;

		//Adds an occurrence to the head of the linked list	
		oc->setNext(occurrences);
		occurrences->setPrev(oc);

		occurrences = oc;
		increment();
	}
	Occurrence* getHeadOccurrence() const
	{
		return occurrences;
	}
	size_t getSize() const
	{
		return size;
	}
	HeapEntry* getHeapPointer() const
	{
		return heapPointer;
	}
};
//ObjectPool<HashTableEntry> hashTablePool = ObjectPool<HashTableEntry>(2000);

// unsigned numAdd(0);
void addOrUpdatePair(RandomHeap& myHeap, unordered_map<unsigned long long, HashTableEntry*>& hashTable, unsigned long long key, unsigned leftPosition, Occurrence* prec = NULL, Occurrence* succ = NULL)
{
	if (key == 0)
		return;

	HeapEntry* hp;

	//if (numAdd % 1000 == 0)
	//	cout << "Updating key: " << key << " (number " << numAdd << ")" << endl;

	// ++numAdd;

	if (hashTable.count(key))
	{
		hashTable[key]->addOccurrence(new Occurrence(key, leftPosition));
	}
	else //First time we've seen this pair
	{
		//Create a heap entry, and initialize the count to 1
		hp = new HeapEntry(key, 1, &myHeap);

		//HeapEntry* hp = heapPool.getNew();
		//hp->init(key, 1);

		//Create a hash table entry, and initialize it with its heap entry pointer
		hashTable[key] = new HashTableEntry(hp, prec, succ, leftPosition); //This creates the first occurrence (see the constructor)
		// hashTable[key] = 0;
		//The order of these calls matters: do this first and the hash table entry won't know the index
		//Is this still true? TODO
		myHeap.insert(hp);
	}
}

void extractPairs(const vector<vector<unsigned> >& versions, RandomHeap& myHeap, unordered_map<unsigned long long, HashTableEntry*>& hashTable, vector<NeighborListItem>& neighborList)
{
	vector<unsigned> wordIDs;
	for (size_t v = 0; v < versions.size(); v++)
	{
		unsigned long long currPair;

		wordIDs = versions[v];

		// hashTable.reserve(50000);

		//The previous entry in the HT (used to set preceding and succeeding pointers)
		Occurrence* prevOccurrence(NULL);

		//Go through the string and get all overlapping pairs, and process them
		for (size_t i = 0; i < wordIDs.size() - 1; i++)
		{
			currPair = combineToUInt64((unsigned long long)wordIDs[i], (unsigned long long)wordIDs[i+1]);
			unsigned left = getLeft(currPair);
			unsigned right = getRight(currPair);

			// cerr << "Left: " << left << endl;
			// cerr << "Right: " << right << endl;
			addOrUpdatePair(myHeap, hashTable, currPair, i);

			//The first occurrence was the last one added because we add to the head
			Occurrence* lastAddedOccurrence = hashTable[currPair]->getHeadOccurrence();

			//Maintain a list of pointers to the leftmost occurrence in each version
			if (i == 0)
			{
				neighborList.push_back(NeighborListItem(lastAddedOccurrence, v));
			}

			//Checks for existence of prev, and links them to each other
			doubleLinkNeighbors(prevOccurrence, lastAddedOccurrence);

			//Update the previous occurrence variable
			prevOccurrence = lastAddedOccurrence;
		}		
	}
}

void removeFromHeap(RandomHeap& myHeap, HeapEntry* hp)
{
	if (hp && !myHeap.empty())
	{
		myHeap.deleteRandom(hp->getIndex());
	}
}

unsigned numRemove(0);
void removeOccurrence(RandomHeap& myHeap, unordered_map<unsigned long long, HashTableEntry*>& hashTable, Occurrence* oc)
{
	if (!oc)
	{
		return;
	}
	// cout << "Removing occurrence number: " << ++numRemove;
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

int binarySearch(const vector<Association>& associations, unsigned target, int leftPos, int rightPos)
{
	// cout << endl;
	// cout << "Target: " << target << endl;
	// cout << "Searching between: " << leftPos << " and " << rightPos << endl;
	// system("pause");

	//indexes that don't make sense, means we haven't found the target
	if (leftPos > rightPos)
		return -1;

	if (leftPos == rightPos)
	{
		if (associations[leftPos].symbol == target)
		{
			return leftPos;
		}
		return -1;
	}

	int mid = floor(((float)leftPos + rightPos) / 2);
	unsigned midVal = associations[mid].symbol;

	// cout << "mid: " << mid << ", val: " << midVal << endl;
	// system("pause");

	//found it
	if (target == midVal)
		return mid;

	//target is on the left
	if (target < midVal)
		return binarySearch(associations, target, leftPos, mid);
	
	//target is on the right
	if (target > midVal)
		return binarySearch(associations, target, mid+1, rightPos);
} 

vector<unsigned> expand(const vector<Association>& associations, int pos, unordered_map<unsigned, vector<unsigned> > knownExpansions)
{
	// cout << "Expanding position: " << pos << endl;
	// system("pause");
	if (knownExpansions[pos].size() > 0)
		return knownExpansions[pos];

	unsigned left = associations[pos].left;
	unsigned right = associations[pos].right;

	int lpos = binarySearch(associations, left, 0, pos);
	int rpos = binarySearch(associations, right, 0, pos);

	vector<unsigned> lret, rret;
	if (lpos == -1)	
	{
		lret = vector<unsigned>();		
		lret.push_back(left);
	}
	else
	{
		lret = expand(associations, lpos, knownExpansions);
	}

	if (rpos == -1)
	{
		rret = vector<unsigned>(); 		
		rret.push_back(right);
	}
	else
	{
		rret = expand(associations, rpos, knownExpansions);
	}

	// return lret + rret; //overload + for vector<unsigned> or just do something else
	//the stl code below might be slow, we don't have to use it if it's a problem
	lret.insert(lret.end(), rret.begin(), rret.end());
	knownExpansions[pos] = lret;
	return lret;
} 

/*

Extract back to original string

*/
vector<unsigned> undoRepair(const vector<Association>& associations)
{
	//knownExpansions is used for memoization
	unordered_map<unsigned, vector<unsigned> > knownExpansions = unordered_map<unsigned, vector<unsigned> >();
	vector<unsigned> result = expand(associations, associations.size() - 1, knownExpansions);
	
	return result;
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
*/
void doRepair(RandomHeap& myHeap, unordered_map<unsigned long long, HashTableEntry*>& hashTable, vector<Association>& associations, unsigned minNumOccurrences, vector<NeighborListItem>& neighborList)
{
	while (!myHeap.empty())
	{
		unsigned symbol;
		unsigned symbolToTheLeft;
		unsigned symbolToTheRight;
		
		//Get the max from the heap
		HeapEntry hp = myHeap.getMax();

		//The string of 2 chars, used to key into the hashmap
		unsigned long long key = hp.getKey();

		//Get the hash table entry (so all occurrences and so on)
		HashTableEntry* max = hashTable[key];
		size_t numOccurrences = max->getSize();

		// TODO think about this number
		// Thought about it: it should be well below the number of versions
		// Imagine a fragment that occurs in numVersions - 2 of the versions. That's a good fragment, let's keep it. Maybe minNumOccurrences := numVersions / 2
		if (numOccurrences < minNumOccurrences)
			return;

		Occurrence* curr;
		Occurrence* prec;
		Occurrence* succ;

		//Will use this as the new symbol (say we're replacing abcd with axd, this is x)
		symbol = nextID();

		//For all occurrences of this entry, do the replacement and modify the corresponding entries
		for (size_t i = 0; i < numOccurrences; i++)
		{
			Occurrence* newLeftOcc(NULL);
			Occurrence* newRightOcc(NULL);
			
			//Get the occurrence and its neighbors
			curr = max->getHeadOccurrence();

			//If curr is null, we have a problem. This should never happen.
			if (!curr)
				break;

			prec = curr->getPrec();
			succ = curr->getSucc();

			//Store the association before you do anything else
			if (i == 0)
				associations.push_back(Association(symbol, curr->getLeft(), curr->getRight(), numOccurrences));
			
			//Now go through all the edge cases (because of the links we have to make, there are a lot)
			bool onLeftEdge(false);
			bool onRightEdge(false);
			bool nearLeftEdge(false);
			bool nearRightEdge(false);

			unsigned long long newLeftKey;
			unsigned long long newRightKey;

			//Use these bools instead of following the pointers repeatedly
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

			//Just creates the occurrence in the hash table and heap, doesn't link it to its neighbors
			addOrUpdatePair(myHeap, hashTable, newLeftKey, oldLeftIndex);
			addOrUpdatePair(myHeap, hashTable, newRightKey, oldRightIndex);

			if (!nearLeftEdge && !onLeftEdge)
			{
				//Have 2 neighbors to the left
				newLeftOcc = hashTable[newLeftKey]->getHeadOccurrence();
				doubleLinkNeighbors(prec->getPrec(), newLeftOcc);
			}
			if (!nearRightEdge && !onRightEdge)
			{
				//Have 2 neighbors to the right
				newRightOcc = hashTable[newRightKey]->getHeadOccurrence();
				doubleLinkNeighbors(newRightOcc, succ->getSucc());
			}
			if (!onRightEdge && !onLeftEdge)
			{
				//A neighbor on each side, link them
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

			if (nearLeftEdge)
			{
				//update neighborList
				if (newLeftKey)
				{
					Occurrence* currLeftMostOcc = hashTable[newLeftKey]->getHeadOccurrence();
					for (unsigned i = 0; i < neighborList.size(); i++)
					{
						if (neighborList[i].leftMostOcc == currLeftMostOcc)
						{
							neighborList[i].leftMostOcc = currLeftMostOcc;
						}
					}
					
				}
			}
		}
	}
} 

vector<unsigned> stringToWordIDs(const string& text, unordered_map<unsigned, string>& IDsToWords)
{
	// Profiler::getInstance().start("stringToWordIDs");
	unordered_map<unsigned, unsigned> uniqueWordIDs = unordered_map<unsigned, unsigned>();

	vector<unsigned> ret = vector<unsigned>();

    istringstream iss(text);
	
	vector<string> tokens;
	//copy(istream_iterator<string>(iss),
	//		 istream_iterator<string>(),
	//		 back_inserter<vector<string> >(tokens));
	
	//remove punctuation and produce vector of strings (called tokens)
	string delimiters = ",.:;\"/!?() []{}\n";
	bool trimEmpty = false;
	tokenize(text, tokens, delimiters, trimEmpty);

	//Would it be faster to download the code for a hash function? TODO
	locale loc;
	const collate<char>& coll = use_facet<collate<char> >(loc);
	unsigned myHash;

	unordered_map<unsigned, unsigned>::iterator it;
	for (size_t i = 0; i < tokens.size(); i++)
	{
		myHash = coll.hash(tokens[i].data(), tokens[i].data() + tokens[i].length());
		it = uniqueWordIDs.find(myHash);
		unsigned theID;
		if (it != uniqueWordIDs.end())
		{
			//found it, so use its ID
			theID = it->second;
		}
		else
		{
			//Didn't find it, give it an ID
			theID = nextID();
			uniqueWordIDs[myHash] = theID;
			IDsToWords[theID] = tokens[i];
			// cerr << tokens[i] << endl;
			//it->second = nextID();
		}
		ret.push_back(theID);
	}
	// Profiler::getInstance().end("stringToWordIDs");
	return ret;
}

char* getText(const string& filename, int& length)
{
	char* buffer;

	ifstream is;
	is.open ( filename.c_str(), ios::binary );

	if (is.fail())
	{
		cerr << "Could not open input file. Moving on..." << endl;
		return NULL;
	}
	// get length of file:
	is.seekg (0, ios::end);
	length = is.tellg();
	is.seekg (0, ios::beg);

	// allocate memory:
	buffer = new char [length];

	// read data as a block:
	is.read (buffer,length);
	is.close();

	return buffer;
}

void cleanup(unordered_map<unsigned long long, HashTableEntry*>& hashTable)
{
	for (unordered_map<unsigned long long, HashTableEntry*>::iterator it = hashTable.begin(); it != hashTable.end(); it++)
	{
		delete it->second;
		it->second = NULL;
	}
}

void writeAssociations(const vector<Association>& associations, ostream& os = cerr)
{
	for (size_t i = 0; i < associations.size(); i++)
	{
		os << associations[i];
	}
}

void getVersions(vector<char**>& versions)
{
	//TODO figure out the input for this, depends on Jinru's code
}

/*
We must be able to re-extract for the algorithm to be correct
Associations must be monotonically decreasing for the algorithm to be optimal
*/
bool checkOutput(vector<Association> associations, vector<unsigned> wordIDs)
{
	//Check to see whether it's correct
	vector<unsigned> extractedWordIDs = undoRepair(associations);
	for (size_t i = 0; i < extractedWordIDs.size(); i++)
	{
		//cerr << "Orig: " << wordIDs[i] << ", compare: " << extractedWordIDs[i] << endl; 
		if (extractedWordIDs[i] != wordIDs[i])
		{
			return false;
		}
	}

	cerr << "Passed correctness check..." << endl;

	//If it's correct, check to see whether it's optimal
	for (size_t i = 0; i < associations.size() - 1; i++)
	{
		// cerr << "i: " << i << " freq[i]: " << associations[i].freq << ", freq[i+1]: " << associations[i+1].freq << endl;
		// system("pause");
		if (associations[i].freq < associations[i+1].freq)
		{
			return false;
		}
	}

	cerr << "Passed optimal check..." << endl;

	return true;
}

vector<vector<unsigned> > getPartitioning(RandomHeap& myHeap, unordered_map<unsigned long long, HashTableEntry*>& hashTable, unsigned minFragSize, vector<NeighborListItem>& neighborList)
{
	vector<vector<unsigned> > ret = vector<vector<unsigned> >();
	if (myHeap.empty())
		return ret;

	for (unsigned i = 0; i < neighborList.size(); i++)
	{
		ret.push_back(vector<unsigned>());
		Occurrence* current = neighborList[i].leftMostOcc;

		unsigned currVal, nextVal, diff;
		// Starting from the left, read the indexes in order, and group small fragments together
		// That is, only add an index to the result array if the difference between it and the next one is large
		ret[i].push_back(0);
		while (current)
		{
			currVal = current->getLeftPositionInSequence();
			if (current->getSucc())
			{
				current = current->getSucc();
				nextVal = current->getLeftPositionInSequence();
				diff = nextVal - currVal;
				if (diff >= minFragSize)
				{
					// if (currVal != 0)
					// 	ret[i].push_back(currVal);
					ret[i].push_back(nextVal);
				}
			}
			else
			{
				break;
			}
		}
	}
	return ret;
}

//TODO save the fragments and find them in the file, to be able to record their frequencies
vector<vector<vector<unsigned> > > printAndSaveFragments(const vector<unsigned>& wordIDs, const vector<vector<unsigned> >& starts, unordered_map<unsigned, string>& IDsToWords, ostream& os = cerr)
{
	vector<vector<vector<unsigned> > > fragments = vector<vector<vector<unsigned> > >();
	unsigned start, end, theID;
	string word;
	for (unsigned v = 0; v < starts.size() - 1; v++)
	{
		fragments.push_back(vector<vector<unsigned> >());
		for (unsigned i = 0; i < starts[v].size() - 1; i++)
		{
			fragments[v].push_back(vector<unsigned>());
			start = starts[v][i];
			end = starts[v][i+1];
			os << "Fragment " << i << ": ";
			for (unsigned j = start; j < end; j++)
			{
				theID = wordIDs[j];
				fragments[v][i].push_back(theID);
				word = IDsToWords[theID];
				os << word << " ";
			}
			os << endl;
		}
	}
	return fragments;
}

void writeResults(const vector<unsigned>& wordIDs, const vector<vector<unsigned> >& starts, const vector<Association>& associations, unordered_map<unsigned, string>& IDsToWords, const string& outFilename, bool printFragments = false, bool printAssociations = false)
{
	ofstream os(outFilename.c_str());

	os << "Results of re-pair partitioning..." << endl << endl;
	os << "First 100 fragment boundaries (or all of them if there are less):" << endl;
	
	for (unsigned v = 0; v < starts.size(); v++)
	{
		os << "Version " << v << endl;
		for (unsigned i = 0; i < starts[v].size() && i < 100; i++)
			os << starts[v][i] << endl;
		os << endl;
	}

	// os << "Number of fragment boundaries: " << starts.size() << endl;
	// os << "Number of fragments: " << (starts.size() - 1) << endl << endl;

	vector<vector<vector<unsigned> > > fragments;
	if (printFragments)
	{
		os << "Printing fragments identified by repair..." << endl;
		//fragments = printAndSaveFragments(wordIDs, starts, IDsToWords, os);
	}
	
	if (printAssociations)
	{
		os << "Printing associations created by repair (symbol -> pair)..." << endl;
		writeAssociations(associations, os);
	}
}

string getFileName(const string& filepath)
{
	vector<string> tokens;
	string delimiters = "/\\";
	bool trimEmpty = false;
	tokenize(filepath.c_str(), tokens, delimiters, trimEmpty);
	return tokens.back();
}

string stripDot(const string& filepath)
{
// 	vector<string> tokens;
// 	bool trimEmpty = false;
// 	tokenize(filepath, tokens, ".", trimEmpty);
// 	string s = tokens[0];
	return filepath.substr(1, filepath.size());
}

int getFileNames(const string& dir, vector<string>& files)
{
    DIR *dp;
    struct dirent *dirp;
    if ((dp = opendir(dir.c_str())) == NULL) {
        cout << "Error(" << errno << ") opening " << dir << endl;
        return errno;
    }

    while ((dirp = readdir(dp)) != NULL) {
    	if (! (string(dirp->d_name) == "." || string(dirp->d_name) == ".." ) )
        	files.push_back(string(dirp->d_name));
    }
    closedir(dp);
    return 0;
}

void printIDtoWordMapping(unordered_map<unsigned, string>& IDsToWords, ostream& os = cerr)
{
	for (unordered_map<unsigned, string>::iterator it = IDsToWords.begin(); it != IDsToWords.end(); it++)
	{
		os << it->first << ": " << it->second << endl;
	}
}

int main(int argc, char* argv[])
{
	//createOutputDir();

	//heap, repair
	string test = "repair";

	if (test == "heap")
	{
		RandomHeapTest test = RandomHeapTest(1000);
		exit(0);
	}

	if (test == "repair")
	{
		Profiler::getInstance().start("all");
		string inputFilepath = "./Input/ints/";

		// Default param values
		unsigned minFragSize = 2; //in words
		unsigned minNumOccurrences = 2; //pairs that occur less than this amount of times will no be replaced

		if (argc == 2 && (string) argv[1] == "help")
		{
			cerr << "Usage:" << endl;
			cerr << "repair <directory filepath> <minFragSize> <minNumOccurrences>" << endl;
			cerr << "repair <directory filepath> <minFragSize> (default minNumOccurrences = " << minNumOccurrences << ")" << endl;
			cerr << "repair <directory filepath> (default minFragSize = " << minFragSize << " and minNumOccurrences = " << minNumOccurrences << ") " << endl;
			cerr << "repair (default directory is: " << inputFilepath << ")" << endl;
			exit(0);
		}

		if (argc > 1)
			inputFilepath = (string)argv[1];

		if (argc > 2)
			minFragSize = atoi(argv[2]);

		if (argc > 3)
			minNumOccurrences = atoi(argv[3]);
		
		vector<string> inputFilenames = vector<string>();
		if (getFileNames(inputFilepath, inputFilenames))
			return errno;

		char* text;
		int fileSize;
		vector<vector<unsigned> > versions = vector<vector<unsigned> >();
		vector<unsigned> wordIDs;
		unordered_map<unsigned, string> IDsToWords = unordered_map<unsigned, string>();
		for (unsigned i = 0; i < inputFilenames.size(); i++)
		{
			stringstream filenameSS;
			filenameSS << inputFilepath << inputFilenames[i];
			string filename = filenameSS.str();
			text = getText(filename, fileSize);
			if (!text)
				continue;
			// cerr << text;
			// system("pause");
			wordIDs = stringToWordIDs(text, IDsToWords);
			versions.push_back(wordIDs);
			delete text;
			text = NULL;
		}
		
		// By this time, IDsToWords should contain the mappings of IDs to words in all versions
		// printIDtoWordMapping(IDsToWords);

		//Allocate the heap, hash table, array of associations, and list of neighbor structures
		RandomHeap myHeap;
		unordered_map<unsigned long long, HashTableEntry*> hashTable = unordered_map<unsigned long long, HashTableEntry*> ();
		vector<Association> associations = vector<Association>();
		vector<NeighborListItem> neighborList = vector<NeighborListItem>();

		extractPairs(versions, myHeap, hashTable, neighborList);
	
		int numPairs = hashTable.size();

		doRepair(myHeap, hashTable, associations, minNumOccurrences, neighborList);
		
		const vector<vector<unsigned> > starts = getPartitioning(myHeap, hashTable, minFragSize, neighborList);

		stringstream outFilenameStream;
		outFilenameStream << "Output/results" << stripDot(inputFilepath);
		string outputFilename = outFilenameStream.str();

		outputFilename = "Output/results.txt";

		bool printFragments = true;
		bool printAssociations = false;

		writeResults(wordIDs, starts, associations, IDsToWords, outputFilename, printFragments, printAssociations);

		stringstream command;
		command << "start " << outputFilename.c_str();
		system(command.str().c_str());

		stringstream ss;
		ss << "Filename: " << inputFilepath << endl;
		ss << "Size: " << fileSize << " bytes" << endl; 
		ss << "Word Count: " << wordIDs.size() << endl;
		ss << "Unique Pair Count (in original file): " << numPairs << endl;

		// cerr << "Checking output... " << endl;
		// if (checkOutput(associations, wordIDs))
		// 	cerr << "Output ok!";
		// else
		// 	cerr << "Check failed!";
		// cerr << endl;

		Profiler::getInstance().end("all");
		Profiler::getInstance().setInputSpec(ss.str());	
		Profiler::getInstance().writeResults("Output/profile-functions.txt");
		// cleanup(hashTable);
		// system("pause");
	}
}