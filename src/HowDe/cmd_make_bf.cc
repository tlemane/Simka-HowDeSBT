// cmd_make_bf.cc-- convert a sequence file to a bloom filter

#include <string>
#include <cstdlib>
#include <cstdint>
#include <iostream>
#include <vector>
#include <fstream>
#include "utilities.h"
#include "hash.h"
#include "jelly_kmers.h"
#include "bloom_filter.h"
#include "bloom_filter_file.h"
#include "support.h"
#include "commands.h"
#include "cmd_make_bf.h"
#include "gzstream.h"

#ifdef __has_include
#  if __has_include(<filesystem>)
#    include <filesystem>
     namespace fs = std::filesystem;
#  elif __has_include(<experimental/filesystem>)
#    include <experimental/filesystem>
     namespace fs = std::experimental::filesystem;
#  else
#    error "Missing <filesystem>"
#  endif
#endif

using std::string;
using std::vector;
using std::cout;
using std::cerr;
using std::endl;
#define u32 std::uint32_t
#define u64 std::uint64_t

void MakeBFCommand::short_description
   (std::ostream& s)
	{
	s << commandName << "-- convert a sequence file to a bloom filter" << endl;
	}

void MakeBFCommand::usage
   (std::ostream& s,
	const string& message)
	{
	if (!message.empty())
		{
		s << message << endl;
		s << endl;
		}

	short_description(s);
	s << "usage: " << commandName << " <filename> [<filename>..] [options]" << endl;
	//    123456789-123456789-123456789-123456789-123456789-123456789-123456789-123456789
	s << "  <filename>         (cumulative) a sequence file, e.g. fasta, fastq, or kmers" << endl;
	s << "                     (one bloom filter is created, for the union of the" << endl;
	s << "                     sequence files)" << endl;
	s << "  --input=<filename>     input file is matrix from simka" << endl;
    s << "  --simkaIn=<filename>    fof simka exp" << endl;
    s << "  --simkaOut=<dir>   directory dump bf" << endl;
    s << "  --memory=<N>       max memory" << endl;
    s << "  --kmersin          input files are kmers" << endl;
	s << "                     (by default input files are expected to be fasta or fastq)" << endl;
	s << "  --out=<filename>   name for bloom filter file" << endl;
	s << "                     (by default this is derived from first sequence filename)" << endl;
	s << "  --list=<filename>  file containing a list of bloom filters to create; this is" << endl;
	s << "                     used in place of the <filename>s on the command line; the" << endl;
	s << "                     file format is described below" << endl;
	s << "  --asper=<filename> name of an existing bloom filter file to extract settings" << endl;
	s << "                     from; that file's --k, --hashes, --seed, --modulus," << endl;
	s << "                     --bits and compression type will be used if they are not" << endl;
	s << "                     otherwise specified on the command line" << endl;
	s << "  --k=<N>            kmer size (number of nucleotides in a kmer)" << endl;
	s << "                     (default is " << defaultKmerSize << ")" << endl;
	s << "  --min=<N>          kmers occuring fewer than N times are left out of the" << endl;
	s << "                     bloom filter; this does not apply when --kmersin is used" << endl;
	s << "                     (default is " << defaultMinAbundance << ")" << endl;
	s << "  --threads=<N>      number of threads to use during kmerization" << endl;
	s << "                     (default is " << defaultNumThreads << ")" << endl;
	s << "  --hashes=<N>       how many hash functions to use for the filter" << endl;
	s << "                     (default is " << defaultNumHashes << ")" << endl;
	s << "  --seed=<number>    the hash function's 56-bit seed" << endl;
	s << "  --seed=<number>,<number>  both the hash function seeds; the second seed is" << endl;
	s << "                     only used if more than one hash function is being used" << endl;
	s << "                     (by default the second seed is the first seed plus 1)" << endl;
	s << "  --modulus=<M>      set the hash modulus, if larger than the number of bits" << endl;
	s << "                     (by default this is the same as the number of bits)" << endl;
	s << "  --bits=<N>         number of bits in the bloom filter" << endl;
	s << "                     (default is " << defaultNumBits << ")" << endl;
	s << "  --uncompressed     make the filter with uncompressed bit vector(s)" << endl;
	s << "                     (this is the default)" << endl;
	s << "  --rrr              make the filter with RRR-compressed bit vector(s)" << endl;
	s << "  --roar             make the filter with roar-compressed bit vector(s)" << endl;
	s << endl;
	s << "When --list is used, each line of the file corresponds to a bloom filter. The" << endl;
	s << "format of each line is" << endl;
	s << "  <filename> [<filename>..] [--kmersin] [--out=<filename>]" << endl;
	s << "with meaning the same as on the command line. No other options (e.g. --k or" << endl;
	s << "--bits) are allowed in the file. These are specified on the command line and" << endl;
	s << "will affect all the bloom filters." << endl;
	s << endl;
	s << "When --kmersin is used, each line of the sequence input files is a single kmer," << endl;
	s << "as the first field in the line. Any additional fields on the line are ignored." << endl;
	s << "For example, with --k=20 this might be" << endl;
	s << "  ATGACCAGATATGTACTTGC" << endl;
	s << "  TCTGCGAACCCAGACTTGGT" << endl;
	s << "  CAAGACCTATGAGTAGAACG" << endl;
	s << "   ..." << endl;
	s << "Every kmer in the file(s) is added to the filter. No counting is performed," << endl;
	s << "and --min is not allowed." << endl;
	}

