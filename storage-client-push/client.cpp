#include "client.h"

client::client (string mds_address, uint16_t mds_port, size_t pool_size_) :
    pool_size_ (pool_size_), signals_ (io_service_), work_ptr_ (new io_service::work (io_service_)), mds_endpoint_ (ip_v4::from_string (mds_address), mds_port), socket_udp (
	io_service_, udp::endpoint (udp::v4 (), 4040)), delay (io_service_, posix_time::seconds (2))
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
	cout << "s_d_r: current endpoint's IP/PORT: " << endpoint.address ().to_string () << ":" << endpoint.port () << endl;

	struct push_protocol_packet *request = (struct push_protocol_packet *) malloc (sizeof(struct push_protocol_packet));

	request->hdr.payload_length = sizeof(request->push_payload.start_storage);
	request->hdr.type = START_STORAGE;
	request->push_payload.start_storage.hash_code = hash_code;

	yellowColor ("client");
	cout << "s_d_r: request size: " << sizeof(request->push_payload.start_storage) << " bytes." << endl;
	yellowColor ("client");
	cout << "s_d_r: request type: " << (int) request->hdr.type << endl;

	socket_udp.async_send_to (buffer (request, sizeof(request->hdr) + request->hdr.payload_length), endpoint,
				  boost::bind (&client::write_start_storage_request, this, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred, request, endpoint));

	read_request ();
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
client::write_start_storage_request (const boost::system::error_code& err, std::size_t n, struct push_protocol_packet *request, udp::endpoint ds_endpoint)
{
  if (!err) {
    /*get lock*/
    encodings_mutex.lock ();

    delay.wait ();

    map<uint32_t, int>::iterator encoding_iter = encodings.find (request->push_payload.start_storage.hash_code);
    if (encoding_iter != encodings.end ()) {

      greenColor ("client");
      cout << "write_start_storage_request()" << endl;
      yellowColor ("client");
      cout << ": ";
      greenColor ("File Found!");
      cout << endl;
      encodings_mutex.unlock ();

    } else {

      redColor ("client");
      cout << ": NOT FOUND" << endl;
      socket_udp.async_send_to (buffer (request, sizeof(request->hdr) + request->hdr.payload_length), ds_endpoint,
				boost::bind (&client::write_start_storage_request, this, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred, request, ds_endpoint));
      encodings_mutex.unlock ();
    }
  } else {
    redColor ("client");
    cout << ": write_start_storage_request: " << err.message () << endl;
  }

}

void
client::start_storage_request_written (const boost::system::error_code&, std::size_t, struct push_protocol_packet *request)
{
  greenColor ("client");
  cout << "::start_storage_request_written() successfully" << endl;

//  if()
//
//
//  struct push_protocol_packet *response = (struct push_protocol_packet *) malloc (sizeof(struct push_protocol_packet));
//
//  udp::endpoint sender_endpoint_
//
//  socket_udp.async_receive_from (boost::asio::buffer (response, sizeof(struct push_protocol_packet)), sender_endpoint_,
//  				  boost::bind (&client::handle_request, this, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred, request, sender_endpoint_));

}

void
client::read_request ()
{

  greenColor ("client");
  cout << "read_request()" << endl;

  struct push_protocol_packet *request = (struct push_protocol_packet *) malloc (sizeof(struct push_protocol_packet));

  socket_udp.async_receive_from (boost::asio::buffer (request, sizeof(struct push_protocol_packet)), sender_endpoint_,
				 boost::bind (&client::handle_request, this, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred, request));

  //read_request();
}

void
client::handle_request (const boost::system::error_code& err, std::size_t n, struct push_protocol_packet *request)
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

	encodings.insert (make_pair (request->push_payload.start_storage.hash_code, 1));

	struct push_protocol_packet *response = (struct push_protocol_packet *) malloc (sizeof(struct push_protocol_packet));

	response->hdr.payload_length = sizeof(request->push_payload.start__storage_ok);
	response->hdr.type = START_STORAGE_OK;
	response->push_payload.start__storage_ok.hash_code = request->push_payload.start_storage.hash_code;

	break;
      }

      default:
	redColor ("client");
	cout << ": fatal - unknown request" << endl;
	free (request);
	break;
    }
  } else {
    redColor ("client");
    cout << ": handle_request: " << err.message () << endl;
  }
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
