#ifndef SERVER_SESSION_H_
#define SERVER_SESSION_H_

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

class server_session : public enable_shared_from_this<server_session>, private noncopyable
{
public:

  server_session (io_service& io_service, udp::endpoint local_udppoint_, data_server *ds);
  ~server_session (void);

  void
  start ();

  /* read header */
  void
  read_request ();

  /* read body */
  void
  handle_request (const system::error_code& err, size_t n);

  void
  handle_fetch_request (const system::error_code& err, size_t n, struct protocol_packet *request);

  void
  write_fetch_response (uint32_t hash_code, char *data, uint32_t data_length);

  void
  fetch_response_written (const system::error_code& err, size_t n, struct protocol_packet *response);

  void
  handle_storage_request (const system::error_code& err, size_t n, struct protocol_packet *request, char *data, uint32_t data_length);

  void
  write_storage_response (uint32_t hash_code);

  void
  storage_response_written (const system::error_code& err, size_t n, struct protocol_packet *response);

  /* variables */
  data_server *ds_;

  udp::socket socket_;
};

#endif /* server_session_ */
