#include "client.h"

using namespace boost;
using namespace asio;
using namespace ip;
using namespace std;
using namespace chrono;

client::client (string mds_address, uint16_t mds_port, size_t pool_size_) :
    pool_size_ (pool_size_), signals_ (io_service_), work_ptr_ (new io_service::work (io_service_)), mds_endpoint_ (ip_v4::from_string (mds_address), mds_port), socket_udp (
	io_service_, udp::endpoint (udp::v4 (), 4040)), delay (io_service_, posix_time::seconds (2)), write_delay(io_service_, posix_time::seconds(5))
{
  cout << endl;
  cout << endl;
  greenColor ("client");
  cout << ": constructor()" << endl;
}

void
client::service_thread (io_service &service)
{
  try {
    service.run ();
  } catch (std::exception& e) {
    redColor ("ERROR");
    cout << "ioservice exception was caught..not sure this is expected - re-running ioservice" << endl;
    service.run ();
  }
}

void
client::init ()
{
  system::error_code err;
  greenColor ("client");
  cout << ": init()" << endl;

  /* start threads */
  for (size_t i = 0; i < pool_size_; i++) {
    thread_grp_.create_thread (bind (&client::service_thread, this, ref (io_service_)));
  }

  signals_.add (SIGINT);
  signals_.add (SIGTERM);
#if defined(SIGQUIT)
  signals_.add (SIGQUIT);
#endif

  signals_.async_wait (bind (&client::handle_stop, this));

  /* create a client session with the metadata-server */
  metadata_session = client_session_ptr (new client_session (io_service_, this));
  metadata_session->connect (mds_endpoint_, err);
  if (err) {
    /* cannot continue here..could not connect to the metadata-server */
    cout << "client: " << err.message () << endl;
    signals_.cancel ();
    work_ptr_.reset ();
  } else {
    greenColor ("client");
    cout << ": successfully connected to the meta-data server" << endl;
  }
}

void
client::join ()
{
  cout << "client: join()" << endl;
  thread_grp_.join_all ();

  /* close the socket to the mds and reset the shared_ptr */
  if (!metadata_session) {
    metadata_session->socket_.close ();
    metadata_session.reset ();
  }
}

void
client::exit ()
{
  cout << "client: exit()" << endl;
  signals_.cancel ();
  work_ptr_.reset ();
}

void
client::handle_stop ()
{
  cout << "client: handle_stop()" << endl;
  signals_.cancel ();
  work_ptr_.reset ();
}

void
client::store_data (string name, uint8_t replicas, char* data, uint32_t length, storage_callback storage_cb)
{
  u_int32_t hash_code = generate_hash_code (name);

  greenColor ("client");
  cout << ": store_data - filename: " << name << ", replication factor: " << (int) replicas << ", data length: " << length << endl;

  metadata_session->resolve_dataservers_storage (hash_code, replicas, bind (&client::storage_dataservers_resolved, this, _1, _2, hash_code, data, length, storage_cb));
}

void
client::fetch_data (string name, fetch_callback fetch_cb)
{
  u_int32_t hash_code = generate_hash_code (name);

  cout << "client: fetch_data - filename: " << name << endl;

  metadata_session->resolve_dataservers_fetch (hash_code, bind (&client::fetch_dataservers_resolved, this, _1, _2, hash_code, fetch_cb));
}

