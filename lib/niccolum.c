/*
  NICCOLUM: Low Energy effcient extension to FUSE
  Copyright (C) 2017  Tony Mason <fsgeek@cs.ubc.ca>

*/

#define _GNU_SOURCE

#include "config.h"
#include "fuse_i.h"
#include "fuse_kernel.h"
#include "fuse_opt.h"
#include "fuse_misc.h"
#include <fuse_lowlevel.h>
#include "niccolum_msg.h"
#include "niccolum_fuse.h"
#include "niccolum-lookup.h"
#include "niccolum-list.h"

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <errno.h>
#include <assert.h>
#include <sys/file.h>
#include <time.h>
#include <mqueue.h>
#include <uuid/uuid.h>


#if !defined(offset_of)
#define offset_of(type, field) (unsigned long)&(((type *)0)->field)
#endif // offset_of

#if !defined(container_of)
#define container_of(ptr, type, member) ((type *)(((char *)ptr) - offset_of(type, member)))
#endif // container_of

typedef struct niccolum_client_mq_map {
	struct niccolum_client_mq_map *next;
	struct niccolum_client_mq_map *prev;
	uuid_t uuid;
	mqd_t mq_descriptor;
} niccolum_client_mq_map_t;

/* protect the client lookup info */
pthread_rwlock_t niccolum_client_mq_map_lock = PTHREAD_RWLOCK_INITIALIZER;
niccolum_client_mq_map_t *niccolum_client_mq_map_list;
unsigned int niccolum_client_mq_map_count = 0;

static mqd_t niccolum_lookup_mq_for_client_locked(uuid_t clientUuid) 
{
	niccolum_client_mq_map_t *map;
	mqd_t mq_descriptor = -ENOENT;

	if (NULL == niccolum_client_mq_map_list) {
		return -ENOENT;
	}

	map = niccolum_client_mq_map_list;
	do {
		if (0 == uuid_compare(clientUuid, map->uuid)) {
			/* we found it */
			mq_descriptor = map->mq_descriptor;
			break;
		}
		map = map->next;
	}
	while (map != niccolum_client_mq_map_list);

	return mq_descriptor;

}

static mqd_t niccolum_lookup_mq_for_client(uuid_t clientUuid)
{
	mqd_t mq_descriptor = -ENOENT;

	if (NULL == niccolum_client_mq_map_list) {
		return -ENOENT;
	}

	pthread_rwlock_rdlock(&niccolum_client_mq_map_lock);
	mq_descriptor = niccolum_lookup_mq_for_client_locked(clientUuid);
	pthread_rwlock_unlock(&niccolum_client_mq_map_lock);

	return mq_descriptor;
}

static void remove_client_mq_map(niccolum_client_mq_map_t *map) 
{
	map->next->prev = map->prev;
	map->prev->next = map->next;
	map->next = map->prev = map;
	if (0 > mq_close(map->mq_descriptor)) {
		fprintf(stderr, "%d @ %s (%s) FAILED TO CLOSE descriptor %d (%s)\n", 
		        __LINE__, __FILE__, __FUNCTION__, (int) map->mq_descriptor, strerror(errno));
	}
	else {
		fprintf(stderr, "%d @ %s (%s) closed descriptor %d\n", 
		        __LINE__, __FILE__, __FUNCTION__, (int) map->mq_descriptor);
	}
	free(map);
	niccolum_client_mq_map_count--;
}

#if 0
static int niccolum_remove_mq_for_client_locked(uuid_t clientUuid)
{
	niccolum_client_mq_map_t *map = NULL;
	int status = ENOENT;

	pthread_rwlock_wrlock(&niccolum_client_mq_map_lock);

	for (map = niccolum_client_mq_map_list->next;
	     map != niccolum_client_mq_map_list;
		 map = map->next) {

		if (0 == memcmp(map->uuid, &clientUuid, sizeof(uuid_t))) {
			remove_client_mq_map(map);
			status = 0;
			break;
		}
	}

	pthread_rwlock_unlock(&niccolum_client_mq_map_lock);
	
	return status;
}
#endif // 0

