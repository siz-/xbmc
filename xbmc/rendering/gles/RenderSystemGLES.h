/*
*      Copyright (C) 2005-2008 Team XBMC
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

#ifndef RENDER_SYSTEM_GLES_H
#define RENDER_SYSTEM_GLES_H

#pragma once

#include "rendering/RenderSystem.h"
#include "xbmc/guilib/GUIShader.h"

enum ESHADERMETHOD
{
  SM_DEFAULT,
  SM_TEXTURE,
  SM_MULTI,
  SM_FONTS,
  SM_TEXTURE_NOBLEND,
  SM_MULTI_BLENDCOLOR,
  SM_TEXTURE_RGBA,
  SM_TEXTURE_RGBA_BLENDCOLOR,
  SM_ESHADERCOUNT
};

struct PackedVertex
{
  float x, y, z;
#ifdef HAS_DX
  unsigned char b, g, r, a;
#else
  unsigned char r, g, b, a;
#endif
  float u1, v1;
  float u2, v2;
};
typedef std::vector<PackedVertex> PackedVertices;

struct BatchTexture
{
  GLubyte textureNum;
  GLubyte diffuseNum;
  unsigned int type;
  bool hasAlpha;
  PackedVertices vertices;
};
typedef std::vector<BatchTexture> BatchTextures;

class CRenderSystemGLES : public CRenderSystemBase
{
public:
  CRenderSystemGLES();
  virtual ~CRenderSystemGLES();

  virtual bool InitRenderSystem();
  virtual bool DestroyRenderSystem();
  virtual bool ResetRenderSystem(int width, int height, bool fullScreen, float refreshRate);

  virtual bool BeginRender();
  virtual bool EndRender();
  virtual bool PresentRender(const CDirtyRegionList &dirty);
  virtual bool ClearBuffers(color_t color);
  virtual bool IsExtSupported(const char* extension);

  virtual void SetVSync(bool vsync);

  virtual void SetViewPort(CRect& viewPort);
  virtual void GetViewPort(CRect& viewPort);

  virtual void SetScissors(const CRect& rect);
  virtual void ResetScissors();

  virtual void CaptureStateBlock();
  virtual void ApplyStateBlock();

  virtual void SetCameraPosition(const CPoint &camera, int screenWidth, int screenHeight);

  virtual void ApplyHardwareTransform(const TransformMatrix &matrix);
  virtual void RestoreHardwareTransform();

  virtual bool TestRender();

  virtual void Project(float &x, float &y, float &z);
  
  void InitialiseGUIShader();
  void EnableGUIShader(ESHADERMETHOD method);
  void DisableGUIShader();
  void SetBlending(bool blend);
  void SetActiveTexture(GLenum texture);

  void AddBatchRegion(BatchTexture &tex);
  void RenderRegions();
  void ClearRegions();

  GLint GUIShaderGetPos();
  GLint GUIShaderGetCol();
  GLint GUIShaderGetCoord0();
  GLint GUIShaderGetCoord1();
  GLint GUIShaderGetUniCol();

protected:
  virtual void SetVSyncImpl(bool enable) = 0;
  virtual bool PresentRenderImpl(const CDirtyRegionList &dirty) = 0;
  void CalculateMaxTexturesize();
  
  int        m_iVSyncMode;
  int        m_iVSyncErrors;
  int64_t    m_iSwapStamp;
  int64_t    m_iSwapRate;
  int64_t    m_iSwapTime;
  bool       m_bVsyncInit;
  int        m_width;
  int        m_height;

  CStdString m_RenderExtensions;

  CGUIShader  **m_pGUIshader;  // One GUI shader for each method
  ESHADERMETHOD m_method;      // Current GUI Shader method

  GLfloat    m_view[16];
  GLfloat    m_projection[16];
  GLint      m_viewPort[4];
  GLuint     m_vbovert;
  GLuint     m_vboidx;
  BatchTextures m_batchRegions;
  PackedVertices m_packedVertices;

  bool   m_blending;
  GLenum m_activeTexture;

  std::vector<GLushort> m_idx;
};

#endif // RENDER_SYSTEM_H
