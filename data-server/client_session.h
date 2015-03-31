#ifndef CLIENT_SESSION_H_
#define CLIENT_SESSION_H_

#include <boost/asio.hpp>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "data_server.h"
#include "protocol.h"

using namespace boost;
using namespace asio;
using namespace ip;

using namespace std;

class data_server;

class client_session
{
public:
  client_session (tcp::endpoint &mds_endpoint, io_service& io_service, data_server *ds_);
  ~client_session ();

  void
  register_data_server (system::error_code &err);

  void
  write_registration_request ();

  void
  registration_request_written (const system::error_code& err, size_t n, struct protocol_packet *request);

  void
  handle_header (const system::error_code& err, size_t n, struct protocol_packet *proto_pckt);

  void
  handle_registration_response (const system::error_code& err, size_t n, protocol_packet *request);

  /* variables */
  data_server *ds_;
  tcp::endpoint mds_endpoint_;
  tcp::socket socket_;
};

#endif /* CLIENT_SESSION_H_ */
