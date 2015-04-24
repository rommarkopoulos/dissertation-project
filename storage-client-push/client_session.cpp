#include "client_session.h"

using namespace boost;
using namespace asio;
using namespace ip;
using namespace std;

client_session::client_session (io_service& io_service, client *client_) :
    socket_ (io_service), strand_ (io_service), client_ (client_)
{
  cout << "client_session: constructor()" << endl;
  is_pending = false;
}

void
client_session::connect (tcp::endpoint remote_endpoint_, system::error_code &err)
{

  socket_.connect (remote_endpoint_, err);
}

/* resolution-related methods */
void
client_session::resolve_dataservers_storage (uint32_t hash_code, u_int8_t replicas, resolution_callback resolution_cb)
{

  is_pending_mutex.lock ();

  if (is_pending == false) {
    cout << "client_session: " << socket_.remote_endpoint () << " ENTER CRITICAL AREA - RESOLUTION" << endl;
    is_pending = true;
    is_pending_mutex.unlock ();
    strand_.post (bind (&client_session::write_storage_resolution_request, this, hash_code, replicas, resolution_cb));
  } else {
    cout << "client_session: " << socket_.remote_endpoint () << " Retrying to ENTER CRITICAL AREA - RESOLUTION" << endl;

    is_pending_mutex.unlock ();
    strand_.post (bind (&client_session::resolve_dataservers_storage, this, hash_code, replicas, resolution_cb));

  }
}

void
client_session::write_storage_resolution_request (uint32_t hash_code, u_int8_t replicas, resolution_callback resolution_cb)
{
  /* create storage request and write it to the socket */
  struct protocol_packet *request = (struct protocol_packet *) malloc (sizeof(struct protocol_packet));

  request->hdr.payload_length = sizeof(request->payload.storage_resolution_req);
  request->hdr.type = STORAGE_RESOLUTION_REQ;

  request->payload.storage_resolution_req.hash_code = hash_code;
  request->payload.storage_resolution_req.replication_factor = replicas;

  async_write (socket_, buffer (request, sizeof(request->hdr) + request->hdr.payload_length),
	       strand_.wrap (bind (&client_session::storage_resolution_request_written, this, placeholders::error, placeholders::bytes_transferred, request, resolution_cb)));
}

void
client_session::storage_resolution_request_written (const system::error_code& err, size_t n, struct protocol_packet *request, resolution_callback resolution_cb)
{
  if (!err) {
    strand_.post (bind (&client_session::read_resolution_response_header, this, resolution_cb));
  } else {
    /* error control through calling resolution callback */
    vector<udp::endpoint> empty_vector;
    resolution_cb (err, empty_vector);
  }
  free (request);
}

void
client_session::resolve_dataservers_fetch (uint32_t hash_code, resolution_callback resolution_cb)
{
  is_pending_mutex.lock ();
  if (is_pending == false) {
    is_pending = true;
    is_pending_mutex.unlock ();
    strand_.post (bind (&client_session::write_fetch_resolution_request, this, hash_code, resolution_cb));
  } else {
    is_pending_mutex.unlock ();
    strand_.post (bind (&client_session::resolve_dataservers_fetch, this, hash_code, resolution_cb));
  }
}

void
client_session::write_fetch_resolution_request (uint32_t hash_code, resolution_callback resolution_cb)
{
  /* create storage request and write it to the socket */
  struct protocol_packet *request = (struct protocol_packet *) malloc (sizeof(struct protocol_packet));

  request->hdr.payload_length = sizeof(request->payload.fetch_resolution_req);
  request->hdr.type = FETCH_RESOLUTION_REQ;

  request->payload.fetch_resolution_req.hash_code = hash_code;

  async_write (socket_, buffer (request, sizeof(request->hdr) + request->hdr.payload_length),
	       strand_.wrap (bind (&client_session::fetch_resolution_request_written, this, placeholders::error, placeholders::bytes_transferred, request, resolution_cb)));
}

void
client_session::fetch_resolution_request_written (const system::error_code& err, size_t n, struct protocol_packet *request, resolution_callback resolution_cb)
{
  if (!err) {
    strand_.post (bind (&client_session::read_resolution_response_header, this, resolution_cb));
  } else {
    /* error control through calling resolution callback */
    vector<udp::endpoint> empty_vector;
    resolution_cb (err, empty_vector);
  }
  free (request);
}

void
client_session::read_resolution_response_header (resolution_callback resolution_cb)
{
  struct protocol_packet *response = (struct protocol_packet *) malloc (sizeof(struct protocol_packet) + 0 /* resolution response is empty - will read endpoints separately */);

  async_read (socket_, buffer (response, sizeof(response->hdr)),
	      strand_.wrap (bind (&client_session::handle_resolution_response_header, this, placeholders::error, placeholders::bytes_transferred, response, resolution_cb)));
}

void
client_session::handle_resolution_response_header (const system::error_code& err, size_t n, struct protocol_packet *response, resolution_callback resolution_cb)
{
  if (!err) {
    /* make sure the type is correct */
    if (response->hdr.type == RESOLUTION_RESP) {
      u_int8_t *replicas_data = (u_int8_t *) malloc (response->hdr.payload_length);
      async_read (socket_, buffer (replicas_data, response->hdr.payload_length),
		  strand_.wrap (bind (&client_session::handle_resolution_payload_response, this, placeholders::error, placeholders::bytes_transferred, response, replicas_data, resolution_cb)));
    } else {
      cout << "/* that's severe - it shouldn't happen*/" << endl;
    }
  } else {
    vector<udp::endpoint> endpoints;
    resolution_cb (err, endpoints);
  }
}

void
client_session::handle_resolution_payload_response (const system::error_code& err, size_t n, struct protocol_packet *response, u_int8_t *replicas_data, resolution_callback resolution_cb)
{
  uint32_t addr;
  uint16_t port;
  vector<udp::endpoint> endpoints;

  if (!err) {
    /* de-serialise replicas to endpoints */
    u_int8_t replicas = response->hdr.payload_length / (sizeof(uint32_t) + sizeof(uint16_t));
    for (int i = 0; i < replicas; i++) {

      memcpy (&addr, replicas_data + i * (sizeof(uint32_t) + sizeof(uint16_t)), sizeof(uint32_t));
      memcpy (&port, replicas_data + i * (sizeof(uint32_t) + sizeof(uint16_t)) + sizeof(uint32_t), sizeof(uint16_t));

      udp::endpoint temp_endpoint (ip::address_v4 (addr), port);
      endpoints.push_back (temp_endpoint);

      is_pending_mutex.lock ();
      is_pending = false;
      cout << "client_session: " << socket_.remote_endpoint () << " LEAVING CRITICAL AREA - RESOLUTION" << endl;
      is_pending_mutex.unlock ();
    }
    resolution_cb (err, endpoints);
  } else {
    resolution_cb (err, endpoints);
  }

  free (response);
  free (replicas_data);
}

/* destructor */
client_session::~client_session (void)
{
  cout << "client_session: destructor" << endl;
}
