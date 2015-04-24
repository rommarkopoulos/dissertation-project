#ifndef PROTOCOL_H_
#define PROTOCOL_H_

#define STORAGE_RESOLUTION_REQ 	0x02
#define FETCH_RESOLUTION_REQ 	0x03
#define RESOLUTION_RESP 	0x04

#define START_STORAGE		0x05
#define START_STORAGE_OK	0x06

#define FETCH_REQ 		0x07
#define FETCH_RESP 		0x08

#define	STOP_STORAGE		0x11
#define STOP_STORAGE_OK		0x12

#define SYMBOL_DATA		0x13

#define OK 			0x00

#define BLOB_ID_SIZE 32
#define BLOB_SIZE 1024 * 1
#define PADDING 8

struct __attribute__((__packed__)) header
{
  uint32_t payload_length;
  uint8_t type;
};

struct __attribute__((__packed__)) storage_resolution_request
{
  uint32_t hash_code;
  uint8_t replication_factor;
};

struct __attribute__((__packed__)) fetch_resolution_request
{
  uint32_t hash_code;
};

struct __attribute__((__packed__)) resolution_response
{
  // will be dynamically allocated later on (scatter/gather)
};


struct __attribute__((__packed__)) fetch_request
{
  uint32_t hash_code;
};

struct __attribute__((__packed__)) fetch_response
{
  uint32_t hash_code;	// so that the client can know which data it refers to
  /* data will be allocated and read separately */
};

struct __attribute__((__packed__)) push_header
{
  uint32_t payload_length;
  uint8_t type;
};

struct __attribute__((__packed__)) start_storage
{
  uint32_t hash_code;
  //char padding[1032];
};

struct __attribute__((__packed__)) start_storage_ok
{
  uint32_t hash_code;
  //char padding[1032];
};

struct __attribute__((__packed__)) symbol_data
{
  uint32_t hash_code;
  uint32_t blob_size;
  uint32_t seed;
  /* symbol data will be allocated and read separately */
};

struct __attribute__((__packed__)) stop_storage
{
  uint32_t hash_code;
  //char padding[1032];
};

struct __attribute__((__packed__)) stop_storage_ok
{
  uint32_t hash_code;
  //char padding[1032];
};

struct __attribute__((__packed__)) protocol_packet
{
  struct header hdr;

  union
  {
    struct storage_resolution_request storage_resolution_req;
    struct fetch_resolution_request fetch_resolution_req;
    struct resolution_response resolution_resp;
    struct fetch_request fetch_req;
    struct fetch_response fetch_resp;
  } payload;
};

struct __attribute__((__packed__)) push_protocol_packet
{
  struct push_header hdr;

  union
  {
    struct start_storage start_storage;
    struct start_storage_ok start_storage_ok;
    struct symbol_data symbol_data;
    struct stop_storage stop_storage;
    struct stop_storage_ok stop_storage_ok;
  } push_payload;
};

#endif

