//
//  decoding_state.h
//  trevi
//
//  Created by Georgios Parisis on 04/08/2014.
//  Copyright (c) 2014 George Parisis. All rights reserved.
//

#ifndef __trevi__decoding_state__
#define __trevi__decoding_state__

#include <deque>
#include <vector>

#include <boost/dynamic_bitset.hpp>
#include <boost/random/uniform_int_distribution.hpp>

#include "symbol.h"
#include "robust_soliton_distribution.h"

using namespace std;

class decoding_state
{
public:
  decoding_state (unsigned char *blob_id, unsigned int blob_size, unsigned char *blob, unsigned int number_of_fragments, unsigned short size_of_last_fragment,
		  robust_soliton_distribution *degree_calculator, boost::random::uniform_int_distribution<unsigned int> *neighbour_calculator);
  decoding_state (const decoding_state& orig);
  virtual
  ~decoding_state ();

  /*members*/
  unsigned char *blob_id; // an 128 bit identifier
  unsigned char *blob; // the blob to be decoded
  unsigned int blob_size; // The size of the blob to be encoded
  unsigned int number_of_fragments; // the number of fragments in this blob
  unsigned short size_of_last_fragment; // the size of the last fragment (note that this can be less than the symbol size - may have to be padded)

  robust_soliton_distribution *degree_calculator; // a pointer to a soliton distribution to calculate degrees
  boost::random::uniform_int_distribution<unsigned int> *neighbour_calculator;

  unsigned int number_of_decoded_symbols; // the number of symbols I have decoded so far
  vector<deque<symbol *> > neighbour_index; // index of currently stored symbols (position 0 refers to neighbour 1 and so on)
  boost::dynamic_bitset<> fragments_bitset; // a bitmap of decoded symbols (position 0 refers to neighbour 1 and so on)
  deque<symbol *> ripple; // for handling multiple decodings
  boost::dynamic_bitset<> duplicate_neighbour_guard;

  unsigned int symbol_counter; // for debugging
  unsigned int average_degree; // for debugging
};

inline
decoding_state::decoding_state (unsigned char *blob_id, unsigned int blob_size, unsigned char *blob, unsigned int number_of_fragments, unsigned short size_of_last_fragment,
				robust_soliton_distribution *degree_calculator, boost::random::uniform_int_distribution<unsigned int> *neighbour_calculator) :
    neighbour_index (number_of_fragments), fragments_bitset (number_of_fragments), duplicate_neighbour_guard (number_of_fragments)
{
  this->blob_id = blob_id;
  this->blob_size = blob_size;
  this->blob = blob;
  this->number_of_fragments = number_of_fragments;
  this->size_of_last_fragment = size_of_last_fragment;
  this->degree_calculator = degree_calculator;
  this->neighbour_calculator = neighbour_calculator;
  this->number_of_decoded_symbols = 0;
//      cout << "Decoding State initialised" << endl;
//	cout << "Blob Size: " << this->blob_size << endl;
//	cout << "Number of Framents: " << this->number_of_fragments << endl;
//	cout << "Size of last Frament: " << this->size_of_last_fragment << endl << endl;
  this->symbol_counter = 0; // for debugging
  this->average_degree = 0; // for debugging
}

inline
decoding_state::decoding_state (const decoding_state& orig)
{
  cout << "decoding_state: copy constructor NOT implemented yet" << endl;
}

inline
decoding_state::~decoding_state ()
{
  vector<deque<symbol *> >::iterator neighbour_index_it;
  deque<symbol *>::iterator deque_it;
  // if the blob is not decoded I need to free some memory from the neighbour_index
  if (number_of_decoded_symbols < number_of_fragments) {
    for (neighbour_index_it = neighbour_index.begin (); neighbour_index_it != neighbour_index.end (); neighbour_index_it++) {
      for (deque_it = (*neighbour_index_it).begin (); deque_it != (*neighbour_index_it).end (); deque_it++) {
	if ((*deque_it)->degree == 1) {
	  //cout << "deleteing - degree: " << (*deque_it)->degree << endl;
	  //cout << "deleting neighbours size: " << (*deque_it)->neighbours.size() << endl;
	  delete (*deque_it);
	} else {
	  (*deque_it)->degree--;
	}
      }
      (*neighbour_index_it).clear ();
    }
  }
}

#endif /* defined(__trevi__decoding_state__) */
