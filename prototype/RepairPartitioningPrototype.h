#ifndef REPAIR_PARTITIONING_PROTOYPE_H
#define REPAIR_PARTITIONING_PROTOYPE_H

#include "../repair-algorithm/Repair.h"
#include "../partitioning/Partitioning.h"
#include "../repair-algorithm/Util.h"
#include <ostream>
#include <sstream>
#include <vector>
#include <unordered_map>
#include <string>
#include <assert.h>


class RepairPartitioningPrototype
{
private:

	// The outer vector represents all versions
	// The vector at position i contains fragment objects for version i
	std::vector<std::vector<FragInfo > > fragments;

	// Unique Fragments in all the versions
	std::unordered_map<std::string, FragInfo> uniqueFrags;

public:
	RepairPartitioningPrototype() 
	{
		fragments = std::vector<std::vector<FragInfo > >();
		uniqueFrags = std::unordered_map<std::string, FragInfo>();
	}

	double getScore(std::ostream& os);

	void setFragmentInfo(
		const std::vector<std::vector<unsigned> >& versions, 
		std::ostream& os, 
		bool print);

	void updateUniqueFragmentHashMap();

	void writeAssociations(
		const std::unordered_map<unsigned, Association>& associations, std::ostream& os) const;

	void writeResults(
		const std::vector<std::vector<unsigned> >& versions, 
		const BaseFragmentsAllVersions& baseFragmentsAllVersions,
		std::unordered_map<unsigned, std::string>& IDsToWords,
		const std::string& outFilename);

	void printIDtoWordMapping(std::unordered_map<unsigned, std::string>& IDsToWords, 
		std::ostream& os = std::cerr) const;

	double runRepairPartitioning(
		std::vector<std::vector<unsigned> > versions,
		BaseFragmentsAllVersions& baseFragmentsAllVersions,
		unsigned numLevelsDown = 5,
		unsigned minFragSize = 1,
		float fragmentationCoefficient = 1.0);


	void checkAssociations(
		const std::vector<std::vector<unsigned> >& versions,
		const std::unordered_map<unsigned, Association>& associations) const;

	void checkOffsets(
		const std::vector<std::vector<unsigned> >& versions,
		unsigned* offsetsAllVersions,
		unsigned* versionPartitionSizes) const;

	int run(int argc, char* argv[]);
};

#endif
