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

#include "WinEGLPlatformAmlogic.h"
extern "C"
{
#include <player.h>
#include <player_set_sys.h>
}

////////////////////////////////////////////////////////////////////////////////////////////
EGLNativeWindowType CWinEGLPlatformAmlogic::InitWindowSystem(int width, int height, int bpp)
{
  fbdev_window *native_window;
  native_window = (fbdev_window*)calloc(1, sizeof(fbdev_window));
  if (width == 1920 && height == 1080)
  {
    width  = 1280;
    height = 720;
  }
  native_window->width  = width;
  native_window->height = height;

  return (EGLNativeWindowType)native_window;
}

void CWinEGLPlatformAmlogic::DestroyWindowSystem(EGLNativeWindowType native_window)
{
  free(native_window);
  DisableFreeScale();
}

bool CWinEGLPlatformAmlogic::SetDisplayResolution(int width, int height, float refresh, bool interlace)
{
  if (width == 1920 && height == 1080 && !interlace)
    SetDisplayResolution("1080p");
  else if (width == 1920 && height == 1080)
    SetDisplayResolution("1080i");
  else if (width == 1280 && height == 720)
    SetDisplayResolution("720p");
  else if (width == 720  && height == 480)
    SetDisplayResolution("480p");

  return true;
}

bool CWinEGLPlatformAmlogic::ClampToGUIDisplayLimits(int &width, int &height)
{
  bool rtn = false;
  if (width == 1920 && height == 1080)
  {
    // we can not render GUI fast enough in 1080p.
    // So we will render GUI at 720p and scale that to 1080p display.
    width  = 1280;
    height = 720;
    rtn = true;
  }
  return rtn;
}

bool CWinEGLPlatformAmlogic::ProbeDisplayResolutions(std::vector<CStdString> &resolutions)
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
      // strips, for example, 720p* to 720p
      if (probe_str[i].Right(1) == "*")
        probe_str[i] = probe_str[i].Left(std::max(0, (int)probe_str[i].size() - 1));
      
      if (probe_str[i].Equals("480p"))            resolutions.push_back("720x480p60Hzp");
      else if (probe_str[i].Equals("720p"))       resolutions.push_back("1280x720p60Hz");
      else if (probe_str[i].Equals("720p50hz"))   resolutions.push_back("1280x720p50Hz");
      else if (probe_str[i].Equals("1080i"))      resolutions.push_back("1920x1080i60Hz");
      else if (probe_str[i].Equals("1080i50hz"))  resolutions.push_back("1920x1080i50Hz");
      else if (probe_str[i].Equals("1080p"))      resolutions.push_back("1920x1080p60Hz");
      else if (probe_str[i].Equals("1080p50hz"))  resolutions.push_back("1920x1080p50Hz");
    }
    if (resolutions.size() == 0)
      resolutions.push_back("1280x720p60Hz");
  }
  
  return false;
}

bool CWinEGLPlatformAmlogic::ShowWindow(bool show)
{
  if (show)
    set_sysfs_int("/sys/class/graphics/fb0/blank", 0);
  else
    set_sysfs_int("/sys/class/graphics/fb0/blank", 1);
  return true;
}

int CWinEGLPlatformAmlogic::GetDisplayResolutionMode()
{
  int mode = DISP_MODE_720P;
  CStdString modestr;
  char display_mode[256] = {0};

  get_sysfs_str("/sys/class/display/mode", display_mode, 255);
  modestr = display_mode;
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

  return mode;
}

bool CWinEGLPlatformAmlogic::SetDisplayResolution(const char *resolution)
{
  CStdString modestr;
  modestr = resolution;

  // switch display resolution
  set_sysfs_str("/sys/class/display/mode", modestr.c_str());

  // setup gui freescale depending on display resolution
  DisableFreeScale();
  if (modestr.Equals("1080i") || modestr.Equals("1080p"))
    EnableFreeScale();

  return true;
}

