//
//  symbol.h
//  trevi
//
//  Created by Georgios Parisis on 15/07/2014.
//  Copyright (c) 2014 George Parisis. All rights reserved.
//

#ifndef __trevi__symbol__
#define __trevi__symbol__

#include <iostream>
#include <stdlib.h>
#include <string.h>
#include <set>

#define SYMBOL_SIZE 1024 // MUST BE dividable to 16 (alignment for sse!)

using namespace std;

/*
 * This class represents a self-contained symbol that can be sent as a payload
 * A symbol can be marshalled to a an unsigned char * and unmarshalled from an unsigned char *
 */
class symbol
{
public:
  symbol ();
  symbol (const symbol& orig);
  virtual
  ~symbol ();

  /*members*/
  unsigned int degree; // the degree of the symbol - no need to send it..The seed alone is enough!
  unsigned int seed;   // a seed to be used on "the other side" to calculate the neighbour set (will reseed a fast random generator)
  unsigned char *symbol_data; // the data of the symbol
  set<unsigned int> neighbours; // a set containing all neighbours for this symbol - no duplicate neighbours exist (used only by the decoder!)
};

inline
symbol::symbol ()
{
  this->degree = 0;
  this->seed = 0;
  ///////POSIX CALL////
  posix_memalign ((void **) &symbol_data, 16, SYMBOL_SIZE);
  memset (symbol_data, 0, SYMBOL_SIZE);
}

inline
symbol::symbol (const symbol& orig)
{
  cout << "symbol: copy constructor NOT implemented yet" << endl;
}

inline
symbol::~symbol ()
{
  ///////POSIX CALL////
  free (symbol_data);
}

#endif /* defined(__trevi__symbol__) */
