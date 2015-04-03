#include <boost/lexical_cast.hpp>

#include "metadata_server.h"

using namespace std;

int
main (int argc, char *argv[])
{
  if (argc != 4) {
    cout << "args: bind_address, bind_port, pool_size" << endl;
    return EXIT_FAILURE;
  }

  metadata_server mds (argv[1], lexical_cast<uint16_t> (argv[2]), lexical_cast<size_t>(argv[3]));

  mds.init ();

  mds.join ();

  return 0;
}
