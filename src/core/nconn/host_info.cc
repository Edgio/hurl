//: ----------------------------------------------------------------------------
//: Copyright (C) 2016 Verizon.  All Rights Reserved.
//: All Rights Reserved
//:
//: \file:    host_info.cc
//: \details: TODO
//: \author:  Reed P. Morrison
//: \date:    11/20/2015
//:
//:   Licensed under the Apache License, Version 2.0 (the "License");
//:   you may not use this file except in compliance with the License.
//:   You may obtain a copy of the License at
//:
//:       http://www.apache.org/licenses/LICENSE-2.0
//:
//:   Unless required by applicable law or agreed to in writing, software
//:   distributed under the License is distributed on an "AS IS" BASIS,
//:   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//:   See the License for the specific language governing permissions and
//:   limitations under the License.
//:
//: ----------------------------------------------------------------------------
//: ----------------------------------------------------------------------------
//: Includes
//: ----------------------------------------------------------------------------
#include "hurl/nconn/host_info.h"
#include <netinet/in.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdio.h>
namespace ns_hurl {
//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
host_info::host_info():
        m_sa(),
        m_sa_len(16),
        m_sock_family(AF_INET),
        m_sock_type(SOCK_STREAM),
        m_sock_protocol(IPPROTO_TCP),
        m_expires_s(0)
{
        ((struct sockaddr_in *)(&m_sa))->sin_family = AF_INET;
};

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void host_info::show(void)
{
        char l_hoststr[NI_MAXHOST] = "";
        char l_portstr[NI_MAXSERV] = "";
        int32_t l_s;
        l_s = getnameinfo((struct sockaddr *)&m_sa,
                          sizeof(struct sockaddr_storage),
                          l_hoststr,
                          sizeof(l_hoststr),
                          l_portstr,
                          sizeof(l_portstr),
                          NI_NUMERICHOST | NI_NUMERICSERV);
        if (l_s != 0)
        {
                // TODO???
        }
        printf("+-----------+\n");
        printf("| Host Info |\n");
        printf("+-----------+-------------------------\n");
        printf(": address:         %s:%s\n", l_hoststr, l_portstr);
        printf(": m_sock_family:   %d\n", m_sock_family);
        printf(": m_sock_type:     %d\n", m_sock_type);
        printf(": m_sock_protocol: %d\n", m_sock_protocol);
        printf(": m_sa_len:        %d\n", m_sa_len);
        printf(": m_expires:       %u\n", m_expires_s);
        printf("+-------------------------------------\n");
}

} //namespace ns_hurl {
