#include "client_session.h"

client_session::client_session (tcp::endpoint &mds_endpoint, io_service& io_service, data_server *ds_) :
    mds_endpoint_ (mds_endpoint), socket_ (io_service), ds_ (ds_)
{
  cout << "tcp_connection: constructor()" << endl;
}

void
client_session::register_data_server (system::error_code &err)
{
  socket_.connect (mds_endpoint_, err);
  cout << "client_session: connecting to meta-data server: " << err.message () << endl;
  if (!err) {
    write_registration_request ();
  }
}

void
client_session::write_registration_request ()
{
  struct protocol_packet *request = (struct protocol_packet *) malloc (sizeof(struct protocol_packet));

  request->hdr.payload_length = sizeof(request->payload.registration_req);
  request->hdr.type = REGISTRATION_REQ;

  request->payload.registration_req.addr = ds_->local_endpoint_.address ().to_v4 ().to_ulong ();
  request->payload.registration_req.port = ds_->local_endpoint_.port ();

  async_write (socket_, buffer (request, sizeof(request->hdr) + request->hdr.payload_length),
	       bind (&client_session::registration_request_written, this, placeholders::error, placeholders::bytes_transferred, request));
}

void
client_session::registration_request_written (const system::error_code& err, size_t n, struct protocol_packet *request)
{
  if (!err) {
    cout << "client_session: successfully written registration request(" << n << " bytes)" << endl;
    /* ask to read the response */
    struct protocol_packet *response = (struct protocol_packet *) malloc (sizeof(struct protocol_packet));
    async_read (socket_, buffer (&response->hdr, sizeof(response->hdr)),
		bind (&client_session::handle_header, this, placeholders::error, placeholders::bytes_transferred, response));
  } else {
    cout << "client_session: error when writing registration request: " << err.message () << endl;
  }
  free (request);
}

void
client_session::handle_header (const system::error_code& err, size_t n, protocol_packet *request)
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
	cout << "server_session: fetch request" << endl;
	async_read (socket_, buffer (&request->payload.registration_resp, sizeof(struct registration_response)),
		    bind (&client_session::handle_registration_response, this, placeholders::error, placeholders::bytes_transferred, request));
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
client_session::handle_registration_response (const system::error_code& err, size_t n, protocol_packet *request) {
  struct registration_response *registration_response = &request->payload.registration_resp;
  if (registration_response->response == OK) {
    cout << "client_session: successfully registered to meta-data server (disconnecting)" << endl;
  } else {
    /* not going to happen! */
  }

  /* closing tcp connection with meta-data server */
  socket_.close ();
  free (request);
}

client_session::~client_session (void)
{
  cout << "tcp_connection: destructor" << endl;
}