void MakeBFCommand::debug_help
   (std::ostream& s)
	{
	s << "--debug= options" << endl;
	s << "  settings" << endl;
	s << "  add" << endl;
	s << "  contains" << endl;
	s << "  kmers" << endl;
	s << "  strings" << endl;
	}

void MakeBFCommand::parse
   (int		_argc,
	char**	_argv)
	{
	int		argc;
	char**	argv;

	// defaults

	listFilename = "";
	inputIsKmers = false;
	bfFilename   = "";
    usingMemory  = false;
    input   = "";
	simka_exp    = "";
    simka_dir    = "";
    kmerSize     = defaultKmerSize;      bool kmerSizeSet     = false;
	minAbundance = defaultMinAbundance;       minAbundanceSet = false;
	numThreads   = defaultNumThreads;
	numHashes    = defaultNumHashes;     bool numHashesSet    = false;
	hashSeed1    = 0;                    bool hashSeed1Set    = false;
	hashSeed2    = 0;                    bool hashSeed2Set    = false;
	numBits      = defaultNumBits;       bool numBitsSet      = false;
	hashModulus  = 0;                    bool hashModulusSet  = false;
	compressor   = bvcomp_uncompressed;  bool compressorSet   = false;
    inputIsSimka = false;

#ifdef useJellyHash
	hashSeed1 = JellyHashSeed;
#endif

	// skip command name

	argv = _argv+1;  argc = _argc - 1;
	if (argc <= 0) chastise ();

	//////////
	// scan arguments
	//////////

	for (int argIx=0 ; argIx<argc ; argIx++)
		{
		string arg = argv[argIx];
		string argVal;
		if (arg.empty()) continue;

		string::size_type argValIx = arg.find('=');
		if (argValIx == string::npos) argVal = "";
		                         else argVal = arg.substr(argValIx+1);

		// --help, etc.

		if ((arg == "--help")
		 || (arg == "-help")
		 || (arg == "--h")
		 || (arg == "-h")
		 || (arg == "?")
		 || (arg == "-?")
		 || (arg == "--?"))
			{ usage (cerr);  std::exit (EXIT_SUCCESS); }

		if ((arg == "--help=debug")
		 || (arg == "--help:debug")
		 || (arg == "?debug"))
			{ debug_help(cerr);  std::exit (EXIT_SUCCESS); }

		// --simka
		
        if (is_prefix_of (arg, "--input="))
            { input = argVal; inputIsSimka = true; continue; }

        
        //--exp
        
        if (is_prefix_of (arg, "--simkaIn="))
            { simka_exp = argVal; continue;}

        //--simkaOut
        
        if (is_prefix_of (arg, "--simkaOut="))
            { simka_dir = argVal; continue;}
        
        //--memory

        if (is_prefix_of (arg, "--memory="))
            { usingMemory = true; maxMemoryAvailable = std::stoi(argVal); continue; }        
    
        // --kmersin

		if ((arg == "--kmersin")
		 ||	(arg == "--askmers="))
			{ inputIsKmers = true;  continue; }

		// --out=<filename>

		if ((is_prefix_of (arg, "--out="))
		 ||	(is_prefix_of (arg, "--output=")))
			{ bfFilename = argVal;  continue; }

		// --list=<filename>

		if (is_prefix_of (arg, "--list="))
			{ listFilename = argVal;  continue; }

		// --asper=<filename>

		if (is_prefix_of (arg, "--asper="))
			{ asPerFilename = argVal;  continue; }

		// --k=<N>

		if ((is_prefix_of (arg, "K="))
		 ||	(is_prefix_of (arg, "--K="))
		 ||	(is_prefix_of (arg, "k="))
		 ||	(is_prefix_of (arg, "--k="))
		 ||	(is_prefix_of (arg, "--kmer="))
		 ||	(is_prefix_of (arg, "--kmersize=")))
			{
			kmerSize = string_to_u32(argVal);
			kmerSizeSet = true;
			continue;
			}

		// --min=<N>

		if ((is_prefix_of (arg, "--min="))
		 ||	(is_prefix_of (arg, "--abundance=")))
			{
			minAbundance = string_to_u32(argVal);
			if (minAbundance < 1) minAbundance = 1;
			minAbundanceSet = (minAbundance > 1);
			continue;
			}

		// --threads=<N>

		if ((is_prefix_of (arg, "--threads="))
		 ||	(is_prefix_of (arg, "T="))
		 ||	(is_prefix_of (arg, "--T=")))
			{
			numThreads = string_to_u32(argVal);
			if (numThreads == 0)
				chastise ("(in \"" + arg + "\") number of threads cannot be zero");
			continue;
			}

		// --hashes=<N>

		if ((is_prefix_of (arg, "--hashes="))
		 ||	(is_prefix_of (arg, "H="))
		 ||	(is_prefix_of (arg, "--H=")))
			{
			numHashes = string_to_u32(argVal);
			numHashesSet = true;
			continue;
			}

		// --seed=<number> or --seed=<number>,<number>

		if ((is_prefix_of (arg, "--seed="))
		 || (is_prefix_of (arg, "S="))
		 || (is_prefix_of (arg, "--S=")))
			{
			string::size_type commaIx = argVal.find(',');
			if (commaIx == string::npos)
				{
			    hashSeed1 = string_to_u64 (argVal);
				hashSeed1Set = true;
				}
			else
				{
			    hashSeed1 = string_to_u64 (argVal.substr(0,commaIx));
			    hashSeed2 = string_to_u64 (argVal.substr(commaIx+1));
				hashSeed1Set = hashSeed2Set = true;
				}
			continue;
			}

		// --modulus=<M>

		if ((is_prefix_of (arg, "--modulus="))
		 ||	(is_prefix_of (arg, "M="))
		 ||	(is_prefix_of (arg, "--M=")))
			{
			hashModulus = string_to_unitized_u64(argVal);
			hashModulusSet = true;
			continue;
			}

		// --bits=<N>

		if ((is_prefix_of (arg, "--bits="))
		 ||	(is_prefix_of (arg, "B="))
		 ||	(is_prefix_of (arg, "--B=")))
			{
			numBits = string_to_unitized_u64(argVal);
			numBitsSet = true;
			continue;
			}

		// bit vector type

		if (arg == "--uncompressed")
			{
			compressor    = bvcomp_uncompressed;
			compressorSet = true;
			continue;
			}

		if ((arg == "--rrr")
		 || (arg == "--RRR"))
			{
			compressor    = bvcomp_rrr;
			compressorSet = true;
			continue;
			}

		if ((arg == "--roar")
		 || (arg == "--roaring"))
			{
			compressor    = bvcomp_roar;
			compressorSet = true;
			continue;
			}

		// (unadvertised) special bit vector types

		if ((arg == "--zeros")
		 || (arg == "--allzeros")
		 || (arg == "--all_zeros")
		 || (arg == "--all-zeros"))
			{
			compressor    = bvcomp_zeros;
			compressorSet = true;
			continue;
			}

		if ((arg == "--ones")
		 || (arg == "--allones")
		 || (arg == "--all_ones")
		 || (arg == "--all-ones"))
			{
			compressor    = bvcomp_ones;
			compressorSet = true;
			continue;
			}

		if (arg == "--uncrrr")
			{
			compressor    = bvcomp_unc_rrr;
			compressorSet = true;
			continue;
			}

		if (arg == "--uncroar")
			{
			compressor    = bvcomp_unc_roar;
			compressorSet = true;
			continue;
			}

		// (unadvertised) debug options

		if (arg == "--debug")
			{ debug.insert ("debug");  continue; }

		if (arg == "--debug-memory") { debug_memory = true; continue;}

		if (is_prefix_of (arg, "--debug="))
			{
		    for (const auto& field : parse_comma_list(argVal))
				debug.insert(to_lower(field));
			continue;
			}

		// unrecognized --option

		if (is_prefix_of (arg, "--"))
			chastise ("unrecognized option: \"" + arg + "\"");

		// <filename>

		seqFilenames.emplace_back(strip_blank_ends(arg));
		}

	// if an "as per" file was given, modify settings that weren't otherwise
	// specified

	if (not asPerFilename.empty())
		{
		BloomFilter bf(asPerFilename);
		bf.load();

		if (!kmerSizeSet)    { kmerSize    = bf.kmerSize;              kmerSizeSet    = true; }
		if (!numHashesSet)   { numHashes   = bf.numHashes;             numHashesSet   = true; }
		if (!hashSeed1Set)   { hashSeed1   = bf.hashSeed1;             hashSeed1Set   = true; }
		if (!hashSeed2Set)   { hashSeed2   = bf.hashSeed2;             hashSeed2Set   = true; }
		if (!hashModulusSet) { hashModulus = bf.hashModulus;           hashModulusSet = true; }
		if (!numBitsSet)     { numBits     = bf.numBits;               numBitsSet     = true; }
		// øøø don't access bvs like this
		if (!compressorSet)  { compressor  = bf.bvs[0]->compressor();  compressorSet  = true; }
		}

	// sanity checks

	if ((compressor == bvcomp_zeros)
	 || (compressor == bvcomp_ones))
		{
		if (not listFilename.empty())
			chastise ("cannot use --list with --zeros or --ones");
		if (not seqFilenames.empty())
			chastise ("cannot use sequence files with --zeros or --ones");
		if (bfFilename.empty())
			chastise ("--zeros or --ones requires --out");
		}
	else if (listFilename.empty())
		{
		if ((seqFilenames.empty()) && (inputIsSimka == false))
			chastise ("at least one sequence filename is required");
		}
	else
		{
		if (not seqFilenames.empty())
			chastise ("cannot use --list with sequence filenames (e.g. " + seqFilenames[0] + ") in the command");

		if (not bfFilename.empty())
			chastise ("cannot use --list with a filter filename (" + bfFilename + ") in the command");
		}

	if (kmerSize == 0)
		chastise ("kmer size cannot be zero");

	if (numHashes == 0)
		chastise ("number of hash functions cannot be zero");

	if (numBits < 2)
		chastise ("number of bits must be at least 2");

	if (numHashes == 1)
		hashSeed2 = 0;
	else if (not hashSeed2Set)
		hashSeed2 = hashSeed1 + 1;

	if (not hashModulusSet)
		hashModulus = numBits;
	if (hashModulus < numBits)
		chastise ("hash modulus (" + std::to_string(hashModulus)+ ")"
		        + " cannot be less than the number of bits"
		        + " (" + std::to_string(numBits) + ")");

	if ((inputIsKmers) and (minAbundanceSet))
		chastise ("cannot use --kmersin with  --min");

	if (contains(debug,"settings"))
		{
		cerr << "kmerSize    = " << kmerSize    << endl;
		cerr << "numHashes   = " << numHashes   << endl;
		cerr << "hashSeed1   = " << hashSeed1   << endl;
		cerr << "hashSeed2   = " << hashSeed2   << endl;
		cerr << "hashModulus = " << hashModulus << endl;
		cerr << "numBits     = " << numBits     << endl;
		cerr << "compressor  = " << compressor  << endl;
		}

	return;
	}


