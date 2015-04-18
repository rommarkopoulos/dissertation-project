#ifndef CLIENT_H_
#define CLIENT_H_

#include <iostream>
#include <vector>
#include <map>

#include <boost/asio.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/thread/thread.hpp>
#include <boost/functional/hash.hpp>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "client_session.h"

using namespace boost;
using namespace asio;
using namespace ip;

using namespace std;

class client_session;

typedef address_v4 ip_v4;
typedef shared_ptr<io_service::work> work_ptr;
typedef shared_ptr<client_session> client_session_ptr;

typedef function<void(const system::error_code&, uint32_t&)> storage_callback;
typedef function<void(const system::error_code&, uint32_t&, char*, uint32_t&)> fetch_callback;

class client : public enable_shared_from_this<client>, private noncopyable
{
public:
  client (string mds_address, uint16_t mds_port, size_t pool_size_);
  ~client (void);

  void
  service_thread (io_service &service);

  void
  init ();

  void
  exit ();

  void
  join ();

  void
  handle_stop ();

  /*  stores given data */
  void
  store_data (string name, uint8_t replicas, char* data, uint32_t length, storage_callback storage_cb);

  void
  storage_dataservers_resolved(const system::error_code& err, vector<udp::endpoint> &endpoints, u_int32_t hash_code, char* data, uint32_t length, storage_callback storage_cb);

  void
  fetch_dataservers_resolved (const system::error_code& err, vector<udp::endpoint> &endpoints, u_int32_t hash_code, fetch_callback fetch_cb);

  void
  storage_request_written(const system::error_code& err, shared_ptr<u_int8_t> replicas_ptr, u_int32_t hash_code, storage_callback storage_cb);

  void
  fetch_request_written(const system::error_code& err, char *data, uint32_t &length, u_int32_t hash_code, fetch_callback fetch_cb);

  /* fetch data using the given name */
  void
  fetch_data (string name, fetch_callback fetch_cb);

  void
  cleanup (void);

  /*new methods*/
  void
  start_handler(const boost::system::error_code&, std::size_t);

  /* generate a hash_code for filenames */
  uint32_t
  generate_hash_code (string s);

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

  /* mutex to lock access to the map of client sessions */
  mutex data_sessions_mutex;

  /* a dedicated client session with the meta-data server */
  tcp::endpoint mds_endpoint_;
  client_session_ptr metadata_session;

  udp::socket socket_udp;
};

#endif /* CLIENT_H_ */
