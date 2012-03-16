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
#include "Util.h"
#include "utils/StdString.h"
#include "utils/log.h"

#include <sys/ioctl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <linux/if.h>
#include <linux/wireless.h>
#include <linux/sockios.h>
#include <errno.h>
#include <resolv.h>
#include <net/if_arp.h>
#include <string.h>
#include <vector>

int PosixParseHex(char *str, unsigned char *addr)
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

bool PosixGuessIsHex(const char *test_hex, size_t length)
{
  // we could get fooled by strings that only
  // have 0-9, A, B, C, D, E, F in them :)
  for (size_t i = 0; i < length; i++)
  {
    switch (*test_hex++)
    {
      default:
        return false;
        break;
      case '0':case '1':case '2':case '3':case '4':
      case '5':case '6':case '7':case '8':case '9':
      case 'a':case 'A':case 'b':case 'B':
      case 'c':case 'C':case 'd':case 'D':
      case 'e':case 'E':case 'f':case 'F':
        break;
    }
  }

  return true;
}

bool IsWireless(int socket, const char *interface)
{
  struct iwreq wrq;
   strcpy(wrq.ifr_name, interface);
   if (ioctl(socket, SIOCGIWNAME, &wrq) < 0)
      return false;

   return true;
}

std::string PosixGetDefaultGateway(const std::string interface)
{
  std::string result = "";

  FILE* fp = fopen("/proc/net/route", "r");
  if (!fp)
    return result;

  char* line     = NULL;
  size_t linel   = 0;
  int n, linenum = 0;
  char   dst[128], iface[16], gateway[128];
  while (getdelim(&line, &linel, '\n', fp) > 0)
  {
    // skip first two lines
    if (linenum++ < 1)
      continue;

    // search where the word begins
    n = sscanf(line, "%16s %128s %128s", iface, dst, gateway);

    if (n < 3)
      continue;

    if (strcmp(iface,   interface.c_str()) == 0 &&
        strcmp(dst,     "00000000") == 0 &&
        strcmp(gateway, "00000000") != 0)
    {
      unsigned char gatewayAddr[4];
      int len = PosixParseHex(gateway, gatewayAddr);
      if (len == 4)
      {
        struct in_addr in;
        in.s_addr = (gatewayAddr[0] << 24) |
          (gatewayAddr[1] << 16) |
          (gatewayAddr[2] << 8)  |
          (gatewayAddr[3]);
        result = inet_ntoa(in);
        break;
      }
    }
  }
  free(line);
  fclose(fp);

  return result;
}

CPosixConnection::CPosixConnection(int socket, const char *interfaceName)
{
  m_socket = socket;
  m_connectionName = interfaceName;
  m_method = IP_CONFIG_DISABLED;

  std::string::size_type start;
  std::string::size_type end;
  if (m_connectionName.find("wire") != std::string::npos)
  {
    m_essid = "Wired";
    // extract the interface name
    start = m_connectionName.find(".") + 1;
    start = m_connectionName.find(".", start) + 1;
    end   = m_connectionName.find(".", start);
    m_interface = m_connectionName.substr(start, end - start);
    m_type = NETWORK_CONNECTION_TYPE_WIRED;
  }
  else if (m_connectionName.find("wifi") != std::string::npos)
  {
    start = m_connectionName.find(".") + 1;
    start = m_connectionName.find(".", start) + 1;
    end   = m_connectionName.find(".", start);
    // extract the interface name
    m_interface = m_connectionName.substr(start, end - start);
    // extract the essid
    start = m_connectionName.find(".", start) + 1;
    end   = m_connectionName.find(".", start);
    m_essid = m_connectionName.substr(start, end - start);
    m_type = NETWORK_CONNECTION_TYPE_WIFI;
    if (m_connectionName.find(".wpa2") != std::string::npos)
      m_encryption = NETWORK_CONNECTION_ENCRYPTION_WPA2;
    else if (m_connectionName.find(".wpa") != std::string::npos)
      m_encryption = NETWORK_CONNECTION_ENCRYPTION_WPA;
    else if (m_connectionName.find(".wep") != std::string::npos)
      m_encryption = NETWORK_CONNECTION_ENCRYPTION_WEP;
    else
      m_encryption = NETWORK_CONNECTION_ENCRYPTION_NONE;
  }
  else
  {
    m_essid = "Unknown";
    m_interface = "unknown";
    m_type = NETWORK_CONNECTION_TYPE_UNKNOWN;
    m_encryption = NETWORK_CONNECTION_ENCRYPTION_UNKNOWN;
  }
  // reformat as simple connection name <wifi.ae:c5:de:bb:4f>
  // so we can use it for passphrase seeds. Extract the IP address.
  start = m_connectionName.find(".") + 1;
  end   = m_connectionName.find(".", start) + 1;
  m_address = m_connectionName.substr(start, end - start);
  if (m_type == NETWORK_CONNECTION_TYPE_WIRED)
    m_connectionName = "wire." + m_essid + "." + m_address;
  else if ( m_type == NETWORK_CONNECTION_TYPE_WIFI)
    m_connectionName = "wifi."  + m_essid + "." + m_address;

  m_state = GetState();
}