int MakeBFCommand::execute()
	{
	// if we're to make a single filter, do so
    std::error_code ec;
	if (listFilename.empty())
        {
            if (inputIsKmers) make_bloom_filter_kmers();
            else if (inputIsSimka && usingMemory)
            {
                if (fs::is_directory(input, ec))
                {
                    make_bloom_filter_simka_gz();
                    if (ec) std::cerr << "Error in is_directory: " << ec.message() << std::endl;
                }
                else if (fs::is_fifo(input, ec))
                {
                    make_bloom_filter_simka_pipe();
                    if (ec) std::cerr << "Error in is_fifo: " << ec.message() << std::endl;
                }
            } 
            else make_bloom_filter_fasta ();
        }

	// otherwise, make a series of filters according to each line specified in
	// a file

	else
		{
		std::ifstream in (listFilename);
	    if (not in)
			fatal ("error: failed to open \"" + listFilename + "\"");

		string line;
		int lineNum = 0;
		while (std::getline (in, line))
			{
			lineNum++;
			seqFilenames.clear();
			inputIsKmers = false;
			bfFilename   = "";

			vector<string> tokens = tokenize(line);
			for (size_t argIx=0 ; argIx<tokens.size() ; argIx++)
				{
				string arg = tokens[argIx];
				string argVal;

				string::size_type argValIx = arg.find('=');
				if (argValIx == string::npos) argVal = "";
				                         else argVal = arg.substr(argValIx+1);

				if ((arg == "--kmersin")
				 ||	(arg == "--askmers="))
					{
					if (minAbundanceSet)
						fatal ("cannot use --kmersin, with --min on the command line"
						       " (at line " + std::to_string(lineNum)
						     + " in " + listFilename + ")");
					inputIsKmers = true;
					continue;
					}

				if ((is_prefix_of (arg, "--out="))
				 ||	(is_prefix_of (arg, "--output=")))
					{ bfFilename = argVal;  continue; }

				if (is_prefix_of (arg, "--"))
					fatal ("unrecognized field: \"" + arg + "\""
					     + " at line " + std::to_string(lineNum)
					     + " in " + listFilename);

				seqFilenames.emplace_back(strip_blank_ends(arg));
				}

			if (inputIsKmers) make_bloom_filter_kmers ();
			else make_bloom_filter_fasta ();
			}

		in.close();
		}

	return EXIT_SUCCESS;
	}


