/*
 *      Copyright (C) 2012 Team XBMC
 *      http://www.xbmc.org
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with XBMC; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *  http://www.gnu.org/copyleft/gpl.html
 *
 */

#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <sys/ioctl.h>

#include "VendorAmlogic.h"
extern "C"
{
#include <player.h>
#include <player_set_sys.h>
}

#define  FBIOPUT_OSD_FREE_SCALE_ENABLE  0x4504
#define  FBIOPUT_OSD_FREE_SCALE_WIDTH   0x4505
#define  FBIOPUT_OSD_FREE_SCALE_HEIGHT  0x4506
#define  FBIOPUT_OSD_FREE_SCALE_AXIS    0x4510

#define  DISP_MODE_PATH "/sys/class/amhdmitx/amhdmitx0/disp_mode"
#define  AXIS_PATH "/sys/class/display/axis"

////////////////////////////////////////////////////////////////////////////////////////////
CVendorAmlogic::CVendorAmlogic()
{
}

CVendorAmlogic::~CVendorAmlogic()
{
}

void* CVendorAmlogic::CreateNativeWindow(int width, int height, int bpp)
{
  fbdev_window *native_window;
  native_window = (fbdev_window*)calloc(1, sizeof(fbdev_window));
  native_window->width  = width;
  native_window->height = height;

  return (void*)native_window;
}

void CVendorAmlogic::DestroyNativeWindow(EGLNativeWindowType native_window)
{
  free(native_window);
}

int CVendorAmlogic::GetDisplayResolutionMode()
{
  int mode = DISP_MODE_720P;
  CStdString modestr;
  char displayrez[1024] = {0};

  get_sysfs_str("/sys/class/display/mode", displayrez, sizeof(displayrez - 1));
  modestr = displayrez;
  if (modestr.Equals("480i"))
    mode = DISP_MODE_480I;  
  else if (modestr.Equals("480p"))
    mode = DISP_MODE_480P;  
  else if (modestr.Equals("576i"))
    mode = DISP_MODE_576I;  
  else if (modestr.Equals("576p"))
    mode = DISP_MODE_576P;  
  else if (modestr.Equals("720p"))
    mode = DISP_MODE_720P;  
  else if (modestr.Equals("1080i"))
    mode = DISP_MODE_1080I;  
  else if (modestr.Equals("1080p"))
    mode = DISP_MODE_1080P;  

  printf("CVendorAmlogic::GetDisplayResolutionMode:mode(%d)\n", mode);
  return mode;
}

bool CVendorAmlogic::SetDisplayResolution(const char *resolution)
{
  CStdString modestr;

  modestr = resolution;

  set_sysfs_str("/sys/class/display/mode", modestr.c_str());
/*
  if (modestr.Equals("480p"))
    SetFreeScale(DISP_MODE_480I);
  else if (modestr.Equals("480p"))
    SetFreeScale(DISP_MODE_480P);
  else if (modestr.Equals("576i"))
    SetFreeScale(DISP_MODE_576I);
  else if (modestr.Equals("576p"))
    SetFreeScale(DISP_MODE_576P);
  else if (modestr.Equals("720p"))
    SetFreeScale(DISP_MODE_720P);
  else if (modestr.Equals("1080i"))
    SetFreeScale(DISP_MODE_1080I);
  else if (modestr.Equals("1080p"))
    SetFreeScale(DISP_MODE_1080P);
*/
  return true;
}

bool CVendorAmlogic::Has720pRenderLimits()
{
  return false;
}

bool CVendorAmlogic::ProbeDisplayResolutions(std::vector<CStdString> &resolutions)
{
  int fd = open("/sys/class/amhdmitx/amhdmitx0/disp_cap", O_RDONLY);
  if (fd >= 0)
  {
    char valstr[256] = {0};

    read(fd, valstr, sizeof(valstr) - 1);
    valstr[strlen(valstr)] = '\0';
    close(fd);
    
    std::vector<CStdString> probe_str;
    StringUtils::SplitString(valstr, "\n", probe_str);

    resolutions.clear();
    for (size_t i = 0; i < probe_str.size(); i++)
    {
      // strips for example, 720p* to 720p
      if (probe_str[i].Right(1) == "*")
        probe_str[i] = probe_str[i].Left(std::max(0, (int)probe_str[i].size() - 1));
      printf("CVendorAmlogic::ProbeDisplayResolutions:%s\n", probe_str[i].c_str());
      
      if (probe_str[i].Equals("480p"))
        resolutions.push_back("720x480p60Hzp");
      else if (probe_str[i].Equals("720p"))
        resolutions.push_back("1280x720p60Hz");
      else if (probe_str[i].Equals("1080i"))
        resolutions.push_back("1920x1080i60Hz");
      else if (probe_str[i].Equals("1080p"))
        resolutions.push_back("1920x1080p60Hz");
    }
    if (resolutions.size() == 0)
      resolutions.push_back("1280x720p60Hz");
  }
  
  return false;
}

void CVendorAmlogic::ShowWindow(bool show)
{
  if (show)
    set_sysfs_int("/sys/class/graphics/fb0/blank", 0);
  else
    set_sysfs_int("/sys/class/graphics/fb0/blank", 1);
}

