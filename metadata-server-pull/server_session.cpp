#include "server_session.h"

server_session::server_session (io_service& io_service, metadata_server *mds) :
    socket_ (io_service), mds (mds)
{
  cout << "server_session: constructor()" << endl;
}

void
server_session::start ()
{
  read_header ();
}

void
server_session::read_header ()
{
  struct protocol_packet *request = (struct protocol_packet *) malloc (sizeof(struct protocol_packet));

  async_read (socket_, buffer (&request->hdr, sizeof(request->hdr)), bind (&server_session::handle_header, this, placeholders::error, placeholders::bytes_transferred, request));
}

void
server_session::handle_header (const system::error_code& err, size_t n, protocol_packet *request)
{
  struct header *hdr = &request->hdr;
  if (!err) {
    cout << "server_session: successfully read header (" << n << " bytes)" << endl;

    cout << "server_session: current thread: " << this_thread::get_id << endl;
    cout << "server_session: payload length: " << hdr->payload_length << endl;
    cout << "server_session: request type: " << (int) hdr->type << endl;

    switch (hdr->type)
    {
      case REGISTRATION_REQ:
      {
	cout << "server_session: register request" << endl;
	async_read (socket_, buffer (&request->payload.registration_req, sizeof(struct registration_request)),
		    bind (&server_session::handle_registration_request, this, placeholders::error, placeholders::bytes_transferred, request));
	break;
      }
      case STORAGE_RESOLUTION_REQ:
      {
	cout << "server_session: storage resolution request" << endl;
	async_read (socket_, buffer (&request->payload.storage_resolution_req, sizeof(struct storage_resolution_request)),
		    bind (&server_session::handle_storage_resolution_request, this, placeholders::error, placeholders::bytes_transferred, request));
	break;
      }
      case FETCH_RESOLUTION_REQ:
      {
	cout << "server_session: fetch resolution request" << endl;
	async_read (socket_, buffer (&request->payload.fetch_resolution_req, sizeof(struct fetch_resolution_request)),
		    bind (&server_session::handle_fetch_resolution_request, this, placeholders::error, placeholders::bytes_transferred, request));
	break;
      }
      default:
	cout << "server_session: fatal - unknown request" << endl;
	free (request);
	break;
    }

  } else {
    free (request);
    cout << "remote endpoint " << socket_.remote_endpoint () << " disconnected.." << "\n";
  }
}

void
server_session::handle_registration_request (const system::error_code& err, size_t n, struct protocol_packet *request)
{
  struct registration_request *registration_req = &request->payload.registration_req;
  if (!err) {
    cout << "server_session: successfully read registration request payload (" << n << " bytes)" << endl;
    udp::endpoint endpoint_ (ip::address_v4 (registration_req->addr), registration_req->port);

    cout << "data-server IP address: " << endpoint_.address ().to_string () << endl;
    cout << "data-server port: " << endpoint_.port () << endl;

    mds->server_addresses.push_back (endpoint_);
    write_registration_response ();
  } else {
    cout << "remote endpoint " << socket_.remote_endpoint () << " disconnected.." << "\n";
  }
  free (request);
}

void
server_session::write_registration_response ()
{

  struct protocol_packet *response = (struct protocol_packet *) malloc (sizeof(struct protocol_packet));

  response->hdr.payload_length = sizeof(response->payload.registration_resp);
  response->hdr.type = REGISTRATION_RESP;
  response->payload.registration_resp.response = OK;

  async_write (socket_, buffer (response, sizeof(response->hdr) + response->hdr.payload_length),
	       bind (&server_session::registration_response_written, this, placeholders::error, placeholders::bytes_transferred, response));
}

void
server_session::registration_response_written (const system::error_code& err, size_t n, struct protocol_packet *response)
{
  if (!err) {
    /* all done - ask to read a new header */
    cout << "server_session: successfully wrote registration response (" << n << " bytes)" << endl;
    read_header ();
  } else {
    cout << "server_session: error occurred when writing registration response: " << err.message () << endl;
  }
  free (response);
}

void
server_session::handle_storage_resolution_request (const system::error_code& err, size_t n, struct protocol_packet *request)
{
  struct storage_resolution_request *storage_resolution_req = &request->payload.storage_resolution_req;
  if (!err) {
    cout << "server_session: successfully read storage resolution request payload (" << n << " bytes)" << endl;

    cout << "server_session: replication factor: " << (int) storage_resolution_req->replication_factor << endl;
    cout << "server_session: hash code: " << storage_resolution_req->hash_code << endl;

    vector<udp::endpoint> replication_addresses;

    random_replication (storage_resolution_req->hash_code, storage_resolution_req->replication_factor, replication_addresses);

    if (replication_addresses.size () > 0) {
      mds->filenames.insert (make_pair (storage_resolution_req->hash_code, replication_addresses));
      cout << "Number of file-to-address mappings: " << mds->filenames.size () << endl;
    }

    write_resolution_response (replication_addresses);
  } else {
    cout << "remote endpoint " << socket_.remote_endpoint () << " disconnected.." << "\n";
  }
  free (request);
}