CPosixConnection::~CPosixConnection()
{
}

bool CPosixConnection::Connect(IPassphraseStorage *storage, CIPConfig &ipconfig)
{
  if (m_type == NETWORK_CONNECTION_TYPE_WIFI)
  {
    if (m_encryption != NETWORK_CONNECTION_ENCRYPTION_NONE)
    {
      if (!storage->GetPassphrase(m_connectionName, m_passphrase))
        return false;
    }
  }

  ipconfig.m_method     = IP_CONFIG_DHCP;
  ipconfig.m_address    = m_address;
  ipconfig.m_netmask    = m_netmask;
  ipconfig.m_gateway    = m_gateway;
  ipconfig.m_interface  = m_interface;
  ipconfig.m_essid      = m_essid;
  ipconfig.m_encryption = m_encryption;
  ipconfig.m_passphrase = m_passphrase;
  if (SetSettings(ipconfig) && GetState() == NETWORK_CONNECTION_STATE_CONNECTED)
  {
    // hack for now
    m_method = ipconfig.m_method;
    return true;
  }

  return false;
}

ConnectionState CPosixConnection::GetState() const
{
  int zero = 0;
  struct ifreq ifr;

  memset(&ifr, 0x00, sizeof(struct ifreq));
  // check if the interface is up.
  strcpy(ifr.ifr_name, m_interface.c_str());
  if (ioctl(m_socket, SIOCGIFFLAGS, &ifr) < 0)
    return NETWORK_CONNECTION_STATE_DISCONNECTED;

  // check for running and not loopback
  if (!(ifr.ifr_flags & IFF_RUNNING) || (ifr.ifr_flags & IFF_LOOPBACK))
    return NETWORK_CONNECTION_STATE_DISCONNECTED;

  // check for an ip address
  if (ioctl(m_socket, SIOCGIFADDR, &ifr) < 0)
    return NETWORK_CONNECTION_STATE_DISCONNECTED;

  if (ifr.ifr_addr.sa_data == NULL)
    return NETWORK_CONNECTION_STATE_DISCONNECTED;

  // return only interfaces which have an ip address
  if (memcmp(ifr.ifr_addr.sa_data + sizeof(short), &zero, sizeof(int)) == 0)
    return NETWORK_CONNECTION_STATE_DISCONNECTED;

  if (m_type == NETWORK_CONNECTION_TYPE_WIFI)
  {
    // for wifi, we need to check we have a wifi driver name.
    struct iwreq wrq;
    strcpy(wrq.ifr_name, m_interface.c_str());
    if (ioctl(m_socket, SIOCGIWNAME, &wrq) < 0)
      return NETWORK_CONNECTION_STATE_DISCONNECTED;

    // since the wifi interface can be connected to
    // any wifi access point, we need to compare the assigned
    // essid to our connection essid. If they match, then
    // this connection is up.
    char essid[IFNAMSIZ];
    memset(&wrq, 0x00, sizeof(struct iwreq));
    wrq.u.essid.pointer = (caddr_t)essid;
    wrq.u.essid.length  = sizeof(essid);
    strncpy(wrq.ifr_name, m_interface.c_str(), IFNAMSIZ);
    if (ioctl(m_socket, SIOCGIWESSID, &wrq) < 0)
      return NETWORK_CONNECTION_STATE_DISCONNECTED;

    if (wrq.u.essid.length <= 0)
      return NETWORK_CONNECTION_STATE_DISCONNECTED;

    std::string test_essid(essid, wrq.u.essid.length);
    if (m_essid.find(test_essid) == std::string::npos)
      return NETWORK_CONNECTION_STATE_DISCONNECTED;
  }

  // finally, we need to see if we have a gateway assigned to our interface.
  std::string default_gateway = PosixGetDefaultGateway(m_interface);
  if (default_gateway.size() <= 0)
    return NETWORK_CONNECTION_STATE_DISCONNECTED;

  //printf("CPosixConnection::GetState, %s: we are up\n", m_connectionName.c_str());

  // passing the above tests means we are connected.
  return NETWORK_CONNECTION_STATE_CONNECTED;
}

