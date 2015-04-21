//#include "server_session.h"
//
//server_session::server_session (io_service& io_service, data_server *ds_) :
//    socket_ (io_service), ds_ (ds_)
//{
//	cout << "server_session: constructor()" << endl;
//}
//
//void
//server_session::start ()
//{
//	read_header ();
//}
//
//void
//server_session::read_header ()
//{
//
//	struct protocol_packet *request = (struct protocol_packet *) malloc (sizeof(struct protocol_packet));
//	async_read (socket_, buffer (&request->hdr, sizeof(request->hdr)),
//	      bind (&server_session::handle_header, this, placeholders::error, placeholders::bytes_transferred, request));
//}
//
//void
//server_session::handle_header (const system::error_code& err, size_t n, protocol_packet *request)
//{
//	struct header *hdr = &request->hdr;
//
//	if (!err) {
//		cout << "server_session: successfully read header(" << n << " bytes)" << endl;
//
//		cout << "server_session: current thread: " << this_thread::get_id << endl;
//		cout << "server_session: payload length: " << hdr->payload_length << endl;
//		cout << "server_session: request type: " << (int) hdr->type << endl;
//
//		switch (hdr->type)
//		{
//			case STORAGE_REQ:
//			{
//				/* subtract size of the hash code from the payload length to get the data length */
//				uint32_t data_length = hdr->payload_length - sizeof(struct storage_request);
//
//				/* allocate the data that will be stored */
//				char *data = (char *) malloc (data_length);
//
//				/* need to create array of buffers for scatter/gather I/O */
//				array<mutable_buffer, 2> buffer_array =
//				{
//					/* the hash code */
//					buffer (&request->payload.storage_req, sizeof(struct storage_request)),
//					/* the data */
//					buffer (data, data_length)
//				};
//
//				cout << "server_session: storage request" << endl;
//
//				async_read (socket_, buffer_array,
//						bind (&server_session::handle_storage_request, this, placeholders::error, placeholders::bytes_transferred, request, data, hdr->payload_length - sizeof(struct storage_request)));
//				break;
//			}
//			case FETCH_REQ:
//			{
//				cout << "server_session: fetch request" << endl;
//				async_read (socket_, buffer (&request->payload.fetch_req, sizeof(struct fetch_request)),
//						bind (&server_session::handle_fetch_request, this, placeholders::error, placeholders::bytes_transferred, request));
//				break;
//			}
//			default:
//				cout << "server_session: fatal - unknown request" << endl;
//				free(request);
//				break;
//			}
//	} else {
//		cout << "remote endpoint " << socket_.remote_endpoint() << " disconnected.." << "\n";
//	}
//}
//
//void
//server_session::handle_storage_request (const system::error_code& err, size_t n, struct protocol_packet *request, char *data, uint32_t data_length)
//{
//	struct stored_data str_data;
//	struct storage_request *storage_req = &request->payload.storage_req;
//
//	if (!err) {
//		cout << "server_session: successfully read storage request payload(" << n << " bytes)" << endl;
//
//		/* store the data */
//		str_data.data = data;
//		str_data.data_length = data_length;
//
//
//		cout << "server_session: data: " << str_data.data << endl;
//		/* TODO: concurrency control*/
//		ds_->storage.insert (pair<uint32_t, stored_data> (storage_req->hash_code, str_data));
//
//		cout << "server_session: successfully store in storage ( size " << ds_->storage.size() << " )" << endl;
//
//		cout << "server_session: filename's hash_code: " << storage_req->hash_code << endl;
//
//		write_storage_response (storage_req->hash_code);
//
//		free (request);
//
//	} else {
//		/* free the data allocated before - I won't store anything */
//		free(data);
//		cout << "remote endpoint " << socket_.remote_endpoint() << " disconnected.." << "\n";
//	}
//}
//
//void
//server_session::write_storage_response (uint32_t hash_code)
//{
//	struct protocol_packet *response = (struct protocol_packet *) malloc (sizeof(struct protocol_packet));
//
//	response->hdr.payload_length = sizeof(response->payload.storage_resp);
//	response->hdr.type = STORAGE_RESP;
//
//	response->payload.storage_resp.hash_code = hash_code;
//	response->payload.storage_resp.response = OK;
//
//	async_write (socket_, buffer (response, sizeof(response->hdr) + sizeof(response->payload.storage_resp)),
//			bind (&server_session::storage_response_written, this, placeholders::error, placeholders::bytes_transferred, response));
//}
//
//void
//server_session::storage_response_written (const system::error_code& err, size_t n, struct protocol_packet *response)
//{
//	if (!err) {
//		cout << "server_session: successfully written storage response(" << n << " bytes)" << endl;
//		/* all done - ask to read a new header */
//		read_header ();
//	} else {
//		cout << "server_session: error when writing storage response: " << err.message () << endl;
//	}
//
//	free (response);
//}
//
//void
//server_session::handle_fetch_request (const system::error_code& err, size_t n, struct protocol_packet *request)
//{
//	char *data;
//	uint32_t data_length;
//
//	if (!err) {
//		cout << "server_session: successfully read fetch request payload(" << n << " bytes)" << endl;
//
//
//		struct fetch_request *fetch_req = &request->payload.fetch_req;
//		map<uint32_t, stored_data>::iterator iter = ds_->storage.find (fetch_req->hash_code);
//		if (iter != ds_->storage.end ()) {
//			/* found data */
//			data = iter->second.data;
//			data_length = iter->second.data_length;
//		} else {
//			/* no data with this identifier */
//			data = NULL;
//			data_length = 0;
//		}
//		write_fetch_response (fetch_req->hash_code, data, data_length);
//
//		free (request);
//
//	} else {
//		cout << "remote endpoint " << socket_.remote_endpoint() << " disconnected.." << "\n";
//	}
//}
//
//void
//server_session::write_fetch_response (uint32_t hash_code, char *data, uint32_t data_length)
//{
//
//	struct protocol_packet *response = (struct protocol_packet *) malloc (sizeof(struct protocol_packet));
//
//	response->hdr.payload_length = sizeof(response->payload.fetch_resp) + data_length;
//	response->hdr.type = FETCH_RESP;
//
//	response->payload.fetch_resp.hash_code = hash_code;
//
//	/* need to create array of buffers for scatter/gather I/O */
//	array<mutable_buffer, 2> buffer_array =
//	{
//			buffer (response, sizeof(response->hdr) + sizeof(response->payload.fetch_resp)),
//			buffer (data, data_length)
//	};
//
//	async_write (socket_, buffer_array,
//	       bind (&server_session::fetch_response_written, this, placeholders::error, placeholders::bytes_transferred, response));
//}
//
//void
//server_session::fetch_response_written (const system::error_code& err, size_t n, struct protocol_packet *response)
//{
//	if (!err) {
//		cout << "server_session: successfully written fetch response(" << n << " bytes)" << endl;
//		/* all done - ask to read a new header */
//		read_header ();
//	} else {
//		cout << "server_session: error when writing fetch response: " << err.message () << endl;
//	}
//	free (response);
//}
//
//server_session::~server_session (void)
//{
//	cout << "server_session: destructor" << endl;
//}