static int niccolum_insert_mq_for_client(uuid_t clientUuid, mqd_t mq_descriptor)
{
	niccolum_client_mq_map_t *map = malloc(sizeof(niccolum_client_mq_map_t));
	int status = 0;

	if (NULL == map) {
		return -ENOMEM;
	}

	map->next = map;
	map->prev = map;
	uuid_copy(map->uuid, clientUuid);
	map->mq_descriptor = mq_descriptor;

	pthread_rwlock_wrlock(&niccolum_client_mq_map_lock);

	if (NULL == niccolum_client_mq_map_list) {
		niccolum_client_mq_map_list = map;
	}

	//
	// TODO: parameterize this and adjust it - there's a low limit to the 
	// number of message queues that are allowed to be opened.
	//
	if (niccolum_client_mq_map_count > 5) {
		remove_client_mq_map(niccolum_client_mq_map_list->prev); // prune the oldest one	
	}

	while (niccolum_client_mq_map_list != map) {
		if (niccolum_lookup_mq_for_client_locked(clientUuid) >= 0) {
			/* already exists! */
			status = -EEXIST;
			break;
		}

		map->next = niccolum_client_mq_map_list;
		map->prev = niccolum_client_mq_map_list->prev;
		niccolum_client_mq_map_list->prev->next = map;
		niccolum_client_mq_map_list->prev = map;
		niccolum_client_mq_map_list = map;
		niccolum_client_mq_map_count++;
		break;
	}
	pthread_rwlock_unlock(&niccolum_client_mq_map_lock);

	return status;
}

/* TODO: add remove! */

struct niccolum_req {
	struct fuse_req fuse_request;

	/* niccolum specific routing information */

};

static void list_init_req(struct fuse_req *req)
{
	req->next = req;
	req->prev = req;
}

static void list_del_req(struct fuse_req *req)
{
	struct fuse_req *prev = req->prev;
	struct fuse_req *next = req->next;
	prev->next = next;
	next->prev = prev;
}


static struct fuse_req *niccolum_alloc_req(struct fuse_session *se)
{
	struct fuse_req *req;

	req = (struct fuse_req *) calloc(1, sizeof(struct fuse_req));
	if (req == NULL) {
		fprintf(stderr, "niccolum (fuse): failed to allocate request\n");
	} else {
		req->se = se;
		req->ctr = 1;
		list_init_req(req);
		fuse_mutex_init(&req->lock);
		niccolum_set_provider(req, 1);
	}
	return req;
}

static void destroy_req(fuse_req_t req)
{
	pthread_mutex_destroy(&req->lock);
	free(req);
}

static void niccolum_free_req(fuse_req_t req)
{
	int ctr;
	struct fuse_session *se = req->se;

	pthread_mutex_lock(&se->lock);
	req->u.ni.func = NULL;
	req->u.ni.data = NULL;
	list_del_req(req);
	ctr = --req->ctr;
	fuse_chan_put(req->ch);
	req->ch = NULL;
	pthread_mutex_unlock(&se->lock);
	if (!ctr)
		destroy_req(req);
}

static const struct fuse_lowlevel_ops *niccolum_original_ops;
    
static int niccolum_mt = 1;

static void niccolum_init(void *userdata, struct fuse_conn_info *conn);
static void niccolum_init(void *userdata, struct fuse_conn_info *conn)
{
	return niccolum_original_ops->init(userdata, conn);
}

static void niccolum_lookup(fuse_req_t req, fuse_ino_t parent, const char *name);
static void niccolum_lookup(fuse_req_t req, fuse_ino_t parent, const char *name)
{
	niccolum_set_provider(req, 0);
	req->niccolum_notify = 1;
	/* TODO: add to lookup table? */
	return niccolum_original_ops->lookup(req, parent, name);
}

static void niccolum_forget(fuse_req_t req, fuse_ino_t ino, uint64_t nlookup);
static void niccolum_forget(fuse_req_t req, fuse_ino_t ino, uint64_t nlookup)
{
	niccolum_set_provider(req, 0);
	req->niccolum_notify = 1;
	/* TODO: remove from lookup table? */
	return niccolum_original_ops->forget(req, ino, nlookup);
}

static void niccolum_getattr(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi);
static void niccolum_getattr(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi)
{
	niccolum_set_provider(req, 0);
	return niccolum_original_ops->getattr(req, ino, fi);
}

