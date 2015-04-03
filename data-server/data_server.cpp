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

  acceptor_.open (local_endpoint_.protocol ());
  acceptor_.set_option (tcp::acceptor::reuse_address (true));
  acceptor_.bind (local_endpoint_);
  acceptor_.listen ();

  new_server_session_ptr = server_session_ptr (new server_session (io_service_, this));
  server_sessions.push_back (new_server_session_ptr);
  acceptor_.async_accept (new_server_session_ptr->socket_, bind (&data_server::handle_accept, this, placeholders::error));

  /* create a client session with the metadata-server */
  client_session_ptr_ = client_session_ptr (new client_session (io_service_, this));

  /* this is synchronous to avoid several complications*/
  client_session_ptr_->connect (mds_endpoint_, err);
  if (err) {
    /* cannot continue here..could not register with metadata-server */
    cout << "data-server: " << err.message () << endl;
    acceptor_.close ();
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
data_server::handle_accept (const system::error_code& err)
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
