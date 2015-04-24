#ifndef CLIENT_SESSION_H_
#define CLIENT_SESSION_H_

#include <boost/asio.hpp>
#include <boost/function.hpp>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "data_server.h"

class client_session;

typedef boost::shared_ptr<client_session> client_session_ptr;
typedef boost::function<void(client_session_ptr, const boost::system::error_code&)> register_callback;

class data_server;

class client_session : public boost::enable_shared_from_this<client_session>, private boost::noncopyable
{
public:
  client_session (boost::asio::io_service& io_service, data_server *ds_);
  ~client_session ();

  void
  connect (boost::asio::ip::tcp::endpoint mds_endpoint_, boost::system::error_code &err);

  void
  register_data_server (register_callback cb);

  void
  write_registration_request (register_callback cb);

  void
  registration_request_written (const boost::system::error_code& err, size_t n, struct protocol_packet *request, register_callback cb);

  void
  handle_header (const boost::system::error_code& err, size_t n, struct protocol_packet *proto_pckt, register_callback cb);

  void
  handle_registration_response (const boost::system::error_code& err, size_t n, protocol_packet *request, register_callback cb);

  /* variables */
  data_server *ds_;
  boost::asio::ip::tcp::socket socket_;
};

#endif /* CLIENT_SESSION_H_ */
