#ifndef META_CLASSES_H
#define META_CLASSES_H

#include "RepairTreeNode.h"
#include <ostream>
#include <set>

class VersionDataItem
{
private:
	RepairTreeNode* firstNode;
	RepairTreeNode* rootNode;
	unsigned versionNum; // aka versionID
	unsigned versionSize; // aka number of words
public:
	// VersionDataItem(RepairTreeNode* firstNode, unsigned versionNum, unsigned versionSize)
	// 	: firstNode(firstNode), versionNum(versionNum), versionSize(versionSize), rootNode(NULL) {}

	VersionDataItem(unsigned versionNum, unsigned versionSize)
		: versionNum(versionNum), versionSize(versionSize) {}

	RepairTreeNode* getRootNode()
	{
		if (rootNode)
			return rootNode;

		RepairTreeNode* current = firstNode;
		if (current)
		{
			while (true)
			{
				if (current->getParent()) // current has a parent, so current is not our root
				{
					current = current->getParent();
				}
				else // current does not have a parent, it must be our root, set it and don't loop through again
				{
					rootNode = current;
					return rootNode;
				}
			}
		}
		return NULL;
	}

	unsigned getVersionSize() const
	{
		return versionSize;
	}

	void setRootNode(RepairTreeNode* rootNode)
	{
		this->rootNode = rootNode; 
	}
};


struct FragInfo
{
	friend std::ostream& operator<<(std::ostream& os, const FragInfo& f)
	{
		return os << f.id << " -> " << "(count = " << f.count << ", fragSize = " << f.fragSize << ")" << std::endl;
	}

	unsigned id;
	unsigned count;
	unsigned fragSize;
	std::string hash;

	FragInfo() : id(-1), count(0), fragSize(0), hash("") {}
	FragInfo(unsigned id, unsigned count, unsigned fragSize, const std::string& hash) : id(id), count(count), fragSize(fragSize), hash(hash) {}
};


//Used to store associations from one symbol to two others
class Association
{
	friend std::ostream& operator<<(std::ostream& os, const Association& a)
	{
		return os << a.symbol << " -> " << "(" << a.left << ", " << a.right << "), " << "freq: " << a.freq << std::endl;
	}

private:
	unsigned symbol;
	unsigned left;
	unsigned right;
	unsigned freq;

	std::multiset<unsigned> versions;
public:
	Association(unsigned symbol, unsigned left, unsigned right, unsigned freq, unsigned firstVersion) :
		symbol(symbol), left(left), right(right), freq(freq)
	{
		versions = std::multiset<unsigned>();
		versions.insert(firstVersion);
	}

	unsigned getSymbol() const
	{
		return symbol;
	}
	unsigned getLeft() const
	{
		return left;
	}
	unsigned getRight() const
	{
		return right;
	}

	void addVersion(unsigned v)
	{
		versions.insert(v);
	}

	std::multiset<unsigned> getVersions() const
	{
		return versions;
	}

	void removeFromVersions(unsigned versionNum)
	{
		versions.erase(versionNum);
	}

	// return begin or false
	int getVersionAtBegin()
	{
		if (versions.begin() == versions.end())
		{
			return -1;
		}
		else
		{
			std::multiset<unsigned>::iterator it = versions.begin();
			return (int)(*it);
		}
	}

};

#endif