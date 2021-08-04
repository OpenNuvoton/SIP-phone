/**
 * @file vc8000_v4l2.c: vc8000 for v4l2 driver
 *
 * Copyright (C) 2021 CHChen59
 */
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <inttypes.h>
#include <errno.h>

#include <re.h>
#include <baresip.h>

#define VC8000_DEV_NAME "/dev/video0"

#include "vc8000_v4l2.h"
#include "msm-v4l2-controls.h"

void baresip_module_vc8000_SetFB(uint8_t *pu8FrameBufAddr, uint32_t u32FrameBufSize, uint32_t u32Planes);


static char *dbg_type[2] = {"OUTPUT", "CAPTURE"};
static char *dbg_status[2] = {"ON", "OFF"};

static uint8_t *s_pu8FrameBufAddr = NULL;
static uint32_t s_u32FrameBufSize = 0;
static uint32_t s_u32FrameBufPlanes = 0;

////////////////////////////////////////////////////////////////////////////////////////
static int v4l2_queue_buf(
	struct video *psVideo,
	int n,
	int l1,
	int l2,
	int type,
	int nplanes
)
{
	struct video *vid = psVideo;
	struct v4l2_buffer buf;
	struct v4l2_plane planes[2];
	int ret;

	memzero(buf);
	memset(planes, 0, sizeof(planes));
	buf.type = type;
	buf.memory = V4L2_MEMORY_MMAP;
	buf.index = n;
	buf.length = nplanes;
	buf.m.planes = planes;

	buf.m.planes[0].bytesused = l1;
	buf.m.planes[1].bytesused = l2;

	buf.m.planes[0].data_offset = 0;
	buf.m.planes[1].data_offset = 0;

	if (type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
		buf.m.planes[0].length = vid->cap_buf_size[0];
	} else {
		buf.m.planes[0].length = vid->out_buf_size;
		if (l1 == 0)
			buf.flags |= V4L2_QCOM_BUF_FLAG_EOS;
	}

	ret = ioctl(vid->fd, VIDIOC_QBUF, &buf);
	if (ret) {
		warning("Failed to queue buffer (index=%d) on %s (ret:%d)",
		    buf.index,
		    dbg_type[type==V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE], ret);
		return -1;
	}

//	dbg("  Queued buffer on %s queue with index %d",
//	    dbg_type[type==V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE], buf.index);

	return 0;
}

static int v4l2_dequeue_buf(
	struct video *psVideo,
	struct v4l2_buffer *buf
)
{
	struct video *vid = psVideo;
	int ret;

	ret = ioctl(vid->fd, VIDIOC_DQBUF, buf);
	if (ret < 0) {
		warning("Failed to dequeue buffer (%d)", -errno);
		return -errno;
	}

//	dbg("Dequeued buffer on %s queue with index %d (flags:%x, bytesused:%d)",
//	    dbg_type[buf->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE],
//	    buf->index, buf->flags, buf->m.planes[0].bytesused);

	return 0;
}


/////////////////////////////////////////////////////////////////////////////////////////////////

/*
 *  Private V4L2 post processing ioctl for VC8K
 */
struct vc8k_pp_params {
	int  enable_pp;
	void  *frame_buf_vaddr;          /* virtual address of frame buffer           */
	int   frame_buff_size;
	int   frame_buf_w;               /* width of frame buffer width               */
	int   frame_buf_h;               /* height of frame buffer                    */
	int   img_out_x;                 /* image original point(x,y) on frame buffer */
	int   img_out_y;                 /* image original point(x,y) on frame buffer */
	int   img_out_w;                 /* image output width on frame buffer        */
	int   img_out_h;                 /* image output height on frame buffer       */
	int   img_out_fmt;               /* image output format                       */
	int   rotation;
};

#define VC8KIOC_PP_SET_CONFIG	_IOW ('v', 91, struct vc8k_pp_params)
#define VC8KIOC_PP_GET_CONFIG	_IOW ('v', 92, struct vc8k_pp_params)

#define PP_ROTATION_NONE                                0U
#define PP_ROTATION_RIGHT_90                            1U
#define PP_ROTATION_LEFT_90                             2U
#define PP_ROTATION_HOR_FLIP                            3U
#define PP_ROTATION_VER_FLIP                            4U
#define PP_ROTATION_180                                 5U