static void niccolum_readlink(fuse_req_t req, fuse_ino_t ino);
static void niccolum_readlink(fuse_req_t req, fuse_ino_t ino) 
{
	niccolum_set_provider(req, 0);
	return niccolum_original_ops->readlink(req, ino);
}

static void niccolum_opendir(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi);
static void niccolum_opendir(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi)
{
	niccolum_set_provider(req, 0);
	req->niccolum_notify = 1;
	return niccolum_original_ops->opendir(req, ino, fi);
}

static void niccolum_readdir(fuse_req_t req, fuse_ino_t ino, size_t size, off_t offset, struct fuse_file_info *fi);
static void niccolum_readdir(fuse_req_t req, fuse_ino_t ino, size_t size, off_t offset, struct fuse_file_info *fi)
{
	niccolum_set_provider(req, 0);
	return niccolum_original_ops->readdir(req, ino, size, offset, fi);
}

static void niccolum_readdirplus(fuse_req_t req, fuse_ino_t ino, size_t size,	off_t offset, struct fuse_file_info *fi);
static void niccolum_readdirplus(fuse_req_t req, fuse_ino_t ino, size_t size,	off_t offset, struct fuse_file_info *fi)
{
	niccolum_set_provider(req, 0);
	return niccolum_original_ops->readdirplus(req, ino, size, offset, fi);
}

static void niccolum_create(fuse_req_t req, fuse_ino_t parent, const char *name,	mode_t mode, struct fuse_file_info *fi);
static void niccolum_create(fuse_req_t req, fuse_ino_t parent, const char *name,	mode_t mode, struct fuse_file_info *fi)
{
	niccolum_set_provider(req, 0);
	return niccolum_original_ops->create(req, parent, name, mode, fi);
}

static void niccolum_open(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi);
static void niccolum_open(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi)
{
	niccolum_set_provider(req, 0);
	return niccolum_original_ops->open(req, ino, fi);
}

static void niccolum_release(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi);
static void niccolum_release(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi)
{
	niccolum_set_provider(req, 0);
	req->niccolum_notify = 1;
	return niccolum_original_ops->release(req, ino, fi);
}

static void niccolum_releasedir(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi);
static void niccolum_releasedir(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi)
{
	niccolum_set_provider(req, 0);
	req->niccolum_notify = 1;
	return niccolum_original_ops->releasedir(req, ino, fi);
}

static void niccolum_read(fuse_req_t req, fuse_ino_t ino, size_t size, off_t offset, struct fuse_file_info *fi);
static void niccolum_read(fuse_req_t req, fuse_ino_t ino, size_t size, off_t offset, struct fuse_file_info *fi)
{
	niccolum_set_provider(req, 0);
	return niccolum_original_ops->read(req, ino, size, offset, fi);
}

static void niccolum_write_buf(fuse_req_t req, fuse_ino_t ino, struct fuse_bufvec *in_buf, off_t off,	struct fuse_file_info *fi);
static void niccolum_write_buf(fuse_req_t req, fuse_ino_t ino, struct fuse_bufvec *in_buf, off_t off,	struct fuse_file_info *fi)
{
	niccolum_set_provider(req, 0);
	return niccolum_original_ops->write_buf(req, ino, in_buf, off, fi);
}


static struct fuse_lowlevel_ops niccolum_ops = {
	.init		= niccolum_init,
	.lookup		= niccolum_lookup,
	.forget		= niccolum_forget,
	.getattr	= niccolum_getattr,
	.readlink	= niccolum_readlink,
	.opendir	= niccolum_opendir,
	.readdir	= niccolum_readdir,
	.readdirplus	= niccolum_readdirplus,
	.releasedir	= niccolum_releasedir,
	.create		= niccolum_create,
	.open		= niccolum_open,
	.release	= niccolum_release,
	.read		= niccolum_read,
	.write_buf      = niccolum_write_buf
};

uuid_t niccolum_server_uuid;

