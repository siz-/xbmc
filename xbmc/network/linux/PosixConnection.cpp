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

#include "PosixConnection.h"
#include "xbmc/utils/StdString.h"

#include <sys/ioctl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#ifndef __APPLE__
#include <linux/if.h>
#include <linux/wireless.h>
#include <linux/sockios.h>
#endif
#include <errno.h>
#include <resolv.h>
#ifdef __APPLE__
#include <sys/sockio.h>
#include <net/if.h>
#include <ifaddrs.h>
#endif
#include <net/if_arp.h>
#include <string.h>

CPosixConnection::CPosixConnection(int socket, const char *interfaceName)
{
  m_socket = socket;
  m_interfaceName = interfaceName;
}

CPosixConnection::~CPosixConnection()
{
}

bool CPosixConnection::Connect(IPassphraseStorage *storage, const CIPConfig &ipconfig)
{
  return false;
}

ConnectionState CPosixConnection::GetConnectionState() const
{
  struct ifreq ifr;
  int zero = 0;
  memset(&ifr,0,sizeof(struct ifreq));
  strcpy(ifr.ifr_name, m_interfaceName.c_str());
  if (ioctl(m_socket, SIOCGIFFLAGS, &ifr) < 0)
    return NETWORK_CONNECTION_STATE_DISCONNECTED;

  // ignore loopback
  int iRunning = ( (ifr.ifr_flags & IFF_RUNNING) && (!(ifr.ifr_flags & IFF_LOOPBACK)));

  if (ioctl(m_socket, SIOCGIFADDR, &ifr) < 0)
    return NETWORK_CONNECTION_STATE_DISCONNECTED;

  // return only interfaces which has ip address
  if (iRunning && (0 != memcmp(ifr.ifr_addr.sa_data+sizeof(short), &zero, sizeof(int))))
    return NETWORK_CONNECTION_STATE_CONNECTED;
  else
    return NETWORK_CONNECTION_STATE_DISCONNECTED;
}

std::string CPosixConnection::GetName() const
{
  return m_interfaceName;
}


std::string CPosixConnection::GetIP() const
{
  struct ifreq ifr;
  strcpy(ifr.ifr_name, m_interfaceName.c_str());
  ifr.ifr_addr.sa_family = AF_INET;

  if (ioctl(m_socket, SIOCGIFADDR, &ifr) >= 0)
    return inet_ntoa((*((struct sockaddr_in *)&ifr.ifr_addr)).sin_addr);
  else
    return "";
}

std::string CPosixConnection::GetNetmask() const
{
  struct ifreq ifr;
  strcpy(ifr.ifr_name, m_interfaceName.c_str());
  ifr.ifr_addr.sa_family = AF_INET;

  if (ioctl(m_socket, SIOCGIFNETMASK, &ifr) >= 0)
    return inet_ntoa((*((struct sockaddr_in*)&ifr.ifr_addr)).sin_addr);
  else
    return "";
}

std::string CPosixConnection::GetMacAddress() const
{
  CStdString result = "";

#ifdef __APPLE__
  result.Format("00:00:00:00:00:00");
#else
  struct ifreq ifr;
  strcpy(ifr.ifr_name, m_interfaceName.c_str());
  if (ioctl(m_socket, SIOCGIFHWADDR, &ifr) >= 0)
  {
    result.Format("%hhX:%hhX:%hhX:%hhX:%hhX:%hhX",  ifr.ifr_hwaddr.sa_data[0],
                                                    ifr.ifr_hwaddr.sa_data[1],
                                                    ifr.ifr_hwaddr.sa_data[2],
                                                    ifr.ifr_hwaddr.sa_data[3],
                                                    ifr.ifr_hwaddr.sa_data[4],
                                                    ifr.ifr_hwaddr.sa_data[5]);
  }
#endif

  return result.c_str();
}

std::string CPosixConnection::GetGateway() const
{
  std::string result = "";

#ifndef __APPLE__
  FILE* fp = fopen("/proc/net/route", "r");
  if (!fp)
    return result;

  char* line = NULL;
  char iface[16];
  char dst[128];
  char gateway[128];
  size_t linel = 0;
  int n;
  int linenum = 0;
  while (getdelim(&line, &linel, '\n', fp) > 0)
  {
    // skip first two lines
    if (linenum++ < 1)
      continue;

    // search where the word begins
    n = sscanf(line, "%16s %128s %128s",
    iface, dst, gateway);

    if (n < 3)
      continue;

    if (strcmp(iface, m_interfaceName.c_str()) == 0 &&
    strcmp(dst, "00000000") == 0 &&
    strcmp(gateway, "00000000") != 0)
    {
      unsigned char gatewayAddr[4];
      int len = ParseHex(gateway, gatewayAddr);
      if (len == 4)
      {
        struct in_addr in;
        in.s_addr = (gatewayAddr[0] << 24) | (gatewayAddr[1] << 16) |
        (gatewayAddr[2] << 8) | (gatewayAddr[3]);
        result = inet_ntoa(in);
        break;
      }
    }
  }
  free(line);
  fclose(fp);
#endif

  return result;
}

unsigned int CPosixConnection::GetStrength() const
{
  return 0;
}

EncryptionType CPosixConnection::GetEncryption() const
{
  return NETWORK_CONNECTION_ENCRYPTION_NONE;
}

unsigned int CPosixConnection::GetConnectionSpeed() const
{
  return 100;
}

ConnectionType CPosixConnection::GetConnectionType() const
{
  return NETWORK_CONNECTION_TYPE_WIRED;
}

int CPosixConnection::ParseHex(char *str, unsigned char *addr)
{
  int len = 0;

  while (*str)
  {
    int tmp;
    if (str[1] == 0)
      return -1;
    if (sscanf(str, "%02x", (unsigned int *)&tmp) != 1)
      return -1;
    addr[len] = tmp;
    len++;
    str += 2;
  }

  return len;
}