int vc8000_v4l2_open(struct video *psVideo)
{
	struct v4l2_capability cap;
	int ret;

	psVideo->fd = open(VC8000_DEV_NAME, O_RDWR, 0);
	if (psVideo->fd < 0) {
		warning("Failed to open video decoder: %s \n", VC8000_DEV_NAME);
		return -1;
	}

	memzero(cap);
	ret = ioctl(psVideo->fd, VIDIOC_QUERYCAP, &cap);
	if (ret) {
		warning("Failed to verify capabilities \n");
		return -1;
	}

	info("caps (%s): driver=\"%s\" bus_info=\"%s\" card=\"%s\" fd=0x%x \n",
	     VC8000_DEV_NAME, cap.driver, cap.bus_info, cap.card, psVideo->fd);

	if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE_MPLANE) ||
	    !(cap.capabilities & V4L2_CAP_VIDEO_OUTPUT_MPLANE) ||
	    !(cap.capabilities & V4L2_CAP_STREAMING)) {
		warning("Insufficient capabilities for video device (is %s correct?) \n", VC8000_DEV_NAME);
		return -1;
	}

	return 0;
}

void vc8000_v4l2_close(struct video *psVideo)
{
	printf("DDDDDDDDDDDDDD vc8000_v4l2_close video fd %x \n", psVideo->fd);
	close(psVideo->fd);
}

//setup output(bitstream) plane
int vc8000_v4l2_setup_output(
	struct video *psVideo,
	unsigned long codec,
	unsigned int size,
	int count
)
{
	struct video *vid = psVideo;
	struct v4l2_format fmt;
	struct v4l2_requestbuffers reqbuf;
	struct v4l2_buffer buf;
	struct v4l2_plane planes[OUT_PLANES];
	int ret;
	int n;

	memzero(fmt);
	fmt.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
	fmt.fmt.pix_mp.width = MAX_H264_WIDTH / 2;
	fmt.fmt.pix_mp.height = MAX_H264_HEIGHT / 2;
	fmt.fmt.pix_mp.pixelformat = codec;

	ret = ioctl(vid->fd, VIDIOC_S_FMT, &fmt);
	if (ret) {
		warning("Failed to set format on OUTPUT (%s) \n", strerror(errno));
		return -1;
	}

	info("Setup decoding OUTPUT buffer size=%u (requested=%u) \n",
	    fmt.fmt.pix_mp.plane_fmt[0].sizeimage, size);

	vid->out_buf_size = fmt.fmt.pix_mp.plane_fmt[0].sizeimage;

	memzero(reqbuf);
	reqbuf.count = count;
	reqbuf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
	reqbuf.memory = V4L2_MEMORY_MMAP;

	ret = ioctl(vid->fd, VIDIOC_REQBUFS, &reqbuf);
	if (ret) {
		warning("REQBUFS failed on OUTPUT queue \n");
		return -1;
	}

	vid->out_buf_cnt = reqbuf.count;

	info("Number of video decoder OUTPUT buffers is %d (requested %d) \n",
	    vid->out_buf_cnt, count);

	for (n = 0; n < vid->out_buf_cnt; n++) {
		memzero(buf);
		memset(planes, 0, sizeof(planes));
		buf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
		buf.memory = V4L2_MEMORY_MMAP;
		buf.index = n;
		buf.m.planes = planes;
		buf.length = OUT_PLANES;

		ret = ioctl(vid->fd, VIDIOC_QUERYBUF, &buf);
		if (ret != 0) {
			warning("QUERYBUF failed on OUTPUT buffer \n");
			return -1;
		}

		vid->out_buf_off[n] = buf.m.planes[0].m.mem_offset;
		vid->out_buf_size = buf.m.planes[0].length;

		vid->out_buf_addr[n] = mmap(NULL, buf.m.planes[0].length,
					    PROT_READ | PROT_WRITE, MAP_SHARED,
					    vid->fd,
					    buf.m.planes[0].m.mem_offset);

		if (vid->out_buf_addr[n] == MAP_FAILED) {
			warning("Failed to MMAP OUTPUT buffer \n");
			return -1;
		}

		vid->out_buf_flag[n] = eV4L2_BUF_DEQUEUE;
	}

	info("Succesfully mmapped %d OUTPUT buffers \n", n);

	return 0;
}

void vc8000_v4l2_release_output(
	struct video *psVideo
)
{
	struct video *vid = psVideo;
	int n;
	
	for(n = 0; n < vid->out_buf_cnt; n++)
	{
		if(vid->out_buf_addr[n])
		{
			if(munmap(vid->out_buf_addr[n], vid->out_buf_size) != 0)
			{
				warning("unable unmap output memeory \n", vid->out_buf_addr[n]);
			}
			vid->out_buf_addr[n] = NULL;			
		}

	}

}