/**
 * Set the provider for the given request.
 */
 void niccolum_set_provider(fuse_req_t req, int niccolum)
 {
     if (niccolum) {
         req->niccolum = 1;
     }
     else {
         req->niccolum = 0;
     } 
 }
 
  int niccolum_get_provider(fuse_req_t req)
  {
     return req->niccolum;
  }


  #undef fuse_session_new

  struct fuse_session *niccolum_session_new(struct fuse_args *args,
     const struct fuse_lowlevel_ops *op,
     size_t op_size, void *userdata)
 {
     (void) op;
     struct fuse_session *se;
     struct mq_attr attr;

     //
     // Save the original ops
     //
     niccolum_original_ops = op;

     se = fuse_session_new(args, &niccolum_ops, op_size, userdata);

     if (NULL == se) {
         return se;
     }

     //
     // For now I'm hard coding these names.  They should be parameterizied
     //
	se->message_queue_name = "/niccolum";

    //
    // Create it if it does not exist.  Permissive permissions
    //
    attr = (struct mq_attr){
        .mq_flags = 0,
        .mq_maxmsg = 10,
        .mq_msgsize = 512,
        .mq_curmsgs = 0,
    };

	se->message_queue_descriptor = mq_open(se->message_queue_name, O_RDONLY | O_CREAT, 0622, &attr);

    if (se->message_queue_descriptor < 0) {
		fprintf(stderr, "fuse (niccolum): failed to create message queue: %s\n", strerror(errno));
        fuse_session_destroy(se);
        se = NULL;
    }
    
    return se;
}

static int niccolum_connect_to_client(uuid_t clientUuid)
{
	char name[64];

	/* TODO parameterize this name */
	strncpy(name, "/niccolum_", sizeof(name));
	uuid_unparse_lower(clientUuid, &name[strlen(name)]);
	return mq_open(name, O_WRONLY);

}

static int niccolum_send_response(uuid_t clientUuid, niccolum_message_t *response)
{
	mqd_t mq_descriptor = niccolum_lookup_mq_for_client(clientUuid);

	if (mq_descriptor == -ENOENT) {
		/* create it */	
		mq_descriptor = niccolum_connect_to_client(clientUuid);

		if (mq_descriptor >= 0) {
		  	fprintf(stderr, "%s @ %d (%s): open mq %d\n", __FILE__, __LINE__, __FUNCTION__, (int) mq_descriptor);
			niccolum_insert_mq_for_client(clientUuid, mq_descriptor);
		}
	}

	if (mq_descriptor < 0) {
		fprintf(stderr, "%s @ %d (%s): failed to connect to client %s\n", __FILE__, __LINE__, __FUNCTION__, strerror(errno));
		return mq_descriptor;
	}

	return mq_send(mq_descriptor, (char *)response, offsetof(niccolum_message_t, Message) + response->MessageLength, 0);

}