void
client::storage_dataservers_resolved (const system::error_code& err, vector<udp::endpoint> &endpoints, u_int32_t hash_code, char* data, uint32_t length, storage_callback storage_cb)
{
  if (!err) {

    unsigned char *blob_to_encode;

    // generate a random blob - for debugging purposes to check that decoding works properly
    boost::random::mt19937 generator (rd ()); // returns unsigned int - 32 bits

    number_of_symbols_to_encode = 10;

    posix_memalign ((void **) &blob_to_encode, 16, BLOB_SIZE);
    memset (blob_to_encode, 0, BLOB_SIZE);

    /////////////////////////////////RANDOMISE BLOB/////////////////////////////////
    for (unsigned int i = 0; i < BLOB_SIZE; i++) {
      blob_to_encode[i] = (unsigned char) generator ();
    }
    string blob_to_encode_str ((const char *) blob_to_encode, BLOB_SIZE);
    boost::hash<std::string> blob_to_encode_str_hash;
    std::size_t encoded_hash = blob_to_encode_str_hash (blob_to_encode_str);
    cout << encoded_hash << endl;
    /////////////////////////////////////////////////////////////////////////////////

    greenColor ("client");
    cout << "storage_dataservers_resolved()" << endl;

    if (endpoints.size () > 0) {

      /* for each endpoint, send a storage request */
      vector<udp::endpoint>::iterator endpoints_iter;
      shared_ptr<u_int8_t> replicas_ptr (new u_int8_t (endpoints.size ()));

      yellowColor ("client");
      cout << "s_d_r: initial value: " << (u_int32_t) (*replicas_ptr) << endl;

      for (endpoints_iter = endpoints.begin (); endpoints_iter != endpoints.end (); endpoints_iter++) {
	udp::endpoint &endpoint = *endpoints_iter;

	yellowColor ("client");
	cout << "s_d_r: sending to DataServer: " << endpoint.address ().to_string () << ":" << endpoint.port () << endl;

	struct push_protocol_packet *request = (struct push_protocol_packet *) malloc (sizeof(struct push_protocol_packet));

	request->hdr.payload_length = sizeof(request->push_payload.start_storage) + SYMBOL_SIZE + PADDING;
	request->hdr.type = START_STORAGE;
	request->push_payload.start_storage.hash_code = hash_code;

	char *padding = (char *) malloc (SYMBOL_SIZE + PADDING);

	vector<boost::asio::mutable_buffer> buffer_write;

	buffer_write.push_back (buffer (request, sizeof(request->hdr) + sizeof(request->push_payload.start_storage)));
	buffer_write.push_back (buffer (padding, SYMBOL_SIZE + PADDING));

	yellowColor ("client");
	cout << "s_d_r: request size: " << request->hdr.payload_length << " bytes." << endl;
	yellowColor ("client");
	cout << "s_d_r: request type: " << (int) request->hdr.type << endl;


	socket_udp.async_send_to (buffer_write, endpoint,
				  boost::bind (&client::write_start_storage_request, this, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred, request, endpoint, encoded_hash));

	read_request (blob_to_encode);
      }
    } else {
      /* I got zero servers back */
      storage_cb (system::error_code (system::errc::protocol_error, system::errno_ecat), hash_code);
    }
  } else {
    /* something went wrong - pass the error to the storage_callback */
    storage_cb (err, hash_code);
  }
}

void
client::storage_request_written (const system::error_code& err, shared_ptr<u_int8_t> replicas_ptr, u_int32_t hash_code, storage_callback storage_cb)
{
  /* this is tricky because of the replication */
  /* the method will be called for each replica written but the storage_cb should be called only when all replicas are written or when an error occurs */
  (*replicas_ptr)--;
  cout << "client::debugging - remaining replicas: " << (u_int32_t) (*replicas_ptr) << endl;
  if (*replicas_ptr == 0) {
    storage_cb (err, hash_code);
  }
}

