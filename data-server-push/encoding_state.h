//
//  encoding_state.h
//  trevi
//
//  Created by Georgios Parisis on 15/07/2014.
//  Copyright (c) 2014 George Parisis. All rights reserved.
//

#ifndef __trevi__encoding_state__
#define __trevi__encoding_state__

#include <boost/dynamic_bitset.hpp>
#include <boost/random/uniform_int_distribution.hpp>

#include "symbol.h"
#include "robust_soliton_distribution.h"

using namespace std;

// An encoding_state object stores some state about a specific blob.
// i.e. number_of_fragments, size_of_last_fragment and most importantly a padded version of the last fragment.
// To avoid lookups in a hash map for each symbol it stores a pointer to the right degree_calculator (a robust soliton distribution).
// Note: I don't know how to produce values from a robust soliton distribution directly.
// For that reason I create discrete distributions after calculating all probabilities for each symbol.
// I keep such a discrete distribution for each possible number of fragments.
// This could potentially create concurrency issues but I will probably just have separate Encoders for each core.

class encoding_state
{
public:
  encoding_state (unsigned char *blob_id, unsigned int blob_size, unsigned char *blob, unsigned int number_of_fragments, unsigned short size_of_last_fragment,
		  robust_soliton_distribution *degree_calculator, boost::random::uniform_int_distribution<unsigned int> *neighbour_calculator);
  encoding_state (const encoding_state& orig);
  virtual
  ~encoding_state ();

  /*members*/
  unsigned char *blob_id; // an 128 bit identifier
  unsigned char *blob; // the blob over which we encode
  unsigned int blob_size; // The size of the blob to be encoded
  unsigned int number_of_fragments; // the number of fragments in this blob
  unsigned short size_of_last_fragment; // the size of the last fragment (note that this can be less than the symbolSize - may have to be padded)
  robust_soliton_distribution *degree_calculator; // a pointer to a soliton distribution to calculate degrees
  boost::random::uniform_int_distribution<unsigned int> *neighbour_calculator;
  boost::dynamic_bitset<> duplicate_neighbour_guard;
};

inline
encoding_state::encoding_state (unsigned char *blob_id, unsigned int blob_size, unsigned char *blob, unsigned int number_of_fragments, unsigned short size_of_last_fragment,
				robust_soliton_distribution *degree_calculator, boost::random::uniform_int_distribution<unsigned int> *neighbour_calculator) :
    duplicate_neighbour_guard (number_of_fragments)
{
  this->blob_id = blob_id;
  this->blob_size = blob_size;
  this->blob = blob;
  this->number_of_fragments = number_of_fragments;
  this->size_of_last_fragment = size_of_last_fragment;
  this->degree_calculator = degree_calculator;
  this->neighbour_calculator = neighbour_calculator;
//	cout << "Encoding State initialised" << endl;
//	cout << "Blob Size: " << this->blob_size << endl;
//	cout << "Number of Framents: " << this->number_of_fragments << endl;
//	cout << "Size of last Frament: " << this->size_of_last_fragment << endl << endl;
}

inline
encoding_state::encoding_state (const encoding_state& orig)
{
  cout << "encoding_state: copy constructor NOT implemented yet" << endl;
}

inline
encoding_state::~encoding_state ()
{
}

#endif /* defined(__trevi__encoding_state__) */
