#ifndef cmd_make_bf_H
#define cmd_make_bf_H

#include <string>
#include <cstdlib>
#include <cstdint>
#include <iostream>
#include "commands.h"
#include "bloom_filter.h"

class MakeBFCommand: public Command
	{
public:
	static const std::uint32_t defaultKmerSize = 20;
	static const std::uint32_t defaultMinAbundance = 1;
	static const std::uint32_t defaultNumThreads = 1;
	static const std::uint32_t defaultNumHashes = 1;
	static const std::uint64_t defaultNumBits = 500*1000;

public:
	MakeBFCommand(const std::string& name): Command(name) {}
	virtual ~MakeBFCommand() {}
	virtual void short_description (std::ostream& s);
	virtual void usage (std::ostream& s, const std::string& message="");
	virtual void debug_help (std::ostream& s);
	virtual void parse (int _argc, char** _argv);
	virtual int execute (void);
	virtual void make_bloom_filter_fasta (void);
	virtual void make_bloom_filter_kmers (void);
    virtual void make_bloom_filter_simka_pipe (void);
    virtual void make_bloom_filter_simka_gz(void);
    static void add_in_bf (BloomFilter* bf, std::string kmer, std::vector<std::string> line, int idThrd);
    virtual std::string build_output_filename(void);

    int maxMemoryAvailable;
    bool usingMemory;
    bool debug_memory;
    std::string listFilename;
	std::vector<std::string> seqFilenames;
	bool inputIsKmers;
	std::string bfFilename;
	std::string asPerFilename;
	std::uint32_t kmerSize;
	std::uint32_t minAbundance;
	bool          minAbundanceSet;
	std::uint32_t numThreads;
	std::uint32_t numHashes;
	std::uint64_t hashSeed1, hashSeed2;
	std::uint64_t hashModulus;
	std::uint64_t numBits;     // nota bene: numBits <= hashModulus
	std::uint32_t compressor;
	bool inputIsSimka;
    std::string input;
    std::string simka_exp;
    std::string simka_dir;
    };

#endif // cmd_make_bf_H
