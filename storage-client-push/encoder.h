//
//  encoder.h
//  trevi
//
//  Created by Georgios Parisis on 15/07/2014.
//  Copyright (c) 2014 George Parisis. All rights reserved.
//

#ifndef __trevi__encoder__
#define __trevi__encoder__

#include <map>

#include <boost/random/uniform_int_distribution.hpp>
#include <boost/random/random_device.hpp>
#include <boost/random/mersenne_twister.hpp>

#include "encoding_state.h"
#include "utils.h"

class encoder {
public:
	encoder();
	encoder(const encoder& orig);
	virtual
	~encoder();
	/*
	 * Initialise state to use throughout the encoding process of some blob
	 */
	encoding_state *
	init_state(unsigned char *blob_id, unsigned int blob_size,
			unsigned char *blob);
	/*
	 * Calculate next symbol for this blob.
	 * encodingState must have been previously initialised
	 * This is a bit tricky here:
	 * reseeding the random generator (for uniformly selecting degree number of symbols)
	 * is VERY expensive for some generators (e.g. Mersenne Twister)
	 * Attention needs to be paid in case the encoder runs in multiple threads (not sure now if that's required!)
	 */
	symbol *
	encode_next(encoding_state *enc_state);

private:
	boost::random::random_device rd;
	boost::random::mt19937 generator;
	boost::random::mt19937 seeder;
	// The encoder keeps a map of all soliton distributions for each different number of fragments.
	// I don't know how to quickly get values from a robust soliton distribution so I model them as discrete distributions after I calculate the pdf.
	// This is obviously quite expensive so I store all these distributions
	std::map<unsigned int, robust_soliton_distribution *> soliton_distributions;
	//map of uniform distributions
	std::map<unsigned int,
			boost::random::uniform_int_distribution<unsigned int> *> uniform_distributions;
};

inline encoder::encoder() :
		generator(rd()), seeder(rd()) {
}

inline encoder::encoder(const encoder& orig) {
	std::cout << "encoder: copy constructor NOT implemented yet" << std::endl;
}

inline encoder::~encoder() {
	//delete all stored soliton and uniform distributions
	std::map<unsigned int, robust_soliton_distribution *>::iterator soliton_distr_iter;
	std::map<unsigned int,
			boost::random::uniform_int_distribution<unsigned int> *>::iterator uniform_distr_iter;
	// delete all stored soliton distributions
	for (soliton_distr_iter = soliton_distributions.begin();
			soliton_distr_iter != soliton_distributions.end();
			soliton_distr_iter++) {
		delete (*soliton_distr_iter).second;
	}
	// delete all stored uniform distributions
	for (uniform_distr_iter = uniform_distributions.begin();
			uniform_distr_iter != uniform_distributions.end();
			uniform_distr_iter++) {
		delete (*uniform_distr_iter).second;
	}
}

inline encoding_state *
encoder::init_state(unsigned char *blob_id, unsigned int blob_size,
		unsigned char *blob) {
	std::map<unsigned int, robust_soliton_distribution *>::iterator soliton_distr_iter;
	std::map<unsigned int,
			boost::random::uniform_int_distribution<unsigned int> *>::iterator uniform_distr_iter;
	robust_soliton_distribution *soliton_distribution;
	boost::random::uniform_int_distribution<unsigned int> *uniform_distribution;
	unsigned int number_of_fragments;
	unsigned short size_of_last_fragment;

	number_of_fragments = calculate_number_of_fragments(blob_size, SYMBOL_SIZE);
	size_of_last_fragment = calculate_size_of_last_fragment(blob_size,
			SYMBOL_SIZE);
	// check if a robust soliton distribution for this number of fragments has been previously calculated
	if ((soliton_distr_iter = soliton_distributions.find(number_of_fragments))
			== soliton_distributions.end()) {
		// create a new robust soliton distribution for number_of_fragments
		soliton_distribution = new robust_soliton_distribution(&this->generator,
				number_of_fragments);
		// add it to the soliton distributions map;
		soliton_distributions.insert(
				std::pair<unsigned int, robust_soliton_distribution *>(
						number_of_fragments, soliton_distribution));
	} else {
		//a robust soliton distribution for this number of fragments has been previously created
		//just set the pointer
		soliton_distribution = (*soliton_distr_iter).second;
	}
	if ((uniform_distr_iter = uniform_distributions.find(number_of_fragments))
			== uniform_distributions.end()) {
		// create a new uniform_int_distribution for number_of_fragments
		uniform_distribution = new boost::random::uniform_int_distribution<
				unsigned int>(1, number_of_fragments);
		// add it to the soliton uniform_distributions map;
		uniform_distributions.insert(
				std::pair<unsigned int,
						boost::random::uniform_int_distribution<unsigned int> *>(
						number_of_fragments, uniform_distribution));
	} else {
		//a uniform_distribution for this number of fragments has been previously created
		//just set the pointer
		uniform_distribution = (*uniform_distr_iter).second;
	}
	return new encoding_state(blob_id, blob_size, blob, number_of_fragments,
			size_of_last_fragment, soliton_distribution, uniform_distribution);
}

inline symbol *
encoder::encode_next(encoding_state *enc_state) {
	std::set<unsigned int> duplicate_neighbours_guard_set;
	unsigned int neighbour;
	symbol *new_symbol = new symbol();
	// get a seed (will be, then, used to seed everything else)
	// I get a seed from the generator to seed the same generator
	// this may be problematic but I think there will be enough randomness in the full system so that this is safe
	new_symbol->seed = seeder(); // CAREFUL: this may have to be more random (e.g. RDRAND)
	// reseed the neigbours distribution1
	generator.seed(new_symbol->seed);
	// get the degree for the next symbol
	new_symbol->degree = enc_state->degree_calculator->get_next_degree();
	// get neighbours for the next symbol
	for (unsigned int i = 0; i < new_symbol->degree; i++) {
		neighbour = (*enc_state->neighbour_calculator)(generator); // I can have duplicate neighbours
		if (!enc_state->duplicate_neighbour_guard.test(neighbour - 1)) {
			enc_state->duplicate_neighbour_guard.set(neighbour - 1);
			if (i == 0) {
				////////////////////////////////MEMCPY - FIRST NEIGHBOUR///////////////////////////////////
				if (neighbour == enc_state->number_of_fragments) {
					//be careful - this is the last fragment
					memcpy(new_symbol->symbol_data,
							enc_state->blob + ((neighbour - 1) * SYMBOL_SIZE),
							enc_state->size_of_last_fragment);
				} else {
					memcpy(new_symbol->symbol_data,
							enc_state->blob + ((neighbour - 1) * SYMBOL_SIZE),
							SYMBOL_SIZE);
				}
				///////////////////////////////////////////////////////////////////////////////////////////
			} else {
				///////////////////////////////////////////XOR/////////////////////////////////////////////
				if (neighbour == enc_state->number_of_fragments) {
					trevi_chunk_xor_128(
							enc_state->blob + (neighbour - 1) * SYMBOL_SIZE,
							new_symbol->symbol_data,
							enc_state->size_of_last_fragment);
				} else {
					trevi_chunk_xor_128(
							enc_state->blob + (neighbour - 1) * SYMBOL_SIZE,
							new_symbol->symbol_data, SYMBOL_SIZE);
				}
				///////////////////////////////////////////////////////////////////////////////////////////
			}
		} else {
			// duplicate neighbour - recalculate
			i--;
		}
	}
	enc_state->duplicate_neighbour_guard.reset();
	return new_symbol;
}

#endif /* defined(__trevi__encoder__) */
