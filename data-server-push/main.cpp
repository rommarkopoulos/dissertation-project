#include <boost/lexical_cast.hpp>

#include "data_server.h"

using namespace std;
using namespace boost;

int
main (int argc, char *argv[])
{
  if(argc != 6) {
    cout << "args: bind_address, bind_port, mds_address, mds_port, pool_size" << endl;
    return EXIT_FAILURE;
  }

  data_server ds (argv[1], lexical_cast<uint16_t>(argv[2]), argv[3],  lexical_cast<uint16_t>(argv[4]),  lexical_cast<size_t>(argv[5]));

  ds.init ();
  ds.join ();

  return 0;
}
