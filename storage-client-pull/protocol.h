#ifndef PROTOCOL_H_
#define PROTOCOL_H_

#define STORAGE_RESOLUTION_REQ 	0x02
#define FETCH_RESOLUTION_REQ 	0x03
#define RESOLUTION_RESP 	0x04

#define START_STORAGE		0x05
#define START_STORAGE_OK	0x06

#define START_FETCH 		0x07
#define START_FETCH_OK		0x08

#define SYMBOL_DATA		0x09
#define SEND_NEXT		0x15

#define	STOP_STORAGE		0x10
#define STOP_STORAGE_OK		0x11

#define STOP_FETCH		0x12
#define STOP_FETCH_OK		0x13

#define OK 			0x20

#define BLOB_ID_SIZE 32
#define BLOB_SIZE 1024 * 10
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

struct __attribute__((__packed__)) start_fetch
{
  uint32_t hash_code;
  //char padding[1032];
};

struct __attribute__((__packed__)) start_fetch_ok
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

struct __attribute__((__packed__)) send_next
{
  uint32_t hash_code;
  //char padding[1032];
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

struct __attribute__((__packed__)) stop_fetch
{
  uint32_t hash_code;
  //char padding[1032];
};

struct __attribute__((__packed__)) stop_fetch_ok
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

  } payload;
};

struct __attribute__((__packed__)) push_protocol_packet
{
  struct push_header hdr;

  union
  {
    struct start_storage start_storage;
    struct start_storage_ok start_storage_ok;
    struct start_fetch start_fetch;
    struct start_fetch_ok start_fetch_ok;
    struct symbol_data symbol_data;
    struct send_next send_next;
    struct stop_storage stop_storage;
    struct stop_storage_ok stop_storage_ok;
    struct stop_fetch stop_fetch;
    struct stop_fetch_ok stop_fetch_ok;
  } push_payload;
};

#endif

