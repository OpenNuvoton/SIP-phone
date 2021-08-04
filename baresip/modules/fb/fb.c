/**
 * @file fb.c: framebuffer module for ma35d1 
 *
 * Copyright (C) 2021 CHChen59
 */
 
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <linux/fb.h>
#include <dlfcn.h>
 
#include <re.h>
#include <rem.h>
#include <baresip.h>

#define FB_DEV_NAME "/dev/fb0"

struct vidisp_st {
	uint32_t u32FrameBufSize;
	uint8_t *pu8FrameBufAddr;
};

static int fb_fd;
static struct vidisp *vid;


static void *dl_handle;
static void (*dl_set_fb)(uint8_t *, uint32_t, uint32_t);
static char *dl_error;

static uint32_t s_u32FrameBufSize;
static uint8_t *s_pu8FrameBufAddr;


static void destructor(void *arg)
{
	struct vidisp_st *st = arg;

#if 0
	if(st->pu8FrameBufAddr)
	{
		munmap(st->pu8FrameBufAddr, st->u32FrameBufSize);
		st->pu8FrameBufAddr = NULL;
	}
#endif
}

static int alloc(struct vidisp_st **stp, const struct vidisp *vd,
		 struct vidisp_prm *prm, const char *dev,
		 vidisp_resize_h *resizeh, void *arg)
{
	struct vidisp_st *st;

    
	/* Not used by FB */
	(void) prm;
	(void) dev;
	(void) resizeh;
	(void) arg;
	(void) vd;

	st = mem_zalloc(sizeof(*st), destructor);
	if (!st)
		return ENOMEM;


	st->u32FrameBufSize = s_u32FrameBufSize;
	st->pu8FrameBufAddr = s_pu8FrameBufAddr;

	*stp = st;

	return 0;
}

static void hide(struct vidisp_st *st)
{
	if(st->pu8FrameBufAddr)
		memset(st->pu8FrameBufAddr, 0x0, st->u32FrameBufSize);

	return;
}

static int display(struct vidisp_st *st, const char *title,
		   const struct vidframe *frame, uint64_t timestamp)
{
	struct vidsz size = {0, 0};

	(void) st;
	(void) title;
	(void) timestamp;

	if (conf_get_vidsz(conf_cur(), "vc8000_panel_size", &size) == 0) 
	{
		//VC8000 post processing enabled. Nothing to do!
		return 0;
	}
	
	//TODO: checking color type and copy frame data to framebuffer. 
	return 0;
}

static int module_init(void)
{
	int err = 0;
	struct fb_var_screeninfo sFBVar;

	fb_fd = open(FB_DEV_NAME, O_RDWR, 0);
	if (fb_fd < 0) {
		warning("Failed to open fb device: %s", FB_DEV_NAME);
		return -1;
	}

	err = ioctl(fb_fd, FBIOGET_VSCREENINFO, &sFBVar);
	if (err < 0) {
	    warning("FBIOGET_VSCREENINFO failed!\n");
		return err;
	}

	s_u32FrameBufSize = sFBVar.xres * sFBVar.yres * 4 *2;	//4:argb8888, 2: two planes
	
	s_pu8FrameBufAddr = (void *)mmap(NULL, s_u32FrameBufSize, PROT_READ|PROT_WRITE, MAP_SHARED, fb_fd, 0);
	if (s_pu8FrameBufAddr == MAP_FAILED) {
		warning("mmap() failed\n");
		err = -2;
		return err;
	}

	dl_handle = dlopen ("/usr/lib/baresip/modules/vc8000.so", RTLD_LAZY);
	if(!dl_handle)
	{
			warning("unable import vc8000 so %s\n", dlerror());
	}
	else
	{	
		dl_set_fb = dlsym(dl_handle, "baresip_module_vc8000_SetFB");
		if ((dl_error = dlerror()) != NULL)  {
			warning("unable import set fb dll function \n");
		}
	}

	printf("DDDDDDDD fb module init and set framebuff %x \n", s_pu8FrameBufAddr);

	memset(s_pu8FrameBufAddr, 0, s_u32FrameBufSize);

	if(dl_set_fb)
		dl_set_fb(s_pu8FrameBufAddr, s_u32FrameBufSize, 2);

	err = vidisp_register(&vid, baresip_vidispl(),
			      "fb", alloc, NULL, display, hide);
	if (err)
		return err;

	return 0;}

static int module_close(void)
{
	if(fb_fd > 0)
	{
		if(s_pu8FrameBufAddr)
		{
			munmap(s_pu8FrameBufAddr, s_u32FrameBufSize);
			s_pu8FrameBufAddr = NULL;
		}

		close(fb_fd);
	}

	if(dl_handle)
		dlclose(dl_handle);

	vid = mem_deref(vid);


	return 0;
}

EXPORT_SYM const struct mod_export DECL_EXPORTS(fb) = {
	"fb",
	"vidisp",
	module_init,
	module_close
};

