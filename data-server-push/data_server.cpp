#include "data_server.h"

using namespace boost;
using namespace asio;
using namespace ip;
using namespace std;

data_server::data_server (string bind_address, uint16_t bind_port, string mds_address, uint16_t mds_port, size_t pool_size_) :
    pool_size_ (pool_size_), signals_ (io_service_), work_ptr_ (new io_service::work (io_service_)), local_endpoint_ (ip_v4::from_string (bind_address), bind_port), mds_endpoint_ (
	ip_v4::from_string (mds_address), mds_port), udp_socket_ (io_service_, udp::endpoint (boost::asio::ip::address_v4::from_string (bind_address), bind_port))
{
  cout << endl;
  cout << endl;
  greenColor ("data_server");
  cout << ": constructor()" << endl;
  yellowColor ("data_server");
  cout << ": IP|PORT: " << udp_socket_.local_endpoint ().address ().to_string () << ":" << udp_socket_.local_endpoint ().port () << endl;
}

void
data_server::service_thread (io_service &service)
{
  try {
    service.run ();
  } catch (std::exception& e) {
    cout << "ioservice exception was caught..not sure this is expected - re-running ioservice" << endl;
    service.run ();
  }
}

void
data_server::init ()
{
  system::error_code err;
  cout << "data_server: init()" << endl;
  /* start threads */
  for (size_t i = 0; i < pool_size_; i++)
    thread_grp_.create_thread (bind (&data_server::service_thread, this, ref (io_service_)));

  signals_.add (SIGINT);
  signals_.add (SIGTERM);
#if defined(SIGQUIT)
  signals_.add (SIGQUIT);
#endif

  signals_.async_wait (bind (&data_server::handle_stop, this));

  /* create a client session with the metadata-server */
  client_session_ptr_ = client_session_ptr (new client_session (io_service_, this));

  /* this is synchronous to avoid several complications*/
  client_session_ptr_->connect (mds_endpoint_, err);
  if (err) {
    /* cannot continue here..could not register with metadata-server */
    cout << "data-server: " << err.message () << endl;
    signals_.cancel ();
    work_ptr_.reset ();
  } else {
    /* I use my own callback here _1 and _2 are like the boost::placeholders stuff */
    client_session_ptr_->register_data_server (bind (&data_server::registered, this, _1, _2));
  }
}

void
data_server::registered (client_session_ptr client_session_ptr_, const system::error_code& err)
{
  if (!err) {
    /* successful */
    cout << "data_server: registered with metadata server (remote endpoint " << client_session_ptr_->socket_.remote_endpoint () << ")" << endl;
  } else {
    /* errors should be handled in such callbacks in general */
  }

  /* close client session with metadata server */
  client_session_ptr_->socket_.close ();

  read_request ();
}

void
data_server::read_request ()
{

  greenColor ("data_server");
  cout << "read_request()" << endl;

  struct push_protocol_packet *request = (struct push_protocol_packet *) malloc (sizeof(struct push_protocol_packet));

  unsigned char *symbol_data = (unsigned char *) malloc (SYMBOL_SIZE);

  vector<boost::asio::mutable_buffer> buffer_read;

  buffer_read.push_back (boost::asio::buffer (&request->hdr, sizeof(request->hdr)));
  buffer_read.push_back (boost::asio::buffer (&request->push_payload.symbol_data, sizeof(request->push_payload.symbol_data)));
  buffer_read.push_back (boost::asio::buffer (symbol_data, SYMBOL_SIZE));

  //boost::asio::buffer (request, sizeof(struct push_protocol_packet)

  udp_socket_.async_receive_from (buffer_read, sender_endpoint_,
				  boost::bind (&data_server::handle_request, this, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred, request, symbol_data));

}

