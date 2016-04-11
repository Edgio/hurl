//: ----------------------------------------------------------------------------
//: Copyright (C) 2014 Verizon.  All Rights Reserved.
//: All Rights Reserved
//:
//: \file:    lsnr.cc
//: \details: TODO
//: \author:  Reed P. Morrison
//: \date:    05/28/2015
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
#include "hlx/lsnr.h"
#include "hlx/url_router.h"
#include "hlx/rqst_h.h"
#include "ndebug.h"

#include <string.h>
#include <errno.h>

#include <arpa/inet.h>
#include <netinet/in.h>

//: ----------------------------------------------------------------------------
//: Constants
//: ----------------------------------------------------------------------------
#define MAX_PENDING_CONNECT_REQUESTS 16384
#define HLX_STATUS_OK 0
#define HLX_STATUS_ERROR -1

//: ----------------------------------------------------------------------------
//: Macros
//: ----------------------------------------------------------------------------
// Set socket option macro...
#define SET_SOCK_OPT(_sock_fd, _sock_opt_level, _sock_opt_name, _sock_opt_val) \
        do { \
                int _l__sock_opt_val = _sock_opt_val; \
                int _l_status = 0; \
                _l_status = ::setsockopt(_sock_fd, \
                                _sock_opt_level, \
                                _sock_opt_name, \
                                &_l__sock_opt_val, \
                                sizeof(_l__sock_opt_val)); \
                                if (_l_status == -1) { \
                                        NDBG_PRINT("STATUS_ERROR: Failed to set %s.  Reason: %s.\n", \
                                                   #_sock_opt_name, strerror(errno)); \
                                        return STATUS_ERROR;\
                                } \
        } while(0)

namespace ns_hlx {

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t lsnr::set_default_route(rqst_h *a_handler)
{
        m_default_handler = a_handler;
        return HLX_STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t lsnr::add_route(const std::string &a_endpoint, const rqst_h *a_rqst_h)
{
        if(!m_url_router)
        {
                return HLX_STATUS_ERROR;
        }
        return m_url_router->add_route(a_endpoint, a_rqst_h);
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
static int32_t create_tcp_server_socket(uint16_t a_port,
                                        uint32_t a_ipv4_addr,
                                        sockaddr_in &ao_sa)
{
        int l_sock_fd;
        int32_t l_status;

        // -------------------------------------------
        // Create socket for incoming connections
        // -------------------------------------------
        l_sock_fd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (l_sock_fd < 0)
        {
                NDBG_PRINT("Error socket() failed. Reason[%d]: %s\n", errno, strerror(errno));
                return STATUS_ERROR;
        }

        // -------------------------------------------
        // Set socket options
        // -Enable Socket reuse.
        // -------------------------------------------
        SET_SOCK_OPT(l_sock_fd, SOL_SOCKET, SO_REUSEADDR, 1);

        // -------------------------------------------
        // Construct local address structure
        // -------------------------------------------
        memset(&ao_sa, 0, sizeof(ao_sa)); // Zero out structure
        // IPv4 for now
        ao_sa.sin_family      = AF_INET;             // Internet address family
        ao_sa.sin_addr.s_addr = htonl(a_ipv4_addr);  // Any incoming interface
        ao_sa.sin_port        = htons(a_port);       // Local port

        // -------------------------------------------
        // Bind to the local address
        // -------------------------------------------
        l_status = bind(l_sock_fd, (struct sockaddr *) &ao_sa, sizeof(ao_sa));
        if (l_status < 0)
        {
                NDBG_PRINT("Error bind() failed (port: %d). Reason[%d]: %s\n", a_port, errno, strerror(errno));
                return STATUS_ERROR;
        }

        // -------------------------------------------
        // Mark the socket so it will listen for
        // incoming connections
        // -------------------------------------------
        l_status = listen(l_sock_fd, MAX_PENDING_CONNECT_REQUESTS);
        if (l_status < 0)
        {
                NDBG_PRINT("Error listen() failed. Reason[%d]: %s\n", errno, strerror(errno));
                return STATUS_ERROR;
        }

        return l_sock_fd;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t lsnr::init(void)
{
        if(m_is_initd)
        {
                return STATUS_OK;
        }

        // Create listen socket
        sockaddr_in *l_sa = (struct sockaddr_in *)(&m_sa);
        m_sa_len = sizeof(struct sockaddr_in);
        m_fd = create_tcp_server_socket(m_port, m_local_addr_v4, *l_sa);
        if(m_fd == STATUS_ERROR)
        {
            NDBG_PRINT("Error performing create_tcp_server_socket with port number = %d\n", m_port);
            return HLX_STATUS_ERROR;
        }
        m_is_initd = true;
        return STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t lsnr::set_local_addr_v4(const char *a_addr_str)
{
        int32_t l_s;
        struct sockaddr_in l_c_addr;
        bzero((char *) &l_c_addr, sizeof(l_c_addr));
        l_s = inet_pton(AF_INET, a_addr_str, &(l_c_addr.sin_addr));
        if(l_s != 1)
        {
                return STATUS_ERROR;
        }
        m_local_addr_v4 = ntohl(l_c_addr.sin_addr.s_addr);
        return STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
lsnr::lsnr(uint16_t a_port, scheme_t a_scheme):
        m_scheme(a_scheme),
        m_local_addr_v4(INADDR_ANY),
        m_port(a_port),
        m_sa(),
        m_sa_len(0),
        m_default_handler(NULL),
        m_url_router(NULL),
        m_fd(-1),
        m_is_initd(false)
{
        m_url_router = new url_router();
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
lsnr::~lsnr()
{
        if(m_fd > 0)
        {
                // shutdown
                shutdown(m_fd, SHUT_RD);
        }
        if(m_url_router)
        {
                delete m_url_router;
                m_url_router = NULL;
        }
}

}
