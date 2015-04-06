#include "client.h"

client::client (string mds_address, uint16_t mds_port, size_t pool_size_) :
    pool_size_ (pool_size_), signals_ (io_service_), work_ptr_ (new io_service::work (io_service_)), mds_endpoint_ (ip_v4::from_string (mds_address), mds_port)
{
  cout << "client: constructor()" << endl;
}

void
client::service_thread (io_service &service)
{
  try {
    service.run ();
  } catch (std::exception& e) {
    cout << "ioservice exception was caught..not sure this is expected - re-running ioservice" << endl;
    service.run ();
  }
}

void
client::init ()
{
  system::error_code err;
  cout << "client: init()" << endl;

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
    cout << "client: successfully connected to the meta-data server" << endl;
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

  /* close the socket to all data servers and reset the respective shared_ptr */
  map<tcp::endpoint, client_session_ptr>::iterator sessions_iter;
  for (sessions_iter = data_sessions.begin (); sessions_iter != data_sessions.end (); sessions_iter++) {
    client_session_ptr client_session_ptr_ = sessions_iter->second;
    client_session_ptr_->socket_.close ();
    client_session_ptr_.reset ();
  }
  data_sessions.clear ();
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

  cout << "client: store_data - filename: " << name << ", replication factor: " << (int) replicas << ", data length: " << length << endl;

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
client::storage_dataservers_resolved (const system::error_code& err, vector<tcp::endpoint> &endpoints, u_int32_t hash_code, char* data, uint32_t length, storage_callback storage_cb)
{
  if (!err) {
    cout << "client: storage_dataservers_resolved" << endl;
    if (endpoints.size () > 0) {
      /* for each endpoint, send a storage request */
      vector<tcp::endpoint>::iterator endpoints_iter;
      shared_ptr<u_int8_t> replicas_ptr (new u_int8_t (endpoints.size ()));
      cout << "initial value: " << (u_int32_t) (*replicas_ptr) << endl;
      for (endpoints_iter = endpoints.begin (); endpoints_iter != endpoints.end (); endpoints_iter++) {
	client_session_ptr data_session;
	tcp::endpoint &endpoint = *endpoints_iter;
	map<tcp::endpoint, client_session_ptr>::iterator data_sessions_iterator;

	/* get lock */
	data_sessions_mutex.lock ();

	/* lookup for existing data session */
	data_sessions_iterator = data_sessions.find (endpoint);
	if (data_sessions_iterator == data_sessions.end ()) {

	  /* connection to the data server does NOT exist - connect, add, unlock */
	  system::error_code connect_err;

	  /* create a client session with the metadata-server */
	  data_session = client_session_ptr (new client_session (io_service_, this));

	  /* connect to the data server */
	  data_session->connect (endpoint, connect_err);

	  if (!connect_err) {
	    cout << "client: connected to data server " << endpoint << endl;

	    /* add to the map of data sessions */
	    data_sessions.insert (make_pair (endpoint, data_session));

	    /* unlock mutex */
	    data_sessions_mutex.unlock ();
	  } else {
	    /* cannot continue here..could not connect to the datadata-server */
	    data_sessions_mutex.unlock ();
	    storage_cb (connect_err, hash_code);
	    return;
	  }
	} else {
	  data_session = data_sessions_iterator->second;
	}
	/* send the storage request to the data server */
	data_session->send_storage_request (hash_code, data, length, bind (&client::storage_request_written, this, _1, replicas_ptr, hash_code, storage_cb));
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
client::fetch_dataservers_resolved (const system::error_code& err, vector<tcp::endpoint> &endpoints, u_int32_t hash_code, fetch_callback fetch_cb)
{
  if (!err) {
    cout << "client: fetch_dataservers_resolved" << endl;

    if (endpoints.size () > 0) {
      client_session_ptr data_session;
      map<tcp::endpoint, client_session_ptr>::iterator data_sessions_iterator;

      /* send fetch request to one of the replicas (the first one for now)*/
      tcp::endpoint &endpoint = endpoints[0];

      /* get lock */
      data_sessions_mutex.lock ();

      /* lookup for existing data session */
      data_sessions_iterator = data_sessions.find (endpoint);
      if (data_sessions_iterator == data_sessions.end ()) {

	/* connection to the data server does NOT exist - connect, add, unlock */
	system::error_code connect_err;

	/* create a client session with the metadata-server */
	data_session = client_session_ptr (new client_session (io_service_, this));

	/* connect to the data server */
	data_session->connect (endpoint, connect_err);

	if (!connect_err) {
	  cout << "client: connected to data server " << endpoint << endl;

	  /* add to the map of data sessions */
	  data_sessions.insert (make_pair (endpoint, data_session));

	  /* unlock mutex */
	  data_sessions_mutex.unlock ();
	} else {
	  /* cannot continue here..could not connect to the datadata-server */
	  data_sessions_mutex.unlock ();
	  uint32_t length = 0;
	  fetch_cb (connect_err, hash_code, NULL, length);
	  return;
	}
      } else {
	data_session = data_sessions_iterator->second;
      }

      data_session->send_fetch_request (hash_code, bind (&client::fetch_request_written, this, _1, _2, _3, hash_code, fetch_cb));

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

//Destructor
client::~client (void)
{
  cout << "client: destructor()" << endl;
}
