#include <boost/lexical_cast.hpp>

#include <iostream>

#include "client.h"

using namespace std;

void
data_stored (const system::error_code& err, uint32_t &hash_code, string name, char *data)
{
  if (!err) {
    cout << "main: data with name " << name << " were successfully stored" << endl;
  } else {
    cout << "main: error in storing data with name " << name << ", " << err.message () << endl;
  }
  free (data);
}

void
data_fetched (const system::error_code& err, uint32_t &hash_code, char *data, uint32_t &length, string name)
{
  if (!err) {
    cout << "main: data with name " << name << " were successfully fetched" << endl;
    free (data);
  } else {
    cout << "main: error in fetching data with name " << name << ", " << err.message () << endl;
  }
}

int
main (int argc, char *argv[])
{
  if (argc != 4) {
    cout << "main: args: mds_address, mds_port, pool_size" << endl;
    return EXIT_FAILURE;
  }

  client client (argv[1], lexical_cast<uint16_t> (argv[2]), lexical_cast<size_t> (argv[3]));

  client.init ();

  string name = "test_data";
  size_t length = 1000;
  char *data = (char *) malloc (length);
  uint8_t replicas = 1;

  client.store_data (name, 1, data, length, bind (&data_stored, _1, _2, name, data));

  sleep (5);

  client.fetch_data (name, bind (&data_fetched, _1, _2, _3, _4, name));

  /*sleep (5);

  string non_existent_name ("non-existent name");
  client.fetch_data (non_existent_name.c_str (), bind (&data_fetched, _1, _2, _3, _4, non_existent_name.c_str ()));

  sleep (5);
*/
  client.exit ();

  client.join ();

  return 0;
}
