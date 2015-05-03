#ifndef CLIENT_H_
#define CLIENT_H_

#define RESET   "\033[0m"
#define BLACK   "\033[30m"      /* Black */
#define RED     "\033[31m"      /* Red */
#define GREEN   "\033[32m"      /* Green */
#define YELLOW  "\033[33m"      /* Yellow */
#define BLUE    "\033[34m"      /* Blue */
#define MAGENTA "\033[35m"      /* Magenta */
#define CYAN    "\033[36m"      /* Cyan */
#define WHITE   "\033[37m"      /* White */
#define BOLDBLACK   "\033[1m\033[30m"      /* Bold Black */
#define BOLDRED     "\033[1m\033[31m"      /* Bold Red */
#define BOLDGREEN   "\033[1m\033[32m"      /* Bold Green */
#define BOLDYELLOW  "\033[1m\033[33m"      /* Bold Yellow */
#define BOLDBLUE    "\033[1m\033[34m"      /* Bold Blue */
#define BOLDMAGENTA "\033[1m\033[35m"      /* Bold Magenta */
#define BOLDCYAN    "\033[1m\033[36m"      /* Bold Cyan */
#define BOLDWHITE   "\033[1m\033[37m"      /* Bold White */

#include <boost/asio/ip/address_v4.hpp>
#include <boost/asio/io_service.hpp>
#include <boost/function.hpp>
#include <boost/system/error_code.hpp>
#include <boost/random/random_device.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/functional/hash.hpp>
#include <boost/chrono/chrono.hpp>

#include <signal.h>

#include "client_session.h"
#include "protocol.h"

#include "decoder.h"
#include "encoder.h"

class client_session;

typedef boost::asio::ip::address_v4 ip_v4;
typedef boost::shared_ptr<boost::asio::io_service::work> work_ptr;
typedef boost::shared_ptr<client_session> client_session_ptr;

typedef std::map<uint32_t, encoding_state*>::iterator encodings_iterator;
typedef std::map<uint32_t, decoding_state*>::iterator decodings_iterator;

typedef std::map<uint32_t, unsigned char *>::iterator blobs_to_encode_iterator;

typedef boost::function<void
(const boost::system::error_code&, uint32_t&)> storage_callback;
typedef boost::function<void
(const boost::system::error_code&, uint32_t&, char*, uint32_t&)> fetch_callback;

class client : public boost::enable_shared_from_this<client>, private boost::noncopyable
{
public:
  client (std::string mds_address, uint16_t mds_port, size_t pool_size_);
  ~client (void);

  void
  service_thread (boost::asio::io_service &service);

  void
  init ();

  void
  exit ();

  void
  join ();

  void
  handle_stop ();

  void
  cleanup ();

  /*  stores given data */
  void
  store_data (std::string name, uint8_t replicas, char* data, uint32_t length, storage_callback storage_cb);

  /* Store methods*/
  void
  storage_dataservers_resolved (const boost::system::error_code& err, std::vector<boost::asio::ip::udp::endpoint> &endpoints, u_int32_t hash_code, char* data, uint32_t length,
				storage_callback storage_cb);

  void
  start_storage_request_written (const boost::system::error_code& err, std::size_t n, struct push_protocol_packet *request,  boost::asio::ip::udp::endpoint ds_endpoint, size_t encoded_hash);

  void
  write_start_storage_request (const boost::system::error_code& err, std::size_t n, struct push_protocol_packet *request, boost::asio::ip::udp::endpoint ds_endpoint, size_t encoded_blob);


  void
  stop_storage_ok_request_written (const boost::system::error_code& err, std::size_t n, struct push_protocol_packet *response, size_t encoded_hash);

  void
  symbol_request_written (const boost::system::error_code& err, std::size_t n, symbol *sym, struct push_protocol_packet *request, struct push_protocol_packet *request_symbol);

  void
  storage_request_written (const boost::system::error_code& err, boost::shared_ptr<u_int8_t> replicas_ptr, u_int32_t hash_code, storage_callback storage_cb);
  /* fetch data using the given name */

  void
  fetch_data (std::string name, fetch_callback fetch_cb);

  /*Fetch methods*/
  void
  fetch_dataservers_resolved (const boost::system::error_code& err, std::vector<boost::asio::ip::udp::endpoint> &endpoints, u_int32_t hash_code, fetch_callback fetch_cb);

  void
  fetch_request_written (const boost::system::error_code& err, char *data, uint32_t &length, u_int32_t hash_code, fetch_callback fetch_cb);

  void
  start_fetch_request_written(const boost::system::error_code& err, std::size_t n, struct push_protocol_packet *request, boost::asio::ip::udp::endpoint ds_endpoint);

  void
  write_start_fetch_request(const boost::system::error_code& err, std::size_t n, struct push_protocol_packet *request, boost::asio::ip::udp::endpoint ds_endpoint);

  void
  stop_fetch_request_written(const boost::system::error_code& err, std::size_t n, struct push_protocol_packet *response);

  /*Read and Handle requests Methods*/
  void
  read_request ();

  void
  handle_request (const boost::system::error_code& err, std::size_t n, struct push_protocol_packet *request, unsigned char *symbol_data, boost::asio::ip::udp::endpoint sender_endpoint_);

  /*Utils*/
  void
  greenColor (std::string text);

  void
  redColor (std::string text);

  void
  yellowColor (std::string text);

  std::string
  sizeToString (size_t sz);

  /* generate a hash_code for filenames */
  uint32_t
  generate_hash_code (std::string s);

  ///////////VARIABLES////////////

  /* size of thread pool */
  size_t pool_size_;

  /* a boost thread_group */
  boost::thread_group thread_grp_;

  /* a single io_service object for the metadata-server */
  boost::asio::io_service io_service_;

  /* set of signals */
  boost::asio::signal_set signals_;

  /* boost work to avoid premature destruction */
  work_ptr work_ptr_;

  /* mutex to lock access to the map of client sessions */
  boost::mutex data_sessions_mutex;

  /* a dedicated client session with the meta-data server */
  boost::asio::ip::tcp::endpoint mds_endpoint_;
  client_session_ptr metadata_session;

  /*udp stuff for client*/
  boost::asio::ip::udp::socket socket_udp;
  boost::asio::ip::udp::endpoint server_endpoint_;
  boost::asio::ip::udp::endpoint sender_endpoint_;

  /*TIMERS might not be necessary*/
  boost::asio::deadline_timer start_storage_delay;
  boost::asio::deadline_timer start_fetch_delay;

  /*FOUNTAIN CODES*/
  boost::random_device rd;
  unsigned char blob_id[BLOB_ID_SIZE];

  /*encoder*/
  encoder enc;
  unsigned int number_of_symbols_to_encode;
  std::map<uint32_t, encoding_state *> encodings;
  boost::mutex encodings_mutex;

  std::map<uint32_t, unsigned char *> blobs_to_encode;
  boost::mutex blobs_to_encode_mutex;

  /*decoder*/
  decoder dec;
  std::map<uint32_t, decoding_state *> decodings;
  boost::mutex decodings_mutex;
};

#endif /* CLIENT_H_ */