void MakeBFCommand::add_in_bf(BloomFilter* bf, std::string kmer, std::vector<std::string> line, int idThrd)
{
	if ( line[idThrd] == "1") bf->add (kmer);
}


void MakeBFCommand::make_bloom_filter_simka_gz ()
{
    std::vector<string> expList;
    std::ifstream expSimka;
    expSimka.open (simka_exp);

    if ( !expSimka.is_open () )
    {
        fatal ("Unable to open : " + simka_exp);
    }

    std::string exp;
    std::string line;
    while ( getline (expSimka, line) )
    {
        std::vector<string> line_split = split (line, ":");
        expList.push_back (line_split[0]);
    }
    expSimka.close();

    std::vector<BloomFilter*> bfList;
    for (auto& exp : expList)
    {
        std::string bfOutFilename = simka_dir + "/" + exp + ".bf";
        bfList.push_back(new BloomFilter (bfOutFilename, kmerSize, numHashes, hashSeed1,
                hashSeed2, numBits, hashModulus));
    }

    for (BloomFilter* bf : bfList)
    {
        if ( contains(debug, "add") )       bf->dbgAdd      = true;
        if ( contains(debug, "contains") )  bf->dbgContains = true;
        bf->new_bits (compressor);
    }

    std::string path_dir = input;
    std::vector<std::string> file_list;
    std::vector<std::string> fullpath_list;

    read_dir(path_dir, file_list);
    for (auto& file : file_list)
    {
        if ( is_suffix_of(file, "gz") )
        {
            if (is_suffix_of(path_dir, "/")) fullpath_list.push_back(path_dir + file);
            else fullpath_list.push_back(path_dir + "/" + file);
        }
    }

    for ( auto& path : fullpath_list)
    {
        igzstream current_matrix(path.c_str());
        std::string line_matrix;
        unsigned long currentLine = 0;

        while (getline(current_matrix, line_matrix))
        {
            ++currentLine;
            std::vector <string> line_split = split(line_matrix, " ");
            std::string l_kmer = line_split[0];
            if (debug_memory) std::cerr << l_kmer << std::endl;

            if (l_kmer.size() != kmerSize)
            {
                fatal("error: expected " + std::to_string(kmerSize) + "-mer"
                      + " but encountered " + std::to_string(l_kmer.size())
                      + "-mer (at line " + std::to_string(currentLine)
                      + " in " + path + ")");
            }

            if (contains(debug, "kmers")) std::cerr << l_kmer << std::endl;

            for ( size_t i = 0; i < line_split[1].size(); ++i )
            {
                if (line_split[1][i] == '1')
                {
                    bfList[i]->add(l_kmer);
                }
            }
        }
    }
    for ( BloomFilter* bf : bfList )
    {
        if ( (compressor == bvcomp_unc_rrr) || (compressor == bvcomp_unc_roar) )
        {
            BitVector* bv = bf->bvs[0];
            bv->unfinished();
        }

        bf->reportSave = true;
        bf->save();
    }

    for ( size_t i=0; i<bfList.size(); i++ )
    {
        delete bfList[i];
    }
    bfList.clear();
}



