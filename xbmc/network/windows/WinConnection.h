#pragma once
/*
 *      Copyright (C) 2005-2010 Team XBMC
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
#ifdef _WIN32
#include "xbmc/network/IConnection.h"
#include "Iphlpapi.h"

class CWinConnection : public IConnection
{
public:
  CWinConnection(IP_ADAPTER_INFO adapter);
  virtual ~CWinConnection();

  virtual bool Connect(IPassphraseStorage *storage, CIPConfig &ipconfig);
  virtual bool Disconnect() { return true; };
  virtual ConnectionState GetState() const;

  virtual std::string GetName() const;

  virtual std::string GetAddress() const;
  virtual std::string GetNetmask() const;
  virtual std::string GetGateway() const;
  virtual std::string GetMacAddress() const;

  virtual unsigned int GetStrength() const;
  virtual EncryptionType GetEncryption() const;
  virtual unsigned int GetConnectionSpeed() const;

  virtual ConnectionType GetType() const;
  virtual IPConfigMethod GetMethod() const;
private:
  IP_ADAPTER_INFO m_adapter;
};
#endif
