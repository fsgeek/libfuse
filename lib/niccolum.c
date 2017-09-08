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
 
int niccolum_session_loop_mt_32(struct fuse_session *se, struct fuse_loop_config *config)
{
    int status;

    niccolum_mt = 1;
    status = fuse_session_loop_mt(se, config);
    if (0 != status) {
        return status;
    }

    /* start niccolum thread */
    status = 0;
    return status;
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

     	/* BEGIN NICCOLUM CODE */
	if (se->message_queue_descriptor) {
		(void) mq_close(se->message_queue_descriptor);
		(void) mq_unlink(se->message_queue_name);
	}
	/* END NICCOLUM CODE */


     fuse_session_destroy(se);
 }
 
 
 