#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <linux/types.h>
#include <linux/ioctl.h>
#include <linux/fb.h>
#include <sys/ioctl.h>

#include "dcfb.h"
#include "mainwindow.h"

#include <QApplication>
#include <QSysInfo>
#include <QScreen>

typedef struct linuxfb_device
{
    int fb_fd;
    unsigned char *fb_data;
    long fb_screensize;
    struct fb_var_screeninfo fb_vinfo;
    struct fb_fix_screeninfo fb_finfo;
} linuxfb_device_t;


/* Open linux framebuffer deive */
static int linuxfb_device_open(linuxfb_device_t *device, const char *fb_name)
{
    /* Open the file for reading and writing */
    device->fb_fd = open(fb_name, O_RDWR);
    if (device->fb_fd == -1)
    {
        perror("Error: cannot open framebuffer device");
        return -1;
    }

    /* Get variable screen information */
     if (ioctl(device->fb_fd, FBIOGET_VSCREENINFO, &device->fb_vinfo) == -1)
     {
         perror("Error: reading variable information");
         goto handle_open_error;
     }

     /* Set virtual display size double the width for double buffering */
     device->fb_vinfo.yoffset = 0;
     device->fb_vinfo.yres_virtual = device->fb_vinfo.yres * 2;
     if (ioctl(device->fb_fd, FBIOPUT_VSCREENINFO, &device->fb_vinfo))
     {
         perror("Error setting variable screen info from fb");
         goto handle_open_error;
     }


     /* Get fixed screen information */
     if (ioctl(device->fb_fd, FBIOGET_FSCREENINFO, &device->fb_finfo) == -1)
     {
         perror("Error reading fixed information");
         goto handle_open_error;
     }

     device->fb_screensize = device->fb_vinfo.xres_virtual * device->fb_vinfo.yres_virtual * device->fb_vinfo.bits_per_pixel / 8;
     printf("-> device->fb_vinfo.xres_virtual=%d\n", device->fb_vinfo.xres_virtual);
     printf("-> device->fb_vinfo.yres_virtual=%d\n", device->fb_vinfo.yres_virtual);
     printf("-> device->fb_vinfo.bits_per_pixel=%d\n", device->fb_vinfo.bits_per_pixel);
     printf("-> device->fb_screensize=%ld\n", device->fb_screensize);
     printf("-> device->fb_vinfo.yoffset=%d\n", device->fb_vinfo.yoffset);
     printf("-> device->fb_vinfo.yres=%d\n", device->fb_vinfo.yres);
     printf("-> device->fb_finfo.smem_len=%d\n", device->fb_finfo.smem_len);

    return 0;

handle_open_error:

    close(device->fb_fd);
    device->fb_fd = -1;
    return -2;
}


/* Destroy a cairo surface */
static void linuxfb_device_close(linuxfb_device_t *device)
{
    if (device == NULL)
        return;

    if(device->fb_fd >= 0)
        close(device->fb_fd);
}

static int SetBlendingMode(int fd, dc_alpha_blending_mode mode)
{
    int ret = 0;

    ret = ioctl(fd, ULTRAFBIO_BLENDING_MODE, &mode);
    if (ret < 0)
    {
        printf("set blending mode error\n");
        return -1;
    }

    return 0;
}

static int SetVarScreenInfo(int fd, struct fb_var_screeninfo *var)
{
    int ret = 0;

    ret = ioctl(fd, FBIOPUT_VSCREENINFO, var);
    if (ret != 0)
    {
        printf("error set var\n");
        return -1;
    }

    return 0;
}

int main(int argc, char *argv[])
{
    //store argv to argument_list;
    int i;

    QStringList argu_list;
    for(i = 0; i < argc; i ++)
    {
        argu_list << argv[i];
    }

    QApplication a(argc, argv);
    MainWindow w;
    linuxfb_device_t fb_device;
    int ret;

    fb_device.fb_fd = -1;

    if(QSysInfo::currentCpuArchitecture() == "arm64"){
        if(QGuiApplication::platformName().compare("linuxfb") == 0){
            int i;
            int index;
            QString dev_name;

            for(i = 0; i < argu_list.size(); i ++)
            {
                index = argu_list.at(i).indexOf("fb=");
                if(index > 0)
                {
                    dev_name = argu_list.at(i).right(index);
                    qDebug() << "found device name" << index << dev_name;
                    break;
                }
            }

            if(dev_name.size() == 0)
                dev_name = "/dev/fb1";

            if(linuxfb_device_open(&fb_device, qPrintable(dev_name)) == 0)
            {
                //Set ma35d1 alpha blending mode
                SetBlendingMode(fb_device.fb_fd, DC_BLEND_MODE_SRC_OVER);
                SetVarScreenInfo(fb_device.fb_fd, &fb_device.fb_vinfo);
            }
        }
    }

#if 0
    //Set window background color
    QPalette pal = w.palette();
    pal.setColor(QPalette::Window, Qt::white);
    w.setPalette(pal);
#endif

    w.show();
    ret = a.exec();

    if(fb_device.fb_fd >= 0)
        linuxfb_device_close(&fb_device);

    return ret;
}