static void *niccolum_mq_worker(void* arg)
{
	struct fuse_session *se = (struct fuse_session *) arg;
	struct mq_attr attr;
	ssize_t bytes_received, bytes_to_send;
	niccolum_message_t *niccolum_request, *niccolum_response;
	struct fuse_req *req;

	if (NULL == se) {
		pthread_exit(NULL);
	}

	if (mq_getattr(se->message_queue_descriptor, &attr) < 0) {
		fprintf(stderr, "niccolum (fuse): failed to get message queue attributes: %s\n", strerror(errno));
		return NULL;
	}

	niccolum_response = malloc(attr.mq_msgsize);
	if (NULL == niccolum_response) {
		fprintf(stderr, "niccolum (fuse): failed to allocate response buffer: %s\n", strerror(errno));
		return NULL;
	}

	niccolum_request = malloc(attr.mq_msgsize);

	while (NULL != niccolum_request) {

		bytes_to_send = 0;

		bytes_received = mq_receive(se->message_queue_descriptor, (char *)niccolum_request, attr.mq_msgsize, NULL /* optional priority */ );
		
		if (bytes_received < 0) {
			fprintf(stderr, "niccolum (fuse): failed to read message from queue: %s\n", strerror(errno));
			return NULL;
		}

		if (bytes_received < offsetof(niccolum_message_t, Message)) {
			// this message isn't even big enough for us to process
			fprintf(stderr, "niccolum (fuse): short message received from queue\n");
			break;
		}

		/* now we have a message from the queue and need to process it */
		if (0 != memcmp(NICCOLUM_MESSAGE_MAGIC, niccolum_request->MagicNumber, NICCOLUM_MESSAGE_MAGIC_SIZE)) {
			/* not a valid message */
			fprintf(stderr, "niccolum (fuse): invalid message received from queue\n");
			break;
		}

		fprintf(stderr, "niccolum (fuse): received message\n");

		/* handle the request */
		switch (niccolum_request->MessageType) {
			default: {
				fprintf(stderr, "niccolum (fuse): invalid message type received %d\n", (int) niccolum_request->MessageType);
				break;
			}

			case NICCOLUM_FUSE_OP_RESPONSE:
			case NICCOLUM_NAME_MAP_RESPONSE:
			case NICCOLUM_FUSE_NOTIFY: {
				fprintf(stderr, "niccolum (fuse): not implemented type received %d\n", (int) niccolum_request->MessageType);
				break;
			}

			case NICCOLUM_NAME_MAP_REQUEST: {
				/* first, is the request here a match for this file system? */
				size_t message_length = strlen(niccolum_request->Message) + offsetof(niccolum_message_t, Message);
				size_t mp_length = strlen(se->mountpoint);

				if ((message_length < bytes_received) ||
				    (0 != strncmp(niccolum_request->Message, se->mountpoint, mp_length))) {
					/* this is not ours */
					memcpy(niccolum_response->MagicNumber, NICCOLUM_MESSAGE_MAGIC, NICCOLUM_MESSAGE_MAGIC_SIZE);
					memcpy(&niccolum_response->SenderUuid, niccolum_server_uuid, sizeof(uuid_t));
					niccolum_response->MessageType = NICCOLUM_NAME_MAP_RESPONSE;
					niccolum_response->MessageId = niccolum_request->MessageId;
					niccolum_response->MessageLength = sizeof(niccolum_name_map_response_t);
					((niccolum_name_map_response_t  *)niccolum_response->Message)->Status = NICCOLUM_MAP_RESPONSE_INVALID;
					// bytes_to_send = offsetof(niccolum_message_t, Message) + sizeof(niccolum_name_map_response_t);
					fprintf(stderr, "%s @ %d (%s): invalid request\n", __FILE__, __LINE__, __FUNCTION__);
					break;
				}

				/* so let's do a lookup */
				/* map the name given */
				fprintf(stderr, "niccolum (fuse): map name request for %s\n", niccolum_request->Message);
				fprintf(stderr, "niccolum (fuse): mount point is %s (len = %zu)\n", se->mountpoint, mp_length);
				fprintf(stderr, "niccolum (fuse): do lookup on %s\n", &niccolum_request->Message[mp_length]);
				req = niccolum_alloc_req(se);
				if (NULL == req) {
					fprintf(stderr, "%s @ %d (%s): alloc failure\n", __FILE__, __LINE__, __FUNCTION__);
					break;
				}
				req->niccolum_req = niccolum_request;
				req->niccolum_rsp = niccolum_response;
				niccolum_response = NULL; /* again, passing it to the lookup, consume it in the completion handler */
				niccolum_original_ops->lookup(req, FUSE_ROOT_ID, &niccolum_request->Message[mp_length+1]);
				niccolum_request = NULL; /* passing it to the lookup */
				break;
			}

			case NICCOLUM_MAP_RELEASE_REQUEST: {
				size_t message_length = sizeof(niccolum_map_release_request_t) + offsetof(niccolum_message_t, Message);
				niccolum_map_release_request_t *mrreq = (niccolum_map_release_request_t *)niccolum_request->Message;
				niccolum_object_t *object = NULL;

				// format generic response info
				memcpy(niccolum_response->MagicNumber, NICCOLUM_MESSAGE_MAGIC, NICCOLUM_MESSAGE_MAGIC_SIZE);
				memcpy(&niccolum_response->SenderUuid, niccolum_server_uuid, sizeof(uuid_t));
				niccolum_response->MessageType = NICCOLUM_MAP_RELEASE_RESPONSE;
				niccolum_response->MessageId = niccolum_request->MessageId;
				niccolum_response->MessageLength = sizeof(niccolum_map_release_response_t);
				bytes_to_send = offsetof(niccolum_message_t, Message) + sizeof(niccolum_map_release_response_t);

				if ((message_length > bytes_received) || // short message
				    (mrreq->Key.KeyLength > niccolum_request->MessageLength) || // short message
				    (mrreq->Key.KeyLength != sizeof(uuid_t))) { // invalid key length
					((niccolum_map_release_response_t  *)niccolum_response->Message)->Status = NICCOLUM_MAP_RESPONSE_INVALID;
					fprintf(stderr, "%s @ %d (%s): invalid request\n", __FILE__, __LINE__, __FUNCTION__);
					break;
				}

				// lookup
				object = niccolum_object_lookup_by_uuid((uuid_t *)mrreq->Key.Key);
				if (NULL != object) {
					niccolum_object_release(object);
				}
				((niccolum_map_release_response_t  *)niccolum_response->Message)->Status = NICCOLUM_MAP_RESPONSE_SUCCESS;
				break;
			}

			case NICCOLUM_DIR_MAP_REQUEST: {

				break;
			}

			case NICCOLUM_FUSE_OP_REQUEST: {
				break;
			}

			case NICCOLUM_TEST: {
				niccolum_test_message_t *test_message = (niccolum_test_message_t *)niccolum_request->Message;
				ssize_t response_length = offsetof(niccolum_message_t, Message) + test_message->MessageLength;

				memcpy(niccolum_response->MagicNumber, NICCOLUM_MESSAGE_MAGIC, NICCOLUM_MESSAGE_MAGIC_SIZE);
				memcpy(&niccolum_response->SenderUuid, niccolum_server_uuid, sizeof(uuid_t));			
				niccolum_response->MessageType = NICCOLUM_TEST_RESPONSE;
				niccolum_response->MessageId = niccolum_request->MessageId;

				if (response_length < niccolum_request->MessageLength) {
					// we received a runt request
					niccolum_response->MessageLength = 0;
					bytes_to_send = sizeof(niccolum_message_t);
					break;
				}

				/* send response */
				niccolum_response->MessageLength = offsetof(niccolum_message_t, Message) + test_message->MessageLength;
				memcpy(niccolum_response->Message, test_message, response_length);
				bytes_to_send = offsetof(niccolum_message_t, Message) + response_length;
				
				break;
			}

		}

		if (NULL == niccolum_request) {
			/* need a new one - must have consumed it */
			niccolum_request = (niccolum_message_t *) malloc(attr.mq_msgsize);	
		}
		
		if (NULL == niccolum_response) {
			niccolum_response = (niccolum_message_t *) malloc(attr.mq_msgsize);
			assert(0 == bytes_to_send);
			bytes_to_send = 0;
		}
		
		if (0 < bytes_to_send) {
			uuid_t uuid;
			fprintf(stderr, "niccolum (fuse): sending response (size = %zu)\n", bytes_to_send);
			memcpy(&uuid, &niccolum_request->SenderUuid, sizeof(uuid_t));
			niccolum_send_response(uuid, niccolum_response);
		}
	
	}

	if (NULL != niccolum_response) {
		free(niccolum_response);
		niccolum_response = NULL;
	}

	return NULL;

}

