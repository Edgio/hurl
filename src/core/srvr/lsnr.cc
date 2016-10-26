//: ----------------------------------------------------------------------------
//: Copyright (C) 2016 Verizon.  All Rights Reserved.
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
#include "ndebug.h"
#include "nconn.h"
#include "t_srvr.h"
#include "hlx/time_util.h"
#include "hlx/lsnr.h"
#include "hlx/url_router.h"
#include "hlx/rqst_h.h"
#include "hlx/status.h"
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
                                        NDBG_PRINT("HLX_STATUS_ERROR: Failed to set %s.  Reason: %s.\n", \
                                                   #_sock_opt_name, strerror(errno)); \
                                        return HLX_STATUS_ERROR;\
                                } \
        } while(0)

namespace ns_hlx {

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
                return HLX_STATUS_ERROR;
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
                return HLX_STATUS_ERROR;
        }

        // -------------------------------------------
        // Mark the socket so it will listen for
        // incoming connections
        // -------------------------------------------
        l_status = listen(l_sock_fd, MAX_PENDING_CONNECT_REQUESTS);
        if (l_status < 0)
        {
                NDBG_PRINT("Error listen() failed. Reason[%d]: %s\n", errno, strerror(errno));
                return HLX_STATUS_ERROR;
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
                return HLX_STATUS_OK;
        }

        // Create listen socket
        sockaddr_in *l_sa = (struct sockaddr_in *)(&m_sa);
        m_sa_len = sizeof(struct sockaddr_in);
        m_fd = create_tcp_server_socket(m_port, m_local_addr_v4, *l_sa);
        if(m_fd == HLX_STATUS_ERROR)
        {
            NDBG_PRINT("Error performing create_tcp_server_socket with port number = %d\n", m_port);
            return HLX_STATUS_ERROR;
        }
        m_is_initd = true;
        return HLX_STATUS_OK;
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
                return HLX_STATUS_ERROR;
        }
        m_local_addr_v4 = ntohl(l_c_addr.sin_addr.s_addr);
        return HLX_STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t lsnr::evr_fd_readable_cb(void *a_data)
{
        //NDBG_PRINT("%sREADABLE%s %p\n", ANSI_COLOR_BG_GREEN, ANSI_COLOR_OFF, a_data);
        if(!a_data)
        {
                return HLX_STATUS_OK;
        }
        nconn* l_nconn = static_cast<nconn*>(a_data);
        if(!l_nconn->get_ctx())
        {
                // TODO log
                return HLX_STATUS_ERROR;
        }
        t_srvr *l_t_srvr = static_cast<t_srvr *>(l_nconn->get_ctx());
        lsnr *l_lsnr = static_cast<lsnr *>(l_nconn->get_data());
        //NDBG_PRINT("%sREADABLE%s LABEL: %s LSNR: %p\n", ANSI_COLOR_FG_GREEN, ANSI_COLOR_OFF,
        //                l_nconn->get_label().c_str(), l_lsnr);
        // Server -incoming client connections
        if(!l_nconn->is_listening())
        {
                return HLX_STATUS_ERROR;
        }

        if(!l_lsnr || !l_nconn)
        {
                return HLX_STATUS_ERROR;
        }

        // Returns new client fd on success
        uint32_t l_read = 0;
        uint32_t l_written = 0;
        int32_t l_s;
        l_s = l_nconn->nc_run_state_machine(EVR_MODE_NONE, NULL, l_read, NULL, l_written);
        if(l_s == nconn::NC_STATUS_ERROR)
        {
                return HLX_STATUS_ERROR;
        }

        // Get new connected client conn
        nconn *l_new_nconn = NULL;
        l_new_nconn = l_t_srvr->get_new_client_conn(l_nconn->get_scheme(), l_lsnr);
        if(!l_new_nconn)
        {
                //NDBG_PRINT("Error performing get_new_client_conn");
                return HLX_STATUS_ERROR;
        }
        clnt_session *l_cs = static_cast<clnt_session *>(l_new_nconn->get_data());

        // ---------------------------------------
        // Set access info
        // TODO move to clnt_session???
        // ---------------------------------------
        l_nconn->get_remote_sa(l_cs->m_access_info.m_conn_clnt_sa,
                               l_cs->m_access_info.m_conn_clnt_sa_len);
        l_lsnr->get_sa(l_cs->m_access_info.m_conn_upsv_sa,
                       l_cs->m_access_info.m_conn_upsv_sa_len);
        l_cs->m_access_info.m_start_time_ms = get_time_ms();
        l_cs->m_access_info.m_total_time_ms = 0;
        l_cs->m_access_info.m_bytes_in = 0;
        l_cs->m_access_info.m_bytes_out = 0;

        // Set connected
        int l_fd = l_s;
        l_s = l_new_nconn->nc_set_accepting(l_fd);
        if(l_s != HLX_STATUS_OK)
        {
                //NDBG_PRINT("Error: performing run_state_machine\n");
                l_t_srvr->cleanup_clnt_session(l_cs, l_new_nconn);
                // TODO Check return
                return HLX_STATUS_ERROR;
        }
        // Add idle timer
        l_cs->set_timeout_ms(l_t_srvr->get_timeout_ms());
        l_cs->set_last_active_ms(get_time_ms());
        l_t_srvr->add_timer(l_cs->get_timeout_ms(),
                            clnt_session::evr_fd_timeout_cb,
                            l_new_nconn,
                            (void **)(&(l_cs->m_timer_obj)));
        // TODO Check status
        return HLX_STATUS_OK;
}

}