std::string CPosixConnection::GetName() const
{
  return m_essid;
}

std::string CPosixConnection::GetAddress() const
{
  struct ifreq ifr;
  strcpy(ifr.ifr_name, m_interface.c_str());
  ifr.ifr_addr.sa_family = AF_INET;

  if (ioctl(m_socket, SIOCGIFADDR, &ifr) >= 0)
    return inet_ntoa((*((struct sockaddr_in*)&ifr.ifr_addr)).sin_addr);
  else
    return "";
}

std::string CPosixConnection::GetNetmask() const
{
  struct ifreq ifr;
  strcpy(ifr.ifr_name, m_interface.c_str());
  ifr.ifr_addr.sa_family = AF_INET;

  if (ioctl(m_socket, SIOCGIFNETMASK, &ifr) >= 0)
    return inet_ntoa((*((struct sockaddr_in*)&ifr.ifr_addr)).sin_addr);
  else
    return "";
}

std::string CPosixConnection::GetGateway() const
{
  return PosixGetDefaultGateway(m_interface);
}

std::string CPosixConnection::GetNameServer() const
{
  std::string nameserver("127.0.0.1");

  res_init();
  for (int i = 0; i < _res.nscount; i ++)
  {
      nameserver = inet_ntoa(((struct sockaddr_in *)&_res.nsaddr_list[0])->sin_addr);
      break;
  }
  return nameserver;
}

std::string CPosixConnection::GetMacAddress() const
{
  CStdString result;
  result.Format("00:00:00:00:00:00");

  struct ifreq ifr;
  strcpy(ifr.ifr_name, m_interface.c_str());
  if (ioctl(m_socket, SIOCGIFHWADDR, &ifr) >= 0)
  {
    result.Format("%02X:%02X:%02X:%02X:%02X:%02X",
      ifr.ifr_hwaddr.sa_data[0], ifr.ifr_hwaddr.sa_data[1],
      ifr.ifr_hwaddr.sa_data[2], ifr.ifr_hwaddr.sa_data[3],
      ifr.ifr_hwaddr.sa_data[4], ifr.ifr_hwaddr.sa_data[5]);
  }

  return result.c_str();
}

