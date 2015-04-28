#ifndef METADATA_SERVER_H_
#define METADATA_SERVER_H_

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

#include "server_session.h"
#include "client_session.h"
#include "protocol.h"

#include "decoder.h"
#include "encoder.h"

typedef boost::asio::ip::address_v4 ip_v4;
typedef boost::shared_ptr<boost::asio::io_service::work> work_ptr;

class server_session;
class client_session;

typedef boost::shared_ptr<server_session> server_session_ptr;
typedef boost::shared_ptr<client_session> client_session_ptr;

typedef map<uint32_t, encoding_state*>::iterator encodings_iterator;
typedef map<uint32_t, decoding_state*>::iterator decodings_iterator;

struct stored_data
{
  char* data;
  uint32_t data_length;
};

class data_server : public boost::enable_shared_from_this<data_server>, private boost::noncopyable
{
public:
  data_server (string bind_address, uint16_t bind_port, string mds_address, uint16_t mds_port, size_t thread_pool_size_);
  ~data_server (void);

  void
  service_thread (boost::asio::io_service &service);

  /* initialise stuff */
  void
  init ();

  /* a callback that will be called by the client_session when the data-server is successfully registered with the metadata-server */
  void
  registered (client_session_ptr client_session_ptr_, const boost::system::error_code& err);

  /* join threads and wait to cleanup */
  void
  join ();

  void
  run ();

  void
  handle_accept (const boost::system::error_code& error);

  void
  handle_stop ();

  void
  read_request ();

  void
  handle_request (const boost::system::error_code& error, std::size_t bytes_transferred, struct push_protocol_packet *request, unsigned char *symbol_data);

  void
  start_storage_ok_request_written (const boost::system::error_code& error, std::size_t bytes_transferred, struct push_protocol_packet *response);

  void
  stop_storage_request_written (const boost::system::error_code& error, std::size_t bytes_transferred, struct push_protocol_packet *response);

  /*color for terminal*/
  void
  greenColor (string text);

  void
  redColor (string text);

  void
  yellowColor (string text);

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

  /* TODO: when remote endpoints disconnect session pointers must be reset and removed from the server_sessions vector */
  server_session_ptr new_server_session_ptr;
  vector<server_session_ptr> server_sessions;

  /* server-related stuff */
  boost::asio::ip::udp::socket udp_socket_;
  boost::asio::ip::udp::endpoint sender_endpoint_;

  /* data-server is also a client that registers to the meta-data server */
  boost::asio::ip::tcp::endpoint mds_endpoint_;
  boost::asio::ip::tcp::endpoint local_endpoint_;
  client_session_ptr client_session_ptr_;

  /* data-server specific variables */
  /* TODO: romanos this must be guarded against concurrent usage*/
  map<uint32_t, stored_data> storage;

  /*FOUNTAIN CODES*/
  boost::random_device rd;
  unsigned char blob_id[BLOB_ID_SIZE];

  /*encoder*/
  encoder enc;
  unsigned int number_of_symbols_to_encode;
  std::map<uint32_t, encoding_state *> encodings;
  boost::mutex encodings_mutex;

  /*decoder*/
  decoder dec;
  std::map<uint32_t, decoding_state *> decodings;
  boost::mutex decodings_mutex;
};

#endif /* METADATA_SERVER_H_*/
