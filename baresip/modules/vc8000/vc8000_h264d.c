/**
 * @file vc8000.c: vc8000 for h264 hardware decoder
 *
 * Copyright (C) 2021 CHChen59
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <poll.h>
#include <fcntl.h>
#include <pthread.h>


#include <re.h>
#include <rem.h>
#include <baresip.h>

#include "vc8000.h"
#include "vc8000_v4l2.h"

#ifndef POLLRDNORM
#define POLLRDNORM	0x0040
#endif

#ifndef POLLRDBAND
#define POLLRDBAND	0x0080
#endif

#ifndef POLLWRNORM
#define POLLWRNORM	0x0100
#endif

#define MAX_VIDDEC_INSTANCE  4			//Maximum video decode instance

static char profile_level_id[256] = "";
struct viddec_state {
	struct video sVideo;	//vc8000 v4l2 instance

	int viddst_width;
	int viddst_height;
	int viddst_offset_x;
	int viddst_offset_y;

	struct mbuf *mb;
	bool got_keyframe;
	size_t frag_start;
	bool frag;

	uint16_t frag_seq;
	int32_t single_seq;
	bool packet_lost;

	struct {
		unsigned n_key;
		unsigned n_lost;
	} stats;

	bool stream_on;
	int iInstNo;
	bool bViddstUpdate;
};

struct viddec_instance{
	struct viddec_state *psViddecState;
};

struct viddec_instance_map{
	struct viddec_instance sViddecInst[MAX_VIDDEC_INSTANCE];
	struct vidsz sPanelSize;
	int iInstOnLine;
	int iMaxInst;
	int iWinN; //N by N window   
	bool bEnablePP;
};

struct viddec_instance_map sViddecInstMap; 
static pthread_mutex_t s_tResourceMutex = PTHREAD_MUTEX_INITIALIZER;

static uint32_t h264_packetization_mode(const char *fmtp)
{
	struct pl pl, mode;

	if (!fmtp)
		return 0;

	pl_set_str(&pl, fmtp);

	if (fmt_param_get(&pl, "packetization-mode", &mode))
		return pl_u32(&mode);

	return 0;
}

static inline int16_t seq_diff(uint16_t x, uint16_t y)
{
	return (int16_t)(y - x);
}


static inline void fragment_rewind(struct viddec_state *vds)
{
	vds->mb->pos = vds->frag_start;
	vds->mb->end = vds->frag_start;
}

static int update_all_instance_vidsiz(void)
{
	int i;
	struct viddec_state *vds;
	bool bCleanFrameBuff = false;

	
	for(i = 0; i < sViddecInstMap.iMaxInst; i ++)
	{
		vds = sViddecInstMap.sViddecInst[i].psViddecState;

		if(vds)
		{
			vds->viddst_width  = sViddecInstMap.sPanelSize.w / sViddecInstMap.iWinN;
			vds->viddst_height = sViddecInstMap.sPanelSize.h / sViddecInstMap.iWinN;

			vds->viddst_offset_x = (i % sViddecInstMap.iWinN) * vds->viddst_width;
			vds->viddst_offset_x = (i / sViddecInstMap.iWinN) * vds->viddst_height;			
			vds->bViddstUpdate = true;	
			bCleanFrameBuff = true;
		}
	}

	if((bCleanFrameBuff) && (sViddecInstMap.bEnablePP))
	{
		//frame buffer clean all
		vc8000_pp_clean_framebuf(sViddecInstMap.sPanelSize.w, sViddecInstMap.sPanelSize.h, 0, 0, sViddecInstMap.sPanelSize.w, sViddecInstMap.sPanelSize.h);
	}

	return 0;
}

static void destructor(void *arg)
{
	struct viddec_state *vds = arg;
	int i;
	int iNewWinN = 1;

	debug("vc8000: decoder stats"
	      " (keyframes:%u, lost_fragments:%u)\n",
	      vds->stats.n_key, vds->stats.n_lost);
	
	//stop vc8000 decode
	vc8000_v4l2_stop(&vds->sVideo);

#if 0
	//release capture and output resource 
	vc8000_v4l2_release_capture(&vds->sVideo);
	vc8000_v4l2_release_output(&vds->sVideo);
#endif

	//close vc8000 v4l2 device
	vc8000_v4l2_close(&vds->sVideo);



	//update instance map
	sViddecInstMap.sViddecInst[vds->iInstNo].psViddecState = NULL;
	sViddecInstMap.iInstOnLine --;

	//find maximum instance in used
	for(i = (sViddecInstMap.iMaxInst - 1); i >= 0; i --)
	{
		if(sViddecInstMap.sViddecInst[i].psViddecState)
			break;
	}

	if(sViddecInstMap.iWinN > 1)
		iNewWinN = sViddecInstMap.iWinN - 1;

	pthread_mutex_lock(&s_tResourceMutex);
	
	if(i <= ((iNewWinN * iNewWinN)- 1))
	{
		sViddecInstMap.iWinN = iNewWinN;
		//adjust instance image size and position
		update_all_instance_vidsiz();
	}
	else
	{
		if(sViddecInstMap.bEnablePP)
		{
			//clean view region
			vc8000_pp_clean_framebuf(sViddecInstMap.sPanelSize.w, sViddecInstMap.sPanelSize.h, vds->viddst_offset_x, vds->viddst_offset_y, vds->viddst_width, vds->viddst_height);
		}
	}
	pthread_mutex_unlock(&s_tResourceMutex);

	mem_deref(vds->mb);
}

static int poll_decode_done(
	struct video *psVideo,
	int *p_cap_index
)
{
	struct pollfd pfd;
	short revents;
	int ret, cap_index, finished, output_index;

	pfd.fd = psVideo->fd;
	pfd.events = POLLIN | POLLRDNORM | POLLOUT | POLLWRNORM |
		     POLLRDBAND | POLLPRI;

	ret = -1;
	cap_index = -1;
	output_index = -1;

	while (1) {
//		ret = poll(&pfd, 1, 2000);
		ret = poll(&pfd, 1, -1);

		if (!ret) {
			//timeout
			ret = -1;
			break;
		} else if (ret < 0) {
			warning("poll error");
			ret = -2;
			break;
		}

		revents = pfd.revents;

		if (revents & (POLLIN | POLLRDNORM)) {
			unsigned int bytesused;

			/* capture buffer is ready */

			ret = vc8000_v4l2_dequeue_capture(psVideo, &cap_index, &finished,
						    &bytesused);
			if (ret < 0)
				goto next_event;

			psVideo->cap_buf_flag[cap_index] = eV4L2_BUF_DEQUEUE;
			psVideo->total_captured++;

			//info("decoded frame %ld", vid->total_captured);

		}

