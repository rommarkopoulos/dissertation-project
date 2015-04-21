#include "data_server.h"

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


  udp_socket_.async_receive_from (boost::asio::buffer (request, sizeof(struct push_protocol_packet)), sender_endpoint_,
				  boost::bind (&data_server::handle_request, this, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred, request));

}

void
data_server::handle_request (const boost::system::error_code& error, std::size_t bytes_transferred, struct push_protocol_packet *request)
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

	struct push_protocol_packet *response = (struct push_protocol_packet *) malloc (sizeof(struct push_protocol_packet));

	response->hdr.payload_length = sizeof(request->push_payload.start__storage_ok);
	response->hdr.type = START_STORAGE_OK;
	response->push_payload.start__storage_ok.hash_code = request->push_payload.start_storage.hash_code;

	udp_socket_.async_send_to (buffer (response, sizeof(response->hdr) + response->hdr.payload_length), sender_endpoint_,
						  boost::bind (&data_server::nothing, this, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred, response));


	break;
      }
      case SYMBOL:
      {
	greenColor ("data_server");
	cout << ": SYMBOL" << endl;

	break;
      }
      default:
	redColor ("data_server");
	cout << ": fatal - unknown request" << endl;
	free (request);
	break;
    }
  } else {
    redColor ("data_server");
    cout << ": handle_request: " << error.message () << endl;
  }

  read_request ();
}

void
data_server::nothing(const boost::system::error_code& error, std::size_t bytes_transferred, struct push_protocol_packet *request)
{
  greenColor("data_server");
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