int CVendorAmlogic::SetFreeScale(int mode)
{
  int fd0 = -1, fd1 = -1;
  bool has_fd0 = true, has_fd1 = true;
  int fd_daxis = -1, fd_vaxis = -1;
/*
  int fd_video = -1;
  int fd_ppmgr = -1;
  int fd_ppmgr_rect = -1;
*/
  int osd_top = 0, osd_left = 0;
  int osd_width = 0, osd_height = 0;
  int ret = -1;

  struct fb_var_screeninfo vinfo;
  char daxis_str[255] = {0};
/*
  char vaxis_str[255] = {0};
  int vaxis_str_valid = 0;
*/

  printf("CVendorAmlogic::SetFreeScale: mode=%d\n", mode);
  if ((fd0 = open("/dev/fb0", O_RDWR)) < 0)
  {
    printf("open /dev/fb0 fail.\n");
    has_fd0 = false;
    goto exit;
  }
  if ((fd1 = open("/dev/fb1", O_RDWR)) < 0)
  {
    printf("open /dev/fb1 fail.\n");
    has_fd1 = false;
  }
  if ((fd_vaxis = open("/sys/class/video/axis", O_RDWR)) < 0)
  {
    printf("open /sys/class/video/axis fail.\n");
    goto exit;
  }

  if ((fd_daxis = open("/sys/class/display/axis", O_RDWR)) < 0)
  {
    printf("open /sys/class/display/axis fail.\n");
    goto exit;
  }
/*
  if ((fd_video = open("/sys/class/video/disable_video", O_RDWR)) < 0)
  {
    printf("open /sys/class/video/disable_video fail.\n");
  }

  if ((fd_ppmgr = open("/sys/class/ppmgr/ppscaler", O_RDWR)) < 0)
  {
    printf("open /sys/class/ppmgr/ppscaler fail.\n");
  }

  if ((fd_ppmgr_rect = open("/sys/class/ppmgr/ppscaler_rect", O_RDWR)) < 0)
  {
    printf("open /sys/class/ppmgr/ppscaler_rect fail.\n");
  }

  if (fd_vaxis >= 0)
  {
    int ret_len = read(fd_vaxis, vaxis_str, sizeof(vaxis_str));
    if (ret_len > 0)
    {
      int x = 0, y = 0, w = 0, h = 0;
      if (sscanf(vaxis_str,"%d %d %d %d", &x, &y, &w, &h) > 0)
      {
        printf("set mode: vaxis: x:%d, y:%d, w:%d, h:%d.\n", x, y, w, h);
        w = w - x + 1;
        h = h - y + 1;
        vaxis_str_valid = 1;
        printf("set mode: vaxis: x:%d, y:%d, w:%d, h:%d.\n", x, y, w, h);
      }
    }
  }
*/
  if (ioctl(fd0, FBIOGET_VSCREENINFO, &vinfo) == 0)
  {
    osd_top  = 0;
    osd_left = 0;
    osd_width  = vinfo.xres;
    osd_height = vinfo.yres;
    sprintf(daxis_str, "0 0 %d %d 0 0 18 18", vinfo.xres, vinfo.yres);
    printf("osd_width = %d\n", osd_width);
    printf("osd_height = %d\n", osd_height);
  }
  else
  {
    printf("get FBIOGET_VSCREENINFO fail.");
    goto exit;
  }

  switch(mode)
  {
    //printf("set mid mode=%d\n", mode);
    case 0:  //panel
      /*
      if (fd_ppmgr >= 0) write(fd_ppmgr, "0", strlen("0"));
      //if (fd_video >= 0) write(fd_video, "1", strlen("1"));
      write(fd_daxis, daxis_str, strlen(daxis_str));
      if (has_fd0) ioctl(fd0, FBIOPUT_OSD_FREE_SCALE_ENABLE, 0);
      if (has_fd1) ioctl(fd1, FBIOPUT_OSD_FREE_SCALE_ENABLE, 0);
      if (has_fd0) ioctl(fd0, FBIOPUT_OSD_FREE_SCALE_ENABLE, 1);
      if (has_fd1) ioctl(fd1, FBIOPUT_OSD_FREE_SCALE_ENABLE, 1);
      if (has_fd0) ioctl(fd0, FBIOPUT_OSD_FREE_SCALE_ENABLE, 0);
      if (has_fd1) ioctl(fd1, FBIOPUT_OSD_FREE_SCALE_ENABLE, 0);
      if ((fd_ppmgr >= 0) && (vaxis_str_valid)) write(fd_vaxis, vaxis_str, strlen(vaxis_str));
      //if (fd_video >= 0) write(fd_video, "2", strlen("2"));
      */
			if (has_fd0) ioctl(fd0, FBIOPUT_OSD_FREE_SCALE_ENABLE, 0);
			if (has_fd1) ioctl(fd1, FBIOPUT_OSD_FREE_SCALE_ENABLE, 0);
			write(fd_daxis, daxis_str, strlen(daxis_str));
      ret = 0;
      break;
    case DISP_MODE_480P:
      /*
      if (fd_ppmgr >= 0) write(fd_ppmgr, "0", strlen("0"));
      if (fd_video >= 0) write(fd_video, "1", strlen("1"));
      if (fd_ppmgr >= 0) write(fd_ppmgr, "1", strlen("1"));
      if (fd_ppmgr_rect >= 0) write(fd_ppmgr_rect, "20 10 700 470 0", strlen("20 10 700 470 0"));
      else if (fd_vaxis >= 0) write(fd_vaxis,      "20 10 700 470",   strlen("20 10 700 470"));
      write(fd_daxis, daxis_str, strlen(daxis_str));
      if (has_fd0) ioctl(fd0, FBIOPUT_OSD_FREE_SCALE_ENABLE, 0);
      if (has_fd1) ioctl(fd1, FBIOPUT_OSD_FREE_SCALE_ENABLE, 0);
      if (has_fd0) ioctl(fd0, FBIOPUT_OSD_FREE_SCALE_WIDTH,  osd_width);
      if (has_fd0) ioctl(fd0, FBIOPUT_OSD_FREE_SCALE_HEIGHT, osd_height);
      if (has_fd1) ioctl(fd1, FBIOPUT_OSD_FREE_SCALE_WIDTH,  osd_width);
      if (has_fd1) ioctl(fd1, FBIOPUT_OSD_FREE_SCALE_HEIGHT, osd_height);
      if (has_fd0) ioctl(fd0, FBIOPUT_OSD_FREE_SCALE_ENABLE, 1);
      if (has_fd1) ioctl(fd1, FBIOPUT_OSD_FREE_SCALE_ENABLE, 1);
      if ((fd_ppmgr >= 0) && (vaxis_str_valid)) write(fd_vaxis, vaxis_str, strlen(vaxis_str));
      if ((fd_video >= 0) && (fd_ppmgr >= 0))   write(fd_video, "2", strlen("2"));
      */
			//write(fd_vaxis, "20 10 700 470", strlen("20 10 700 470"));
			write(fd_vaxis, "0 0 720 480", strlen("0 0 720 480"));
			write(fd_daxis, daxis_str, strlen(daxis_str));
			if (has_fd0) ioctl(fd0, FBIOPUT_OSD_FREE_SCALE_ENABLE, 0);
			if (has_fd1) ioctl(fd1, FBIOPUT_OSD_FREE_SCALE_ENABLE, 0);
			if (has_fd0) ioctl(fd0, FBIOPUT_OSD_FREE_SCALE_WIDTH,  osd_width);
			if (has_fd0) ioctl(fd0, FBIOPUT_OSD_FREE_SCALE_HEIGHT, osd_height); 
			if (has_fd1) ioctl(fd1, FBIOPUT_OSD_FREE_SCALE_WIDTH,  osd_width);
			if (has_fd1) ioctl(fd1, FBIOPUT_OSD_FREE_SCALE_HEIGHT, osd_height);	
			if (has_fd0) ioctl(fd0, FBIOPUT_OSD_FREE_SCALE_ENABLE, 1);
			if (has_fd1) ioctl(fd1, FBIOPUT_OSD_FREE_SCALE_ENABLE, 1);	
      ret = 0;
      break;
    case DISP_MODE_720P:
      {
        /*
        if (fd_ppmgr >= 0) write(fd_ppmgr, "0", strlen("0"));
        if (fd_video >= 0) write(fd_video, "1", strlen("1"));
        if (fd_ppmgr >= 0) write(fd_ppmgr, "1", strlen("1"));
        if (fd_ppmgr_rect >= 0) write(fd_ppmgr_rect, "40 15 1240 705 0", strlen("40 15 1240 705 0"));
        else if (fd_vaxis >= 0) write(fd_vaxis,      "40 15 1240 705",   strlen("40 15 1240 705"));
        write(fd_daxis, daxis_str, strlen(daxis_str));
        if (has_fd0) ioctl(fd0, FBIOPUT_OSD_FREE_SCALE_ENABLE, 0);
        if (has_fd1) ioctl(fd1, FBIOPUT_OSD_FREE_SCALE_ENABLE, 0);
        if (has_fd0) ioctl(fd0, FBIOPUT_OSD_FREE_SCALE_WIDTH,  osd_width);
        if (has_fd0) ioctl(fd0, FBIOPUT_OSD_FREE_SCALE_HEIGHT, osd_height);
        if (has_fd1) ioctl(fd1, FBIOPUT_OSD_FREE_SCALE_WIDTH,  osd_width);
        if (has_fd1) ioctl(fd1, FBIOPUT_OSD_FREE_SCALE_HEIGHT, osd_height);
        if (fd_ppmgr >= 0) write(fd_ppmgr, "1", strlen("1"));
        if (has_fd0) ioctl(fd0, FBIOPUT_OSD_FREE_SCALE_ENABLE, 1);
        if (has_fd1) ioctl(fd1, FBIOPUT_OSD_FREE_SCALE_ENABLE, 1);
        if ((fd_ppmgr >= 0) && (vaxis_str_valid)) write(fd_vaxis, vaxis_str, strlen(vaxis_str));
        if ((fd_video >= 0) && (fd_ppmgr >= 0))   write(fd_video, "2", strlen("2"));
        */
        //write(fd_vaxis, "40 15 1240 705", strlen("40 15 1240 705"));
        write(fd_vaxis, "0 0 1280 720", strlen("0 0 1280 720"));
        write(fd_daxis, daxis_str, strlen(daxis_str));
        if (has_fd0) ioctl(fd0, FBIOPUT_OSD_FREE_SCALE_ENABLE, 0);
        if (has_fd1) ioctl(fd1, FBIOPUT_OSD_FREE_SCALE_ENABLE, 0);
        if (has_fd0) ioctl(fd0, FBIOPUT_OSD_FREE_SCALE_WIDTH,  osd_width);
        if (has_fd0) ioctl(fd0, FBIOPUT_OSD_FREE_SCALE_HEIGHT, osd_height);
        if (has_fd1) ioctl(fd1, FBIOPUT_OSD_FREE_SCALE_WIDTH,  osd_width);
        if (has_fd1) ioctl(fd1, FBIOPUT_OSD_FREE_SCALE_HEIGHT, osd_height);
        if (has_fd0) ioctl(fd0, FBIOPUT_OSD_FREE_SCALE_ENABLE, 1);
        if (has_fd1) ioctl(fd1, FBIOPUT_OSD_FREE_SCALE_ENABLE, 1);
        ret = 0;
      }
      break;
    case DISP_MODE_1080I:
    case DISP_MODE_1080P:
      {
        /*
        if (fd_ppmgr >= 0) write(fd_ppmgr, "0", strlen("0"));
        if (fd_video >= 0) write(fd_video, "1", strlen("1"));
        if (fd_ppmgr >= 0) write(fd_ppmgr, "1", strlen("1"));
        if (fd_ppmgr_rect >= 0) write(fd_ppmgr_rect, "40 20 1880 1060 0", strlen("40 20 1880 1060 0"));
        else if (fd_vaxis >= 0) write(fd_vaxis,      "40 20 1880 1060",   strlen("40 20 1880 1060"));
        write(fd_daxis, daxis_str, strlen(daxis_str));
        if (has_fd0) ioctl(fd0, FBIOPUT_OSD_FREE_SCALE_ENABLE, 0);
        if (has_fd1) ioctl(fd1, FBIOPUT_OSD_FREE_SCALE_ENABLE, 0);
        if (has_fd0) ioctl(fd0, FBIOPUT_OSD_FREE_SCALE_WIDTH,  osd_width);
        if (has_fd0) ioctl(fd0, FBIOPUT_OSD_FREE_SCALE_HEIGHT, osd_height);
        if (has_fd1) ioctl(fd1, FBIOPUT_OSD_FREE_SCALE_WIDTH,  osd_width);
        if (has_fd1) ioctl(fd1, FBIOPUT_OSD_FREE_SCALE_HEIGHT, osd_height);
        if (has_fd0) ioctl(fd0, FBIOPUT_OSD_FREE_SCALE_ENABLE, 1);
        if (has_fd1) ioctl(fd1, FBIOPUT_OSD_FREE_SCALE_ENABLE, 1);
        if ((fd_ppmgr >= 0) && (vaxis_str_valid)) write(fd_vaxis, vaxis_str, strlen(vaxis_str));
        if ((fd_video >= 0) && (fd_ppmgr >= 0))   write(fd_video, "2", strlen("2"));
        */
        //write(fd_vaxis, "40 20 1880 1060", strlen("40 20 1880 1060"));
        write(fd_vaxis, "0 0 1920 1080", strlen("0 0 1920 1080"));
        write(fd_daxis, daxis_str, strlen(daxis_str));
        if (has_fd0) ioctl(fd0, FBIOPUT_OSD_FREE_SCALE_ENABLE, 0);
        if (has_fd1) ioctl(fd1, FBIOPUT_OSD_FREE_SCALE_ENABLE, 0);
        if (has_fd0) ioctl(fd0, FBIOPUT_OSD_FREE_SCALE_WIDTH,  osd_width);
        if (has_fd0) ioctl(fd0, FBIOPUT_OSD_FREE_SCALE_HEIGHT, osd_height);
        if (has_fd1) ioctl(fd1, FBIOPUT_OSD_FREE_SCALE_WIDTH,  osd_width);
        if (has_fd1) ioctl(fd1, FBIOPUT_OSD_FREE_SCALE_HEIGHT, osd_height);
        if (has_fd0) ioctl(fd0, FBIOPUT_OSD_FREE_SCALE_ENABLE, 1);
        if (has_fd1) ioctl(fd1, FBIOPUT_OSD_FREE_SCALE_ENABLE, 1);
        ret = 0;
      }
      break;
    default:
      break;
  }

exit:
  close(fd0);
  close(fd1);
  close(fd_vaxis);
  close(fd_daxis);
  /*
  close(fd_video);
  close(fd_ppmgr);
  close(fd_ppmgr_rect);
  */
  return ret;
}

