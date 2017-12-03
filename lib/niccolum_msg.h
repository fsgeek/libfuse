/*
 * (C) Copyright 2017 Tony Mason
 * All Rights Reserved
*/

#if !defined(__NICCOLUM_MSG_H__)
#define __NICCOLUM_MSG_H__ (1)

// These are the messages used by niccolum for shared memory.  These should probably be replaced at some
// point with something more efficient.
//

//
// we need to know what kind of message is being sent.
//
typedef enum {
	NICCOLUM_TEST = 42, // a random test value
	NICCOLUM_TEST_RESPONSE,
	
    NICCOLUM_NAME_MAP_REQUEST = 61, // map the name to a usable identifier
    NICCOLUM_NAME_MAP_RESPONSE, // respond to the name map request
	NICCOLUM_MAP_RELEASE_REQUEST, // release a name map (handle)
	NICCOLUM_MAP_RELEASE_RESPONSE, // respond to the release request
	NICCOLUM_DIR_MAP_REQUEST, // request a mapping of the directory contents
	NICCOLUM_DIR_MAP_RESPONSE, // response to dir map request
	NICCOLUM_UNLINK_REQUEST, // request file unlink
	NICCOLUM_UNLINK_RESPONSE, // respond to file unlink request

	// Everything beyond this is TODO
    NICCOLUM_FUSE_OP_REQUEST,
	NICCOLUM_FUSE_OP_RESPONSE,
	NICCOLUM_FUSE_NOTIFY, // FUSE notify
} niccolum_message_type_t;

//
// This is the generic niccolum key (identifier)
//
typedef struct niccolum_key {
     u_int8_t KeyLength;
     unsigned char Key[1];
} niccolum_key_t;

struct niccolum_uuid_t {
	uint32_t data1;
	uint16_t data2;
	uint16_t data3;
	u_char data4[8];
};
typedef struct niccolum_uuid_t niccolum_uuid_t;


typedef struct {
	u_int8_t MessageLength;
	u_char Message[1];
} niccolum_test_message_t;

//
// This is the generic niccolum message
//
#define NICCOLUM_MESSAGE_MAGIC "NICCOLUM"
#define NICCOLUM_MESSAGE_MAGIC_SIZE (8)

typedef struct niccolum_message {
	char MagicNumber[NICCOLUM_MESSAGE_MAGIC_SIZE];
	niccolum_uuid_t SenderUuid;
	niccolum_message_type_t MessageType;
    u_int64_t MessageId;
    u_int32_t MessageLength;
    char Message[1];
} niccolum_message_t;

 
//
// This is the name being requested
//
typedef struct niccolum_name_map_request {
    u_int16_t NameLength;
    unsigned char Name[1];
 } niccolum_name_map_request_t;


//
// This is the name response
//
typedef struct niccolum_name_map_response {
	u_int32_t Status;
	niccolum_key_t Key;
} niccolum_name_map_response_t;

#define NICCOLUM_MAP_RESPONSE_SUCCESS 0
#define NICCOLUM_MAP_RESPONSE_NOTFOUND 2
#define NICCOLUM_MAP_RESPONSE_INVALID 22

typedef niccolum_uuid_t niccolum_buf_t;

typedef struct niccolum_map_release_request {
	niccolum_key_t Key;
} niccolum_map_release_request_t;

typedef struct niccolum_map_release_response {
	u_int32_t Status;
} niccolum_map_release_response_t;

//
// Directory map logic
//
typedef struct niccolum_dir_map_request {
	niccolum_key_t Key;
} niccolum_dir_map_reqeust_t;

typedef struct niccolum_dir_map_response {
	niccolum_uuid_t MapUuid;
} niccolum_dir_map_response_t;

//
// Unlink structures
//
typedef struct niccolum_unlink_requst {
    u_int16_t NameLength;
    char Name[1];
} niccolum_unlink_request_t;

typedef struct niccolum_unlink_response {
	u_int32_t Status;
} niccolum_unlink_response_t;

#if 0
struct fuse_buf {
	/**
	 * Size of data in bytes
	 */
	size_t size;

	/**
	 * Buffer flags
	 */
	enum fuse_buf_flags flags;

	/**
	 * Memory pointer
	 *
	 * Used unless FUSE_BUF_IS_FD flag is set.
	 */
	void *mem;

	/**
	 * File descriptor
	 *
	 * Used if FUSE_BUF_IS_FD flag is set.
	 */
	int fd;

	/**
	 * File position
	 *
	 * Used if FUSE_BUF_FD_SEEK flag is set.
	 */
	off_t pos;
};

#endif // 0

#endif // !defined(__NICCOLUM_MSG_H__)
