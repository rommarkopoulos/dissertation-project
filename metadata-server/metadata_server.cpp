#include "metadata_server.h"

metadata_server::metadata_server (string ip_address, uint32_t port, size_t pool_size) :
    pool_size_ (pool_size), signals_ (io_service_), acceptor_ (io_service_), work_ptr_ (new io_service::work (io_service_)), local_endpoint_ (ip_v4::from_string (ip_address), port)
{
  cout << "metadata_server: constructor()" << endl;
}

void
metadata_server::service_thread (io_service &service)
{
  service.run ();
}

void
metadata_server::init ()
{
  cout << "metadata_server: init()" << endl;
  /* start threads */
  for (size_t i = 0; i < pool_size_; i++) {
    thread_grp_.create_thread (bind (&metadata_server::service_thread, this, ref (io_service_)));
  }

  signals_.add (SIGINT);
  signals_.add (SIGTERM);
#if defined(SIGQUIT)
  signals_.add (SIGQUIT);
#endif

  signals_.async_wait (bind (&metadata_server::handle_stop, this));

  acceptor_.open (local_endpoint_.protocol ());
  acceptor_.set_option (tcp::acceptor::reuse_address (true));
  acceptor_.bind (local_endpoint_);
  acceptor_.listen ();

  new_server_session_ptr = server_session_ptr (new server_session (io_service_, this));
  server_sessions.push_back (new_server_session_ptr);
  acceptor_.async_accept (new_server_session_ptr->socket_, bind (&metadata_server::handle_accept, this, placeholders::error));
}

void
metadata_server::join ()
{
  cout << "metadata_server: join()" << endl;
  thread_grp_.join_all ();
  io_service_.stop ();
}

void
metadata_server::handle_stop ()
{
  cout << "metadata_server: handle_stop()" << endl;
  work_ptr_.reset ();
  io_service_.stop ();
}

void
metadata_server::handle_accept (const system::error_code& error)
{
  if (!error) {
    new_server_session_ptr->start ();
    cout << "metadata-server: client connected" << endl;
  }

  new_server_session_ptr = server_session_ptr (new server_session (io_service_, this));
  server_sessions.push_back (new_server_session_ptr);
  acceptor_.async_accept (new_server_session_ptr->socket_, bind (&metadata_server::handle_accept, this, placeholders::error));
}

metadata_server::~metadata_server (void)
{
  cout << "metadata_server: destructor" << endl;
}
