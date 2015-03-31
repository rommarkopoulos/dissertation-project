#ifndef PROTOCOL_H_
#define PROTOCOL_H_

#define REGISTRATION_REQ 	0x00
#define REGISTRATION_RESP 	0x01

#define STORAGE_REQ 		0x02
#define STORAGE_RESP 		0x03

#define FETCH_REQ 		0x04
#define FETCH_RESP 		0x05

#define OK 			0x00

struct __attribute__((__packed__)) header
{
  uint32_t payload_length;
  uint8_t type;
};

struct __attribute__((__packed__)) registration_request
{
  uint32_t addr;		/* IP address in network order*/
  uint16_t port;		/* Port number */
};

struct __attribute__((__packed__)) registration_response
{
  uint8_t response; 	// always OK
};

struct __attribute__((__packed__)) storage_request
{
  uint32_t hash_code;
  /* data will be allocated and read separately */
};

struct __attribute__((__packed__)) storage_response
{
  uint32_t hash_code;	// so that the client can know which data it refers to
  uint8_t response; 	// always OK
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

struct __attribute__((__packed__)) protocol_packet
{
  struct header hdr;

  union
  {
    struct registration_request registration_req;
    struct registration_response registration_resp;
    struct storage_request storage_req;
    struct storage_response storage_resp;
    struct fetch_request fetch_req;
    struct fetch_response fetch_resp;
  } payload;
};

#endif