next_event:

		if (revents & (POLLOUT | POLLWRNORM)) {

			ret = vc8000_v4l2_dequeue_output(psVideo, &output_index);
			if (ret < 0) {
				warning("dequeue output buffer fail");
			} else {
				psVideo->out_buf_flag[output_index] = eV4L2_BUF_DEQUEUE;
			}

			break;
			// dbg("dequeued output buffer %d", n);
		}
	}

//	printf("DDDDDDDDDDDDDD file: %s, line %d, cap_index %d \n", __FILE__, __LINE__, cap_index);
	*p_cap_index = cap_index;

	return ret;
}

static int vc8000_decode(
	struct viddec_state *st,
	struct vidframe *frame,
	bool *intra
)
{
	int n;
	struct video *psVideo = &st->sVideo;
	int err = -1;

	pthread_mutex_lock(&s_tResourceMutex);

	if(st->bViddstUpdate)
	{
		// update video size and poistion
		struct video_pp vpp;

		//Stop capture plane
		vc8000_v4l2_stop_capture(psVideo);
		
		//Setup new capture plane
		vpp.panel_w = sViddecInstMap.sPanelSize.w;
		vpp.panel_h = sViddecInstMap.sPanelSize.h;
		vpp.offset_x = st->viddst_offset_x;
		vpp.offset_y = st->viddst_offset_y;
		vpp.enabled = sViddecInstMap.bEnablePP;

		vc8000_v4l2_setup_capture(psVideo, V4L2_PIX_FMT_ARGB32, 2, st->viddst_width, st->viddst_height, &vpp);
		
		//capture plane stream on
		vc8000_v4l2_stream(psVideo, V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE, VIDIOC_STREAMON);
		
		st->bViddstUpdate = false;
	}

	pthread_mutex_unlock(&s_tResourceMutex);

	if(st->stream_on == false)
	{
		vc8000_v4l2_stream(psVideo, V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE, VIDIOC_STREAMON);
		vc8000_v4l2_stream(psVideo, V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE, VIDIOC_STREAMON);		

		st->stream_on = true;
	}

	//put capture dequeued buffer into queue
	for(n = 0; n < psVideo->cap_buf_cnt; n ++)
	{
		if(psVideo->cap_buf_flag[n] == eV4L2_BUF_DEQUEUE)
		{
			vc8000_v4l2_queue_capture(psVideo, n);
			psVideo->cap_buf_flag[n] = eV4L2_BUF_INQUEUE;
		}	
	}
	
	//Get output dequeued buffer 
	for(n = 0; n < psVideo->out_buf_cnt; n ++)
	{
		if(psVideo->out_buf_flag[n] == eV4L2_BUF_DEQUEUE)
			break;
	}

	if(n == psVideo->out_buf_cnt)
	{
		warning("No available output buffer to put bitstream \n");
		return EBUSY;
	}

	//fill bitstream to dequeued output buffer
	memcpy(psVideo->out_buf_addr[n], st->mb->buf, st->mb->end);

	//put dequeued output buffer into queue
	vc8000_v4l2_queue_output(psVideo, n, st->mb->end);
	psVideo->out_buf_flag[n] = eV4L2_BUF_INQUEUE;


	*intra = false;
	n = -1;
	poll_decode_done(psVideo, &n);
	
	if(n >= 0){
		//capture a deoced frame
		frame->data[0]     = (uint8_t *)psVideo->cap_buf_addr[n][0];
		frame->data[1]     = NULL;
		frame->data[2]     = NULL;
		frame->data[3]     = NULL;
		frame->linesize[0] = st->viddst_width * 4;
		frame->linesize[1] = 0;
		frame->linesize[2] = 0;
		frame->linesize[3] = 0;
		frame->size.w = st->viddst_width;
		frame->size.h = st->viddst_height;
		frame->fmt = VID_FMT_RGB32;
		err = 0;
	}

	return err;
}

