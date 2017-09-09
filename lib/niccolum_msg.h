/*
 * (C) Copyright 2017 Tony Mason
 * All Rights Reserved
 */

 // These are the messages used by niccolum for shared memory.  These should probably be replaced at some
 // point with something more efficient.
 //

 //
 // we need to know what kind of message is being sent.
 //
 typedef enum {
    NICCOLUM_NAME_MAP_REQUEST = 61, // map the name to a usable identifier
    NICCOLUM_NAME_MAP_RESPONSE, // respond to the name map request
    NICCOLUM_FUSE_OP_REQUEST,   // FUSE request
	NICCOLUM_FUSE_OP_RESPONSE,  // FUSE response
	NICCOLUM_FUSE_NOTIFY, // FUSE notify
 } niccolum_message_type_t;

 //
 // This is the generic niccolum key (identifier)
 //
 typedef struct niccolum_key {
     u_int8_t KeyLength;
     unsigned char Key[1];
 } niccolum_key_t;

 //
 // This is the generic niccolum message
 //
 typedef struct niccolum_message {
    niccolum_message_type_t MessageType;
    u_int64_t MessageId;
    u_int32_t MessageLength;
    unsigned char Message[1];
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
     niccolum_key_t Key;
 } niccolum_name_map_response_t;


 struct niccolum_uuid_t {
	uint32_t data1;
	uint16_t data2;
	uint16_t data3;
	u_char data4[8];
};
typedef struct niccolum_uuid_t niccolum_uuid_t;

typedef niccolum_uuid_t niccolum_buf_t;

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
