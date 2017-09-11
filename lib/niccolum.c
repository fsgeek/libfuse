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
#include "niccolum_msg.h"

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

struct niccolum_req {
	struct fuse_req fuse_request;

	/* niccolum specific routing information */

};

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
	/* TODO: add to lookup table? */
	return niccolum_original_ops->lookup(req, parent, name);
}

static void niccolum_forget(fuse_req_t req, fuse_ino_t ino, uint64_t nlookup);
static void niccolum_forget(fuse_req_t req, fuse_ino_t ino, uint64_t nlookup)
{
	niccolum_set_provider(req, 0);
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
	return niccolum_original_ops->release(req, ino, fi);
}

static void niccolum_releasedir(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi);
static void niccolum_releasedir(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi)
{
	niccolum_set_provider(req, 0);
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
     
     se = fuse_session_new(args, &niccolum_ops, op_size, userdata);

     if (NULL == se) {
         return se;
     }

     //
     // Save the original ops
     //
     niccolum_original_ops = op;

     //
     // For now I'm hard coding these names.  They should be parameterizied
     //
	se->message_queue_name = "/niccolum";

    //
    // Create it if it does not exist.  Permissive permissions
    //
	se->message_queue_descriptor = mq_open(se->message_queue_name, O_RDONLY | O_CREAT, 0666, NULL);

    if (se->message_queue_descriptor < 0) {
		fprintf(stderr, "fuse (niccolum): failed to create message queue: %s\n", strerror(errno));
        fuse_session_destroy(se);
        se = NULL;
    }
    
    return se;
}

static void *niccolum_mq_worker(void* arg)
{
	struct fuse_session *se = (struct fuse_session *) arg;
	struct mq_attr attr;
	ssize_t bytes_received, bytes_to_send;
	niccolum_message_t *niccolum_request, *niccolum_response;

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
				if (0 != strcmp(niccolum_request->Message, se->mountpoint)) {
					/* this is not ours */
					memcpy(niccolum_response->MagicNumber, NICCOLUM_MESSAGE_MAGIC, NICCOLUM_MESSAGE_MAGIC_SIZE);
					memcpy(&niccolum_response->SenderUuid, niccolum_server_uuid, sizeof(uuid_t));
					niccolum_response->MessageType = NICCOLUM_NAME_MAP_RESPONSE;
					niccolum_response->MessageId = niccolum_request->MessageId;
					niccolum_response->MessageLength = sizeof(niccolum_name_map_response_t);
					((niccolum_name_map_response_t  *)niccolum_response->Message)->Status = NICCOLUM_MAP_RESPONSE_INVALID;
				}
				else {
					/* so let's do a lookup */
					/* map the name given */
					fprintf(stderr, "niccolum (fuse): map name request for %s\n", niccolum_request->Message);
					fprintf(stderr, "niccolum (fuse): mount point is %s (len = %zu)\n", se->mountpoint, strlen(se->mountpoint));
					fprintf(stderr, "niccolum (fuse): do lookup on %s\n", &niccolum_request->Message[strlen(se->mountpoint) + 1]);
					
				}
				break;
			}

			case NICCOLUM_FUSE_OP_REQUEST: {
				break;
			}

		}
		
		
		if (0 < bytes_to_send) {
			fprintf(stderr, "niccolum (fuse): sending response %zu\n", bytes_to_send);
		}
	
	}	
	return NULL;

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
 
 
 