/* 
Setup capture(decoded) plane
pixel_format: 
	V4L2_PIX_FMT_NV12
	V4L2_PIX_FMT_ARGB32
	V4L2_PIX_FMT_RGB565
*/

int vc8000_v4l2_setup_capture(struct video *psVideo, int pixel_format, int extra_buf, int w, int h, struct video_pp *psVideoPP)
{
	struct video *vid = psVideo;
	struct v4l2_format fmt;
	struct v4l2_requestbuffers reqbuf;
	struct v4l2_buffer buf;
	struct v4l2_plane planes[CAP_PLANES];
	int ret;
	int n;
	struct vc8k_pp_params  sVC8K_PP;

	if(psVideoPP)
	{
#if 1
		sVC8K_PP.enable_pp = true; //psVideoPP->enabled;
		sVC8K_PP.frame_buff_size = s_u32FrameBufSize;
		sVC8K_PP.frame_buf_vaddr= s_pu8FrameBufAddr;
		sVC8K_PP.frame_buf_w = psVideoPP->panel_w;
		sVC8K_PP.frame_buf_h = psVideoPP->panel_h;
		sVC8K_PP.img_out_x = psVideoPP->offset_x;
		sVC8K_PP.img_out_y = psVideoPP->offset_y;
		sVC8K_PP.img_out_w = w;
		sVC8K_PP.img_out_h = h;
		sVC8K_PP.rotation = PP_ROTATION_NONE;
		sVC8K_PP.img_out_fmt = pixel_format;
		ioctl(psVideo->fd, VC8KIOC_PP_SET_CONFIG, sVC8K_PP);

		printf("DDDDD ======================== \n");
		printf("DDDDD vc8000_v4l2_setup_capture sVC8K_PP.enable_pp %x \n", sVC8K_PP.enable_pp);
		printf("DDDDD vc8000_v4l2_setup_capture sVC8K_PP.frame_buf_vaddr %x \n", sVC8K_PP.frame_buf_vaddr);
		printf("DDDDD vc8000_v4l2_setup_capture sVC8K_PP.frame_buff_size %d \n", sVC8K_PP.frame_buff_size);
		printf("DDDDD vc8000_v4l2_setup_capture sVC8K_PP.frame_buf_w %d \n", sVC8K_PP.frame_buf_w);
		printf("DDDDD vc8000_v4l2_setup_capture sVC8K_PP.frame_buf_h %d \n", sVC8K_PP.frame_buf_h);
		printf("DDDDD vc8000_v4l2_setup_capture sVC8K_PP.img_out_x %d \n", sVC8K_PP.img_out_x);
		printf("DDDDD vc8000_v4l2_setup_capture sVC8K_PP.img_out_y %d \n", sVC8K_PP.img_out_y);
		printf("DDDDD vc8000_v4l2_setup_capture sVC8K_PP.img_out_w %d \n", sVC8K_PP.img_out_w);
		printf("DDDDD vc8000_v4l2_setup_capture sVC8K_PP.img_out_h %d \n", sVC8K_PP.img_out_h);
		printf("DDDDD vc8000_v4l2_setup_capture sVC8K_PP.img_out_fmt %d \n", sVC8K_PP.img_out_fmt);
#endif
	}

	fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;

	if(psVideoPP->enabled)
	{
		//driver directly output framebuffer, so set tiny image size to save memory usage
		fmt.fmt.pix_mp.height = 48;
		fmt.fmt.pix_mp.width = 48;
	}
	else
	{
		fmt.fmt.pix_mp.height = h;
		fmt.fmt.pix_mp.width = w;
	}

	info("video_setup_capture: %dx%d\n", w, h);
	fmt.fmt.pix_mp.pixelformat = pixel_format;
	ret = ioctl(vid->fd, VIDIOC_S_FMT, &fmt);
	if (ret) {
		warning("Failed to set format (%dx%d) \n", w, h);
		return -1;
	}

	vid->cap_w = fmt.fmt.pix_mp.width;
	vid->cap_h = fmt.fmt.pix_mp.height;

	vid->cap_buf_size[0] = fmt.fmt.pix_mp.plane_fmt[0].sizeimage;
//	vid->cap_buf_size[1] = fmt.fmt.pix_mp.plane_fmt[1].sizeimage;

	vid->cap_buf_cnt = 4 + extra_buf;
	vid->cap_buf_cnt_min = 4;
	vid->cap_buf_queued = 0;

//	info("video decoder buffer parameters: %dx%d plane[0]=%d plane[1]=%d",
//	    fmt.fmt.pix_mp.width, fmt.fmt.pix_mp.height,
//	    vid->cap_buf_size[0], vid->cap_buf_size[1]);

	info("video decoder buffer parameters: %dx%d plane[0]=%d \n",
	    fmt.fmt.pix_mp.width, fmt.fmt.pix_mp.height,
	    vid->cap_buf_size[0]);

	memzero(reqbuf);
	reqbuf.count = vid->cap_buf_cnt;
	reqbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	reqbuf.memory = V4L2_MEMORY_MMAP;

	ret = ioctl(vid->fd, VIDIOC_REQBUFS, &reqbuf);
	if (ret != 0) {
		warning("REQBUFS failed on CAPTURE queue (%s) \n", strerror(errno));
		return -1;
	}

	info("Number of CAPTURE buffers is %d (requested %d, extra %d) \n",
	    reqbuf.count, vid->cap_buf_cnt, extra_buf);

	vid->cap_buf_cnt = reqbuf.count;

	for (n = 0; n < vid->cap_buf_cnt; n++) {
		memzero(buf);
		memset(planes, 0, sizeof(planes));
		buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
		buf.memory = V4L2_MEMORY_MMAP;
		buf.index = n;
		buf.m.planes = planes;
		buf.length = CAP_PLANES;

		ret = ioctl(vid->fd, VIDIOC_QUERYBUF, &buf);
		if (ret != 0) {
			warning("QUERYBUF failed on CAPTURE queue (%s) \n",
			    strerror(errno));
			return -1;
		}

		vid->cap_buf_off[n][0] = buf.m.planes[0].m.mem_offset;

		vid->cap_buf_addr[n][0] = mmap(NULL, buf.m.planes[0].length,
					       PROT_READ | PROT_WRITE,
					       MAP_SHARED,
					       vid->fd,
					       buf.m.planes[0].m.mem_offset);

		if (vid->cap_buf_addr[n][0] == MAP_FAILED) {
			warning("Failed to MMAP CAPTURE buffer on plane0 \n");
			return -1;
		}

		vid->cap_buf_flag[n] = eV4L2_BUF_DEQUEUE;
		vid->cap_buf_size[0] = buf.m.planes[0].length;
	}

	info("Succesfully mmapped %d CAPTURE buffers \n", n);

	return 0;
}

