#ifndef PROTOCOL_H_
#define PROTOCOL_H_

#define REGISTRATION_REQ 	0x00
#define REGISTRATION_RESP 	0x01

#define STORAGE_RESOLUTION_REQ 	0x02
#define FETCH_RESOLUTION_REQ 	0x03
#define RESOLUTION_RESP 	0x04

#define OK 			0x00

struct __attribute__((__packed__)) header
{
  uint32_t payload_length;
  uint8_t type;
};

struct __attribute__((__packed__)) registration_request
{
  uint32_t addr; /* IP address */
  uint16_t port; /* Port number */
};

struct __attribute__((__packed__)) registration_response
{
  uint8_t response; 	// always OK
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

struct __attribute__((__packed__)) protocol_packet
{
  struct header hdr;

  union
  {
    struct registration_request registration_req;
    struct registration_response registration_resp;
    struct storage_resolution_request storage_resolution_req;
    struct fetch_resolution_request fetch_resolution_req;
    struct resolution_response resolution_resp;
  } payload;
};

#endif
