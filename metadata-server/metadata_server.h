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

using namespace boost;
using namespace asio;
using namespace ip;

using namespace std;

typedef address_v4 ip_v4;
typedef shared_ptr<io_service::work> work_ptr;

class server_session;

typedef shared_ptr<server_session> server_session_ptr;

class metadata_server : public enable_shared_from_this<metadata_server>, private noncopyable
{
public:
  metadata_server (string ip_address, uint32_t port, size_t thread_pool_size_);
  ~metadata_server (void);

  void
  service_thread (io_service &service);

  /* initialise stuff */
  void
  init ();

  /* join threads and wait to cleanup */
  void
  join ();

  void
  run ();

  void
  handle_accept (const system::error_code& error);

  void
  handle_stop ();

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

  tcp::acceptor acceptor_;
  tcp::endpoint local_endpoint_;

  /* TODO: this must be guarded against concurrent usage */
  vector<tcp::endpoint> server_addresses;
  map<uint32_t, vector<tcp::endpoint> > filenames;
};

#endif /* mds.h */