unsigned int CPosixConnection::GetStrength() const
{
  int strength = 100;
  if (m_type == NETWORK_CONNECTION_TYPE_WIFI)
  {
    struct iwreq wreq;
    // wireless tools says this is large enough
    char   buffer[sizeof(struct iw_range) * 2];
    int max_qual_level = 0;
    double max_qual = 92.0;

    // Fetch the range
    memset(buffer, 0x00, sizeof(iw_range) * 2);
    memset(&wreq,  0x00, sizeof(struct iwreq));
    wreq.u.data.pointer = (caddr_t)buffer;
    wreq.u.data.length  = sizeof(buffer);
    wreq.u.data.flags   = 0;
    strncpy(wreq.ifr_name, m_interface.c_str(), IFNAMSIZ);
    if (ioctl(m_socket, SIOCGIWRANGE, &wreq) >= 0)
    {
      struct iw_range *range = (struct iw_range*)buffer;
      if (range->max_qual.qual > 0)
        max_qual = range->max_qual.qual;
      if (range->max_qual.level > 0)
        max_qual_level = range->max_qual.level;
    }

    struct iw_statistics stats;
    memset(&wreq, 0x00, sizeof(struct iwreq));
    // Fetch the stats
    wreq.u.data.pointer = (caddr_t)&stats;
    wreq.u.data.length  = sizeof(stats);
    wreq.u.data.flags   = 1;     // Clear updated flag
    strncpy(wreq.ifr_name, m_interface.c_str(), IFNAMSIZ);
    if (ioctl(m_socket, SIOCGIWSTATS, &wreq) < 0) {
        printf("Failed to fetch signal stats, %s", strerror(errno));
        return 0;
    }

    // this is not correct :)
    strength = (100 * wreq.u.qual.qual)/256;

    //printf("CPosixConnection::GetStrength, strength(%d)\n", strength);
  }
  return strength;
}

EncryptionType CPosixConnection::GetEncryption() const
{
  return m_encryption;
}

unsigned int CPosixConnection::GetSpeed() const
{
  int speed = 100;
  return speed;
}

ConnectionType CPosixConnection::GetType() const
{
  return m_type;
}

IPConfigMethod CPosixConnection::GetMethod() const
{
  return m_method;
}

void CPosixConnection::GetIPConfig(CIPConfig &ipconfig) const
{
  ipconfig.m_method     = m_method;
  ipconfig.m_address    = m_address;
  ipconfig.m_netmask    = m_netmask;
  ipconfig.m_gateway    = m_gateway;
  ipconfig.m_interface  = m_interface;
  ipconfig.m_essid      = m_essid;
  ipconfig.m_encryption = m_encryption;
  ipconfig.m_passphrase = m_passphrase;
}

bool CPosixConnection::PumpNetworkEvents()
{
  bool state_changed = false;

  ConnectionState state = GetState();
  if (m_state != state)
  {
    //printf("CPosixConnection::PumpNetworkEvents, m_connectionName(%s), m_state(%d) -> state(%d)\n",
    //  m_connectionName.c_str(), m_state, state);
    m_state = state;
    state_changed = true;
  }

  return state_changed;
}

