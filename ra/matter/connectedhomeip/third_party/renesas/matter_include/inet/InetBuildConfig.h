#ifndef INET_INETBUILDCONFIG_H_
#define INET_INETBUILDCONFIG_H_

#include "matter_config.h"

#define INET_CONFIG_ENABLE_TCP_ENDPOINT 1

#define HAVE_LWIP_RAW_BIND_NETIF 1
#define INET_PROJECT_CONFIG_INCLUDE <CHIPProjectConfig.h>
#define INET_PLATFORM_CONFIG_INCLUDE <platform/renesas/InetPlatformConfig.h>
#define INET_TCP_END_POINT_IMPL_CONFIG_FILE <inet/TCPEndPointImplLwIP.h>
#define INET_UDP_END_POINT_IMPL_CONFIG_FILE <inet/UDPEndPointImplLwIP.h>

#endif  // INET_INETBUILDCONFIG_H_