void MakeBFCommand::make_bloom_filter_simka_pipe()
{
    std::vector<string> expList;
    std::ifstream expSimka;
    expSimka.open (simka_exp);

    if ( !expSimka.is_open() )
    {
        fatal ("Unable to open : " + simka_exp);
    }

    std::string exp;
    std::string line;
    while ( getline(expSimka, line) )
    {
        std::vector<string> line_split = split (line, ":");
        expList.push_back (line_split[0]);
    }
    expSimka.close();

    int nb_exp = expList.size();
    double bf_size_g = (double)numBits / (double)8000000000;
    int max_memory = maxMemoryAvailable - 2;
    int max_multiple_bf = max_memory / bf_size_g;

    if (debug_memory)
    {
        std::cerr << "Bf size in bits = " << std::to_string(numBits) << std::endl;
        std::cerr << "Nb exp <=> Nb bf = " << std::to_string(nb_exp) << std::endl;
        std::cerr << "Bf size in g = " << std::to_string(bf_size_g) << std::endl;
        std::cerr << "Max memory in g = " << std::to_string(max_memory) << std::endl;
        std::cerr << "Max multiple bf = " << std::to_string(max_multiple_bf) << std::endl;
    }

    if (max_multiple_bf > nb_exp)
    {
        std::vector<BloomFilter*> bfList;
        for (auto& exp : expList)

        {
            std::string bfOutFilename = simka_dir + "/" + exp + ".bf";
            bfList.push_back(new BloomFilter (bfOutFilename, kmerSize, numHashes, hashSeed1,
                                              hashSeed2, numBits, hashModulus));
        }

        for (BloomFilter* bf : bfList)
        {
            if ( contains(debug, "add") )       bf->dbgAdd      = true;
            if ( contains(debug, "contains") )  bf->dbgContains = true;
            bf->new_bits (compressor);
        }

        std::ifstream matrixSimka;
        matrixSimka.open (input);

        std::string line_matrix;

        if ( !matrixSimka.is_open() ) { fatal ("Unable to open : " + input); }

        const struct timespec t[] {{0,1}};
        unsigned long long currentLine = 0;
        while ( getline(matrixSimka, line_matrix) )
        {
            ++currentLine;

            std::vector<string> line_split = split (line_matrix, " ");
            std::string l_kmer = line_split[0];

            if (debug_memory) std::cerr << l_kmer << std::endl;

            if ( l_kmer.size() != kmerSize )
            {
                fatal ("error: expected " + std::to_string(kmerSize) + "-mer"
                       + " but encountered " + std::to_string(l_kmer.size())
                       + "-mer (at line " + std::to_string(currentLine)
                       + " in " + input + ")");
            }

            if ( contains(debug, "kmers") ) std::cerr << l_kmer << std::endl;

            for ( size_t i=0; i<line_split[1].size(); ++i )
            {
                if ( line_split[1][i] == '1' )
                {
                    bfList[i]->add (l_kmer);
                }
            }

            nanosleep(t, NULL);

        }

        for ( BloomFilter* bf : bfList )
        {
            if ( (compressor == bvcomp_unc_rrr) || (compressor == bvcomp_unc_roar) )
            {
                BitVector* bv = bf->bvs[0];
                bv->unfinished();
            }

            bf->reportSave = true;
            bf->save();
        }

        for ( size_t i=0; i<bfList.size(); i++ )
        {
            delete bfList[i];
        }
        bfList.clear();
    }

    else
    {
        fatal ("Out of memory, open " + std::to_string(nb_exp) + " required " + std::to_string(bf_size_g*nb_exp) + " Gb available memory");
    }

}


