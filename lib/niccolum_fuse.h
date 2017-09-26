#ifndef __NICCOLUM_FUSE_H__
#define __NICCOLUM_FUSE_H__

extern int niccolum_send_reply_iov(fuse_req_t req, int error, struct iovec *iov, int count);

#endif // __NICCOLUM_FUSE_H__

