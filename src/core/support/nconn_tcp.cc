//: ----------------------------------------------------------------------------
//: Copyright (C) 2014 Verizon.  All Rights Reserved.
//: All Rights Reserved
//:
//: \file:    nconn_tcp.cc
//: \details: TODO
//: \author:  Reed P. Morrison
//: \date:    02/07/2014
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
#include "nconn_tcp.h"
#include "util.h"
#include "evr.h"

// Fcntl and friends
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <netinet/tcp.h>

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
                        NDBG_PRINT("STATUS_ERROR: Failed to set %s count.  Reason: %s.\n", #_sock_opt_name, strerror(errno)); \
                        return STATUS_ERROR;\
                } \
        } while(0)

//: ----------------------------------------------------------------------------
//: Fwd Decl's
//: ----------------------------------------------------------------------------


//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t nconn_tcp::setup_socket(const host_info_t &a_host_info)
{
        // Make a socket.
        m_fd = socket(a_host_info.m_sock_family,
                      a_host_info.m_sock_type,
                      a_host_info.m_sock_protocol);

        //NDBG_OUTPUT("%sSOCKET %s[%3d]: \n", ANSI_COLOR_BG_BLUE, ANSI_COLOR_OFF, m_fd);

        if (m_fd < 0)
        {
                NCONN_ERROR("HOST[%s]: Error creating socket. Reason: %s\n", m_host.c_str(), strerror(errno));
                return STATUS_ERROR;
        }

        // -------------------------------------------
        // Socket options
        // -------------------------------------------
        // TODO --set to REUSE????
        SET_SOCK_OPT(m_fd, SOL_SOCKET, SO_REUSEADDR, 1);

        if(m_sock_opt_send_buf_size)
        {
                SET_SOCK_OPT(m_fd, SOL_SOCKET, SO_SNDBUF, m_sock_opt_send_buf_size);
        }

        if(m_sock_opt_recv_buf_size)
        {
                SET_SOCK_OPT(m_fd, SOL_SOCKET, SO_RCVBUF, m_sock_opt_recv_buf_size);
        }


        if(m_sock_opt_no_delay)
        {
                SET_SOCK_OPT(m_fd, SOL_TCP, TCP_NODELAY, 1);
        }

        // -------------------------------------------
        // Can set with set_sock_opt???
        // -------------------------------------------
        // Set the file descriptor to no-delay mode.
        const int flags = fcntl(m_fd, F_GETFL, 0);
        if (flags == -1)
        {
                NCONN_ERROR("HOST[%s]: Error getting flags for fd. Reason: %s\n", m_host.c_str(), strerror(errno));
                return STATUS_ERROR;
        }

        if (fcntl(m_fd, F_SETFL, flags | O_NONBLOCK) < 0)
        {
                NCONN_ERROR("HOST[%s]: Error setting fd to non-block mode. Reason: %s\n", m_host.c_str(), strerror(errno));
                return STATUS_ERROR;
        }

        // -------------------------------------------
        // Initalize the http response parser
        // -------------------------------------------
        m_http_parser.data = this;
        http_parser_init(&m_http_parser, HTTP_RESPONSE);

        return STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t nconn_tcp::send_request(bool is_reuse)
{
        m_tcp_state = TCP_STATE_CONNECTED;

        //NDBG_OUTPUT("%s: REQUEST-->\n%s%.*s%s\n", m_host.c_str(), ANSI_COLOR_BG_MAGENTA, (int)m_req_buf_len, m_req_buf, ANSI_COLOR_OFF);

        // Bump number requested
        ++m_num_reqs;

        // Save last connect time for reuse
        if(is_reuse)
        {
                if(m_collect_stats_flag)
                {
                        m_stat.m_tt_connect_us = m_last_connect_time_us;
                }
        }

        // Reset to support reusing connection
        m_read_buf_idx = 0;

        if(m_verbose)
        {
                if(m_color)
                        NDBG_OUTPUT("%s: REQUEST-->\n%s%.*s%s\n", m_host.c_str(), ANSI_COLOR_FG_MAGENTA, (int)m_req_buf_len, m_req_buf, ANSI_COLOR_OFF);
                else
                        NDBG_OUTPUT("%s: REQUEST-->\n%.*s\n", m_host.c_str(), (int)m_req_buf_len, m_req_buf);

        }

        // TODO Wrap with while and check for errors
        uint32_t l_bytes_written = 0;
        int l_status;
        while(l_bytes_written < m_req_buf_len)
        {
                l_status = write(m_fd, m_req_buf + l_bytes_written, m_req_buf_len - l_bytes_written);
                if(l_status < 0)
                {
                        NCONN_ERROR("HOST[%s]: Error: performing write.  Reason: %s.\n", m_host.c_str(), strerror(errno));
                        return STATUS_ERROR;
                }
                l_bytes_written += l_status;
        }

        // TODO REMOVE
        //NDBG_OUTPUT("%sWRITE  %s[%3d]: Status %3d. Reason: %s\n",
        //                ANSI_COLOR_BG_CYAN, ANSI_COLOR_OFF, m_fd, l_status, strerror(errno));

        // Get request time
        if(m_collect_stats_flag)
        {
                m_request_start_time_us = get_time_us();
        }

        m_tcp_state = TCP_STATE_READING;
        //header_state = HDST_LINE1_PROTOCOL;

        return STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t nconn_tcp::receive_response(void)
{

        uint32_t l_total_bytes_read = 0;
        int32_t l_bytes_read = 0;
        int32_t l_max_read = m_max_read_buf - m_read_buf_idx;
        bool l_should_give_up = false;

        do {

                do {
                        l_bytes_read = read(m_fd, m_read_buf + m_read_buf_idx, l_max_read);
                        //NDBG_PRINT("%sHOST%s: %s fd[%3d] READ: %d bytes. Reason: %s\n",
                        //                ANSI_COLOR_FG_RED, ANSI_COLOR_OFF, m_host.c_str(), m_fd,
                        //                l_bytes_read,
                        //                strerror(errno));

                } while((l_bytes_read < 0) && (errno == EAGAIN));

                //NDBG_PRINT("%sHOST%s: %s fd[%3d]\n", ANSI_COLOR_FG_RED, ANSI_COLOR_OFF, m_host.c_str(), m_fd);
                //ns_hlo::mem_display((uint8_t *)(m_read_buf + m_read_buf_idx), l_bytes_read);

                // TODO Handle EOF -close connection...
                if ((l_bytes_read <= 0) && (errno != EAGAIN))
                {
                        if(m_verbose)
                        {
                                NDBG_PRINT("%s: %sl_bytes_read%s[%d] <= 0 total = %u--error: %s\n",
                                                m_host.c_str(),
                                                ANSI_COLOR_FG_RED, ANSI_COLOR_OFF, l_bytes_read, l_total_bytes_read, strerror(errno));
                        }
                        //close_connection(nowP);
                        return STATUS_ERROR;
                }

                if((errno == EAGAIN) || (errno == EWOULDBLOCK))
                {
                        l_should_give_up = true;
                }

                l_total_bytes_read += l_bytes_read;

                // Stats
                m_stat.m_total_bytes += l_bytes_read;
                if(m_collect_stats_flag)
                {
                        if((m_read_buf_idx == 0) && (m_stat.m_tt_first_read_us == 0))
                        {
                                m_stat.m_tt_first_read_us = get_delta_time_us(m_request_start_time_us);
                        }
                }

                // -----------------------------------------
                // Parse result
                // -----------------------------------------
                size_t l_parse_status = 0;
                //NDBG_PRINT("%sHTTP_PARSER%s: m_read_buf: %p, m_read_buf_idx: %d, l_bytes_read: %d\n",
                //                ANSI_COLOR_BG_YELLOW, ANSI_COLOR_OFF,
                //                m_read_buf, (int)m_read_buf_idx, (int)l_bytes_read);
                l_parse_status = http_parser_execute(&m_http_parser, &m_http_parser_settings, m_read_buf + m_read_buf_idx, l_bytes_read);
                if(l_parse_status < (size_t)l_bytes_read)
                {
                        if(m_verbose)
                        {
                                NCONN_ERROR("HOST[%s]: Error: parse error.  Reason: %s: %s\n",
                                                m_host.c_str(),
                                                //"","");
                                                http_errno_name((enum http_errno)m_http_parser.http_errno),
                                                http_errno_description((enum http_errno)m_http_parser.http_errno));
                                //NDBG_PRINT("%s: %sl_bytes_read%s[%d] <= 0 total = %u idx = %u\n",
                                //		m_host.c_str(),
                                //		ANSI_COLOR_FG_RED, ANSI_COLOR_OFF, l_bytes_read, l_total_bytes_read, m_read_buf_idx);
                                //ns_hlo::mem_display((const uint8_t *)m_read_buf + m_read_buf_idx, l_bytes_read);

                        }

                        m_read_buf_idx += l_bytes_read;
                        return STATUS_ERROR;

                }

                m_read_buf_idx += l_bytes_read;
                if(l_bytes_read < l_max_read)
                {
                        break;
                }
                // Wrap the index and read again if was too large...
                else
                {
                        m_read_buf_idx = 0;
                }

                if(l_should_give_up)
                        break;

        } while(l_bytes_read > 0);

        return l_total_bytes_read;

}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t nconn_tcp::cleanup(void)
{
        // Shut down connection

        //NDBG_PRINT("CLOSE[%lu--%d] %s--CONN--%s\n", m_id, m_fd, ANSI_COLOR_BG_RED, ANSI_COLOR_OFF);
        close(m_fd);
        m_fd = -1;

        // Reset all the values
        // TODO Make init function...
        // Set num use back to zero -we need reset method here?
        m_tcp_state = TCP_STATE_FREE;
        m_read_buf_idx = 0;
        m_num_reqs = 0;

        return STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t nconn_tcp::run_state_machine(evr_loop *a_evr_loop, const host_info_t &a_host_info)
{

        uint32_t l_retry_connect_count = 0;

        // Cancel last timer if was not in free state
        if(m_tcp_state != TCP_STATE_FREE)
        {
                a_evr_loop->cancel_timer(&(m_timer_obj));
        }

        //NDBG_PRINT("%sRUN_STATE_MACHINE%s: STATE[%d] --START\n", ANSI_COLOR_BG_YELLOW, ANSI_COLOR_OFF, m_tcp_state);
state_top:
        //NDBG_PRINT("%sRUN_STATE_MACHINE%s: STATE[%d]\n", ANSI_COLOR_BG_YELLOW, ANSI_COLOR_OFF, m_tcp_state);
        switch (m_tcp_state)
        {

        // -------------------------------------------------
        // STATE: FREE
        // -------------------------------------------------
        case TCP_STATE_FREE:
        {
                int32_t l_status;
                l_status = setup_socket(a_host_info);
                if(l_status != STATUS_OK)
                {
                        return STATUS_ERROR;
                }

                // Get start time
                // Stats
                if(m_collect_stats_flag)
                {
                        m_connect_start_time_us = get_time_us();
                }

                if (0 != a_evr_loop->add_fd(m_fd,
                                            EVR_FILE_ATTR_MASK_STATUS_ERROR,
                                            this))
                {
                        NCONN_ERROR("HOST[%s]: Error: Couldn't add socket file descriptor\n", m_host.c_str());
                        return STATUS_ERROR;
                }

                m_tcp_state = TCP_STATE_CONNECTING;
                goto state_top;

        }

        // -------------------------------------------------
        // STATE: CONNECTING
        // -------------------------------------------------
        case TCP_STATE_CONNECTING:
        {
                //int l_status;
                //NDBG_PRINT("%sCNST_CONNECTING%s\n", ANSI_COLOR_BG_RED, ANSI_COLOR_OFF);
                int l_connect_status = 0;
                l_connect_status = connect(m_fd,
                                           (struct sockaddr*) &(a_host_info.m_sa),
                                           (a_host_info.m_sa_len));

                // TODO REMOVE
                //++l_retry;
                //NDBG_PRINT_BT();
                //NDBG_PRINT("%sCONNECT%s[%3d]: Retry: %d Status %3d. Reason[%d]: %s\n",
                //           ANSI_COLOR_FG_CYAN, ANSI_COLOR_OFF,
                //           m_fd, l_retry_connect_count, l_connect_status,
                //           errno,
                //           strerror(errno));

                if (l_connect_status < 0)
                {
                        switch (errno)
                        {
                        case EISCONN:
                        {
                                //NDBG_PRINT("%sCONNECT%s[%3d]: SET CONNECTED\n",
                                //                ANSI_COLOR_BG_CYAN, ANSI_COLOR_OFF,
                                //                m_fd);
                                // Ok!
                                m_tcp_state = TCP_STATE_CONNECTED;
                                break;
                        }
                        case EINVAL:
                        {
                                int l_err;
                                socklen_t l_errlen;
                                l_errlen = sizeof(l_err);
                                if (getsockopt(m_fd, SOL_SOCKET, SO_ERROR, (void*) &l_err, &l_errlen) < 0)
                                {
                                        NCONN_ERROR("HOST[%s]: Error performing getsockopt. Unknown connect error\n",
                                                        m_host.c_str());
                                }
                                else
                                {
                                        NCONN_ERROR("HOST[%s]: Error performing getsockopt. %s\n", m_host.c_str(),
                                                        strerror(l_err));
                                }
                                return STATUS_ERROR;
                        }
                        case ECONNREFUSED:
                        {
                                NCONN_ERROR("HOST[%s]: Error Connection refused. Reason: %s\n", m_host.c_str(), strerror(errno));
                                return STATUS_ERROR;
                        }
                        case EAGAIN:
                        case EINPROGRESS:
                        {
                                //NDBG_PRINT("Error Connection in progress. Reason: %s\n", strerror(errno));
                                m_tcp_state = TCP_STATE_CONNECTING;
                                //l_in_progress = true;

                                // Set to writeable and try again
                                if (0 != a_evr_loop->mod_fd(m_fd,
                                                            EVR_FILE_ATTR_MASK_WRITE|EVR_FILE_ATTR_MASK_STATUS_ERROR,
                                                            this))
                                {
                                        NCONN_ERROR("HOST[%s]: Error: Couldn't add socket file descriptor\n", m_host.c_str());
                                        return STATUS_ERROR;
                                }
                                return STATUS_OK;
                        }
                        case EADDRNOTAVAIL:
                        {
                                // TODO -bad to spin like this???
                                // Retry connect
                                //NDBG_PRINT("%sRETRY CONNECT%s\n", ANSI_COLOR_BG_YELLOW, ANSI_COLOR_OFF);
                                if(++l_retry_connect_count < 1024)
                                {
                                        usleep(1000);
                                        goto state_top;
                                }
                                else
                                {
                                        NCONN_ERROR("HOST[%s]: Error connect().  Reason: %s\n", m_host.c_str(), strerror(errno));
                                        return STATUS_ERROR;
                                }
                                break;
                        }
                        default:
                        {
                                NCONN_ERROR("HOST[%s]: Error Unkown. Reason: %s\n", m_host.c_str(), strerror(errno));
                                return STATUS_ERROR;
                        }
                        }
                }

                m_tcp_state = TCP_STATE_CONNECTED;

                // Stats
                if(m_collect_stats_flag)
                {
                        m_stat.m_tt_connect_us = get_delta_time_us(m_connect_start_time_us);

                        // Save last connect time for reuse
                        m_last_connect_time_us = m_stat.m_tt_connect_us;
                }

                //NDBG_PRINT("%s connect: %" PRIu64 " -- start: %" PRIu64 " %s.\n",
                //              ANSI_COLOR_BG_RED,
                //              m_stat.m_tt_connect_us,
                //              m_connect_start_time_us,
                //              ANSI_COLOR_OFF);

                // -------------------------------------------
                // Add to event handler
                // -------------------------------------------
                if (0 != a_evr_loop->mod_fd(m_fd,
                                            EVR_FILE_ATTR_MASK_READ|EVR_FILE_ATTR_MASK_STATUS_ERROR,
                                            this))
                {
                        NCONN_ERROR("HOST[%s]: Error: Couldn't add socket file descriptor\n", m_host.c_str());
                        return STATUS_ERROR;
                }

                goto state_top;
        }

        // -------------------------------------------------
        // STATE: CONNECTED
        // -------------------------------------------------
        case TCP_STATE_CONNECTED:
        {
                // -------------------------------------------
                // Send request
                // -------------------------------------------
                int32_t l_request_status = STATUS_OK;
                //NDBG_PRINT("%sSEND_REQUEST%s\n", ANSI_COLOR_BG_CYAN, ANSI_COLOR_OFF);
        	if(!m_connect_only)
        	{
			l_request_status = send_request(false);
			if(l_request_status != STATUS_OK)
			{
			        NCONN_ERROR("HOST[%s]: Error: performing send_request\n", m_host.c_str());
				return STATUS_ERROR;
			}
        	}
        	// connect only -we outtie!
        	else
        	{
        		m_tcp_state = TCP_STATE_DONE;
        	}
                break;
        }

        // -------------------------------------------------
        // STATE: READING
        // -------------------------------------------------
        case TCP_STATE_READING:
        {
                int l_read_status = 0;
                //NDBG_PRINT("%sCNST_READING%s\n", ANSI_COLOR_BG_CYAN, ANSI_COLOR_OFF);
                l_read_status = receive_response();
                //NDBG_PRINT("%sCNST_READING%s: receive_response(): total: %d\n", ANSI_COLOR_BG_CYAN, ANSI_COLOR_OFF, (int)m_stat.m_total_bytes);
                if(l_read_status < 0)
                {
                        NCONN_ERROR("HOST[%s]: Error: performing receive_response\n", m_host.c_str());
                        return STATUS_ERROR;
                }
                return l_read_status;
        }
        default:
                break;
        }

        return STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t nconn_tcp::set_opt(uint32_t a_opt, const void *a_buf, uint32_t a_len)
{
        switch(a_opt)
        {
        case OPT_TCP_REQ_BUF_LEN:
        {
                m_req_buf_len = a_len;
                break;
        }
        case OPT_TCP_RECV_BUF_SIZE:
        {
                m_sock_opt_recv_buf_size = a_len;
                break;
        }
        case OPT_TCP_SEND_BUF_SIZE:
        {
                m_sock_opt_send_buf_size = a_len;
                break;
        }
        case OPT_TCP_NO_DELAY:
        {
                m_sock_opt_no_delay = (bool)a_len;
                break;
        }
        default:
        {
                //NDBG_PRINT("Error unsupported option: %d\n", a_opt);
                return m_opt_unhandled;
        }
        }

        return STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t nconn_tcp::get_opt(uint32_t a_opt, void **a_buf, uint32_t *a_len)
{

        switch(a_opt)
        {
        case OPT_TCP_REQ_BUF:
        {
                *a_buf = m_req_buf;
                *a_len = 0;
                break;
        }
        case OPT_TCP_REQ_BUF_LEN:
        {
                *a_len = m_req_buf_len;
                break;
        }
        default:
        {
                //NDBG_PRINT("Error unsupported option: %d\n", a_opt);
                return m_opt_unhandled;
        }
        }

        return STATUS_OK;
}


