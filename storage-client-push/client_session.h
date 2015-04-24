#ifndef CLIENT_SESSION_H_
#define CLIENT_SESSION_H_

#include <boost/function.hpp>
#include <boost/system/error_code.hpp>
#include <boost/asio.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/thread.hpp>

#include "protocol.h"

typedef boost::function<void
(const boost::system::error_code&, std::vector<boost::asio::ip::udp::endpoint> &)> resolution_callback;

typedef boost::function<void
(const boost::system::error_code&)> storage_data_callback;

typedef boost::function<void
(const boost::system::error_code&, char*, u_int32_t&)> fetch_data_callback;

class client;

class client_session : public boost::enable_shared_from_this<client_session>, private boost::noncopyable
{
public:
  client_session (boost::asio::io_service& io_service, client *client_);
  ~client_session ();

  void
  connect (boost::asio::ip::tcp::endpoint remote_endpoint_, boost::system::error_code &err);

  /* resolution-related methods */

  void
  resolve_dataservers_storage (uint32_t hash_code, u_int8_t replicas, resolution_callback resolution_cb);

  void
  write_storage_resolution_request (uint32_t hash_code, u_int8_t replicas, resolution_callback resolution_cb);

  void
  storage_resolution_request_written (const boost::system::error_code& err, size_t n, struct protocol_packet *request, resolution_callback resolution_cb);

  void
  resolve_dataservers_fetch (uint32_t hash_code, resolution_callback resolution_cb);

  void
  write_fetch_resolution_request (uint32_t hash_code, resolution_callback resolution_cb);

  void
  fetch_resolution_request_written (const boost::system::error_code& err, size_t n, struct protocol_packet *request, resolution_callback resolution_cb);

  void
  read_resolution_response_header (resolution_callback resolution_cb);

  void
  handle_resolution_response_header (const boost::system::error_code& err, size_t n, struct protocol_packet *response, resolution_callback resolution_cb);

  void
  handle_resolution_payload_response (const boost::system::error_code& err, size_t n, struct protocol_packet *response, u_int8_t *replicas_data, resolution_callback resolution_cb);

  /* storage-related methods */
  void
  send_storage_request (uint32_t hash_code, char * data, u_int32_t length, storage_data_callback storage_data_cb);

  void
  send_storage_request_helper (std::vector<boost::asio::mutable_buffer> buffer_store, struct protocol_packet *request, storage_data_callback storage_data_cb);

  void
  send_storage_request_written (const boost::system::error_code& err, size_t n, struct protocol_packet *request, storage_data_callback storage_data_cb);

  void
  handle_send_storage_reuqest_response_header (const boost::system::error_code& err, size_t n, struct protocol_packet *response, storage_data_callback storage_data_cb);

  void
  handle_send_storage_request_response (const boost::system::error_code& err, size_t n, struct protocol_packet *response, storage_data_callback storage_data_cb);

  /* fetch-related methods */
  void
  send_fetch_request (uint32_t hash_code, fetch_data_callback fetch_data_cb);

  void
  send_fetch_request_written (const boost::system::error_code& err, size_t n, struct protocol_packet *request, fetch_data_callback fetch_data_cb);

  void
  handle_send_fetch_request_response_header (const boost::system::error_code& err, size_t n, struct protocol_packet *response, fetch_data_callback fetch_data_cb);

  void
  handle_send_fetch_request_response (const boost::system::error_code& err, size_t n, struct protocol_packet *response, char* data, uint32_t data_length, fetch_data_callback fetch_data_cb);

  /* variables */
  client *client_;
  boost::asio::ip::tcp::socket socket_;
  boost::asio::io_service::strand strand_;

  bool is_pending;
  boost::mutex is_pending_mutex;
};

#endif /* CLIENT_SESSION_H_ */
