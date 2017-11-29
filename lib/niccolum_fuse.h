#ifndef __NICCOLUM_FUSE_H__
#define __NICCOLUM_FUSE_H__

extern int niccolum_send_reply_iov(fuse_req_t req, int error, struct iovec *iov, int count, int free_req);
extern void niccolum_notify_reply_iov(fuse_req_t req, int error, struct iovec *iov, int count);

#endif // __NICCOLUM_FUSE_H__