void CWinEGLPlatformAmlogic::EnableFreeScale()
{
  // remove default OSD and video path (default_osd default)
  set_sysfs_str("/sys/class/vfm/map", "rm all");

  // add OSD path
  set_sysfs_str("/sys/class/vfm/map", "add osdpath osd amvideo");
  // enable OSD free scale using frame buffer size of 720p
  set_sysfs_int("/sys/class/graphics/fb0/free_scale", 0);
  set_sysfs_int("/sys/class/graphics/fb1/free_scale", 0);
  set_sysfs_int("/sys/class/graphics/fb0/scale_width",  1280);
  set_sysfs_int("/sys/class/graphics/fb0/scale_height", 720);
  set_sysfs_int("/sys/class/graphics/fb1/scale_width",  1280);
  set_sysfs_int("/sys/class/graphics/fb1/scale_height", 720);
  set_sysfs_int("/sys/class/graphics/fb0/free_scale", 1);
  set_sysfs_int("/sys/class/graphics/fb1/free_scale", 1);
  // remove OSD path
  set_sysfs_int("/sys/class/graphics/fb0/free_scale", 0);
  set_sysfs_int("/sys/class/graphics/fb1/free_scale", 0);
  set_sysfs_str("/sys/class/vfm/map", "rm osdpath");
  // add video path
  set_sysfs_str("/sys/class/vfm/map", "add videopath decoder ppmgr amvideo");
  // enable video free scale (scaling to 1920x1080 with frame buffer size 1280x720)
  set_sysfs_int("/sys/class/ppmgr/ppscaler", 0);
  set_sysfs_int("/sys/class/video/disable_video", 1);
  set_sysfs_int("/sys/class/ppmgr/ppscaler", 1);
  set_sysfs_str("/sys/class/ppmgr/ppscaler_rect", "0 0 1919 1079 0");
  set_sysfs_str("/sys/class/ppmgr/disp", "1280 720");
  //
  set_sysfs_int("/sys/class/graphics/fb0/free_scale", 0);
  set_sysfs_int("/sys/class/graphics/fb1/free_scale", 0);
  set_sysfs_int("/sys/class/graphics/fb0/scale_width",  1280);
  set_sysfs_int("/sys/class/graphics/fb0/scale_height", 720);
  set_sysfs_int("/sys/class/graphics/fb1/scale_width",  1280);
  set_sysfs_int("/sys/class/graphics/fb1/scale_height", 720);
  set_sysfs_int("/sys/class/graphics/fb0/free_scale", 1);
  set_sysfs_int("/sys/class/graphics/fb1/free_scale", 1);
  //
  set_sysfs_int("/sys/class/video/disable_video", 2);
  set_sysfs_str("/sys/class/display/axis", "0 0 1279 719");
  set_sysfs_str("/sys/class/ppmgr/ppscaler_rect", "0 0 1279 719 1");
}

void CWinEGLPlatformAmlogic::DisableFreeScale()
{
  // turn off frame buffer freescale
  set_sysfs_int("/sys/class/graphics/fb0/free_scale", 0);
  set_sysfs_int("/sys/class/graphics/fb1/free_scale", 0);
  // revert to default video paths
  set_sysfs_str("/sys/class/vfm/map", "rm all");
  set_sysfs_str("/sys/class/vfm/map", "add default_osd osd amvideo");
  set_sysfs_str("/sys/class/vfm/map", "add default decoder ppmgr amvideo");
  // disable post processing scaler and disable_video special mode
  set_sysfs_int("/sys/class/ppmgr/ppscaler", 0);
  set_sysfs_int("/sys/class/video/disable_video", 0);

  // revert display axis
  int fd0;
  if ((fd0 = open("/dev/fb0", O_RDWR)) >= 0)
  {
    struct fb_var_screeninfo vinfo;
    if (ioctl(fd0, FBIOGET_VSCREENINFO, &vinfo) == 0)
    {
      char daxis_str[255] = {0};
      sprintf(daxis_str, "%d %d %d %d", 0, 0, vinfo.xres, vinfo.yres);
      set_sysfs_str("/sys/class/display/axis", daxis_str);
    }
    close(fd0);
  }
}