#if 0
	FUSE_LOOKUP	   = 1,
	FUSE_FORGET	   = 2,  /* no reply */
	FUSE_GETATTR	   = 3,
	FUSE_SETATTR	   = 4,
	FUSE_READLINK	   = 5,
	FUSE_SYMLINK	   = 6,
	FUSE_MKNOD	   = 8,
	FUSE_MKDIR	   = 9,
	FUSE_UNLINK	   = 10,
	FUSE_RMDIR	   = 11,
	FUSE_RENAME	   = 12,
	FUSE_LINK	   = 13,
	FUSE_OPEN	   = 14,
	FUSE_READ	   = 15,
	FUSE_WRITE	   = 16,
	FUSE_STATFS	   = 17,
	FUSE_RELEASE       = 18,
	FUSE_FSYNC         = 20,
	FUSE_SETXATTR      = 21,
	FUSE_GETXATTR      = 22,
	FUSE_LISTXATTR     = 23,
	FUSE_REMOVEXATTR   = 24,
	FUSE_FLUSH         = 25,
	FUSE_INIT          = 26,
	FUSE_OPENDIR       = 27,
	FUSE_READDIR       = 28,
	FUSE_RELEASEDIR    = 29,
	FUSE_FSYNCDIR      = 30,
	FUSE_GETLK         = 31,
	FUSE_SETLK         = 32,
	FUSE_SETLKW        = 33,
	FUSE_ACCESS        = 34,
	FUSE_CREATE        = 35,
	FUSE_INTERRUPT     = 36,
	FUSE_BMAP          = 37,
	FUSE_DESTROY       = 38,
	FUSE_IOCTL         = 39,
	FUSE_POLL          = 40,
	FUSE_NOTIFY_REPLY  = 41,
	FUSE_BATCH_FORGET  = 42,
	FUSE_FALLOCATE     = 43,
	FUSE_READDIRPLUS   = 44,
	FUSE_RENAME2       = 45,
	FUSE_LSEEK         = 46,