int CVendorAmlogic::EnableFreeScale(int mode)
{
  int fd0 = -1, fd1 = -1;
  bool has_fd0 = true, has_fd1 = true;
  int fd_daxis = -1, fd_vaxis = -1;
/*
  int fd_ppmgr = -1,fd_ppmgr_rect = -1;
  int fd_video = -1;
*/
  int osd_width = 0, osd_height = 0;
  int ret = -1;

  struct fb_var_screeninfo vinfo;
  char daxis_str[32] = {0};
  /*
  char vaxis_str[80] = {0};
  int vaxis_str_valid = 0;
  */

  if (mode == 0) return 0;

  if ((fd0 = open("/dev/fb0", O_RDWR)) < 0)
  {
    printf("open /dev/fb0 fail.\n");
    has_fd0 = false;
    goto exit;
  }
  if ((fd1 = open("/dev/fb1", O_RDWR)) < 0)
  {
    printf("open /dev/fb1 fail.\n");
    has_fd1 = false;
  }
  if ((fd_vaxis = open("/sys/class/video/axis", O_RDWR)) < 0)
  {
    printf("open /sys/class/video/axis fail.\n");
    goto exit;
  }

  if ((fd_daxis = open("/sys/class/display/axis", O_RDWR)) < 0)
  {
    printf("open /sys/class/display/axis fail.\n");
    goto exit;
  }
/*
  if ((fd_video = open("/sys/class/video/disable_video", O_RDWR)) < 0)
  {
    printf("open /sys/class/video/disable_video fail.\n");
  }

  if ((fd_ppmgr = open("/sys/class/ppmgr/ppscaler", O_RDWR)) < 0)
  {
    printf("open /sys/class/ppmgr/ppscaler fail.\n");
  }

  if ((fd_ppmgr_rect = open("/sys/class/ppmgr/ppscaler_rect", O_RDWR)) < 0)
  {
    printf("open /sys/class/ppmgr/ppscaler_rect fail.\n");
  }

  if (fd_vaxis >= 0)
  {
    int ret_len = read(fd_vaxis, vaxis_str, sizeof(vaxis_str));
    if (ret_len > 0)
    {
      int x = 0, y = 0, w = 0, h = 0;
      if (sscanf(vaxis_str,"%d %d %d %d",&x,&y,&w,&h)>0)
      {
        w = w - x + 1;
        h = h - y + 1;
        vaxis_str_valid = 1;
        printf("enable mode: vaxis: x:%d, y:%d, w:%d, h:%d.\n",x,y,w,h);
        if (w <= 0 || h <= 0)
          goto exit;
      }
    }
  }
*/
  if (ioctl(fd0, FBIOGET_VSCREENINFO, &vinfo) == 0)
  {
    osd_width  = vinfo.xres;
    osd_height = vinfo.yres;
    sprintf(daxis_str, "0 0 %d %d 0 0 18 18", vinfo.xres, vinfo.yres);

    //printf("osd_width = %d\n", osd_width);
    //printf("osd_height = %d\n", osd_height);
  }
  else
  {
    printf("get FBIOGET_VSCREENINFO fail.\n");
    goto exit;
  }

  switch(mode)
  {
    //printf("set mid mode=%d\n", mode);
    case 0:  //panel
      /*
      if (fd_ppmgr >= 0) write(fd_ppmgr, "0", strlen("0"));
      if (fd_video >= 0) write(fd_video, "1", strlen("1"));
      if (has_fd0) ioctl(fd0, FBIOPUT_OSD_FREE_SCALE_ENABLE, 0);
      if (has_fd1) ioctl(fd1, FBIOPUT_OSD_FREE_SCALE_ENABLE, 0);
      write(fd_daxis, daxis_str, strlen(daxis_str));
      if ((fd_ppmgr >= 0) && (vaxis_str_valid)) write(fd_vaxis, vaxis_str, strlen(vaxis_str));
      if (fd_video >= 0) write(fd_video, "2", strlen("2"));
      */
			if (has_fd0) ioctl(fd0,FBIOPUT_OSD_FREE_SCALE_ENABLE,0);
			if (has_fd1) ioctl(fd1,FBIOPUT_OSD_FREE_SCALE_ENABLE,0);
			write(fd_daxis, daxis_str, strlen(daxis_str));
      ret = 0;
      break;
    case DISP_MODE_480P:
      /*
      if (fd_ppmgr >= 0) write(fd_ppmgr, "0", strlen("0"));
      if (fd_video >= 0) write(fd_video, "1", strlen("1"));
      if (fd_ppmgr >= 0) write(fd_ppmgr, "1", strlen("1"));
      if (fd_ppmgr_rect >= 0) write(fd_ppmgr_rect, "20 10 700 470 0", strlen("20 10 700 470 0"));
      else if (fd_vaxis >= 0) write(fd_vaxis,      "20 10 700 470",   strlen("20 10 700 470"));
      write(fd_daxis, daxis_str, strlen(daxis_str));
      if (has_fd0) ioctl(fd0, FBIOPUT_OSD_FREE_SCALE_ENABLE, 0);
      if (has_fd1) ioctl(fd1, FBIOPUT_OSD_FREE_SCALE_ENABLE, 0);
      if (has_fd0) ioctl(fd0, FBIOPUT_OSD_FREE_SCALE_WIDTH,  osd_width);
      if (has_fd0) ioctl(fd0, FBIOPUT_OSD_FREE_SCALE_HEIGHT, osd_height);
      if (has_fd1) ioctl(fd1, FBIOPUT_OSD_FREE_SCALE_WIDTH,  osd_width);
      if (has_fd1) ioctl(fd1, FBIOPUT_OSD_FREE_SCALE_HEIGHT, osd_height);
      if (has_fd0) ioctl(fd0, FBIOPUT_OSD_FREE_SCALE_ENABLE, 1);
      if (has_fd1) ioctl(fd1, FBIOPUT_OSD_FREE_SCALE_ENABLE, 1);
      if ((fd_ppmgr >= 0) && (vaxis_str_valid)) write(fd_vaxis, vaxis_str, strlen(vaxis_str));
      if ((fd_video >= 0) && (fd_ppmgr >= 0))   write(fd_video, "2", strlen("2"));
      */
			write(fd_vaxis, "20 10 700 470", strlen("20 10 700 470"));
			write(fd_daxis, daxis_str, strlen(daxis_str));
			if (has_fd0) ioctl(fd0, FBIOPUT_OSD_FREE_SCALE_ENABLE, 0);
			if (has_fd1) ioctl(fd1, FBIOPUT_OSD_FREE_SCALE_ENABLE, 0);
			if (has_fd0) ioctl(fd0, FBIOPUT_OSD_FREE_SCALE_WIDTH,  osd_width);
			if (has_fd0) ioctl(fd0, FBIOPUT_OSD_FREE_SCALE_HEIGHT, osd_height); 
			if (has_fd1) ioctl(fd1, FBIOPUT_OSD_FREE_SCALE_WIDTH,  osd_width);
			if (has_fd1) ioctl(fd1, FBIOPUT_OSD_FREE_SCALE_HEIGHT, osd_height);	
			if (has_fd0) ioctl(fd0, FBIOPUT_OSD_FREE_SCALE_ENABLE, 1);
			if (has_fd1) ioctl(fd1, FBIOPUT_OSD_FREE_SCALE_ENABLE, 1);	
      ret = 0;
      break;
    case DISP_MODE_720P:
      /*
      if (fd_ppmgr >= 0) write(fd_ppmgr, "0", strlen("0"));
      if (fd_video >= 0) write(fd_video, "1", strlen("1"));
      if (fd_ppmgr >= 0) write(fd_ppmgr, "1", strlen("1"));
      if (fd_ppmgr_rect >= 0) write(fd_ppmgr_rect, "40 15 1240 705 0", strlen("40 15 1240 705 0"));
      else if (fd_vaxis >= 0) write(fd_vaxis,      "40 15 1240 705",   strlen("40 15 1240 705"));
      write(fd_daxis, daxis_str, strlen(daxis_str));
      if (has_fd0) ioctl(fd0, FBIOPUT_OSD_FREE_SCALE_ENABLE, 0);
      if (has_fd1) ioctl(fd1, FBIOPUT_OSD_FREE_SCALE_ENABLE, 0);
      if (has_fd0) ioctl(fd0, FBIOPUT_OSD_FREE_SCALE_WIDTH,  osd_width);
      if (has_fd0) ioctl(fd0, FBIOPUT_OSD_FREE_SCALE_HEIGHT, osd_height);
      if (has_fd1) ioctl(fd1, FBIOPUT_OSD_FREE_SCALE_WIDTH,  osd_width);
      if (has_fd1) ioctl(fd1, FBIOPUT_OSD_FREE_SCALE_HEIGHT, osd_height);
      if (has_fd0) ioctl(fd0, FBIOPUT_OSD_FREE_SCALE_ENABLE, 1);
      if (has_fd1) ioctl(fd1, FBIOPUT_OSD_FREE_SCALE_ENABLE, 1);
      if ((fd_ppmgr >= 0) && (vaxis_str_valid)) write(fd_vaxis, vaxis_str, strlen(vaxis_str));
      if ((fd_video >= 0) && (fd_ppmgr >= 0))   write(fd_video, "2", strlen("2"));
      */
			write(fd_vaxis, "40 15 1240 705", strlen("40 15 1240 705"));
			write(fd_daxis, daxis_str, strlen(daxis_str));
			if (has_fd0) ioctl(fd0, FBIOPUT_OSD_FREE_SCALE_ENABLE, 0);
			if (has_fd1) ioctl(fd1, FBIOPUT_OSD_FREE_SCALE_ENABLE, 0);
			if (has_fd0) ioctl(fd0, FBIOPUT_OSD_FREE_SCALE_WIDTH,  osd_width);
			if (has_fd0) ioctl(fd0, FBIOPUT_OSD_FREE_SCALE_HEIGHT, osd_height); 
			if (has_fd1) ioctl(fd1, FBIOPUT_OSD_FREE_SCALE_WIDTH,  osd_width);
			if (has_fd1) ioctl(fd1, FBIOPUT_OSD_FREE_SCALE_HEIGHT, osd_height);	
			if (has_fd0) ioctl(fd0, FBIOPUT_OSD_FREE_SCALE_ENABLE, 1);
			if (has_fd1) ioctl(fd1, FBIOPUT_OSD_FREE_SCALE_ENABLE, 1);	
      ret = 0;
      break;
    case DISP_MODE_1080I:
    case DISP_MODE_1080P:
      /*
      if (fd_ppmgr >= 0) write(fd_ppmgr, "0", strlen("0"));
      if (fd_video >= 0) write(fd_video, "1", strlen("1"));
      if (fd_ppmgr >= 0) write(fd_ppmgr, "1", strlen("1"));
      if (fd_ppmgr_rect >= 0) write(fd_ppmgr_rect, "40 20 1880 1060 0", strlen("40 20 1880 1060 0"));
      else if (fd_vaxis >= 0) write(fd_vaxis,      "40 20 1880 1060",   strlen("40 20 1880 1060"));
      write(fd_daxis, daxis_str, strlen(daxis_str));
      if (has_fd0) ioctl(fd0, FBIOPUT_OSD_FREE_SCALE_ENABLE, 0);
      if (has_fd1) ioctl(fd1, FBIOPUT_OSD_FREE_SCALE_ENABLE, 0);
      if (has_fd0) ioctl(fd0, FBIOPUT_OSD_FREE_SCALE_WIDTH,  osd_width);
      if (has_fd0) ioctl(fd0, FBIOPUT_OSD_FREE_SCALE_HEIGHT, osd_height);
      if (has_fd1) ioctl(fd1, FBIOPUT_OSD_FREE_SCALE_WIDTH,  osd_width);
      if (has_fd1) ioctl(fd1, FBIOPUT_OSD_FREE_SCALE_HEIGHT, osd_height);
      if (has_fd0) ioctl(fd0, FBIOPUT_OSD_FREE_SCALE_ENABLE, 1);
      if (has_fd1) ioctl(fd1, FBIOPUT_OSD_FREE_SCALE_ENABLE, 1);
      if ((fd_ppmgr >= 0) && (vaxis_str_valid)) write(fd_vaxis, vaxis_str, strlen(vaxis_str));
      if ((fd_video >= 0) && (fd_ppmgr >= 0))   write(fd_video, "2", strlen("2"));
      */
			write(fd_vaxis, "40 20 1880 1060", strlen("40 20 1880 1060"));
			write(fd_daxis, daxis_str, strlen(daxis_str));
			if (has_fd0) ioctl(fd0, FBIOPUT_OSD_FREE_SCALE_ENABLE, 0);
			if (has_fd1) ioctl(fd1, FBIOPUT_OSD_FREE_SCALE_ENABLE, 0);
			if (has_fd0) ioctl(fd0, FBIOPUT_OSD_FREE_SCALE_WIDTH,  osd_width);
			if (has_fd0) ioctl(fd0, FBIOPUT_OSD_FREE_SCALE_HEIGHT, osd_height); 
			if (has_fd1) ioctl(fd1, FBIOPUT_OSD_FREE_SCALE_WIDTH,  osd_width);
			if (has_fd1) ioctl(fd1, FBIOPUT_OSD_FREE_SCALE_HEIGHT, osd_height);	
			if (has_fd0) ioctl(fd0, FBIOPUT_OSD_FREE_SCALE_ENABLE, 1);
			if (has_fd1) ioctl(fd1, FBIOPUT_OSD_FREE_SCALE_ENABLE, 1);	
      ret = 0;
      break;
    default:
      break;
  }

exit:
  close(fd0);
  close(fd1);
  close(fd_vaxis);
  close(fd_daxis);
  /*
  close(fd_ppmgr);
  close(fd_video);
  close(fd_ppmgr_rect);
  */

  return ret;
}

