#include <vector>
#include <algorithm>
#include "UndoRepair.h"
using namespace std;

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