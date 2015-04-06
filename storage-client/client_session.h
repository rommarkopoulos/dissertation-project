#ifndef CLIENT_SESSION_H_
#define CLIENT_SESSION_H_

#include <boost/asio.hpp>
#include <boost/function.hpp>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "client.h"
#include "protocol.h"

using namespace boost;
using namespace asio;
using namespace ip;

using namespace std;

typedef function<void(const system::error_code&, vector<tcp::endpoint> &)> resolution_callback;

typedef function<void(const system::error_code&)> storage_data_callback;

typedef function<void(const system::error_code&, char*, u_int32_t&)> fetch_data_callback;

class client;

class client_session : public enable_shared_from_this<client_session>, private noncopyable
{
public:
  client_session (io_service& io_service, client *client_);
  ~client_session ();

  void
  connect (tcp::endpoint remote_endpoint_, system::error_code &err);

  /* resolution-related methods */

  void
  resolve_dataservers_storage (uint32_t hash_code, u_int8_t replicas, resolution_callback resolution_cb);

  void
  write_storage_resolution_request (uint32_t hash_code, u_int8_t replicas, resolution_callback resolution_cb);

  void
  storage_resolution_request_written (const system::error_code& err, size_t n, struct protocol_packet *request, resolution_callback resolution_cb);

  void
  resolve_dataservers_fetch (uint32_t hash_code, resolution_callback resolution_cb);

  void
  write_fetch_resolution_request (uint32_t hash_code, resolution_callback resolution_cb);

  void
  fetch_resolution_request_written (const system::error_code& err, size_t n, struct protocol_packet *request, resolution_callback resolution_cb);

  void
  read_resolution_response_header (resolution_callback resolution_cb);

  void
  handle_resolution_response_header (const system::error_code& err, size_t n, struct protocol_packet *response, resolution_callback resolution_cb);

  void
  handle_resolution_payload_response (const system::error_code& err, size_t n, struct protocol_packet *response, u_int8_t *replicas_data, resolution_callback resolution_cb);

  /* storage-related methods */
  void
  send_storage_request (uint32_t hash_code, char * data, u_int32_t length, storage_data_callback storage_data_cb);

  void
  send_storage_request_written (const system::error_code& err, size_t n, struct protocol_packet *request, storage_data_callback storage_data_cb);

  void
  handle_send_storage_reuqest_response_header(const system::error_code& err, size_t n, struct protocol_packet *response ,storage_data_callback storage_data_cb);

  void
  handle_send_storage_request_response(const system::error_code& err, size_t n, struct protocol_packet *response, storage_data_callback storage_data_cb);

  /* fetch-related methods */
  void
  send_fetch_request (uint32_t hash_code, fetch_data_callback fetch_data_cb);

  void
  send_fetch_request_written(const system::error_code& err, size_t n, struct protocol_packet *request, fetch_data_callback fetch_data_cb);

  void
  handle_send_fetch_request_response_header(const system::error_code& err, size_t n, struct protocol_packet *response , fetch_data_callback fetch_data_cb);

  void
  handle_send_fetch_reqeust_response(const system::error_code& err, size_t n, struct protocol_packet *response ,char* data, uint32_t data_length, fetch_data_callback fetch_data_cb);

  /* variables */
  client *client_;
  tcp::socket socket_;
};

#endif /* CLIENT_SESSION_H_ */