void vc8000_v4l2_release_capture(
	struct video *psVideo
)
{
	struct video *vid = psVideo;
	int n;
	
	for(n = 0; n < vid->cap_buf_cnt; n++)
	{
		if(vid->cap_buf_addr[n][0])
		{
			if(munmap(vid->cap_buf_addr[n][0], vid->cap_buf_size[0]) != 0)
			{
				warning("unable unmap capture memeory \n", vid->cap_buf_addr[n][0]);
			}
			vid->cap_buf_addr[n][0] = NULL;			
		}
	}
}

int vc8000_v4l2_queue_output(
	struct video *psVideo,
	int n,
	int length
)
{
	struct video *vid = psVideo;

	if (n >= vid->out_buf_cnt) {
		warning("Tried to queue a non exisiting buffer");
		return -1;
	}

	return v4l2_queue_buf(psVideo, n, length, 0,
			       V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE, OUT_PLANES);
}

int vc8000_v4l2_queue_capture(
	struct video *psVideo,
	int n
)
{
	struct video *vid = psVideo;

	if (n >= vid->cap_buf_cnt) {
		warning("Tried to queue a non exisiting buffer");
		return -1;
	}

//	return v4l2_queue_buf(psVideo, n, vid->cap_buf_size[0], vid->cap_buf_size[1],
	return v4l2_queue_buf(psVideo, n, vid->cap_buf_size[0], vid->cap_buf_size[0],
			       V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE, CAP_PLANES);
}

int vc8000_v4l2_dequeue_output(
	struct video *psVideo,
	int *n
)
{
	struct v4l2_buffer buf;
	struct v4l2_plane planes[OUT_PLANES];
	int ret;

	memzero(buf);
	buf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
	buf.memory = V4L2_MEMORY_MMAP;
	buf.m.planes = planes;
	buf.length = OUT_PLANES;

	ret = v4l2_dequeue_buf(psVideo, &buf);
	if (ret < 0)
		return ret;

	*n = buf.index;

	return 0;
}

