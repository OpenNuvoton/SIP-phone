/**
 * @file vc8000.h For ma35d1 hardware decoder Interface
 *
 * Copyright (C) 2021 CHChen59
 */
#ifndef __VC8000_H__
#define __VC8000_H__

#define DEFAULT_VC8000_VIDDST_WIDTH		(800)
#define DEFAULT_VC8000_VIDDST_HEIGHT	(480)

/* This is the size of the buffer for the compressed stream.
 * It limits the maximum compressed frame size. */
#define STREAM_BUFFER_SIZE	(1024 * 1024)


struct vc8000_vidcodec {
	struct vidcodec vc;
};

int vc8000_h264_fmtp_enc(struct mbuf *mb, const struct sdp_format *fmt, bool offer, void *arg);
bool vc8000_h264_fmtp_cmp(const char *lfmtp, const char *rfmtp, void *data);
int vc8000_decode_update(struct viddec_state **vdsp, const struct vidcodec *vc, const char *fmtp);
int vc8000_decode_h264(struct viddec_state *st, struct vidframe *frame, bool *intra, bool eof, uint16_t seq, struct mbuf *src);
int vc8000_init(void);


#endif
