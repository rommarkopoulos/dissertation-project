//
//  decoder.h
//  trevi
//
//  Created by Georgios Parisis on 15/07/2014.
//  Copyright (c) 2014 George Parisis. All rights reserved.
//

#ifndef __trevi__decoder__
#define __trevi__decoder__

#include <map>

#include <boost/random/uniform_int_distribution.hpp>
#include <boost/random/random_device.hpp>
#include <boost/random/mersenne_twister.hpp>

#include <emmintrin.h>

#include "decoding_state.h"
#include "utils.h"

using namespace std;

class decoder
{
public:
  decoder ();
  decoder (const decoder& orig);
  virtual
  ~decoder ();

  decoding_state *
  init_state (unsigned char *blob_id, unsigned int blob_size, unsigned char *blob); // Initialise state to use throughout the decoding process of some blob
  bool
  decode_next (decoding_state *dec_state, symbol *a_symbol);
  void
  handle_decoded_symbol (decoding_state *dec_state); // a recursive method that will handle chain reactions of decoding symbols
  void
  print_debugging_info (decoding_state *dec_state);
private:
  boost::random::random_device rd;
  boost::random::mt19937 generator;
  // The decoder keeps a map of all soliton distributions for each different number of fragments.
  // I don't know how to quickly get values from a robust soliton distribution so I model them as discrete distributions after I calculate the pdf.
  // This is obviously quite expensive so I cache all these distributions.
  map<unsigned int, robust_soliton_distribution *> soliton_distributions;
  //map of uniform distributions
  map<unsigned int, boost::random::uniform_int_distribution<unsigned int> *> uniform_distributions;
};

inline
decoder::decoder () : generator (rd())
{
}

inline
decoder::decoder (const decoder& orig) :
    generator (rand ())
{
  cout << "decoder: copy constructor NOT implemented yet" << endl;
}

inline
decoder::~decoder ()
{
  //delete all stored soliton and uniform distributions
  map<unsigned int, robust_soliton_distribution *>::iterator soliton_distr_iter;
  map<unsigned int, boost::random::uniform_int_distribution<unsigned int> *>::iterator uniform_distr_iter;
  for (soliton_distr_iter = soliton_distributions.begin (); soliton_distr_iter != soliton_distributions.end (); soliton_distr_iter++) {
    delete (*soliton_distr_iter).second;
  }
  for (uniform_distr_iter = uniform_distributions.begin (); uniform_distr_iter != uniform_distributions.end (); uniform_distr_iter++) {
    delete (*uniform_distr_iter).second;
  }
}

inline decoding_state *
decoder::init_state (unsigned char *blob_id, unsigned int blob_size, unsigned char *blob)
{
  map<unsigned int, robust_soliton_distribution *>::iterator soliton_distr_iter;
  map<unsigned int, boost::random::uniform_int_distribution<unsigned int> *>::iterator uniform_distr_iter;
  robust_soliton_distribution *soliton_distribution;
  boost::random::uniform_int_distribution<unsigned int> *uniform_distribution;
  unsigned int number_of_fragments;
  unsigned short size_of_last_fragment;

  number_of_fragments = calculate_number_of_fragments (blob_size, SYMBOL_SIZE);
  size_of_last_fragment = calculate_size_of_last_fragment (blob_size, SYMBOL_SIZE);
  // check if a robust soliton distribution for this number of fragments has been previously calculated
  if ((soliton_distr_iter = soliton_distributions.find (number_of_fragments)) == soliton_distributions.end ()) {
    // create a new robust soliton distribution for number_of_fragments
    soliton_distribution = new robust_soliton_distribution (&this->generator, number_of_fragments);
    // add it to the soliton distributions map;
    soliton_distributions.insert (pair<unsigned int, robust_soliton_distribution *> (number_of_fragments, soliton_distribution));
  } else {
    //a robust soliton distribution for this number of fragments has been previously created
    //just set the pointer
    soliton_distribution = (*soliton_distr_iter).second;
  }
  if ((uniform_distr_iter = uniform_distributions.find (number_of_fragments)) == uniform_distributions.end ()) {
    // create a new uniform_int_distribution for number_of_fragments
    uniform_distribution = new boost::random::uniform_int_distribution<unsigned int> (1, number_of_fragments);
    // add it to the soliton uniform_distributions map;
    uniform_distributions.insert (pair<unsigned int, boost::random::uniform_int_distribution<unsigned int> *> (number_of_fragments, uniform_distribution));
  } else {
    //a uniform_distribution for this number of fragments has been previously created
    //just set the pointer
    uniform_distribution = (*uniform_distr_iter).second;
  }
  return new decoding_state (blob_id, blob_size, blob, number_of_fragments, size_of_last_fragment, soliton_distribution, uniform_distribution);
}

