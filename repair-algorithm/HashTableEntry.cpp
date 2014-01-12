#include "HashTableEntry.h"
using namespace std;

void HashTableEntry::increment()
{
	heapEntryPointer->increment();
}

void HashTableEntry::decrement()
{
	heapEntryPointer->decrement();
}

unordered_set<int> HashTableEntry::getLocationsAtVersion(unsigned version)
{
	assert(locationsInDoc.find(version) != locationsInDoc.end());
	return locationsInDoc[version];
}

void HashTableEntry::addOccurrence(unsigned version, int idx)
{
	if (locationsInDoc.find(version) == locationsInDoc.end()) {
		locationsInDoc[version] = unordered_set<int>();
	}
	locationsInDoc[version].insert(idx);
	increment();
}

void HashTableEntry::removeOccurrence(unsigned version, int idx)
{
	assert(locationsInDoc.find(version) != locationsInDoc.end());
	locationsInDoc[version].erase(idx);
	decrement();
	if (locationsInDoc[version].size() < 1)
	{
		this->heapEntryPointer->setDeleted();
	}
}

// The heap's priority is defined as the number of occurrences of the pair (the pair is that entry's key)
size_t HashTableEntry::getSize() const
{
	return this->heapEntryPointer->getPriority();
}

// TODO remove these 2 (getter and setter)
HeapEntry* HashTableEntry::getHeapEntryPointer() const
{
	return heapEntryPointer;
}
void HashTableEntry::setHeapEntryPointer(HeapEntry* newHeapEntryPointer)
{
	heapEntryPointer = newHeapEntryPointer;
}
