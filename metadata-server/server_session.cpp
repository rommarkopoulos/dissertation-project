#include "server_session.h"

server_session::server_session (io_service& io_service, metadata_server *mds_) :
    socket_ (io_service), mds_ (mds_)
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

  async_read (socket_, buffer (&request->hdr, sizeof(request->hdr)),
	      bind (&server_session::handle_header, this, placeholders::error, placeholders::bytes_transferred, request));
}

void
server_session::handle_header (const system::error_code& err, size_t n, protocol_packet *request)
{
  struct header *hdr = &request->hdr;
  if (!err) {
    cout << "server_session: successfully read header(" << n << " bytes)" << endl;

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
      case RESOLUTION_REQ:
      {
	cout << "server_session: resolution request" << endl;
	async_read (socket_, buffer (&request->payload.resolution_req, sizeof(struct resolution_request)),
		    bind (&server_session::handle_resolution_request, this, placeholders::error, placeholders::bytes_transferred, request));
	break;
      }
      default:
	cout << "server_session: fatal - unknown request" << endl;
	free(request);
	break;
    }

  } else {
    cout << err.message () << "\n";
    // TODO: client disconnected - you need to (safely) remove the server_session from the map in the server
  }
}

void
server_session::handle_registration_request (const system::error_code& err, size_t n, struct protocol_packet *request)
{
  struct registration_request *registraction_req = &request->payload.registration_req;
  if (!err) {
    cout << "server_session: successfully read registration request payload(" << n << " bytes)" << endl;
    tcp::endpoint endpoint_ (ip::address_v4 (registraction_req->addr), registraction_req->port);

    cout << "data-server IP address: " << endpoint_.address ().to_string () << endl;
    cout << "data-server port: " << endpoint_.port () << endl;

    mds_->server_addresses.push_back (endpoint_);
    write_registration_response ();
  } else {
    cout << err.message () << "\n";
    // TODO: client disconnected - you need to (safely) remove the server_session from the map in the server
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
    cout << "server_session: successfully wrote registration response(" << n << " bytes)" << endl;
    read_header ();
  } else {
    cout << "server_session: error occurred when writing registration response: " << err.message () << endl;
    // TODO: client disconnected - you need to (safely) remove the server_session from the map in the server
  }
  free (response);
}

void
server_session::handle_resolution_request (const system::error_code& err, size_t n, struct protocol_packet *request)
{
  struct resolution_request *resolution_req = &request->payload.resolution_req;
  if (!err) {
    cout << "server_session: successfully read resolution request payload(" << n << " bytes)" << endl;

    cout << "server_session: replication factor: " << resolution_req->replication_factor << endl;
    cout << "server_session: hash code: " << resolution_req->hash_code << endl;

    vector<tcp::endpoint> replication_addresses;
    random_replication (resolution_req->hash_code, resolution_req->replication_factor, replication_addresses);
    write_resolution_response (replication_addresses);
  } else {
    cout << err.message () << "\n";
    // TODO: client disconnected - you need to (safely) remove the server_session from the map in the server
  }
  free (request);
}

void
server_session::write_resolution_response (vector<tcp::endpoint> &replication_addresses)
{
  struct protocol_packet *response = (struct protocol_packet *) malloc (sizeof(struct protocol_packet) + (replication_addresses.size () * (sizeof(uint32_t) + sizeof(uint16_t))));

  response->hdr.payload_length = replication_addresses.size () * (sizeof(uint32_t) + sizeof(uint16_t));
  response->hdr.type = RESOLUTION_RESP;

  for (int i = 0; i < replication_addresses.size (); i++) {
    tcp::endpoint temp_endpoint = replication_addresses[i];
    uint32_t addr = temp_endpoint.address ().to_v4 ().to_ulong ();
    uint16_t port = temp_endpoint.port ();
    memcpy (response->payload.resolution_resp.replicas_data + i * (sizeof(uint32_t) + sizeof(uint16_t)), &addr, sizeof(uint32_t));
    memcpy (response->payload.resolution_resp.replicas_data + i * (sizeof(uint32_t) + sizeof(uint16_t)) + sizeof(uint32_t), &port, sizeof(uint16_t));
  }

  async_write (socket_, buffer (response, sizeof(response->hdr) + response->hdr.payload_length),
	       bind (&server_session::resolution_response_written, this, placeholders::error, placeholders::bytes_transferred, response));
}

void
server_session::resolution_response_written (const system::error_code& err, size_t n, struct protocol_packet *response)
{
  if (!err) {
    cout << "server_session: successfully wrote resolution response(" << n << " bytes)" << endl;
    read_header ();
  } else {
    cout << "server_session: error occurred when writing resolution response: " << err.message () << endl;
    // TODO: client disconnected - you need to (safely) remove the server_session from the map in the server
  }
  free (response);
}

//this function generates randomly a vector with the addresses of the dataservers that data will be replicated.
void
server_session::random_replication (uint32_t hash_code, uint32_t rep_num, vector<tcp::endpoint> &replication_addresses)
{
  vector<tcp::endpoint> temp_address_vector;

  for (int i = 0; i < mds_->server_addresses.size (); i++) {
    tcp::endpoint temp_endpoint = mds_->server_addresses.at (i);
    temp_address_vector.push_back (temp_endpoint);
  }

  srand ((unsigned int) time (NULL));

  while (rep_num != 0) {
    int randomIndex = rand () % temp_address_vector.size ();
    tcp::endpoint temp_endpoint = temp_address_vector.at (randomIndex);
    replication_addresses.push_back (temp_endpoint);
    temp_address_vector.erase (temp_address_vector.begin () + randomIndex);
    rep_num--;
  }

  cout << "-----------REPLICATIONS SERVERS-----------" << endl;
  cout << endl;
  cout << "DS-ID		   IP     PORT" << endl;

  for (int i = 0; i < replication_addresses.size (); i++) {
    cout << "DS" << i + 1 << ":		";

    // TODO: this should be a method in the metadata_server class - proper concurrency control is required
    tcp::endpoint temp_endpoint = replication_addresses[i];
    cout << temp_endpoint.address ().to_string () << endl;
    cout << ":" << temp_endpoint.port () << endl;
  }

  cout << "------------------------------------------" << endl;

  mds_->filenames.insert (make_pair (hash_code, replication_addresses));

  cout << "Number of files and addresses mappings: " << mds_->filenames.size ();
}

server_session::~server_session (void)
{
  cout << "server_session: destructor" << endl;
}