#endif // 0

void niccolum_notify_reply_iov(fuse_req_t req, int error, struct iovec *iov, int count)
{
	if (0 != error) {
		// So far we don't care about the error outcomes
		return;
	}

	if (count < 2) {
		// not sure what this means
		return;
	}

	//
	// We want to process some requests here
	//
	switch(req->opcode) {
		default:
			// no action is the default
			break;
		case FUSE_LOOKUP: {
			ino_t ino;
			struct fuse_entry_out *arg = (struct fuse_entry_out *)iov[1].iov_base;
			niccolum_object_t *nicobj;

			assert(iov[1].iov_len >= sizeof(struct fuse_entry_out));
			ino = arg->nodeid;
			// TODO: figure out how to insert this
			nicobj = niccolum_object_create(ino, NULL);
			assert(NULL != nicobj);
			niccolum_object_release(nicobj);
			break;
		}
		case FUSE_RELEASE: {
			// TODO
			break;
		}
		case FUSE_OPENDIR: {
			// TODO
			break;
		}
		case FUSE_RELEASEDIR: {
			// TODO
			break;
		}
	}
	return;
}

int niccolum_send_reply_iov(fuse_req_t req, int error, struct iovec *iov, int count, int free_req)
{
	struct fuse_out_header out;
	niccolum_message_t *niccolum_request = req->niccolum_req;
	niccolum_message_t *niccolum_response = req->niccolum_rsp;
	size_t bytes_to_send = 0;

	(void) count;

	if (error <= -1000 || error > 0) {
		fprintf(stderr, "fuse: bad error value: %i\n",	error);
		error = -ERANGE;
	}

	out.unique = req->unique;
	out.error = error;

	iov[0].iov_base = &out;
	iov[0].iov_len = sizeof(struct fuse_out_header);

	/* return fuse_send_msg(req->se, req->ch, iov, count); */

	switch (niccolum_request->MessageType) {
		default:
		// don't know what is being asked here, so abort
		assert(0); 
		break;

		case NICCOLUM_NAME_MAP_REQUEST: {
			niccolum_name_map_response_t *nmr;

			assert(NULL != niccolum_request);
			assert(NULL != niccolum_response);

			/* let's set up the response here */
			memcpy(niccolum_response->MagicNumber, NICCOLUM_MESSAGE_MAGIC, NICCOLUM_MESSAGE_MAGIC_SIZE);
			memcpy(&niccolum_response->SenderUuid, niccolum_server_uuid, sizeof(uuid_t));
			niccolum_response->MessageType = NICCOLUM_NAME_MAP_RESPONSE;
			niccolum_response->MessageId = niccolum_request->MessageId;
			niccolum_response->MessageLength = sizeof(niccolum_name_map_response_t);
			((niccolum_name_map_response_t  *)niccolum_response->Message)->Status = NICCOLUM_MAP_RESPONSE_INVALID;

			nmr = (niccolum_name_map_response_t *)niccolum_response->Message;

			while (0 == error) {
				struct fuse_entry_out *arg = (struct fuse_entry_out *)iov[1].iov_base;
				niccolum_object_t *nobj;

				nobj = niccolum_object_create((ino_t) arg->nodeid, NULL);

				if (NULL == nobj) {
					error = ENOMEM;
					break;
				}

				nmr->Status = NICCOLUM_MAP_RESPONSE_SUCCESS;
				nmr->Key.KeyLength = sizeof(uuid_t);
				memcpy(nmr->Key.Key, &nobj->uuid, sizeof(uuid_t));
				niccolum_response->MessageLength = offsetof(niccolum_name_map_response_t, Key.Key) + nmr->Key.KeyLength;
				niccolum_object_release(nobj);
				break;
			}

			if (0 != error) {
				nmr->Status = NICCOLUM_MAP_RESPONSE_INVALID;
				nmr->Key.KeyLength = 0;
			}

			bytes_to_send = offsetof(niccolum_message_t, Message);
			bytes_to_send += niccolum_response->MessageLength;

		}
		break;

	}

	if (0 < bytes_to_send) {
		uuid_t uuid;
		fprintf(stderr, "niccolum (fuse): sending response %zu (%d @ %s)\n", bytes_to_send, __LINE__, __FILE__);
		memcpy(&uuid, &niccolum_request->SenderUuid, sizeof(uuid_t));
		niccolum_send_response(uuid, niccolum_response);
	}


	if (NULL != req->niccolum_req) {
		free(req->niccolum_req);
		req->niccolum_req = NULL;
	}

	if (NULL != req->niccolum_rsp) {
		free(req->niccolum_rsp);
		req->niccolum_rsp = NULL;
	}

	if (free_req) {
		niccolum_free_req(req);
	}
	
	return 0;
}