void MakeBFCommand::make_bloom_filter_fasta()  // this also supports fastq
	{
	string bfOutFilename = build_output_filename();

	// create the hash table, with jellyfish defaults

	const u64 hashSize    = 10*1000*1000;
	const u32 numReprobes = 126;
	const u32 counterLen  = 7;

	unsigned int savedKmerSize = jellyfish::mer_dna::k();
	jellyfish::mer_dna::k(kmerSize);

	mer_hash_type merHash (hashSize, kmerSize*2, counterLen, numThreads, numReprobes);

	// count the kmers
	// nota bene: MerCounter internally discards kmers containing any non-ACGT
	// $$$ ERROR_CHECK need to trap exceptions from the jellyfish stuff
	// $$$ ERROR_CHECK does jellyfish give us any indication if one of the sequence files doesn't exist?

	MerCounter counter(numThreads, merHash, seqFilenames.begin(), seqFilenames.end());
	counter.exec_join (numThreads);

	// build the bloom filter

	BloomFilter* bf = new BloomFilter(bfOutFilename, kmerSize,
	                                  numHashes, hashSeed1, hashSeed2,
	                                  numBits, hashModulus);
	if (contains(debug,"add"))      bf->dbgAdd      = true;
	if (contains(debug,"contains")) bf->dbgContains = true;

	bf->new_bits (compressor);

	const auto jfAry = merHash.ary();
	const auto end   = jfAry->end();
	for (auto kmer=jfAry->begin() ; kmer!=end ; ++kmer)
		{
		auto& keyValuePair = *kmer;
		if (keyValuePair.second >= minAbundance)
			{
			if (contains(debug,"kmers"))
				cerr << keyValuePair.first << " " << keyValuePair.second << endl;

			if (contains(debug,"strings"))
				bf->add (keyValuePair.first.to_str());
			else
				bf->add ((u64*) keyValuePair.first.data());
			}
		}

	jellyfish::mer_dna::k(savedKmerSize);	// restore jellyfish kmer size

	if ((compressor == bvcomp_unc_rrr)
	 || (compressor == bvcomp_unc_roar))
		{
		BitVector* bv = bf->bvs[0];
		bv->unfinished();
		}

	bf->reportSave = true;
	bf->save();
	delete bf;
	}