void
data_server::handle_request (const boost::system::error_code& error, std::size_t bytes_transferred, struct push_protocol_packet *request, unsigned char *symbol_data)
{
  if (!error) {
    greenColor ("data_server");
    cout << "handle_request()" << endl;
    yellowColor ("data_server");
    cout << ": sender info: " << sender_endpoint_.address ().to_string () << ":" << sender_endpoint_.port () << endl;
    yellowColor ("data_server");
    cout << ": request size: " << request->hdr.payload_length << endl;
    yellowColor ("data_server");
    cout << ": request type: " << (int) request->hdr.type << endl;

    switch (request->hdr.type)
    {
      case START_STORAGE:
      {
	greenColor ("data_server");
	cout << ": START STORAGE for " << request->push_payload.start_storage.hash_code << endl;

	unsigned char *decoded_blob;
	posix_memalign ((void **) &decoded_blob, 16, BLOB_SIZE);
	memset (decoded_blob, 0, BLOB_SIZE);

	decodings_mutex.lock();
	decodings_iterator decoding_iter = decodings.find (request->push_payload.start_storage.hash_code);
	if (decoding_iter != decodings.end ()) {
	  yellowColor ("data_server");
	  cout << "Already decoding file: " << request->push_payload.start_storage.hash_code << endl;
	  decodings_mutex.unlock();
	} else {
	  decoding_state *dec_state = dec.init_state (blob_id, BLOB_SIZE, decoded_blob);
	  decodings.insert (make_pair (request->push_payload.start_storage.hash_code, dec_state));
	  decodings_mutex.unlock();
	}

	struct push_protocol_packet *response = (struct push_protocol_packet *) malloc (sizeof(struct push_protocol_packet));

	response->hdr.payload_length = sizeof(request->push_payload.start_storage_ok);
	response->hdr.type = START_STORAGE_OK;
	response->push_payload.start_storage_ok.hash_code = request->push_payload.start_storage.hash_code;

	char *padding = (char *) malloc (SYMBOL_SIZE + PADDING);

	vector<boost::asio::mutable_buffer> buffer_write;

	buffer_write.push_back (buffer (response, sizeof(response->hdr) + response->hdr.payload_length));
	buffer_write.push_back (buffer (padding, SYMBOL_SIZE + PADDING));

	udp_socket_.async_send_to (buffer_write, sender_endpoint_, boost::bind (&data_server::nothing, this, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred, response));

	break;
      }
      case SYMBOL_DATA:
      {
	greenColor ("data_server");
	cout << ": SYMBOL" << endl;

	symbol *sym = new symbol ();

	sym->seed = request->push_payload.symbol_data.seed;
	sym->symbol_data = symbol_data;

	yellowColor("data_server");
	cout << ": Symbol for : " << request->push_payload.symbol_data.hash_code << endl;
	yellowColor("data_server");
	cout << ": Blob size : " << request->push_payload.symbol_data.blob_size << endl;
	yellowColor("data_server");
	cout << ": Seed : " << sym->seed << endl;

	decodings_mutex.lock();
	decodings_iterator decoding_iter = decodings.find (request->push_payload.start_storage.hash_code);
	if (dec.decode_next (decoding_iter->second, sym) == true) {
	  greenColor ("client");
	  greenColor ("DECODED!");


	  string decoded_blob_str ((const char *) decoding_iter->second->blob, BLOB_SIZE);
	  boost::hash<string> decoded_blob_str_hash;
	  std::size_t d = decoded_blob_str_hash (decoded_blob_str);
	  cout << "                    " << d << endl;

	  struct push_protocol_packet *response = (struct push_protocol_packet *) malloc (sizeof(struct push_protocol_packet));

	  response->hdr.payload_length = sizeof(request->push_payload.stop_storage);
	  response->hdr.type = STOP_STORAGE;
	  response->push_payload.stop_storage.hash_code = request->push_payload.start_storage.hash_code;

	  char *padding = (char *) malloc (SYMBOL_SIZE + PADDING);

	  vector<boost::asio::mutable_buffer> buffer_write;

	  buffer_write.push_back (buffer (response, sizeof(response->hdr) + response->hdr.payload_length));
	  buffer_write.push_back (buffer (padding, SYMBOL_SIZE + PADDING));

	  udp_socket_.async_send_to (buffer_write, sender_endpoint_, boost::bind (&data_server::nothing, this, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred, response));
	  decodings_mutex.unlock();
	} else {

	  decodings_mutex.unlock();
	}
	//free(sym);
	break;
      }
      case STOP_STORAGE_OK:
      {
	greenColor ("data_server");
	cout << ": STOP_STORAGE_OK for " << request->push_payload.stop_storage_ok.hash_code << endl;
//	decodings_mutex.lock();
//	decodings.erase(request->push_payload.start_storage.hash_code);
//	decodings_mutex.unlock();
	break;
      }
      default:
	redColor ("data_server");
	cout << ": fatal - unknown request" << endl;
	break;
    }
  } else {
    redColor ("data_server");
    cout << ": handle_request: " << error.message () << endl;
  }

  free (request);
  read_request ();
}

void
data_server::nothing (const boost::system::error_code& error, std::size_t bytes_transferred, struct push_protocol_packet *request)
{
  greenColor ("data_server");
  cout << "::nothing()" << endl;
}

void
data_server::join ()
{
  cout << "data_server: join()" << endl;
  thread_grp_.join_all ();
  io_service_.stop ();
}

void
data_server::handle_stop ()
{
  cout << "data_server: handle_stop()" << endl;
  io_service_.stop ();
}

void
data_server::greenColor (string text)
{
  cout << "[" << BOLDGREEN << text << RESET << "]";
}

void
data_server::redColor (string text)
{
  cout << "[" << BOLDRED << text << RESET << "]";
}

void
data_server::yellowColor (string text)
{
  cout << "[" << BOLDYELLOW << text << RESET << "]";
}

data_server::~data_server (void)
{
  cout << "data_server: destructor" << endl;
}
