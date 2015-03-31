#include "metadata_server.h"

using namespace std;

int
main (void)
{
  metadata_server mds("127.0.0.1", 9999, 10);

  mds.init();

  mds.join();

  return 0;
}
