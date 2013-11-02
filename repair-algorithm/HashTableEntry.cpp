#include "HashTableEntry.h"
using namespace std;

HashTableEntry::HashTableEntry(HeapEntry* hp, unsigned version) : heapEntryPointer(hp), size(1)
{
	unsigned long long key = hp->getKey();
	occurrences = new Occurrence(key, version); // The head of the linked list (Occurrences have a next pointer)
}

void HashTableEntry::increment()
{
	size++;
	heapEntryPointer->increment();
}
void HashTableEntry::decrement()
{
	size--;
	heapEntryPointer->decrement();
}
void HashTableEntry::removeOccurrence(Occurrence* target)
{
	if (!target || !heapEntryPointer)
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
	target = NULL;
}
void HashTableEntry::addOccurrence(Occurrence* oc)
{
	if (!oc || !occurrences)
		return;

	// Adds an occurrence to the head of the linked list	
	oc->setNext(occurrences);
	occurrences->setPrev(oc);

	occurrences = oc;
	increment();
}
Occurrence* HashTableEntry::getHeadOccurrence() const
{
	return occurrences;
}
size_t HashTableEntry::getSize() const
{
	return size;
}
HeapEntry* HashTableEntry::getHeapEntryPointer() const
{
	return heapEntryPointer;
}

HashTableEntry::~HashTableEntry() {
	// heapEntryPointer, what happens when this object is done?
	// Who owns the occurrences?
	if (heapEntryPointer != NULL) {
		delete heapEntryPointer;
		heapEntryPointer = NULL;
	}

	// We do this during repair, they should all be gone by now
	// Occurrence* next(NULL);
	// while (occurrences != NULL) {
	// 	next = occurrences->getNext();
	// 	delete occurrences;
	// 	occurrences = next;
	// }
}