//////////////////////////////////////////////////////////////////////////////////

int vc8000_decode_update(struct viddec_state **vdsp, const struct vidcodec *vc, const char *fmtp)
{
	struct viddec_state *vds;
	int err = 0;
	struct vidsz size = {0, 0};
	struct video_pp vpp;
	int iSlotIndex = -1;
	
	(void)vc;
	(void)fmtp;

	if (!vdsp)
		return EINVAL;

	vds = *vdsp;
	
	if(!vds)
	{
		int i;

		//Checking decode instance available or not
		if(sViddecInstMap.iInstOnLine + 1 > sViddecInstMap.iMaxInst)
		{
			return EBUSY;
		}

		//Find instance slot
		for( i = 0; i < sViddecInstMap.iMaxInst; i ++)
		{
			if(sViddecInstMap.sViddecInst[i].psViddecState == NULL)
				break;
		}

		iSlotIndex = i;

		vds = mem_zalloc(sizeof(*vds), destructor);
		if (!vds)
			return ENOMEM;
	}

	*vdsp = vds;
	
	if(vc8000_v4l2_open(&vds->sVideo) != 0)
	{
		return EBUSY;
	}

	if(vc8000_v4l2_setup_output(&vds->sVideo, V4L2_PIX_FMT_H264, STREAM_BUFFER_SIZE, 3) != 0)
	{
		vc8000_v4l2_close(&vds->sVideo);
		return EINVAL;
	}

	vpp.panel_w = sViddecInstMap.sPanelSize.w;
	vpp.panel_h = sViddecInstMap.sPanelSize.h;

	if(sViddecInstMap.iMaxInst == 1)
	{
		uint32_t offset;

		if (conf_get_vidsz(conf_cur(), "vc8000_viddst_size", &size) == 0) 
		{
			vds->viddst_width  = size.w;
			vds->viddst_height = size.h;
		}
		else
		{
			vds->viddst_width  = DEFAULT_VC8000_VIDDST_WIDTH;
			vds->viddst_height = DEFAULT_VC8000_VIDDST_HEIGHT;
		}

		vpp.offset_x = 0;
		vpp.offset_y = 0;

		if (conf_get_u32(conf_cur(), "vc8000_offset_x", &offset) == 0) 
		{
			vpp.offset_x = offset;
		}

		if (conf_get_u32(conf_cur(), "vc8000_offset_y", &offset) == 0) 
		{
			vpp.offset_y = offset;
		}
	}
	else {
		//auto size
		if(iSlotIndex >= 0)
		{
			int iNewInstOnLine = sViddecInstMap.iInstOnLine + 1;

			if(iNewInstOnLine >  (sViddecInstMap.iWinN * sViddecInstMap.iWinN))
			{
				sViddecInstMap.iWinN ++;

				pthread_mutex_lock(&s_tResourceMutex);
				//adjust instance image size and position
				update_all_instance_vidsiz();
				pthread_mutex_unlock(&s_tResourceMutex);

			}

			vds->viddst_width  = sViddecInstMap.sPanelSize.w / sViddecInstMap.iWinN;
			vds->viddst_height = sViddecInstMap.sPanelSize.h / sViddecInstMap.iWinN;

			vpp.offset_x = (iSlotIndex % sViddecInstMap.iWinN) * vds->viddst_width;
			vpp.offset_y = (iSlotIndex / sViddecInstMap.iWinN) * vds->viddst_height;			

			vds->viddst_offset_x = vpp.offset_x;
			vds->viddst_offset_y = vpp.offset_y;			
		}
	}

	vpp.enabled = sViddecInstMap.bEnablePP;

	if(vc8000_v4l2_setup_capture(&vds->sVideo, V4L2_PIX_FMT_ARGB32, 2, vds->viddst_width, vds->viddst_height, &vpp) != 0)
	{
		vc8000_v4l2_close(&vds->sVideo);
		return EINVAL;
	}

	if(iSlotIndex >= 0)
	{
		sViddecInstMap.sViddecInst[iSlotIndex].psViddecState = vds;
		sViddecInstMap.iInstOnLine ++;
		vds->iInstNo = iSlotIndex;
	}

	vds->single_seq = -1;
	vds->stream_on = false;
	vds->mb = mbuf_alloc(1024);
	return err;
}

