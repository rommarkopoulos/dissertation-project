#include "data_server.h"

data_server::data_server (string bind_address, uint16_t bind_port, string mds_address, uint16_t mds_port, size_t pool_size_) :
    pool_size_ (pool_size_), signals_ (io_service_), acceptor_ (io_service_), work_ptr_ (new io_service::work (io_service_)), local_endpoint_ (ip_v4::from_string (bind_address), bind_port),
    local_udppoint_ (ip_v4::from_string (bind_address), bind_port), mds_endpoint_ (ip_v4::from_string (mds_address), mds_port),
    udp_socket_(io_service_, local_udppoint_), tcp_socket_(io_service_)
{
  cout << "data_server: constructor()" << endl;
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


  tcp_socket_.connect(mds_endpoint_, err);
  if(err){
    cout << "data-server: " << err.message () << endl;
    signals_.cancel ();
    work_ptr_.reset ();
  } else {

    struct protocol_packet *request = (struct protocol_packet *) malloc (sizeof(struct protocol_packet));

      request->hdr.payload_length = sizeof(request->payload.registration_req);
      request->hdr.type = REGISTRATION_REQ;

      request->payload.registration_req.addr = local_endpoint_.address ().to_v4 ().to_ulong ();
      request->payload.registration_req.port = local_endpoint_.port ();

      async_write (tcp_socket_, buffer (request, sizeof(request->hdr) + request->hdr.payload_length),
      	       bind (&data_server::registration_request_written, this, placeholders::error, placeholders::bytes_transferred, request));

  }

//  /* create a client session with the metadata-server */
//  client_session_ptr_ = client_session_ptr (new client_session (io_service_, this));
//
//  /* this is synchronous to avoid several complications*/
//  client_session_ptr_->connect (mds_endpoint_, err);
//  if (err) {
//    /* cannot continue here..could not register with metadata-server */
//    cout << "data-server: " << err.message () << endl;
//    //acceptor_.close ();
//    signals_.cancel ();
//    work_ptr_.reset ();
//  } else {
//    /* I use my own callback here _1 and _2 are like the boost::placeholders stuff */
//    client_session_ptr_->register_data_server (bind (&data_server::registered, this, _1, _2));
//  }


}

//void
//data_server::registered (client_session_ptr client_session_ptr_, const system::error_code& err)
//{
//  if (!err) {
//    /* successful */
//    cout << "data_server: registered with metadata server (remote endpoint " << client_session_ptr_->socket_.remote_endpoint () << ")" << endl;
//  } else {
//    /* errors should be handled in such callbacks in general */
//  }
//
//  /* close client session with metadata server */
//  client_session_ptr_->socket_.close ();
//}

void
data_server::registration_request_written (const system::error_code& err, size_t n, struct protocol_packet *request)
{
  if (!err) {
      cout << "client_session: successfully written registration request(" << n << " bytes)" << endl;
      /* ask to read the response */
      struct protocol_packet *response = (struct protocol_packet *) malloc (sizeof(struct protocol_packet));
      async_read (tcp_socket_, buffer (&response->hdr, sizeof(response->hdr)), bind (&data_server::handle_header, this, placeholders::error, placeholders::bytes_transferred, response));
    } else {
      cout << "client_session: error when writing registration request: " << err.message () << endl;
    }
    free (request);
}

void
data_server::handle_header(const system::error_code& err, size_t n, protocol_packet *request)
{
  struct header *hdr = &request->hdr;

   if (!err) {
     cout << "client_session: successfully read header(" << n << " bytes)" << endl;

     cout << "client_session: current thread: " << this_thread::get_id << endl;
     cout << "client_session: payload length: " << hdr->payload_length << endl;
     cout << "client_session: request type: " << (int) hdr->type << endl;

     switch (hdr->type)
     {
       case REGISTRATION_RESP:
       {
 	cout << "server_session: registration response" << endl;
 	async_read (tcp_socket_, buffer (&request->payload.registration_resp, sizeof(struct registration_response)),
 		    bind (&data_server::handle_registration_response, this, placeholders::error, placeholders::bytes_transferred, request));
 	break;
       }
       default:
 	cout << "server_session: fatal - unknown request" << endl;
 	free (request);
 	break;
     }
   } else {
     cout << "client_session: error when reading header" << err.message () << "\n";
   }
}

void
data_server::handle_registration_response(const system::error_code& err, size_t n, protocol_packet *request)
{

  if(!err)
  {
    struct registration_response *registration_response = &request->payload.registration_resp;
    if (registration_response->response == OK) {
      cout << "SO FAR SO GOOD" << endl;

      struct test *request = (struct test *) malloc (sizeof(struct test));

      udp::endpoint sender_endpoint_;

      udp_socket_.async_receive_from (boost::asio::buffer (&request, sizeof(request)), sender_endpoint_,
			      bind (&data_server::handle_request, this, placeholders::error, placeholders::bytes_transferred));

      cout << "here" << endl;

    } else {

      cout << "client_session: error - response is not OK" << endl;
    }
  } else {
    cout << "client_session: error when handling response" << err.message () << "\n";
  }

  free (request);
}

void
data_server::handle_request(const system::error_code& err, size_t n)
{
  cout << "data_server_session:handle_request" << endl;
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

//void
//data_server::start_udp_server ()
//{
//    cout << "udp_server:start" << endl;
//    read_request ();
//}

//void
//data_server::read_request ()
//{
//
//  cout << "server_session::read_request()" << endl;
//  //struct push_protocol_packet *request = (struct push_protocol_packet *) malloc (sizeof(struct push_protocol_packet));
//
//  struct test *request = (struct test *) malloc (sizeof(struct test));
//
//  udp::endpoint sender_endpoint_;
//
//  udp_socket_.async_receive_from (boost::asio::buffer (&request, sizeof(request)), sender_endpoint_,
//			      bind (&data_server::handle_request, this, placeholders::error, placeholders::bytes_transferred));
//
//  cout << "server_session::read_request() | started" << endl;
//  cout << "UDP LOCAL ADDRESS: " << udp_socket_.local_endpoint().address().to_string() << ":" << udp_socket_.local_endpoint().port() << endl;
//}

//void
//data_server::handle_request(const system::error_code& err, size_t n)
//{
//  cout << "we got there" << endl;
//}

data_server::~data_server (void)
{
  cout << "data_server: destructor" << endl;
}
