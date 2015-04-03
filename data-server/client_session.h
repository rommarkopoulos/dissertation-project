#ifndef CLIENT_SESSION_H_
#define CLIENT_SESSION_H_

#include <boost/asio.hpp>
#include <boost/function.hpp>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "data_server.h"
#include "protocol.h"

using namespace boost;
using namespace asio;
using namespace ip;

using namespace std;

class client_session;

typedef shared_ptr<client_session> client_session_ptr;
typedef function<void(client_session_ptr, const system::error_code&)> register_callback;

class data_server;

class client_session : public enable_shared_from_this<client_session>, private noncopyable
{
public:
  client_session (io_service& io_service, data_server *ds_);
  ~client_session ();

  void
  connect (tcp::endpoint mds_endpoint_, system::error_code &err);

  void
  register_data_server (register_callback cb);

  void
  write_registration_request (register_callback cb);

  void
  registration_request_written (const system::error_code& err, size_t n, struct protocol_packet *request, register_callback cb);

  void
  handle_header (const system::error_code& err, size_t n, struct protocol_packet *proto_pckt, register_callback cb);

  void
  handle_registration_response (const system::error_code& err, size_t n, protocol_packet *request, register_callback cb);

  /* variables */
  data_server *ds_;
  tcp::socket socket_;
};

#endif /* CLIENT_SESSION_H_ */