int vc8000_h264_fmtp_enc(struct mbuf *mb, const struct sdp_format *fmt,
		  bool offer, void *arg)
{
	struct vc8000_vidcodec* psVC8KVideoCodec = (struct vc8000_vidcodec*)arg;
	const struct vidcodec *vc = &(psVC8KVideoCodec->vc);
	uint8_t profile_idc = 0x42; /* baseline profile */
	uint8_t profile_iop = 0xe0;
	uint8_t h264_level_idc = 0x1f;
	(void)offer;

	if (!mb || !fmt || !psVC8KVideoCodec)
		return 0;

	conf_get_str(conf_cur(), "vc8000_profile_level_id",				//can specify in config file for vc8000 h264 decoder
		     profile_level_id, sizeof(profile_level_id));

	if (str_isset(profile_level_id)) {
		struct pl prof;

		pl_set_str(&prof, profile_level_id);
		if (prof.l != 6) {
			warning("vc8000: invalid profile_level_id"
				" (%r) using default\n",
				profile_level_id);
			goto out;
		}

		prof.l = 2;
		profile_idc    = pl_x32(&prof); prof.p += 2;
		profile_iop    = pl_x32(&prof); prof.p += 2;
		h264_level_idc = pl_x32(&prof);
	}

 out:
	return mbuf_printf(mb, "a=fmtp:%s"
			   " %s"
			   ";profile-level-id=%02x%02x%02x"
			   "\r\n",
			   fmt->id, vc->variant,
			   profile_idc, profile_iop, h264_level_idc);
}