inline bool
decoder::decode_next (decoding_state *dec_state, symbol *a_symbol)
{
  unsigned int neighbour = 0;
  unsigned int degree_counter = 0;
  set<unsigned int>::iterator set_it;
  // reseed the distribution
  generator.seed (a_symbol->seed);
  // get the degree for the  symbol
  a_symbol->degree = dec_state->degree_calculator->get_next_degree ();
  //cout << "decoding symbol with degree " << a_symbol->degree << endl;
  dec_state->average_degree += a_symbol->degree; // for debugging
  dec_state->symbol_counter++;                   // for debugging
  degree_counter = a_symbol->degree;             // for debugging
  // get neighbours for the symbol
  for (unsigned int i = 0; i < degree_counter; i++) {
    neighbour = (*dec_state->neighbour_calculator) (generator);
    if (!dec_state->duplicate_neighbour_guard.test (neighbour - 1)) {
      dec_state->duplicate_neighbour_guard.set (neighbour - 1);
      if (dec_state->fragments_bitset.test (neighbour - 1) == true) {
	// the fragment at position (neighbour - 1) (i.e. the one that refers to neighbour) is decoded
	// XOR the decoded fragment to a_symbol
	if (neighbour == dec_state->number_of_fragments) {
	  trevi_chunk_xor_128 (dec_state->blob + (neighbour - 1) * SYMBOL_SIZE, a_symbol->symbol_data, dec_state->size_of_last_fragment);
	} else {
	  trevi_chunk_xor_128 (dec_state->blob + (neighbour - 1) * SYMBOL_SIZE, a_symbol->symbol_data, SYMBOL_SIZE);
	}
	a_symbol->degree--; // decrease the degree of the symbol
      } else {
	a_symbol->neighbours.insert (neighbour);
	dec_state->neighbour_index.at (neighbour - 1).push_back (a_symbol); // add the symbol to the index kept as part of this decoding_state
      }
    } else {
      // duplicate neighbour - recalculate
      i--;
    }
  }
  dec_state->duplicate_neighbour_guard.reset ();
  //print_debugging_info(dec_state);
  if (a_symbol->degree > 1) {
    // not decoded yet - return false and move on
    //cout << "a_symbol->degree > 1: moving on" << endl;
    return false;
  } else if (a_symbol->degree == 1) {
    // this symbol has been decoded - use it to try to decode other symbols that were previously received
    //cout << "a_symbol->degree == 1: handle_decoded_symbol" << endl;
    dec_state->ripple.push_back (a_symbol);
    handle_decoded_symbol (dec_state);
    if (dec_state->number_of_decoded_symbols >= dec_state->number_of_fragments) {
      // I may end up decoding more than I want so this must be checked here - that's why the >=
      return true;
    }
  } else {
    //cout << "a_symbol->degree == 0: deleting and moving on" << endl;
    // a_symbol->degree is 0
    // this symbol is useless - all its neighbours have already been decoded
    // Given that we ended up here, the blob has not been decoded yet - delete symbol and return false
    delete a_symbol;
    return false;
  }
  return false;
}

inline void
decoder::handle_decoded_symbol (decoding_state *dec_state)
{
  deque<symbol *>::iterator index_it;
  symbol *a_symbol, *b_symbol;
  unsigned int last_neighbour;

  while (dec_state->ripple.size () > 0) {
    a_symbol = dec_state->ripple.front ();
    last_neighbour = *(a_symbol->neighbours.begin ());
    index_it = dec_state->neighbour_index.at (last_neighbour - 1).begin ();
    // copy a_symbol to decoded_blob
    if (last_neighbour == dec_state->number_of_fragments) {
      memcpy (dec_state->blob + ((last_neighbour - 1) * SYMBOL_SIZE), a_symbol->symbol_data, dec_state->size_of_last_fragment);
    } else {
      memcpy (dec_state->blob + ((last_neighbour - 1) * SYMBOL_SIZE), a_symbol->symbol_data, SYMBOL_SIZE);
    }
    dec_state->ripple.pop_front (); // pop a_symbol
    // check if a_symbol has been previously decoded
    if (!dec_state->fragments_bitset.test (last_neighbour - 1)) {
      dec_state->fragments_bitset.set (last_neighbour - 1);
      dec_state->number_of_decoded_symbols++;
    }
//        cout << "ripple queue size: " << dec_state->ripple.size() << endl;
//        cout << "a_symbol degree " << a_symbol->degree << " - number of neighbours: " << a_symbol->neighbours.size() << endl;
//        cout << "last neighbour: " << last_neighbour << endl;
//        print_debugging_info(dec_state);

    // Now use a_symbol to decode other undecoded symbols - more symbols could be added in the ripple
    while (index_it != dec_state->neighbour_index.at (last_neighbour - 1).end ()) {
      b_symbol = *index_it;
      index_it = dec_state->neighbour_index.at (last_neighbour - 1).erase (index_it); // erase from the index
      if (b_symbol->degree == 1) {
	// that's b_symbol itself
	// do nothing - the symbols is erased from the index - see above
      } else {
	b_symbol->neighbours.erase (last_neighbour);
	b_symbol->degree--;
	trevi_chunk_xor_128 (a_symbol->symbol_data, b_symbol->symbol_data, SYMBOL_SIZE); // XOR a_symbol to b_symbol
	if (b_symbol->degree == 1) {
	  dec_state->ripple.push_back (b_symbol); // add b_symbol to the ripple deque
	}
      }
      //print_debugging_info(dec_state);
    }
    delete a_symbol;
  }
}

inline void
decoder::print_debugging_info (decoding_state *dec_state)
{
  deque<symbol *>::iterator index_it;
  cout << dec_state->fragments_bitset << endl;
  for (unsigned int i = 0; i < dec_state->number_of_fragments; i++) {
    index_it = dec_state->neighbour_index.at (i).begin ();
    cout << i + 1 << ": ";
    while (index_it != dec_state->neighbour_index.at (i).end ()) {
      cout << "[" << (*index_it)->degree << ": ";
      set<unsigned int>::iterator set_it = (*index_it)->neighbours.begin ();
      while (set_it != (*index_it)->neighbours.end ()) {
	cout << *set_it << ",";
	set_it++;
      }
      cout << "], ";
      index_it++;
    }
    cout << endl;
  }
}

#endif /* defined(__trevi__decoder__) */
