#ifndef SERVER_SESSION_H_
#define SERVER_SESSION_H_

#include <boost/asio.hpp>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "metadata_server.h"
#include "protocol.h"

using namespace boost;
using namespace asio;
using namespace ip;

using namespace std;

class metadata_server;

class server_session : public enable_shared_from_this<server_session>, private noncopyable
{
public:

  server_session (io_service& io_service, metadata_server *mds);
  ~server_session (void);

  void
  start ();

  /* read header */
  void
  read_header ();

  /* read body */
  void
  handle_header (const system::error_code& err, size_t n, protocol_packet *proto_pckt);

  void
  handle_registration_request (const system::error_code& err, size_t n, struct protocol_packet *request);

  void
  write_registration_response ();

  void
  registration_response_written (const system::error_code& err, size_t n, struct protocol_packet *response);

  void
  handle_resolution_request (const system::error_code& err, size_t n, struct protocol_packet *request);

  void
  write_resolution_response (vector<tcp::endpoint> &replication_addresses);

  void
  resolution_response_written (const system::error_code& err, size_t n, struct protocol_packet *response);

  /* random replication method */
  void
  random_replication (uint32_t hash_code, uint32_t rep_num, vector<tcp::endpoint> &replication_addresses);

  /* variables */
  metadata_server *mds_;

  tcp::socket socket_;
};

#endif /* server_session_ */
