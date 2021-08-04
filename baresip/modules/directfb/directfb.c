/**
 * @file directfb.c  DirectFB video display module
 *
 * Copyright (C) 2010 Alfred E. Heggestad
 * Copyright (C) 2013 Andreas Shimokawa <andi@fischlustig.de>
 */
#include <re.h>
#include <rem.h>
#include <baresip.h>
#include <directfb.h>


struct vidisp_st {
	struct vidsz size;             /**< Current size          */
	IDirectFBWindow *window;       /**< DirectFB Window       */
	IDirectFBSurface *surface;     /**< Surface for pixels    */
	IDirectFBDisplayLayer *layer;  /**< Display layer         */
};


static IDirectFB *dfb;
static struct vidisp *vid;


static void destructor(void *arg)
{
	struct vidisp_st *st = arg;

	if (st->surface)
		st->surface->Release(st->surface);
	if (st->window)
		st->window->Release(st->window);
	if (st->layer)
		st->layer->Release(st->layer);
}


static int alloc(struct vidisp_st **stp, const struct vidisp *vd,
		 struct vidisp_prm *prm, const char *dev,
		 vidisp_resize_h *resizeh, void *arg)
{
	struct vidisp_st *st;
	int err = 0;

	/* Not used by DirectFB */
	(void) prm;
	(void) dev;
	(void) resizeh;
	(void) arg;

	st = mem_zalloc(sizeof(*st), destructor);
	if (!st)
		return ENOMEM;

	dfb->GetDisplayLayer(dfb, DLID_PRIMARY, &st->layer);

	if (err)
		mem_deref(st);
	else
		*stp = st;

	return err;
}


static int display(struct vidisp_st *st, const char *title,
		   const struct vidframe *frame, uint64_t timestamp)
{
	void *pixels;
	int pitch, i;
	unsigned h;
	uint8_t *p;
	bool packed_color;
	(void) title;
	(void) timestamp;

	if (!vidsz_cmp(&st->size, &frame->size)) {
		if (st->size.w && st->size.h) {
			info("directfb: reset: %u x %u ---> %u x %u\n",
			     st->size.w, st->size.h,
			     frame->size.w, frame->size.h);
		}

		if (st->surface) {
			st->surface->Release(st->surface);
			st->surface = NULL;
		}
		if (st->window) {
			st->window->Release(st->window);
			st->window = NULL;
		}
	}

	if (!st->window) {
		DFBWindowDescription desc;

	//modify by CHChen59: added window position setting
		desc.flags = DWDESC_POSX|DWDESC_POSY| DWDESC_WIDTH|DWDESC_HEIGHT|DWDESC_PIXELFORMAT;
		desc.width = frame->size.w;
		desc.height = frame->size.h;
//		desc.width = 160;
//		desc.height = 120;
		desc.posx = 0;
		desc.posy = 0;
		
	/////////////////////////////////////////////////////////////////////////////////////////
#if 1
		if(frame->fmt == VID_FMT_RGB32)
			desc.pixelformat = DSPF_RGB32;
		else if(frame->fmt == VID_FMT_ARGB)
			desc.pixelformat = DSPF_ARGB;
		else if(frame->fmt == VID_FMT_YUV420P)
			desc.pixelformat = DSPF_I420;
		else if(frame->fmt == VID_FMT_YUYV422)
			desc.pixelformat = DSPF_YUY2;
#else
			//DSPF_RGB16
			desc.pixelformat = DSPF_NV12;
#endif

		st->layer->CreateWindow(st->layer, &desc, &st->window);

		st->size = frame->size;
		st->window->SetOpacity(st->window, 0xff);
		st->window->GetSurface(st->window, &st->surface);
	}

	st->surface->Lock(st->surface, DSLF_WRITE, &pixels, &pitch);

	p = pixels;
	
	//modify by CHChen59: add packed color checking
	if(!frame->data[1])
		packed_color = true;
	else
		packed_color = false;

#if 1		
	if(packed_color) {
		//packet
		uint32_t u32TotalCpyBytes = 0;
		const uint8_t *s   = frame->data[0];
#if 0
//		const uint32_t linesize = 640;
		//testing code. force to NV12 color case
		memcpy(p, s, (640*480)*3/2);

//		for (h = 0; h < frame->size.h; h ++) {

//			memcpy(p, s, linesize);

//			s += linesize;
//			p += linesize;
//			u32TotalCpyBytes += linesize;
//		}

#else

		const uint32_t linesize = frame->linesize[0];
		for (h = 0; h < frame->size.h; h ++) {

			memcpy(p, s, linesize);

			s += linesize;
			p += linesize;
			u32TotalCpyBytes += linesize;
		}
#endif
	}
	else {
		//planar color type
		for (i=0; i<3; i++) {

			const uint8_t *s   = frame->data[i];
			const unsigned stp = frame->linesize[0] / frame->linesize[i];
			const unsigned sz  = frame->size.w / stp;

			for (h = 0; h < frame->size.h; h += stp) {

				memcpy(p, s, sz);

				s += frame->linesize[i];
				p += (pitch / stp);
			}
		}
	}
#endif
	//////////////////////////////////////////////
	st->surface->Unlock(st->surface);

	/* Update the screen! */
	st->surface->Flip(st->surface, 0, 0);

	return 0;
}


static void hide(struct vidisp_st *st)
{
	if (!st || !st->window)
		return;

	st->window->SetOpacity(st->window, 0x00);
}


static int module_init(void)
{
	int err = 0;
	DFBResult ret;

	ret = DirectFBInit(NULL, NULL);
	if (ret) {
		DirectFBError("DirectFBInit() failed", ret);
		return (int) ret;
	}

	ret = DirectFBCreate(&dfb);
	if (ret) {
		DirectFBError("DirectFBCreate() failed", ret);
		return (int) ret;
	}

	err = vidisp_register(&vid, baresip_vidispl(),
			      "directfb", alloc, NULL, display, hide);
	if (err)
		return err;

	return 0;
}


static int module_close(void)
{
	vid = mem_deref(vid);

	if (dfb) {
		dfb->Release(dfb);
		dfb = NULL;
	}

	return 0;
}


EXPORT_SYM const struct mod_export DECL_EXPORTS(directfb) = {
	"directfb",
	"vidisp",
	module_init,
	module_close
};