void MakeBFCommand::make_bloom_filter_kmers()
	{
	string bfOutFilename = build_output_filename();

	// build the bloom filter

	BloomFilter* bf = new BloomFilter(bfOutFilename, kmerSize,
	                                  numHashes, hashSeed1, hashSeed2,
	                                  numBits, hashModulus);
	if (contains(debug,"add"))      bf->dbgAdd      = true;
	if (contains(debug,"contains")) bf->dbgContains = true;

	bf->new_bits (compressor);

	for (const auto& kmersFilename : seqFilenames)
		{
		std::ifstream in (kmersFilename);
		if (not in)
			fatal ("error: failed to open \"" + kmersFilename + "\"");

		string line, kmer;
		int lineNum = 0;
		while (std::getline (in, line))
			{
			lineNum++;
			line = strip_blank_ends (line);
			if (line.empty()) continue;

			string::size_type whitespaceIx = line.find_first_of(" \t");
			if (whitespaceIx == string::npos) kmer = line;
			                             else kmer = line.substr(0,whitespaceIx);

			if (kmer.length() != kmerSize)
				fatal ("error: expected " + std::to_string(kmerSize) + "-mer"
				     + " but encountered " + std::to_string(kmer.length()) + "-mer"
				     + " (at line " + std::to_string(lineNum)
				     + " in " + kmersFilename + ")");

			string::size_type badIx = kmer.find_first_not_of("ACGTacgt");
			if (badIx != string::npos) continue;

			if (contains(debug,"kmers"))
				cerr << kmer << endl;

			bf->add (kmer);
			}
		}

	if ((compressor == bvcomp_unc_rrr)
	 || (compressor == bvcomp_unc_roar))
		{
		BitVector* bv = bf->bvs[0];
		bv->unfinished();
		}

	bf->reportSave = true;
	bf->save();
	delete bf;
	}


string MakeBFCommand::build_output_filename()
	{
	string bfOutFilename = bfFilename;

	if (bfOutFilename.empty())
		{
		string ext = "." + BitVector::compressor_to_string(compressor) + ".bf";
		if (ext == ".uncompressed.bf") ext = ".bf";

		string seqFilename = seqFilenames[0];
		string::size_type dotIx = seqFilename.find_last_of(".");
		if (dotIx == string::npos)
			bfOutFilename = seqFilename + ext;
		else
			bfOutFilename = seqFilename.substr(0,dotIx) + ext;
		}

	return bfOutFilename;
	}