int CVendorAmlogic::DisableFreeScale(int mode)
{
  int fd0 = -1, fd1 = -1;
  bool has_fd0 = true, has_fd1 = true;
  int fd_daxis = -1, fd_vaxis = -1;
  /*
  int fd_ppmgr = -1, fd_video = -1;
  */
  int osd_width = 0, osd_height = 0;
  int ret = -1;

  struct fb_var_screeninfo vinfo;
  char daxis_str[32] = {0};
  /*
  char vaxis_str[80] = {0};
  int vaxis_str_valid = 0;
  */

  if (mode == 0) return 0;

  if ((fd0 = open("/dev/fb0", O_RDWR)) < 0)
  {
    printf("open /dev/fb0 fail.\n");
    has_fd0 = false;
    goto exit;
  }
  if ((fd1 = open("/dev/fb1", O_RDWR)) < 0)
  {
    printf("open /dev/fb1 fail.\n");
    has_fd1 = false;
  }
  if ((fd_vaxis = open("/sys/class/video/axis", O_RDWR)) < 0)
  {
    printf("open /sys/class/video/axis fail.\n");
    goto exit;
  }

  if ((fd_daxis = open("/sys/class/display/axis", O_RDWR)) < 0)
  {
    printf("open /sys/class/display/axis fail.\n");
    goto exit;
  }
/*
  if ((fd_video = open("/sys/class/video/disable_video", O_RDWR)) < 0)
  {
    printf("open /sys/class/video/disable_video fail.\n");
  }

  if ((fd_ppmgr = open("/sys/class/ppmgr/ppscaler", O_RDWR)) < 0)
  {
    printf("open /sys/class/ppmgr/ppscaler fail.\n");
  }

  if (fd_vaxis >= 0)
  {
    int ret_len = read(fd_vaxis, vaxis_str, sizeof(vaxis_str));
    if (ret_len > 0)
    {
      int x = 0, y = 0, w = 0, h = 0;
      if (sscanf(vaxis_str,"%d %d %d %d",&x,&y,&w,&h)>0)
      {
        w = w - x + 1;
        h = h - y + 1;
        vaxis_str_valid = 1;
        printf("disable mode: vaxis: x:%d, y:%d, w:%d, h:%d.\n",x,y,w,h);
      }
    }
  }
*/
  if (ioctl(fd0, FBIOGET_VSCREENINFO, &vinfo) == 0)
  {
    osd_width = vinfo.xres;
    osd_height = vinfo.yres;
    //printf("osd_width = %d\n", osd_width);
    //printf("osd_height = %d\n", osd_height);
  }
  else
  {
    printf("get FBIOGET_VSCREENINFO fail.\n");
    goto exit;
  }

  switch(mode)
  {
    //printf("set mid mode=%d\n", mode);
    case 0:  //panel
      ret = 0;
      break;
    case DISP_MODE_480P:
      /*
      if (fd_ppmgr >= 0) write(fd_ppmgr, "0", strlen("0"));
      if (fd_video >= 0) write(fd_video, "1", strlen("1"));
      if (has_fd0) ioctl(fd0, FBIOPUT_OSD_FREE_SCALE_ENABLE, 0);
      if (has_fd1) ioctl(fd1, FBIOPUT_OSD_FREE_SCALE_ENABLE, 0);
      sprintf(daxis_str, "0 0 %d %d 0 0 18 18", vinfo.xres, vinfo.yres);
      write(fd_daxis, daxis_str, strlen(daxis_str));
      if ((fd_ppmgr >= 0) && (vaxis_str_valid)) write(fd_vaxis, vaxis_str, strlen(vaxis_str));
      if (fd_video >= 0)  write(fd_video, "2", strlen("2"));
      */
			if (has_fd0) ioctl(fd0, FBIOPUT_OSD_FREE_SCALE_ENABLE, 0);
			if (has_fd1) ioctl(fd1, FBIOPUT_OSD_FREE_SCALE_ENABLE, 0);			
			sprintf(daxis_str, "0 0 %d %d 0 0 18 18", vinfo.xres, vinfo.yres);
			write(fd_daxis, daxis_str, strlen(daxis_str));
      ret = 0;
      break;
    case DISP_MODE_720P:
      /*
      if (fd_ppmgr >= 0) write(fd_ppmgr, "0", strlen("0"));
      if (fd_video >= 0) write(fd_video, "1", strlen("1"));
      if (has_fd0) ioctl(fd0, FBIOPUT_OSD_FREE_SCALE_ENABLE, 0);
      if (has_fd1) ioctl(fd1, FBIOPUT_OSD_FREE_SCALE_ENABLE, 0);
      sprintf(daxis_str, "%d %d %d %d %d %d 18 18",
        (1280 - vinfo.xres) / 2,
        (720  - vinfo.yres) / 2,
        vinfo.xres,
        vinfo.yres,
        (1280 - vinfo.xres) / 2,
        (720  - vinfo.yres) / 2);
      write(fd_daxis, daxis_str, strlen(daxis_str));
      if ((fd_ppmgr >= 0) && (vaxis_str_valid)) write(fd_vaxis, vaxis_str, strlen(vaxis_str));
      if (fd_video >= 0) write(fd_video, "2", strlen("2"));
      */
			if (has_fd0) ioctl(fd0,FBIOPUT_OSD_FREE_SCALE_ENABLE,0);
			if (has_fd1) ioctl(fd1,FBIOPUT_OSD_FREE_SCALE_ENABLE,0);
			sprintf(daxis_str, "%d %d %d %d %d %d 18 18", (1280 - vinfo.xres)/2, 
																										(720-   vinfo.yres)/2,
																										vinfo.xres, 
																										vinfo.yres,
																										(1280 - vinfo.xres)/2,
																										(720-   vinfo.yres)/2);
			write(fd_daxis, daxis_str, strlen(daxis_str));
      ret = 0;
      break;
    case DISP_MODE_1080I:
    case DISP_MODE_1080P:
      /*
      if (fd_ppmgr >= 0) write(fd_ppmgr, "0", strlen("0"));
      if (fd_video >= 0) write(fd_video, "1", strlen("1"));
      if (has_fd0) ioctl(fd0, FBIOPUT_OSD_FREE_SCALE_ENABLE, 0);
      if (has_fd1) ioctl(fd1, FBIOPUT_OSD_FREE_SCALE_ENABLE, 0);
      sprintf(daxis_str, "%d %d %d %d %d %d 18 18",
        (1920 - vinfo.xres) / 2,
        (1080 - vinfo.yres) / 2,
        vinfo.xres,
        vinfo.yres,
        (1920 - vinfo.xres) / 2,
        (1080 - vinfo.yres) / 2);
      write(fd_daxis, daxis_str, strlen(daxis_str));
      if ((fd_ppmgr >= 0) && (vaxis_str_valid)) write(fd_vaxis, vaxis_str, strlen(vaxis_str));
      if (fd_video >= 0) write(fd_video, "2", strlen("2"));
      */
			if (has_fd0) ioctl(fd0,FBIOPUT_OSD_FREE_SCALE_ENABLE,0);
			if (has_fd0) ioctl(fd1,FBIOPUT_OSD_FREE_SCALE_ENABLE,0);
			sprintf(daxis_str, "%d %d %d %d %d %d 18 18", (1920 - vinfo.xres)/2, 
																										(1080 - vinfo.yres)/2,
																										vinfo.xres, 
																										vinfo.yres,
																										(1920 - vinfo.xres)/2,
																										(1080 - vinfo.yres)/2);
			write(fd_daxis, daxis_str, strlen(daxis_str));
      ret = 0;
      break;
    default:
      break;
  }

exit:
  close(fd0);
  close(fd1);
  close(fd_vaxis);
  close(fd_daxis);
/*
  close(fd_ppmgr);
  close(fd_video);
*/
  return ret;
}