int vc8000_v4l2_dequeue_capture(
	struct video *psVideo,
	int *n, 
	int *finished,
	unsigned int *bytesused
)
{
	struct v4l2_buffer buf;
	struct v4l2_plane planes[CAP_PLANES];

	memzero(buf);
	buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	buf.memory = V4L2_MEMORY_MMAP;
	buf.m.planes = planes;
	buf.length = CAP_PLANES;

	if (v4l2_dequeue_buf(psVideo, &buf))
		return -1;

	*finished = 0;

	if (buf.flags & V4L2_QCOM_BUF_FLAG_EOS ||
	    buf.m.planes[0].bytesused == 0)
		*finished = 1;

	*bytesused = buf.m.planes[0].bytesused;
	*n = buf.index;

	return 0;
}

int vc8000_v4l2_stream(
	struct video *psVideo,
	enum v4l2_buf_type type, 
	int status
)
{
	struct video *vid = psVideo;
	int ret;

	ret = ioctl(vid->fd, status, &type);
	if (ret) {
		warning("Failed to change streaming (type=%s, status=%s)",
		    dbg_type[type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE],
		    dbg_status[status == VIDIOC_STREAMOFF]);
		return -1;
	}

	info("Stream %s on %s queue", dbg_status[status==VIDIOC_STREAMOFF],
	    dbg_type[type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE]);

	return 0;
}

int vc8000_v4l2_stop_capture(
	struct video *psVideo
)
{
	int ret;
	struct v4l2_requestbuffers reqbuf;
	struct video *vid = psVideo;

	ret = vc8000_v4l2_stream(psVideo, V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE,
			   VIDIOC_STREAMOFF);
	if (ret < 0)
		warning("STREAMOFF CAPTURE queue failed (%s)", strerror(errno));


	memzero(reqbuf);
	reqbuf.memory = V4L2_MEMORY_MMAP;
	reqbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;

	ret = ioctl(vid->fd, VIDIOC_REQBUFS, &reqbuf);
	if (ret < 0) {
		warning("REQBUFS with count=0 on CAPTURE queue failed (%s)",
		    strerror(errno));
		return -1;
	}

	vc8000_v4l2_release_capture(psVideo);

	return 0;
}

int vc8000_v4l2_stop_output(
	struct video *psVideo
)
{
	int ret;
	struct v4l2_requestbuffers reqbuf;
	struct video *vid = psVideo;

	ret = vc8000_v4l2_stream(psVideo, V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE,
			   VIDIOC_STREAMOFF);
	if (ret < 0)
		warning("STREAMOFF OUTPUT queue failed (%s)", strerror(errno));


	memzero(reqbuf);
	reqbuf.memory = V4L2_MEMORY_MMAP;
	reqbuf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;

	ret = ioctl(vid->fd, VIDIOC_REQBUFS, &reqbuf);
	if (ret < 0) {
		warning("REQBUFS with count=0 on OUTPUT queue failed (%s)",
		    strerror(errno));
		return -1;
	}

	vc8000_v4l2_release_output(psVideo);

	return 0;
}

int vc8000_v4l2_stop(
	struct video *psVideo
)
{
	vc8000_v4l2_stop_capture(psVideo);
	vc8000_v4l2_stop_output(psVideo);

	return 0;
}

void baresip_module_vc8000_SetFB(uint8_t *pu8FrameBufAddr, uint32_t u32FrameBufSize, uint32_t u32Planes)
{
	s_pu8FrameBufAddr = pu8FrameBufAddr;
	s_u32FrameBufPlanes = u32Planes;
	s_u32FrameBufSize = u32FrameBufSize;
}

void vc8000_pp_clean_framebuf
(
	uint32_t u32FramebufW,
	uint32_t u32FramebufH,
	uint32_t u32CleanRegionX,
	uint32_t u32CleanRegionY,
	uint32_t u32CleanRegionW,
	uint32_t u32CleanRegionH
)
{
	uint32_t u32Line;
	uint32_t u32Plane;
	uint32_t u32PixelBytes = (s_u32FrameBufSize / s_u32FrameBufPlanes) / (u32FramebufW * u32FramebufH);
	uint8_t *pu8CleanAddr;
	uint8_t *pu8PlanAddr;

	for (u32Plane = 0 ; u32Plane < s_u32FrameBufPlanes; u32Plane ++)
	{
		pu8PlanAddr = s_pu8FrameBufAddr + (u32Plane * (u32FramebufW * u32FramebufH) * u32PixelBytes);

		for(u32Line = u32CleanRegionY; u32Line < (u32CleanRegionY + u32CleanRegionH); u32Line ++)
		{
			pu8CleanAddr = pu8PlanAddr + (((u32Line * u32FramebufW) + u32CleanRegionX) * u32PixelBytes);
			memset(pu8CleanAddr, 0, u32CleanRegionW * u32PixelBytes);
		}
	}
}




