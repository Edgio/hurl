//! ----------------------------------------------------------------------------
//! Copyright Edgio Inc.
//!
//! \file:    TODO
//! \details: TODO
//!
//! Licensed under the terms of the Apache 2.0 open source license.
//! Please refer to the LICENSE file in the project root for the terms.
//! ----------------------------------------------------------------------------
//! ----------------------------------------------------------------------------
//! includes
//! ----------------------------------------------------------------------------
#include "support/time_util.h"
#include "support/ndebug.h"
#include "dns/nlookup.h"
#include "nconn/host_info.h"
#include "status.h"
#include <unistd.h>
#include <netdb.h>
#include <string.h>
// for inet_pton
#include <arpa/inet.h>
namespace ns_hurl {
//! ----------------------------------------------------------------------------
//! \details: slow resolution
//! \return:  TODO
//! \param:   TODO
//! ----------------------------------------------------------------------------
int32_t nlookup(const std::string &a_host,
                uint16_t a_port,
                host_info &ao_host_info,
                int a_ai_family)
{
        //NDBG_PRINT("%sRESOLVE%s: a_host: %s a_port: %d\n",
        //           ANSI_COLOR_BG_RED, ANSI_COLOR_OFF,
        //           a_host.c_str(), a_port);
        // Initialize...
        ao_host_info.m_sa_len = sizeof(ao_host_info.m_sa);
        memset((void*) &(ao_host_info.m_sa), 0, ao_host_info.m_sa_len);
        // ---------------------------------------
        // get address...
        // ---------------------------------------
        struct addrinfo l_hints;
        memset(&l_hints, 0, sizeof(l_hints));
        l_hints.ai_family = a_ai_family;
        l_hints.ai_socktype = SOCK_STREAM;
        char portstr[10];
        snprintf(portstr, sizeof(portstr), "%d", (int) a_port);
        struct addrinfo* l_addrinfo;
        int l_gaierr;
        l_gaierr = getaddrinfo(a_host.c_str(), portstr, &l_hints, &l_addrinfo);
        if (l_gaierr != 0)
        {
                //NDBG_PRINT("Error getaddrinfo '%s': %s\n", a_host.c_str(), gai_strerror(l_gaierr));
                return STATUS_ERROR;
        }
        // Find the first IPv4 and IPv6 entries.
        struct addrinfo* l_addrinfo_v4 = nullptr;
        struct addrinfo* l_addrinfo_v6 = nullptr;
        for (struct addrinfo* i_addrinfo = l_addrinfo;
             i_addrinfo != (struct addrinfo*) 0;
             i_addrinfo = i_addrinfo->ai_next)
        {
                switch (i_addrinfo->ai_family)
                {
                case AF_INET:
                {
                        if (l_addrinfo_v4 == (struct addrinfo*) 0)
                        {
                                l_addrinfo_v4 = i_addrinfo;
                        }
                        break;
                }
                case AF_INET6:
                {
                        if (l_addrinfo_v6 == (struct addrinfo*) 0)
                        {
                                l_addrinfo_v6 = i_addrinfo;
                        }
                        break;
                }
                }
                // hack to force to check all lookups for localhost...
                if ((a_host != "localhost") &&
                   (l_addrinfo_v4 ||
                    l_addrinfo_v6))
                {
                       break;
                }
        }
        // If there's an IPv4 address, use that, otherwise try IPv6.
        if (l_addrinfo_v4 != nullptr)
        {
                if (sizeof(ao_host_info.m_sa) < l_addrinfo_v4->ai_addrlen)
                {
                        //NDBG_PRINT("Error %s - sockaddr too small (%lu < %lu)\n",
                        //           a_host.c_str(),
                        //      (unsigned long) sizeof(ao_host_info.m_sa),
                        //      (unsigned long) l_addrinfo_v4->ai_addrlen);
                        return STATUS_ERROR;
                }
                ao_host_info.m_sock_family = l_addrinfo_v4->ai_family;
                ao_host_info.m_sock_type = l_addrinfo_v4->ai_socktype;
                ao_host_info.m_sock_protocol = l_addrinfo_v4->ai_protocol;
                ao_host_info.m_sa_len = l_addrinfo_v4->ai_addrlen;
                //NDBG_PRINT("memmove: addrlen: %d\n", l_addrinfo_v4->ai_addrlen);
                //mem_display((const uint8_t *)l_addrinfo_v4->ai_addr,
                //            l_addrinfo_v4->ai_addrlen,
                //            true);
                memmove(&(ao_host_info.m_sa),
                        l_addrinfo_v4->ai_addr,
                        l_addrinfo_v4->ai_addrlen);
                // Set the port
                ((sockaddr_in *)(&(ao_host_info.m_sa)))->sin_port = htons(a_port);
                freeaddrinfo(l_addrinfo);
        }
        else if (l_addrinfo_v6 != nullptr)
        {
                if (sizeof(ao_host_info.m_sa) < l_addrinfo_v6->ai_addrlen)
                {
                        //NDBG_PRINT("Error %s - sockaddr too small (%lu < %lu)\n",
                        //           a_host.c_str(),
                        //      (unsigned long) sizeof(ao_host_info.m_sa),
                        //      (unsigned long) l_addrinfo_v6->ai_addrlen);
                        return STATUS_ERROR;
                }
                ao_host_info.m_sock_family = l_addrinfo_v6->ai_family;
                ao_host_info.m_sock_type = l_addrinfo_v6->ai_socktype;
                ao_host_info.m_sock_protocol = l_addrinfo_v6->ai_protocol;
                ao_host_info.m_sa_len = l_addrinfo_v6->ai_addrlen;
                //NDBG_PRINT("memmove: addrlen: %d\n", l_addrinfo_v6->ai_addrlen);
                //ns_hurl::mem_display((const uint8_t *)l_addrinfo_v6->ai_addr,
                //                     l_addrinfo_v6->ai_addrlen,
                //                     true);
                memmove(&ao_host_info.m_sa,
                        l_addrinfo_v6->ai_addr,
                        l_addrinfo_v6->ai_addrlen);
                // Set the port
                ((sockaddr_in6 *)(&(ao_host_info.m_sa)))->sin6_port = htons(a_port);
                freeaddrinfo(l_addrinfo);
        }
        else
        {
                //NDBG_PRINT("Error no valid address found for host %s\n", a_host.c_str());
                return STATUS_ERROR;
        }
        // Set to 60min -cuz getaddr-info stinks...
        ao_host_info.m_expires_s = get_time_s() + 3600;
        return STATUS_OK;
}
}