// static struct sigevent niccolum_mq_sigevent;
static pthread_attr_t niccolum_mq_thread_attr;
#define NICCOLUM_MAX_THREADS (1)
pthread_t niccolum_threads[NICCOLUM_MAX_THREADS];
#undef fuse_session_loop_mt
 
int niccolum_session_loop_mt_32(struct fuse_session *se, struct fuse_loop_config *config)
{
    int status;

    niccolum_mt = 1;

	status = 0;

	while (se->message_queue_descriptor >= 0) {
		memset(&niccolum_mq_thread_attr, 0, sizeof(niccolum_mq_thread_attr));
		status = pthread_attr_init(&niccolum_mq_thread_attr);
		if (status < 0) {
			fprintf(stderr, "niccolum (fuse): pthread_attr_init failed: %s\n", strerror(errno));
			return status; // no cleanup
		}
		status = pthread_attr_setdetachstate(&niccolum_mq_thread_attr, PTHREAD_CREATE_DETACHED);
		if (status < 0) {
			fprintf(stderr, "niccolum (fuse): pthread_attr_setdetachstate failed: %s\n", strerror(errno));
			break;
		}

		uuid_generate_time_safe(niccolum_server_uuid);

		/* TODO: start worker thread(s) */
		for (unsigned int index = 0; index < NICCOLUM_MAX_THREADS; index++) {
			status = pthread_create(&niccolum_threads[index], &niccolum_mq_thread_attr, niccolum_mq_worker, se);
			if (status < 0) {
				fprintf(stderr, "niccolum (fuse): pthread_create failed: %s\n", strerror(errno));
			}
		}
	
		/* done */
		break;
	}

	if (status < 0) {
		pthread_attr_destroy(&niccolum_mq_thread_attr);
		return status;
	}

	return fuse_session_loop_mt(se, config);

}

 int niccolum_session_loop_mt_31(struct fuse_session *se, int clone_fd)
 {
	struct fuse_loop_config config;
	config.clone_fd = clone_fd;
	config.max_idle_threads = 10;
	return niccolum_session_loop_mt_32(se, &config);
 }
 
 int niccolum_session_loop(struct fuse_session *se)
 {
     /* for now we don't support any niccolum functionality in single threaded mode */
     niccolum_mt = 0;
     return fuse_session_loop(se);
 }
 
 void niccolum_session_destroy(struct fuse_session *se)
 {
     /* TODO: need to add the niccolum specific logic here */

	if (se->message_queue_descriptor) {
		(void) mq_close(se->message_queue_descriptor);
		(void) mq_unlink(se->message_queue_name);
		pthread_attr_destroy(&niccolum_mq_thread_attr);
	}


     fuse_session_destroy(se);
 }
 
 
 