bool vc8000_h264_fmtp_cmp(const char *lfmtp, const char *rfmtp, void *arg)
{
	struct vc8000_vidcodec* psVC8KVideoCodec = (struct vc8000_vidcodec*)arg;
	const struct vidcodec *vc = &(psVC8KVideoCodec->vc);
	(void)lfmtp;

	if (!psVC8KVideoCodec)
		return false;

	return h264_packetization_mode(vc->variant) ==
		h264_packetization_mode(rfmtp);
}

int vc8000_decode_h264(
	struct viddec_state *st,
	struct vidframe *frame,
	bool *intra,
	bool marker,
	uint16_t seq,
	struct mbuf *src
)
{
	struct h264_nal_header h264_hdr;
	const uint8_t nal_seq[3] = {0, 0, 1};
	int err;

	if (!st || !frame || !intra || !src)
		return EINVAL;

	*intra = false;

//	printf("DDDDDDDDDD vc8000_decode_h264 start seq %d \n", seq);

	err = h264_nal_header_decode(&h264_hdr, src);
	if (err)
		return err;

//	if (h264_hdr.type == H264_NALU_SLICE && !st->got_keyframe) {
//		warning("vc8000: decoder waiting for keyframe\n");
//		return EPROTO;
//	}

	if (h264_hdr.f) {
		info("vc8000: H264 forbidden bit set!\n");
		return EBADMSG;
	}

	if (st->frag && h264_hdr.type != H264_NALU_FU_A) {
		warning("vc8000: fragment flag set, but new packet is not fragment type\n");
		fragment_rewind(st);
		st->frag = false;
		++st->stats.n_lost;
	}

#if 0
	static uint32_t s_u32SeqNo = 0;
	
	if((s_u32SeqNo + 1) != seq)
	{
		printf("DDDDDDDDDDDD vc8000d on nal type %d: packet lost \n", h264_hdr.type);
	}
	
	s_u32SeqNo = seq;
#endif

	/* handle NAL types */
	if (1 <= h264_hdr.type && h264_hdr.type <= 23) {
		//single packet type
		--src->pos;

		if(h264_hdr.type == H264_NALU_IDR_SLICE)
		{
			uint8_t *temp = mbuf_buf(src);
						
			if(temp && (*(temp + 1) & 0x80)) //check fist mb in IDR slice
			{
				st->packet_lost = false;
			}
			st->got_keyframe = true;
		}
		else
		{
			st->got_keyframe = false;
		}

		if((!marker) && (st->single_seq >= 0))
 		{
			if(seq_diff(st->single_seq, seq) != 1)
			{
				warning("vc8000: lost single NAL packet detected\n");
				st->packet_lost = true;
			}
		}
		
		st->single_seq = seq;
		
		/* prepend H.264 NAL start sequence */
		err  = mbuf_write_mem(st->mb, nal_seq, 3);

		err |= mbuf_write_mem(st->mb, mbuf_buf(src),
				      mbuf_get_left(src));
		if (err)
			goto out;
	}
	else if (H264_NALU_FU_A == h264_hdr.type) {
		struct h264_fu fu;

		err = h264_fu_hdr_decode(&fu, src);
		if (err)
			return err;
		h264_hdr.type = fu.type;

		st->single_seq = -1;

		if (fu.s) {
			if (st->frag) {
				warning("vc8000: (seq %d) start: lost fragments;"
				      " ignoring previous NAL\n",seq);
				fragment_rewind(st);
				st->packet_lost = true;
				++st->stats.n_lost;
			}

			if(h264_hdr.type == H264_NALU_IDR_SLICE)
			{
				uint8_t *temp = mbuf_buf(src);
				
				if(temp && (*(temp) & 0x80)) //check fist mb in IDR slice
				{
					st->packet_lost = false;
				}

				st->got_keyframe = true;
			}
			else
			{
				st->got_keyframe = false;
			}

			st->frag_start = st->mb->pos;
			st->frag = true;

			/* prepend H.264 NAL start sequence */
			mbuf_write_mem(st->mb, nal_seq, 3);

			/* encode NAL header back to buffer */
			err = h264_nal_header_encode(st->mb, &h264_hdr);
			if (err){
				warning("vc8000: (seq %d) lost fragments; unable encode NAL header\n", seq);
				st->packet_lost = true;
				st->frag_seq = seq;
				goto out;
			}
		}
		else {
			if (!st->frag) {
				warning("vc8000:(seq %d) ignoring fragment"
				      " (nal=%u)\n", seq, fu.type);
				++st->stats.n_lost;
				st->packet_lost = true;
				st->frag_seq = seq;
				return 0;
			}

			if (seq_diff(st->frag_seq, seq) != 1) {
				warning("vc8000: (seq %d) lost fragments detected\n", seq);
				fragment_rewind(st);
				st->frag = false;
				++st->stats.n_lost;
				st->packet_lost = true;
				st->frag_seq = seq;
				return 0;
			}
		}

		st->frag_seq = seq;

		err = mbuf_write_mem(st->mb, mbuf_buf(src),
				     mbuf_get_left(src));
		if (err)
			goto out;

		if (fu.e)
			st->frag = false;

	}
	else if (H264_NALU_STAP_A == h264_hdr.type) {
		st->single_seq = -1;

		while (mbuf_get_left(src) >= 2) {

			const uint16_t len = ntohs(mbuf_read_u16(src));
			struct h264_nal_header lhdr;

			if (mbuf_get_left(src) < len)
				return EBADMSG;

			err = h264_nal_header_decode(&lhdr, src);
			if (err)
				return err;

			--src->pos;

			if(lhdr.type == H264_NALU_IDR_SLICE)
			{
				uint8_t *temp = mbuf_buf(src);
				
				if(temp && (*(temp) & 0x80)) //check fist mb in IDR slice
				{
					printf("DDDDDDDDDDDD Got new IDR first slice on stap a packet\n");
					st->packet_lost = false;
				}
				st->got_keyframe = true;
			}
			else
			{
				st->got_keyframe = false;
			}


			err  = mbuf_write_mem(st->mb, nal_seq, 3);
			err |= mbuf_write_mem(st->mb, mbuf_buf(src), len);
			if (err)
				goto out;

			src->pos += len;
		}
	}
	else {
		st->single_seq = -1;
		warning("vc8000: decode: unknown NAL type %u\n",
			h264_hdr.type);
		return EBADMSG;
	}

	if (!marker) {

		if (st->mb->end > STREAM_BUFFER_SIZE) {
			warning("vc8000: decode buffer size exceeded\n");
			err = ENOMEM;
			goto out;
		}

		return 0;
	}
	else
	{
		//marker bit is set. end packet of a frame.
		if(st->packet_lost)
		{
			//packet lost. will discard frames util new IDR frame
			err = EPROTO;
			goto out;

		}
	}

	if (st->frag) {
		err = EPROTO;
		goto out;
	}

	if(st->got_keyframe)
		*intra = true;
	
	err = vc8000_decode(st, frame, intra);
 out:

	mbuf_rewind(st->mb);
	st->frag = false;

	return err;
}

int vc8000_init(void)
{
	bool bAutoSize = false;

	memset(&sViddecInstMap, 0, sizeof(struct viddec_instance_map));

	conf_get_bool(conf_cur(), "vc8000_viddst_auto_size", &bAutoSize);

	if (bAutoSize == false)
	{
		sViddecInstMap.iMaxInst = 1;
	}
	else
	{
		sViddecInstMap.iMaxInst = MAX_VIDDEC_INSTANCE;
	}

	if (conf_get_vidsz(conf_cur(), "vc8000_panel_size", &sViddecInstMap.sPanelSize) != 0) 
	{
		sViddecInstMap.sPanelSize.w = DEFAULT_VC8000_VIDDST_WIDTH;
		sViddecInstMap.sPanelSize.h = DEFAULT_VC8000_VIDDST_HEIGHT;		
	}

	conf_get_bool(conf_cur(), "vc8000_pp_enable", &sViddecInstMap.bEnablePP); 

	sViddecInstMap.iWinN = 1;

	return 0;
}
