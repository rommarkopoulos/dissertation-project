#include "data_server.h"

int
main (void)
{
  data_server ds ("127.0.0.1", 8888, "127.0.0.1", 9999, 10);
  ds.init ();

  ds.join ();

  return 0;
}
