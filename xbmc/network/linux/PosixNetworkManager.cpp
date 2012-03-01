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

#include "PosixNetworkManager.h"
#include "PosixConnection.h"

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

CPosixNetworkManager::CPosixNetworkManager()
{
  m_socket = socket(AF_INET, SOCK_DGRAM, 0);
}

CPosixNetworkManager::~CPosixNetworkManager()
{
  if (m_socket != -1)
    close(m_socket);
}

bool CPosixNetworkManager::CanManageConnections()
{
  return false;
}

ConnectionList CPosixNetworkManager::GetConnections()
{
  ConnectionList connections;

#ifdef __APPLE__
  // Query the list of interfaces.
  struct ifaddrs *list;

  if (getifaddrs(&list) < 0)
    return connections;

  struct ifaddrs *cur;
  for(cur = list; cur != NULL; cur = cur->ifa_next)
  {
    if(cur->ifa_addr->sa_family != AF_INET)
      continue;

    // Add the interface
    connections.push_back(CConnectionPtr(new CPosixConnection(m_socket, cur->ifa_name));
  }

  freeifaddrs(list);
#else
  FILE* fp = fopen("/proc/net/dev", "r");
  if (!fp)
    return connections;

  char* line = NULL;
  size_t linel = 0;
  int n;
  char* interfaceName;
  int linenum = 0;

  while (getdelim(&line, &linel, '\n', fp) > 0)
  {
    // skip first two lines
    if (linenum++ < 2)
      continue;

    // search where the word begins
    interfaceName = line;
    while (isspace(*interfaceName))
      ++interfaceName;

    // read word until :
    n = strcspn(interfaceName, ": \t");
    interfaceName[n] = 0;

    // make sure the device has ethernet encapsulation
    struct ifreq ifr;
    strcpy(ifr.ifr_name, interfaceName);
    if (ioctl(m_socket, SIOCGIFHWADDR, &ifr) >= 0 && ifr.ifr_hwaddr.sa_family == ARPHRD_ETHER)
      connections.push_back(CConnectionPtr(new CPosixConnection(m_socket, interfaceName)));
  }

  free(line);
  fclose(fp);
#endif

  return connections;
}

bool CPosixNetworkManager::PumpNetworkEvents(INetworkEventsCallback *callback)
{
  return false;
}
