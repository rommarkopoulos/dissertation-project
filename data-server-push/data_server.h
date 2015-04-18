#ifndef METADATA_SERVER_H_
#define METADATA_SERVER_H_

#include <iostream>
#include <vector>
#include <map>

#include <boost/asio.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/thread/thread.hpp>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "server_session.h"
#include "client_session.h"

using namespace boost;
using namespace asio;
using namespace ip;

using namespace std;

typedef address_v4 ip_v4;
typedef shared_ptr<io_service::work> work_ptr;

class server_session;
class client_session;

typedef shared_ptr<server_session> server_session_ptr;
typedef shared_ptr<client_session> client_session_ptr;

struct stored_data
{
  char* data;
  uint32_t data_length;
};

class data_server : public enable_shared_from_this<data_server>, private noncopyable
{
public:
  data_server (string bind_address, uint16_t bind_port, string mds_address, uint16_t mds_port, size_t thread_pool_size_);
  ~data_server (void);

  void
  service_thread (io_service &service);

  /* initialise stuff */
  void
  init ();

  /* a callback that will be called by the client_session when the data-server is successfully registered with the metadata-server */
  void
  registered (client_session_ptr client_session_ptr_, const system::error_code& err);

  /* join threads and wait to cleanup */
  void
  join ();

  void
  run ();

  void
  handle_accept (const system::error_code& error);

  void
  handle_stop ();

  void
  registration_request_written (const system::error_code& err, size_t n, struct protocol_packet *request);

  void
  handle_header(const system::error_code& err, size_t n, protocol_packet *request);

  void
  handle_registration_response(const system::error_code& err, size_t n, protocol_packet *request);

  void
  handle_request(const system::error_code& err, size_t n);


  /* size of thread pool */
  size_t pool_size_;

  /* a boost thread_group */
  thread_group thread_grp_;

  /* a single io_service object for the metadata-server */
  io_service io_service_;

  /* set of signals */
  signal_set signals_;

  /* boost work to avoid premature destruction */
  work_ptr work_ptr_;

  /* TODO: when remote endpoints disconnect session pointers must be reset and removed from the server_sessions vector */
  server_session_ptr new_server_session_ptr;
  vector<server_session_ptr> server_sessions;

  /* server-related stuff */
  tcp::acceptor acceptor_;
  tcp::endpoint local_endpoint_;
  tcp::socket tcp_socket_;

  udp::endpoint local_udppoint_;
  udp::socket udp_socket_;

  /* data-server is also a client that registers to the meta-data server */
  tcp::endpoint mds_endpoint_;
  client_session_ptr client_session_ptr_;

  /* data-server specific variables */
  /* TODO: romanos this must be guarded against concurrent usage*/
  map<uint32_t, stored_data> storage;
};

#endif