void
server_session::handle_fetch_resolution_request (const system::error_code& err, size_t n, struct protocol_packet *request)
{
  struct fetch_resolution_request *fetch_resolution_req = &request->payload.fetch_resolution_req;

  if (!err) {
    cout << "server_session: successfully read fetch resolution request payload (" << n << " bytes)" << endl;

    cout << "server_session: hash code: " << fetch_resolution_req->hash_code << endl;

    /* find the replication addresses - could be empty for non-stored data */
    map<uint32_t, vector<udp::endpoint> >::iterator iter = mds->filenames.find (fetch_resolution_req->hash_code);

    vector<udp::endpoint> replication_addresses;
    if (iter != mds->filenames.end ()) {
      replication_addresses = iter->second;
    } else {
      /*leave it empty */
    }

    write_resolution_response (replication_addresses);

  } else {
    cout << "remote endpoint " << socket_.remote_endpoint () << " disconnected.." << "\n";
  }
  free (request);
}

void
server_session::write_resolution_response (vector<udp::endpoint> &endpoints)
{
  struct protocol_packet *response = (struct protocol_packet *) malloc (sizeof(struct protocol_packet));

  uint32_t addr;
  uint16_t port;

  u_int32_t replicas_data_size = endpoints.size () * (sizeof(uint32_t) + sizeof(uint16_t));
  u_int8_t *replicas_data = (u_int8_t *) malloc (replicas_data_size);

  response->hdr.payload_length = endpoints.size () * (sizeof(uint32_t) + sizeof(uint16_t));
  response->hdr.type = RESOLUTION_RESP;

  for (int i = 0; i < endpoints.size (); i++) {
    udp::endpoint temp_endpoint = endpoints[i];

    addr = temp_endpoint.address ().to_v4 ().to_ulong ();
    port = temp_endpoint.port ();

    memcpy (replicas_data + (i * (sizeof(uint32_t) + sizeof(uint16_t))), &addr, sizeof(uint32_t));
    memcpy (replicas_data + (i * (sizeof(uint32_t) + sizeof(uint16_t)) + sizeof(uint32_t)), &port, sizeof(uint16_t));
  }

  array<mutable_buffer, 2> buffer_array =
  { buffer (response, sizeof(response->hdr)), buffer (replicas_data, replicas_data_size) };

  async_write (socket_, buffer_array, bind (&server_session::resolution_response_written, this, placeholders::error, placeholders::bytes_transferred, response, replicas_data));
}

void
server_session::resolution_response_written (const system::error_code& err, size_t n, struct protocol_packet *response, u_int8_t *replicas_data)
{
  if (!err) {
    cout << "server_session: successfully wrote resolution response (" << n << " bytes)" << endl;
    read_header ();
  } else {
    cout << "server_session: error occurred when writing resolution response: " << err.message () << endl;
  }
  free (replicas_data);
  free (response);
}

/* this function generates randomly a vector with the addresses of the dataservers that data will be replicated */
/* TODO: it crashes when no data servers are registered */
void
server_session::random_replication (uint32_t hash_code, u_int8_t replicas, vector<udp::endpoint> &endpoints)
{
  vector<udp::endpoint> temp_address_vector;

  if (mds->server_addresses.size () > 0) {
    for (int i = 0; i < mds->server_addresses.size (); i++) {
      udp::endpoint temp_endpoint = mds->server_addresses.at (i);
      temp_address_vector.push_back (temp_endpoint);
    }

    while (replicas != 0) {
      int randomIndex = rand () % temp_address_vector.size ();
      udp::endpoint temp_endpoint = temp_address_vector.at (randomIndex);
      endpoints.push_back (temp_endpoint);
      temp_address_vector.erase (temp_address_vector.begin () + randomIndex);
      replicas--;
    }

    cout << "-----------REPLICATIONS SERVERS-----------" << endl;
    for (int i = 0; i < endpoints.size (); i++) {
      // TODO: this should be a method in the metadata_server class (e.g. callback using bind) - concurrency control is required
      udp::endpoint temp_endpoint = endpoints[i];
      cout << "IP address: " << temp_endpoint.address ().to_string () << endl;
      cout << "Port:       " << temp_endpoint.port () << endl;
    }
    cout << "------------------------------------------" << endl;
  }
}

server_session::~server_session (void)
{
  cout << "server_session: destructor" << endl;
}