void
client::write_start_storage_request (const boost::system::error_code& err, std::size_t n, struct push_protocol_packet *request, udp::endpoint ds_endpoint, size_t encoded_hash)
{
  if (!err) {


    delay.wait ();

    encodings_iterator encoding_iter;

    /*get lock*/
    encodings_mutex.lock ();

    encoding_iter= encodings.find (request->push_payload.start_storage.hash_code);

    if (encoding_iter != encodings.end ()) {

      encodings_mutex.unlock();

      greenColor ("client");
      cout << "write_start_storage_request()" << endl;
      yellowColor ("client");
      cout << ": ";
      greenColor ("File Found!");
      cout << endl;
      yellowColor ("client");
      cout << "Starting the encoding of "<< number_of_symbols_to_encode << " symbols..." << endl;

      /*pointer is useless*/
      unsigned char *justtocallread;
      read_request(justtocallread);

      ////////////////////////////////////ENCODING BLOB////////////////////////////////////
      for (unsigned int i = 0; i < number_of_symbols_to_encode; i++) {

	encodings_mutex.lock ();
	if(encoding_iter != encodings.end()) {
	  encodings_mutex.unlock ();

	  /*symbol created*/
	  symbol *sym = enc.encode_next (encoding_iter->second);

	  struct push_protocol_packet *request_symbol = (struct push_protocol_packet *) malloc (sizeof(struct push_protocol_packet));

	  request_symbol->hdr.payload_length = sizeof(request->push_payload.symbol_data) + SYMBOL_SIZE;
	  request_symbol->hdr.type = SYMBOL_DATA;
	  request_symbol->push_payload.symbol_data.hash_code = request->push_payload.start_storage.hash_code;
	  request_symbol->push_payload.symbol_data.blob_size = BLOB_SIZE;
	  request_symbol->push_payload.symbol_data.seed = sym->seed;

	  vector<boost::asio::mutable_buffer> buffer_store;

	  buffer_store.push_back (boost::asio::buffer (request_symbol, sizeof(request_symbol->hdr) + sizeof(request_symbol->push_payload.symbol_data)));
	  buffer_store.push_back (boost::asio::buffer (sym->symbol_data, SYMBOL_SIZE));

	  redColor ("client");
	  cout << "Times inside here: " << i + 1 << endl;


//	  yellowColor ("client");
//	  cout << "r_s: sending to DataServer: " << ds_endpoint.address ().to_string () << ":" << ds_endpoint.port () << endl;
//	  yellowColor("client");
//	  cout << "r_s: request size: " << request_symbol->hdr.payload_length << " bytes." << endl;
//	  yellowColor("client");
//	  cout << "r_s: request type: " << (int) request_symbol->hdr.type << endl;
//	  yellowColor("client");
//	  cout << "r_s: Symbol for : " << request_symbol->push_payload.symbol_data.hash_code << endl;
//	  yellowColor("client");
//	  cout << "r_s: Blob size : " << request_symbol->push_payload.symbol_data.blob_size << endl;
//	  yellowColor("client");
//	  cout << "r_s: Seed : " << sym->seed << endl;

	  socket_udp.async_send_to (buffer_store, ds_endpoint, boost::bind (&client::symbol_request_written, this, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred, sym, request, request_symbol, encoded_hash));


	} else {
	  redColor("symbol DECODED");
	  encodings_mutex.unlock ();
	  break;
	}
      }
    } else {

      redColor ("client");
      cout << ": NOT FOUND" << endl;
      socket_udp.async_send_to (buffer (request, sizeof(request->hdr) + request->hdr.payload_length), ds_endpoint,
				boost::bind (&client::write_start_storage_request, this, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred, request, ds_endpoint, encoded_hash));
      encodings_mutex.unlock ();
    }
  } else {
    redColor ("client");
    cout << ": write_start_storage_request: " << err.message () << endl;
  }
}

void
client::symbol_request_written (const boost::system::error_code& err, std::size_t n, symbol *sym, struct push_protocol_packet *request, struct push_protocol_packet *request_symbol, size_t encoded_hash)
{
  if (!err) {
    greenColor ("client");
    cout << "symbol of encoded blob " << encoded_hash << " written" << endl;
    free (sym);
    //free (request);
    free (request_symbol);
  } else {
    redColor ("client");
    cout << ": symbol_request_written: " << err.message () << endl;
  }
}

void
client::start_storage_request_written (const boost::system::error_code& err, std::size_t n , struct push_protocol_packet *request)
{
  if(!err)
  {
    greenColor ("client");
    cout << "::start_storage_request_written() successfully" << endl;
  } else {
    redColor ("client");
    cout << ": start_storage_request_written: " << err.message () << endl;
  }


}

void
client::read_request (unsigned char* blob_to_encode)
{

  greenColor ("client");
  cout << "read_request()" << endl;

  struct push_protocol_packet *request = (struct push_protocol_packet *) malloc (sizeof(struct push_protocol_packet));

  socket_udp.async_receive_from (boost::asio::buffer (request, sizeof(struct push_protocol_packet)), sender_endpoint_,
				 boost::bind (&client::handle_request, this, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred, request, blob_to_encode, sender_endpoint_));

}

