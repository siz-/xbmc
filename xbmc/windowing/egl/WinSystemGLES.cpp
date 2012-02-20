/*
 *      Copyright (C) 2011 Team XBMC
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
#include "system.h"

#ifdef HAS_EGLGLES

#include "WinSystemGLES.h"
#include "filesystem/SpecialProtocol.h"
#include "settings/Settings.h"
#include "guilib/Texture.h"
#include "utils/log.h"
#include "windowing/vendor/VendorAmlogic.h"
#include "WinBindingEGL.h"

////////////////////////////////////////////////////////////////////////////////////////////
CWinSystemGLES::CWinSystemGLES() : CWinSystemBase()
{
  m_window = NULL;
  m_eglBinding = new CWinBindingEGL();
  m_eWindowSystem = WINDOW_SYSTEM_EGL;
}

CWinSystemGLES::~CWinSystemGLES()
{
  DestroyWindowSystem();
  delete m_eglBinding;
}

bool CWinSystemGLES::InitWindowSystem()
{
  printf("CWinSystemGLES::InitWindowSystem\n");
  m_fb_width  = 1280;
  m_fb_height = 720;
  m_fb_bpp    = 8;

  CLog::Log(LOGDEBUG, "Video mode: %dx%d with %d bits per pixel.",
    m_fb_width, m_fb_height, m_fb_bpp);

  CVendorAmlogic amlogic;
  m_window  = (fbdev_window*)amlogic.CreateNativeWindow(m_fb_width, m_fb_height, m_fb_bpp);
  m_display = EGL_DEFAULT_DISPLAY;

  if (!CWinSystemBase::InitWindowSystem())
    return false;

  return true;
}

bool CWinSystemGLES::DestroyWindowSystem()
{
  printf("CWinSystemGLES::DestroyWindowSystem\n");
  return true;
}

bool CWinSystemGLES::CreateNewWindow(const CStdString& name, bool fullScreen, RESOLUTION_INFO& res, PHANDLE_EVENT_FUNC userFunction)
{
  printf("CWinSystemGLES::CreateNewWindow\n");
  m_nWidth  = res.iWidth;
  m_nHeight = res.iHeight;
  m_bFullScreen = fullScreen;

  CVendorAmlogic amlogic;
  if (res.iScreenWidth == 1920 && res.iScreenHeight == 1080 && (res.dwFlags & D3DPRESENTFLAG_PROGRESSIVE))
    amlogic.SetDisplayResolution("1080p");
  else if (res.iScreenWidth == 1920 && res.iScreenHeight == 1080)
    amlogic.SetDisplayResolution("1080i");
  else if (res.iScreenWidth == 1280 && res.iScreenHeight == 720)
    amlogic.SetDisplayResolution("720p");
  else if (res.iScreenWidth == 720  && res.iScreenHeight == 480)
    amlogic.SetDisplayResolution("480p");

  if (!m_eglBinding->CreateWindow((EGLNativeDisplayType)m_display, (EGLNativeWindowType)m_window))
    return false;

  m_bWindowCreated = true;
  Show();

  return true;
}

bool CWinSystemGLES::DestroyWindow()
{
  printf("CWinSystemGLES::DestroyWindow\n");
  Hide();

  m_eglBinding->DestroyWindow();

  if (m_window)
  {
    CVendorAmlogic amlogic;
    amlogic.DestroyNativeWindow(m_window);
    m_window = NULL;
  }

  m_bWindowCreated = false;

  return true;
}

bool CWinSystemGLES::ResizeWindow(int newWidth, int newHeight, int newLeft, int newTop)
{
  printf("CWinSystemGLES::ResizeWindow\n");
  CRenderSystemGLES::ResetRenderSystem(newWidth, newHeight, true, 0);
  return true;
}

bool CWinSystemGLES::SetFullScreen(bool fullScreen, RESOLUTION_INFO& res, bool blankOtherDisplays)
{
  printf("CWinSystemGLES::SetFullScreen\n");
  CLog::Log(LOGDEBUG, "CWinSystemDFB::SetFullScreen");
  m_nWidth  = res.iWidth;
  m_nHeight = res.iHeight;
  m_bFullScreen = fullScreen;

  m_eglBinding->ReleaseSurface();
  CreateNewWindow("", fullScreen, res, NULL);

  CRenderSystemGLES::ResetRenderSystem(res.iWidth, res.iHeight, true, 0);

  return true;
}

void CWinSystemGLES::UpdateResolutions()
{
  printf("CWinSystemGLES::UpdateResolutions\n");

  std::vector<CStdString> resolutions;

  CVendorAmlogic amlogic;
  amlogic.ProbeDisplayResolutions(resolutions);

  for (size_t i = 0; i < resolutions.size(); i++)
    printf("CWinSystemGLES::UpdateResolutions:%s\n", resolutions[i].c_str());

  bool got_display_rez = false;
  RESOLUTION Res720p60 = RES_INVALID;
  RESOLUTION res_index = RES_DESKTOP;

  for (size_t i = 0; i < resolutions.size(); i++)
  {
    // we expect this type of output from popen
    // printf("CWinSystemGLES::UpdateResolutions = %s", line);
    //   1280x720p50Hz
    //   1280x720p60Hz
    //   1920x1080i50Hz
    //   1920x1080i60Hz
    //   1920x1080p24Hz
    //   1920x1080p50Hz
    //   1920x1080p60Hz

    char interlacing;
    int  refresh, width, height;
    if (sscanf(resolutions[i].c_str(), "%dx%d%c%dHz", &width, &height, &interlacing, &refresh) == 4)
    {
      // We only care about progressive 60, 50 or 24Hz resolutions with a height of >= 720
      if (height < 720 || interlacing == 'i' || !(refresh == 60 || refresh == 50 ||refresh == 24))
        continue;

      got_display_rez = true;
      // if this is a new setting,
      // create a new empty setting to fill in.
      if ((int)g_settings.m_ResInfo.size() <= res_index)
      {
        RESOLUTION_INFO res;
        g_settings.m_ResInfo.push_back(res);
      }
      int gui_width  = width;
      int gui_height = height;
      float gui_refresh = refresh;
      if (gui_width == 1920 && gui_height == 1080)
      {
        // we can not render GUI fast enough in 1080p.
        // So we will render GUI at 720p and scale that to 1080p display.
        gui_width  = 1280;
        gui_height = 720;
      }

      g_settings.m_ResInfo[res_index].iScreen       = 0;
      g_settings.m_ResInfo[res_index].bFullScreen   = true;
      g_settings.m_ResInfo[res_index].iSubtitles    = (int)(0.965 * gui_height);
      g_settings.m_ResInfo[res_index].dwFlags       = D3DPRESENTFLAG_PROGRESSIVE;
      g_settings.m_ResInfo[res_index].fRefreshRate  = gui_refresh;
      g_settings.m_ResInfo[res_index].fPixelRatio   = 1.0f;
      g_settings.m_ResInfo[res_index].iWidth        = gui_width;
      g_settings.m_ResInfo[res_index].iHeight       = gui_height;
      g_settings.m_ResInfo[res_index].iScreenWidth  = width;
      g_settings.m_ResInfo[res_index].iScreenHeight = height;
      g_settings.m_ResInfo[res_index].strMode.Format("%dx%d @ %.2f - Full Screen", width, height, gui_refresh);
      g_graphicsContext.ResetOverscan(g_settings.m_ResInfo[res_index]);

      CLog::Log(LOGINFO, "Found possible resolution for display %d with %d x %d @ %f Hz\n",
        g_settings.m_ResInfo[res_index].iScreen,
        g_settings.m_ResInfo[res_index].iScreenWidth,
        g_settings.m_ResInfo[res_index].iScreenHeight,
        g_settings.m_ResInfo[res_index].fRefreshRate);

      if (width == 1280 && height == 720 && refresh == 60)
        Res720p60 = res_index;

      res_index = (RESOLUTION)((int)res_index + 1);
    }
    // swap desktop index for 720p if available
    if (Res720p60 != RES_INVALID)
    {
      CLog::Log(LOGINFO, "Found 720p at %d, setting to RES_DESKTOP at %d", (int)Res720p60, (int)RES_DESKTOP);

      RESOLUTION_INFO desktop = g_settings.m_ResInfo[RES_DESKTOP];
      g_settings.m_ResInfo[RES_DESKTOP] = g_settings.m_ResInfo[Res720p60];
      g_settings.m_ResInfo[Res720p60] = desktop;
    }
  }
}

bool CWinSystemGLES::IsExtSupported(const char* extension)
{
  if(strncmp(extension, "EGL_", 4) != 0)
    return CRenderSystemGLES::IsExtSupported(extension);

  return m_eglBinding->IsExtSupported(extension);
}

bool CWinSystemGLES::PresentRenderImpl(const CDirtyRegionList &dirty)
{
  m_eglBinding->SwapBuffers();

  return true;
}

void CWinSystemGLES::SetVSyncImpl(bool enable)
{
  m_iVSyncMode = enable ? 10 : 0;
  if (m_eglBinding->SetVSync(enable) == FALSE)
    CLog::Log(LOGERROR, "CWinSystemDFB::SetVSyncImpl: Could not set egl vsync");
}

void CWinSystemGLES::ShowOSMouse(bool show)
{
}

void CWinSystemGLES::NotifyAppActiveChange(bool bActivated)
{
}

bool CWinSystemGLES::Minimize()
{
  Hide();
  return true;
}

bool CWinSystemGLES::Restore()
{
  Show(true);
  return false;
}

bool CWinSystemGLES::Hide()
{
  CVendorAmlogic amlogic;
  amlogic.ShowWindow(false);
  return true;
}

bool CWinSystemGLES::Show(bool raise)
{
  CVendorAmlogic amlogic;
  amlogic.ShowWindow(true);
  return true;
}

#endif