bool CPosixConnection::SetSettings(const CIPConfig &ipconfig)
{
  // TODO: handle static in addition to dhcp settings.

  //printf("CPosixConnection::SetSettings %s, method(%d)\n",
  //  m_connectionName.c_str(), ipconfig.m_method);

  FILE *fr = fopen("/etc/network/interfaces", "r");
  if (!fr)
    return false;

  char *line = NULL;
  size_t line_length = 0;
  std::vector<std::string> interfaces_lines;
  while (getdelim(&line, &line_length, '\n', fr) > 0)
    interfaces_lines.push_back(line);
  fclose(fr);

  std::vector<std::string> new_interfaces_lines;
  std::vector<std::string> ifdown_interfaces;
  for (size_t i = 0; i < interfaces_lines.size(); i++)
  {
    //printf("CPosixConnection::SetSettings, interfaces_lines:%s", interfaces_lines[i].c_str());
    // always copy auto section over
    if (interfaces_lines[i].find("auto") != std::string::npos)
    {
      new_interfaces_lines.push_back(interfaces_lines[i]);
      continue;
    }

    // always copy loopback iface section over
    if (interfaces_lines[i].find("iface lo") != std::string::npos)
    {
      new_interfaces_lines.push_back(interfaces_lines[i]);
      continue;
    }

    // look for "iface <interface name> inet"
    if (interfaces_lines[i].find("iface") != std::string::npos)
    {
      // we always copy the iface line over.
      new_interfaces_lines.push_back(interfaces_lines[i]);

      // we will take all interfaces down, then bring up this one.
      // so find all iface names.
      std::string ifdown_interface = interfaces_lines[i];
      std::string::size_type start = ifdown_interface.find("iface") + sizeof("iface");
      std::string::size_type end   = ifdown_interface.find("inet", start);
      ifdown_interfaces.push_back(ifdown_interface.substr(start, end - start));

      // is this our interface section (ethX or wlanX)
      if (interfaces_lines[i].find(ipconfig.m_interface) != std::string::npos)
      {
        // we only touch wifi settings right now.
        if (m_type == NETWORK_CONNECTION_TYPE_WIFI)
        {
          std::string tmp;
          if (m_encryption == NETWORK_CONNECTION_ENCRYPTION_NONE)
          {
            tmp = "  wireless-essid \"" + ipconfig.m_essid + "\"\n";
            new_interfaces_lines.push_back(tmp);
          }
          else if (m_encryption == NETWORK_CONNECTION_ENCRYPTION_WEP)
          {
            tmp = "  wireless-essid \"" + ipconfig.m_essid + "\"\n";
            new_interfaces_lines.push_back(tmp);
            tmp = "  wireless-mode managed\n";
            new_interfaces_lines.push_back(tmp);
            // if ascii, then quote it, if hex, no quotes
            if (PosixGuessIsHex(ipconfig.m_passphrase.c_str(), ipconfig.m_passphrase.size()))
              tmp = "  wireless-key " + ipconfig.m_passphrase + "\n";
            else
              tmp = "  wireless-key \"s:" + ipconfig.m_passphrase + "\"\n";
            new_interfaces_lines.push_back(tmp);
          }
          else if (m_encryption == NETWORK_CONNECTION_ENCRYPTION_WPA ||
            m_encryption == NETWORK_CONNECTION_ENCRYPTION_WPA2)
          {
            tmp = "  wpa-ssid \"" + ipconfig.m_essid + "\"\n";
            new_interfaces_lines.push_back(tmp);
            // if ascii, then quote it, if hex, no quotes
            if (PosixGuessIsHex(ipconfig.m_passphrase.c_str(), ipconfig.m_passphrase.size()))
              tmp = "  wpa-psk " + ipconfig.m_passphrase + "\n";
            else
              tmp = "  wpa-psk \"" + ipconfig.m_passphrase + "\"\n";
            new_interfaces_lines.push_back(tmp);
            if (ipconfig.m_encryption == NETWORK_CONNECTION_ENCRYPTION_WPA)
              tmp = "  wpa-proto WPA\n";
            else
              tmp = "  wpa-proto WPA2\n";
            new_interfaces_lines.push_back(tmp);
          }
        }
      }
    }
  }

  FILE* fw = fopen("/etc/network/interfaces.temp", "w");
  if (!fw)
    return false;
  for (size_t i = 0; i < new_interfaces_lines.size(); i++)
  {
    printf("CPosixConnection::SetSettings, new_interfaces_lines:%s", new_interfaces_lines[i].c_str());
    fwrite(new_interfaces_lines[i].c_str(), new_interfaces_lines[i].size(), 1, fw);
  }
  fclose(fw);

  // Rename the file (remember, you can not rename across devices)
  if (rename("/etc/network/interfaces.temp", "/etc/network/interfaces") < 0)
    return false;

  int rtn_error;
  std::string cmd;
  for (size_t i = 0; i < ifdown_interfaces.size(); i++)
  {
    cmd = "/sbin/ifdown " + ifdown_interfaces[i];
    rtn_error = system(cmd.c_str());
    if (rtn_error != 0 && rtn_error != ECHILD)
      CLog::Log(LOGERROR, "Unable to stop interface %s, %s", ifdown_interfaces[i].c_str(), strerror(errno));
    else
      CLog::Log(LOGINFO, "Stopped interface %s", ifdown_interfaces[i].c_str());
  }

  cmd = "/sbin/ifup " + ipconfig.m_interface;
  rtn_error = system(cmd.c_str());
  if (rtn_error != 0 && rtn_error != ECHILD)
    CLog::Log(LOGERROR, "Unable to start interface %s, %s", ipconfig.m_interface.c_str(), strerror(errno));
  else
    CLog::Log(LOGINFO, "Started interface %s", ipconfig.m_interface.c_str());

  return true;
}
