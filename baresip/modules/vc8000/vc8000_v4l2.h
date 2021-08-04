/**
 * @file vc8000_v4l2.h vc8000 v4l2 driver
 *
 * Copyright (C) 2021 CHChen59
 */

#ifndef __VC8000_V4L2_H__
#define __VC8000_V4L2_H__

#include <stdio.h>
#include <time.h>
#include <linux/videodev2.h>

/* Maximum number of output buffers */
#define MAX_OUT_BUF		16

/* Maximum number of capture buffers (32 is the limit imposed by MFC */
#define MAX_CAP_BUF		32

/* Number of output planes */
#define OUT_PLANES		1

/* Number of capture planes */
#define CAP_PLANES		1

/* Maximum number of planes used in the application */
#define MAX_PLANES		CAP_PLANES

/* H264 maximum decode resolution*/
#define MAX_H264_WIDTH          1920
#define MAX_H264_HEIGHT         1088

#define memzero(x)	memset(&(x), 0, sizeof (x));

typedef enum {
	eV4L2_BUF_DEQUEUE,	
	eV4L2_BUF_INQUEUE
}E_V4L2_BUF_STATUS;

/* video decoder related parameters */
struct video {
	int fd;

	/* Output queue related for encoded bitstream*/ 
	int out_buf_cnt;
	int out_buf_size;
	int out_buf_off[MAX_OUT_BUF];
	char *out_buf_addr[MAX_OUT_BUF];
	E_V4L2_BUF_STATUS out_buf_flag[MAX_OUT_BUF];

	/* Capture queue related for decoded buffer*/
	int cap_w;
	int cap_h;
	int cap_crop_w;
	int cap_crop_h;
	int cap_crop_left;
	int cap_crop_top;
	int cap_buf_cnt;
	int cap_buf_cnt_min;
	int cap_buf_size[CAP_PLANES];
	int cap_buf_off[MAX_CAP_BUF][CAP_PLANES];
	char *cap_buf_addr[MAX_CAP_BUF][CAP_PLANES];
	E_V4L2_BUF_STATUS cap_buf_flag[MAX_CAP_BUF];
	int cap_buf_queued;

	unsigned long total_captured;
};

// video decode post processing
struct video_pp {
	bool enabled;
	int panel_w;
	int panel_h;
	int offset_x;
	int offset_y;
};

int vc8000_v4l2_open(struct video *psVideo);
void vc8000_v4l2_close(struct video *psVideo);

/*setup vc8000 v4l2 output(bitstream) plane
codec:
	V4L2_PIX_FMT_H264
	V4L2_PIX_FMT_JPEG
*/

int vc8000_v4l2_setup_output(
	struct video *psVideo,
	unsigned long codec,
	unsigned int size,
	int count
);

/* 
Setup capture(decoded) plane
pixel_format: 
	V4L2_PIX_FMT_NV12
	V4L2_PIX_FMT_ARGB32
	V4L2_PIX_FMT_RGB565
*/

int vc8000_v4l2_setup_capture(
	struct video *psVideo,
	int pixel_format,
	int extra_buf,
	int w,
	int h,
	struct video_pp *psVideoPP
);

//Release output and capture plane
void vc8000_v4l2_release_output(
	struct video *psVideo
);

void vc8000_v4l2_release_capture(
	struct video *psVideo
);

//queue output buffer
int vc8000_v4l2_queue_output(
	struct video *psVideo,
	int n,
	int length
);

//queue capture buffer
int vc8000_v4l2_queue_capture(
	struct video *psVideo,
	int n
);

//dequeue output buffer
int vc8000_v4l2_dequeue_output(
	struct video *psVideo,
	int *n
);

//dequeue capture buffer
int vc8000_v4l2_dequeue_capture(
	struct video *psVideo,
	int *n, 
	int *finished,
	unsigned int *bytesused
);

//set output/capture stream on/off
int vc8000_v4l2_stream(
	struct video *psVideo,
	enum v4l2_buf_type type, 
	int status
);

int vc8000_v4l2_stop_capture(
	struct video *psVideo
);

int vc8000_v4l2_stop_output(
	struct video *psVideo
);

//stop vc8000 v4l2 decode all plane
int vc8000_v4l2_stop(
	struct video *psVideo
);

int vc8000_v4l2_setup_pp(struct video *psVideo, int pixel_format, int w, int h, struct video_pp *psVideoPP);

void vc8000_pp_clean_framebuf
(
	uint32_t u32FramebufW,
	uint32_t u32FramebufH,
	uint32_t u32CleanRegionX,
	uint32_t u32CleanRegionY,
	uint32_t u32CleanRegionW,
	uint32_t u32CleanRegionH
);

#endif
