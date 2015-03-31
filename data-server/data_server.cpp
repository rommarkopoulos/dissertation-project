#include "data_server.h"

data_server::data_server (string bind_address, uint16_t bind_port, string mds_address, uint16_t mds_port, size_t pool_size_) :
    pool_size_ (pool_size_), signals_ (io_service_), acceptor_ (io_service_), work_ptr_ (new io_service::work (io_service_)), local_endpoint_ (ip_v4::from_string (bind_address), bind_port), mds_endpoint_ (
	ip_v4::from_string (mds_address), mds_port)
{
  cout << "data_server: constructor()" << endl;
}

void
data_server::service_thread (io_service &service)
{
  service.run ();
}

void
data_server::init ()
{
  system::error_code err;
  cout << "data_server: init()" << endl;
  /* start threads */
  for (size_t i = 0; i < pool_size_; i++) {
    thread_grp_.create_thread (bind (&data_server::service_thread, this, ref (io_service_)));
  }

  signals_.add (SIGINT);
  signals_.add (SIGTERM);
#if defined(SIGQUIT)
  signals_.add (SIGQUIT);
#endif

  signals_.async_wait (bind (&data_server::handle_stop, this));

  acceptor_.open (local_endpoint_.protocol ());
  acceptor_.set_option (tcp::acceptor::reuse_address (true));
  acceptor_.bind (local_endpoint_);
  acceptor_.listen ();

  new_server_session_ptr = server_session_ptr (new server_session (io_service_, this));
  server_sessions.push_back (new_server_session_ptr);
  acceptor_.async_accept (new_server_session_ptr->socket_, bind (&data_server::handle_accept, this, placeholders::error));

  /* create a client session with the metadata-server */
  client_session_ptr_ = client_session_ptr (new client_session (mds_endpoint_, io_service_, this));
  client_session_ptr_->register_data_server (err);
  if (err) {
    /* cannot continue here..could not register with metadata-server */
    acceptor_.close ();
    signals_.cancel ();
    work_ptr_.reset ();
  }
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
  work_ptr_.reset ();
  io_service_.stop ();
}

void
data_server::handle_accept (const boost::system::error_code& err)
{
  if (!err) {
    new_server_session_ptr->start ();
    cout << "data-server: client connected" << endl;

    new_server_session_ptr = server_session_ptr (new server_session (io_service_, this));
    server_sessions.push_back (new_server_session_ptr);
    acceptor_.async_accept (new_server_session_ptr->socket_, bind (&data_server::handle_accept, this, placeholders::error));
  } else {
    cout << "data_server: acceptor error: " << err.message () << endl;
  }
}

data_server::~data_server (void)
{
  cout << "data_server: destructor" << endl;
}