void
client::handle_request (const boost::system::error_code& err, std::size_t n, struct push_protocol_packet *request, unsigned char* blob_to_encode, udp::endpoint sender_endpoint_)
{
  greenColor ("client");
  cout << "handle_request()" << endl;

  if (!err) {
    switch (request->hdr.type)
    {
      case START_STORAGE_OK:
      {
	greenColor ("client");
	cout << ": START STORAGE OK for " << request->push_payload.start_storage.hash_code << endl;

	encoding_state *enc_state = enc.init_state (blob_id, BLOB_SIZE, blob_to_encode);

	encodings_mutex.lock();
	encodings.insert (make_pair (request->push_payload.start_storage.hash_code, enc_state));
	encodings_mutex.unlock();

	break;
      }
      case STOP_STORAGE:
      {
	greenColor ("client");
	cout << ": STOP STORAGE for " << request->push_payload.start_storage.hash_code << endl;

	encodings_mutex.lock();
	encodings.erase(request->push_payload.start_storage.hash_code);
	encodings_mutex.unlock();

	struct push_protocol_packet *response = (struct push_protocol_packet *) malloc (sizeof(struct push_protocol_packet));

	response->hdr.payload_length = sizeof(response->push_payload.stop_storage_ok) + PADDING + SYMBOL_SIZE;
	response->hdr.type = STOP_STORAGE_OK;
	response->push_payload.stop_storage_ok.hash_code = request->push_payload.start_storage.hash_code;

	char *padding = (char *) malloc (SYMBOL_SIZE + PADDING);

	vector<boost::asio::mutable_buffer> buffer_write;

	buffer_write.push_back (buffer (response, sizeof(response->hdr) + sizeof(response->push_payload.stop_storage_ok)));
	buffer_write.push_back (buffer (padding, SYMBOL_SIZE + PADDING));

	yellowColor ("client");
	cout << "r_s: sending to DataServer: " << sender_endpoint_.address ().to_string () << ":" << sender_endpoint_.port () << endl;
	yellowColor("client");
	cout << "r_s: request size: " << response->hdr.payload_length << " bytes." << endl;
	yellowColor("client");
	cout << "r_s: request type: " << (int) response->hdr.type << endl;

	socket_udp.async_send_to (buffer_write, sender_endpoint_,
				  boost::bind (&client::stop_storage_request_written, this, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred, response));

	break;
      }

      default:
	redColor ("client");
	cout << ": fatal - unknown request" << endl;

	break;
    }
  } else {
    redColor ("client");
    cout << ": handle_request: " << err.message () << endl;
  }
  free (request);
  //free (blob_to_encode);
}
void
client::stop_storage_request_written(const boost::system::error_code& err, std::size_t n, struct push_protocol_packet *response)
{
  if(!err)
  {
    greenColor("client");
    cout << "stop_storage_request_written()" << endl;
  } else {

  }
  free(response);
}

void
client::fetch_dataservers_resolved (const system::error_code& err, vector<udp::endpoint> &endpoints, u_int32_t hash_code, fetch_callback fetch_cb)
{
  if (!err) {
    cout << "client: fetch_dataservers_resolved" << endl;

    if (endpoints.size () > 0) {
      client_session_ptr data_session;
      map<udp::endpoint, client_session_ptr>::iterator data_sessions_iterator;

      /* send fetch request to one of the replicas (the first one for now)*/
      udp::endpoint &endpoint = endpoints[0];

    } else {
      /* no replicas for the requested data - e.g. the name does not exist */
      uint32_t length = 0;
      fetch_cb (system::error_code (system::errc::no_such_file_or_directory, system::errno_ecat), hash_code, NULL, length);
    }
  } else {
    /* something went wrong - pass the error to the storage_callback */
    uint32_t length = 0;
    fetch_cb (system::error_code (system::errc::protocol_error, system::errno_ecat), hash_code, NULL, length);
  }
}

void
client::fetch_request_written (const system::error_code& err, char *data, uint32_t &length, u_int32_t hash_code, fetch_callback fetch_cb)
{
  if (!err) {
    fetch_cb (err, hash_code, data, length);
  } else {
    u_int32_t length = 0;
    fetch_cb (err, hash_code, NULL, length);
  }
}

/* generate the hash code of a string (filenames) */
uint32_t
client::generate_hash_code (string s)
{
  hash<std::string> string_hash;
  std::size_t hash_value = string_hash (s);

  cout << "client::generate_hash_code: name " << s << " hashed to " << static_cast<uint32_t> (hash_value) << endl;

  return static_cast<uint32_t> (hash_value);
}

void
client::cleanup (void)
{

}

void
client::greenColor (string text)
{
  cout << "[" << BOLDGREEN << text << RESET << "]";
}

void
client::redColor (string text)
{
  cout << "[" << BOLDRED << text << RESET << "]";
}

void
client::yellowColor (string text)
{
  cout << "[" << BOLDYELLOW << text << RESET << "]";
}

//Destructor
client::~client (void)
{
  cout << "client: destructor()" << endl;
}
