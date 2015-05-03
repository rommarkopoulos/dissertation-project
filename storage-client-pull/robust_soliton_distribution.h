//
//  robust_soliton_distribution.h
//  trevi
//
//  Created by Georgios Parisis on 15/07/2014.
//  Copyright (c) 2014 George Parisis. All rights reserved.
//

#ifndef __trevi__robust_soliton_distribution__
#define __trevi__robust_soliton_distribution__

#include <iostream>

#include <boost/random/discrete_distribution.hpp>
#include <boost/random/mersenne_twister.hpp>

#define c 0.02    // a robust soliton parameter
#define delta 0.6 // a robust soliton parameter

class robust_soliton_distribution
{
public:
  robust_soliton_distribution (boost::random::mt19937 *generator, unsigned int max);
  robust_soliton_distribution (const robust_soliton_distribution& orig);
  virtual
  ~robust_soliton_distribution ();
  int
  get_next_degree ();
  boost::random::discrete_distribution<unsigned int> *
  init_discrete_distribution (unsigned int max);
  double
  p (unsigned int k, unsigned int max);
  double
  t (unsigned int k, unsigned int max);
  /*members*/
  boost::random::discrete_distribution<unsigned int> *the_distribution; // a discrete distribution emulating the robust soliton distribution
  boost::random::mt19937 *generator;
  double R;                                                             // a robust soliton parameter
};

//inline robust_soliton_distribution::robust_soliton_distribution (simple_well512 *generator, unsigned int max)
inline
robust_soliton_distribution::robust_soliton_distribution (boost::random::mt19937 *generator, unsigned int max)
{
  R = c * log (max / delta) * sqrt ((double) max);
  this->generator = generator;
  the_distribution = init_discrete_distribution (max);
}

inline
robust_soliton_distribution::robust_soliton_distribution (const robust_soliton_distribution& orig)
{
  std::cout << "robust_soliton_distribution: copy constructor NOT implemented yet" << std::endl;
}

inline
robust_soliton_distribution::~robust_soliton_distribution ()
{
  delete the_distribution;
}

inline boost::random::discrete_distribution<unsigned int> *
robust_soliton_distribution::init_discrete_distribution (unsigned int max)
{
  std::vector<double> pdf;
  pdf.push_back (0.0);
  for (unsigned int i = 1; i <= max; i++) {
    pdf.push_back (p (i, max) + t (i, max));
  }

  return new boost::random::discrete_distribution<unsigned int> (pdf.begin (), pdf.end ());
}

inline double
robust_soliton_distribution::p (unsigned int i, unsigned int max)
{
  double output;
  if (i == 1) {
    output = (double) 1 / max;
  } else {
    output = (double) 1 / (i * (i - 1));
  }
  return output;
}

inline double
robust_soliton_distribution::t (unsigned int i, unsigned int max)
{
  double output;
  if (i <= ((unsigned int) (max / R)) - 1) {
    output = (double) (R / (double) (i * max));
  } else if (i == (int) (max / R)) {
    output = (R * log (R / delta)) / (double) max;
  } else {
    output = 0.0;
  }
  return output;
}

inline int
robust_soliton_distribution::get_next_degree ()
{
  return (*the_distribution) (*generator);
}

#endif /* defined(__trevi__robust_soliton_distribution__) */
