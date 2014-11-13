//: ----------------------------------------------------------------------------
//: Copyright (C) 2014 Verizon.  All Rights Reserved.
//: All Rights Reserved
//:
//: \file:    nconn.cc
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
#include "nconn.h"
#include "util.h"
#include "reqlet.h"
#include "ndebug.h"

#include <errno.h>
#include <string.h>
#include <string>

// Fcntl and friends
#include <unistd.h>
#include <fcntl.h>

#include <netinet/tcp.h>

#include <stdint.h>
#ifndef __STDC_FORMAT_MACROS
#define __STDC_FORMAT_MACROS
#endif
#include <inttypes.h>

#include <sys/stat.h>
#include <pthread.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/bio.h>
#include <openssl/rand.h>
#include <openssl/crypto.h>

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
                                        \
                } while(0)

//: ----------------------------------------------------------------------------
//: Fwd Decl's
//: ----------------------------------------------------------------------------


//: ----------------------------------------------------------------------------
//: http-parser callbacks
//: ----------------------------------------------------------------------------

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int nconn::hp_on_message_begin(http_parser* a_parser)
{
        nconn *l_conn = static_cast <nconn *>(a_parser->data);
        if(l_conn)
        {
                if(l_conn->m_verbose)
                {
                        NDBG_OUTPUT("%s: message begin\n", l_conn->m_host.c_str());
                }
        }
        return 0;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int nconn::hp_on_url(http_parser* a_parser, const char *a_at, size_t a_length)
{
        nconn *l_conn = static_cast <nconn *>(a_parser->data);
        if(l_conn)
        {
                if(l_conn->m_verbose)
                {
                        if(l_conn->m_color)
                                NDBG_OUTPUT("%s: url:   %s%.*s%s\n", l_conn->m_host.c_str(), ANSI_COLOR_FG_YELLOW, (int)a_length, a_at, ANSI_COLOR_OFF);
                        else
                                NDBG_OUTPUT("%s: url:   %.*s\n", l_conn->m_host.c_str(), (int)a_length, a_at);
                }
        }
        return 0;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int nconn::hp_on_status(http_parser* a_parser, const char *a_at, size_t a_length)
{
        nconn *l_conn = static_cast <nconn *>(a_parser->data);
        if(l_conn)
        {
                if(l_conn->m_verbose)
                {
                        if(l_conn->m_color)
                                NDBG_OUTPUT("%s: status: %s%.*s%s --STATUS = HTTP %d.%d METHOD[%d] %d--\n",
                                                l_conn->m_host.c_str(),
                                                ANSI_COLOR_FG_YELLOW, (int)a_length, a_at, ANSI_COLOR_OFF,
                                                a_parser->http_major,
                                                a_parser->http_minor,
                                                a_parser->method,
                                                a_parser->status_code);
                        else
                                NDBG_OUTPUT("%s: status: %.*s --STATUS = HTTP %d.%d METHOD[%d] %d--\n",
                                                l_conn->m_host.c_str(),
                                                (int)a_length, a_at,
                                                a_parser->http_major,
                                                a_parser->http_minor,
                                                a_parser->method,
                                                a_parser->status_code);
                }

                // Set status code
                l_conn->m_stat.m_status_code = a_parser->status_code;

                if(l_conn->m_save_response_in_reqlet)
                {
                        // Get reqlet
                        reqlet *l_reqlet = static_cast<reqlet *>(l_conn->m_data1);
                        if(l_reqlet)
                        {
                                std::string l_status;
                                l_status.append(a_at, a_length);
                                l_reqlet->m_response_headers["Status"] = l_status;
                                l_reqlet->m_response_status = a_parser->status_code;
                        }
                }

        }
        return 0;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int nconn::hp_on_header_field(http_parser* a_parser, const char *a_at, size_t a_length)
{
        nconn *l_conn = static_cast <nconn *>(a_parser->data);
        if(l_conn)
        {
                if(l_conn->m_verbose)
                {
                        if(l_conn->m_color)
                                NDBG_OUTPUT("%s: field:  %s%.*s%s\n", l_conn->m_host.c_str(), ANSI_COLOR_FG_BLUE, (int)a_length, a_at, ANSI_COLOR_OFF);
                        else
                                NDBG_OUTPUT("%s: field:  %.*s\n", l_conn->m_host.c_str(), (int)a_length, a_at);
                }

                if(l_conn->m_save_response_in_reqlet)
                {
                        // Get reqlet
                        reqlet *l_reqlet = static_cast<reqlet *>(l_conn->m_data1);
                        if(l_reqlet)
                        {
                                std::string l_header;
                                l_header.append(a_at, a_length);
                                l_reqlet->m_response_headers[l_header] = "";
                                l_reqlet->m_next_response_value = l_reqlet->m_response_headers.find(l_header);
                        }
                }

        }
        return 0;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int nconn::hp_on_header_value(http_parser* a_parser, const char *a_at, size_t a_length)
{
        nconn *l_conn = static_cast <nconn *>(a_parser->data);
        if(l_conn)
        {
                if(l_conn->m_verbose)
                {
                        if(l_conn->m_color)
                                NDBG_OUTPUT("%s: value:  %s%.*s%s\n", l_conn->m_host.c_str(), ANSI_COLOR_FG_GREEN, (int)a_length, a_at, ANSI_COLOR_OFF);
                        else
                                NDBG_OUTPUT("%s: value:  %.*s\n", l_conn->m_host.c_str(), (int)a_length, a_at);
                }

                if(l_conn->m_save_response_in_reqlet)
                {
                        // Get reqlet
                        reqlet *l_reqlet = static_cast<reqlet *>(l_conn->m_data1);
                        if(l_reqlet)
                        {
                                (l_reqlet->m_next_response_value)->second.append(a_at, a_length);
                        }
                }

        }
        return 0;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int nconn::hp_on_headers_complete(http_parser* a_parser)
{
        nconn *l_conn = static_cast <nconn *>(a_parser->data);
        if(l_conn)
        {
                if(l_conn->m_verbose)
                {
                        NDBG_OUTPUT("%s: headers_complete\n", l_conn->m_host.c_str());
                }

                // Stats
                l_conn->m_stat.m_tt_header_completion_us = get_delta_time_us(l_conn->m_request_start_time_us);

        }
        return 0;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int nconn::hp_on_body(http_parser* a_parser, const char *a_at, size_t a_length)
{
        nconn *l_conn = static_cast <nconn *>(a_parser->data);
        if(l_conn)
        {
                if(l_conn->m_verbose)
                {
                        if(l_conn->m_color)
                                NDBG_OUTPUT("%s: body:  %s%.*s%s\n", l_conn->m_host.c_str(), ANSI_COLOR_FG_YELLOW, (int)a_length, a_at, ANSI_COLOR_OFF);
                        else
                                NDBG_OUTPUT("%s: body:  %.*s\n", l_conn->m_host.c_str(), (int)a_length, a_at);
                }

                // Stats
                l_conn->m_stat.m_body_bytes += a_length;

                if(l_conn->m_save_response_in_reqlet)
                {
                        // Get reqlet
                        reqlet *l_reqlet = static_cast<reqlet *>(l_conn->m_data1);
                        if(l_reqlet)
                        {
                                l_reqlet->m_response_body.append(a_at, a_length);
                        }
                }

        }
        return 0;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int nconn::hp_on_message_complete(http_parser* a_parser)
{
        nconn *l_conn = static_cast <nconn *>(a_parser->data);
        if(l_conn)
        {
                if(l_conn->m_verbose)
                {
                        NDBG_OUTPUT("%s: message complete\n", l_conn->m_host.c_str());
                }

                // Stats
                l_conn->m_stat.m_tt_completion_us = get_delta_time_us(l_conn->m_request_start_time_us);

                //NDBG_PRINT("CONN[%u--%d] m_request_start_time_us: %" PRIu64 " m_tt_completion_us: %" PRIu64 "\n",
                //		l_conn->m_connection_id,
                //		l_conn->m_fd,
                //		l_conn->m_request_start_time_us,
                //		l_conn->m_stat.m_tt_completion_us);
                //if(!l_conn->m_stat.m_connect_start_time_us) abort();

                if(http_should_keep_alive(a_parser))
                {
                        l_conn->m_server_response_supports_keep_alives = true;
                }
                else
                {
                        l_conn->m_server_response_supports_keep_alives = false;
                }

                // we outtie
                l_conn->m_state = CONN_STATE_DONE;

        }


        return 0;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t nconn::do_connect(host_info_t &a_host_info, const std::string &a_host)
{

        // TODO only in verbose mode???
        m_host = a_host;

        // Make a socket.
        m_fd = socket(a_host_info.m_sock_family,
                        a_host_info.m_sock_type,
                        a_host_info.m_sock_protocol);

        //NDBG_OUTPUT("%sSOCKET %s[%3d]: \n", ANSI_COLOR_BG_BLUE, ANSI_COLOR_OFF, m_fd);

        if (m_fd < 0)
        {
                NDBG_PRINT("Error creating socket. Reason: %s\n", strerror(errno));
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
                NDBG_PRINT("Error getting flags for fd. Reason: %s\n", strerror(errno));
                return STATUS_ERROR;
        }

        if (fcntl(m_fd, F_SETFL, flags | O_NONBLOCK) < 0)
        {
                NDBG_PRINT("Error setting fd to non-block mode. Reason: %s\n", strerror(errno));
                return STATUS_ERROR;
        }

        // Get start time
        m_connect_start_time_us = get_time_us();
        //NDBG_PRINT("CONN[%u--%d] %s--SET_--%s m_connect_start_time_us: %" PRIu64 "\n", m_connection_id, m_fd, ANSI_COLOR_BG_BLUE, ANSI_COLOR_OFF, m_stat.m_connect_start_time_us);

        // -------------------------------------------
        // HTTPS
        // -------------------------------------------
        if (m_scheme == URL_SCHEME_HTTPS)
        {
                // Create SSL Context
                m_ssl = SSL_new(m_ssl_ctx);
                // TODO Check for NULL

                SSL_set_fd(m_ssl, m_fd);
                // TODO Check for Errors
        }

        // -------------------------------------------
        // Connect
        // -------------------------------------------
        int l_connect_status = 0;
        bool l_in_progress = false;
        uint32_t l_retry_connect_count = 0;

retry_connect:
        //NDBG_OUTPUT("%sCONNECT%s[%3d]\n", ANSI_COLOR_FG_YELLOW, ANSI_COLOR_OFF, m_fd);

        l_connect_status = connect(m_fd,
                        (struct sockaddr*) &(a_host_info.m_sa),
                        sizeof(a_host_info.m_sa));

        // TODO REMOVE
        //NDBG_OUTPUT("%sCONNECT%s[%3d]: %3d. Reason: %s\n", ANSI_COLOR_FG_YELLOW, ANSI_COLOR_OFF, m_fd, l_connect_status, strerror(errno));

        if (l_connect_status < 0)
        {
                if (errno == EINPROGRESS)
                {
                        //NDBG_PRINT("Error Connection in progress. Reason: %s\n", strerror(errno));
                        m_state = CONN_STATE_CONNECTING;
                        l_in_progress = true;
                }
                else if (errno == ECONNREFUSED)
                {
                        NDBG_PRINT("Error Connection refused for host: %s. Reason: %s\n", m_host.c_str(), strerror(errno));
                        return STATUS_ERROR;
                }
                else if(errno == EADDRNOTAVAIL)
                {
                        // TODO -bad to spin like this???
                        // Retry connect
                        //NDBG_PRINT("%sRETRY CONNECT%s\n", ANSI_COLOR_BG_YELLOW, ANSI_COLOR_OFF);
                        if(++l_retry_connect_count < 1024)
                        {
                                usleep(1000);
                                goto retry_connect;
                        }
                        return STATUS_ERROR;

                }
                else
                {
                        NDBG_PRINT("Error connect() for host: %s.  Reason: %s\n", m_host.c_str(), strerror(errno));
                        return STATUS_ERROR;
                }
        }
        else
        {
                m_state = CONN_STATE_CONNECTED;
        }

        // -------------------------------------------
        // Initalize the http response parser
        // -------------------------------------------
        m_http_parser.data = this;
        http_parser_init(&m_http_parser, HTTP_RESPONSE);

        // Get start time
        //m_stat.m_connect_start_time_us = get_time_ms();
        //NDBG_PRINT("CONN[%u--%d] %s--SET_--%s m_connect_start_time_us: %" PRIu64 "\n", m_connection_id, m_fd, ANSI_COLOR_BG_BLUE, ANSI_COLOR_OFF, m_stat.m_connect_start_time_us);

        // Stats --TODO Only add if success???
        //m_stat.m_tt_connect_ms = get_delta_time_ms(m_stat.m_connect_start_time_us);

        if(l_in_progress)
        {
                return STATUS_ERROR;
        }

        // -------------------------------------------
        // Try rest of connect
        // -------------------------------------------
        int32_t l_status;
        l_status = connect_cb(a_host_info);
        if(STATUS_OK != l_status)
        {
                NDBG_PRINT("Error performing make_request for host: %s.\n", m_host.c_str());
                return STATUS_ERROR;
        }

        return STATUS_OK;

}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t nconn::connect_cb(host_info_t &a_host_info)
{

        // -------------------------------------------
        // double check???
        // -------------------------------------------
        if(CONN_STATE_CONNECTING == m_state)
        {
                /* Check to make sure the non-blocking connect succeeded. */

                // TODO REMOVE
                //int l_retry = 0;

                while(CONN_STATE_CONNECTING == m_state)
                {
                        int l_connect_status = 0;
                        l_connect_status = connect(m_fd,
                                        (struct sockaddr*) &(a_host_info.m_sa),
                                        (a_host_info.m_sa_len));

                        // TODO REMOVE
                        //NDBG_OUTPUT("%sCONNECT%s[%3d]: Status %3d. Reason: %s\n",
                        //                ANSI_COLOR_BG_RED, ANSI_COLOR_OFF, m_fd, l_connect_status, strerror(errno));

                        if (l_connect_status < 0)
                        {
                                switch (errno)
                                {
                                case EISCONN:
                                        // Ok!
                                        m_state = CONN_STATE_CONNECTED;
                                        break;
                                case EINVAL:
                                {
                                        int l_err;
                                        socklen_t l_errlen;
                                        l_errlen = sizeof(l_err);
                                        if (getsockopt(m_fd, SOL_SOCKET, SO_ERROR, (void*) &l_err, &l_errlen) < 0)
                                        {
                                                NDBG_PRINT("%s: unknown connect error\n",
                                                                m_host.c_str());
                                        }
                                        else
                                        {
                                                NDBG_PRINT("%s: %s\n", m_host.c_str(),
                                                                strerror(l_err));
                                        }
                                        return STATUS_ERROR;
                                        break;
                                }
                                case ECONNREFUSED:
                                        return STATUS_ERROR;
                                        break;
                                case EINPROGRESS:
                                        break;
                                default:
                                        return STATUS_ERROR;
                                        break;
                                }
                        }
                        else
                        {
                                m_state = CONN_STATE_CONNECTED;
                        }
                }
        }
        // Stats
        m_stat.m_tt_connect_us = get_delta_time_us(m_connect_start_time_us);

        // Save last connect time for reuse
        m_last_connect_time_us = m_stat.m_tt_connect_us;

        //NDBG_PRINT("%s connect: %" PRIu64 " -- start: %" PRIu64 " %s.\n",
        //		ANSI_COLOR_BG_RED,
        //		m_stat.m_tt_connect_us,
        //		m_connect_start_time_us,
        //		ANSI_COLOR_OFF);

        // -------------------------------------------
        // HTTPS
        // -------------------------------------------
        if ((m_scheme == URL_SCHEME_HTTPS) && (CONN_STATE_SSL_CONNECTED != m_state))
        {

                int l_status;
                m_state = CONN_STATE_CONNECTING;
                l_status = SSL_connect(m_ssl);
                // TODO REMOVE
                //NDBG_OUTPUT("%sSSL_CON%s[%3d]: Status %3d. Reason: %s\n",
                //                ANSI_COLOR_BG_GREEN, ANSI_COLOR_OFF, m_fd, l_status, strerror(errno));
                if (l_status <= 0)
                {
                        //fprintf(stderr, "%s: SSL connection failed - %d\n", "hlo", l_status);
                        //NDBG_PRINT("Showing error.\n");

                        int l_ssl_error = SSL_get_error(m_ssl, l_status);
                        switch(l_ssl_error) {
                        case SSL_ERROR_SSL:
                        {
                                NDBG_PRINT("SSL_ERROR_SSL %lu: %s\n", ERR_get_error(), ERR_error_string(ERR_get_error(), NULL));
                                break;
                        }
                        case SSL_ERROR_WANT_READ:
                        case SSL_ERROR_WANT_WRITE:
                        {
                                // Recoverable error try again
                                return EAGAIN;
                        }

                        case SSL_ERROR_WANT_X509_LOOKUP:
                        {
                                NDBG_PRINT("SSL_ERROR_WANT_X509_LOSTATUS_OKUP\n");
                                break;
                        }

                        // look at error stack/return value/errno
                        case SSL_ERROR_SYSCALL:
                        {
                                NDBG_PRINT("SSL_ERROR_SYSCALL %lu: %s\n", ERR_get_error(), ERR_error_string(ERR_get_error(), NULL));
                                if(l_status == 0) {
                                        NDBG_PRINT("An EOF was observed that violates the protocol\n");
                                } else if(l_status == -1) {
                                        NDBG_PRINT("%s\n", strerror(errno));
                                }
                                break;
                        }
                        case SSL_ERROR_ZERO_RETURN:
                        {
                                NDBG_PRINT("SSL_ERROR_ZERO_RETURN\n");
                                break;
                        }
                        case SSL_ERROR_WANT_CONNECT:
                        {
                                NDBG_PRINT("SSL_ERROR_WANT_CONNECT\n");
                                break;
                        }
                        case SSL_ERROR_WANT_ACCEPT:
                        {
                                NDBG_PRINT("SSL_ERROR_WANT_ACCEPT\n");
                                break;
                        }
                        }


                        ERR_print_errors_fp(stderr);
                        return STATUS_ERROR;
                }
                else if(1 == l_status)
                {
                        m_state = CONN_STATE_CONNECTED;
                }

                // Stats
                m_stat.m_tt_ssl_connect_us = get_delta_time_us(m_connect_start_time_us);

#if 0
                static bool s_first = true;
                if (s_first)
                {
                        s_first = false;
                        const char* cipher_name = SSL_get_cipher_name(m_ssl);
                        const char* cipher_version = SSL_get_cipher_version(m_ssl);
                        if(m_verbose)
                        {
                                NDBG_PRINT("got ssl m_cipher %s %s\n", cipher_name, cipher_version);
                        }
                }
#endif
        }

        //did_connect = 1;

        // -------------------------------------------
        // Send request
        // -------------------------------------------
        int32_t l_request_status = STATUS_OK;
        l_request_status = send_request(false);
        if(l_request_status != STATUS_OK)
        {
                return STATUS_ERROR;
        }

        return STATUS_OK;

}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t nconn::send_request(bool is_reuse)
{
        // Bump number requested
        ++m_num_reqs;

        // Save last connect time for reuse
        if(is_reuse)
        {
                m_stat.m_tt_connect_us = m_last_connect_time_us;
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
                if (m_scheme == URL_SCHEME_HTTPS)
                {
                        l_status = SSL_write(m_ssl, m_req_buf + l_bytes_written, m_req_buf_len - l_bytes_written);
                        if(l_status < 0)
                        {
                                NDBG_PRINT("Error: performing SSL_write.\n");
                                return STATUS_ERROR;
                        }
                        l_bytes_written += l_status;
                }
                else
                {
                        l_status = write(m_fd, m_req_buf + l_bytes_written, m_req_buf_len - l_bytes_written);
                        if(l_status < 0)
                        {
                                NDBG_PRINT("Error: performing write.  Reason: %s.\n", strerror(errno));
                                return STATUS_ERROR;
                        }
                        l_bytes_written += l_status;

                }
        }

        // TODO REMOVE
        //NDBG_OUTPUT("%sWRITE  %s[%3d]: Status %3d. Reason: %s\n",
        //                ANSI_COLOR_FG_CYAN, ANSI_COLOR_OFF, m_fd, l_status, strerror(errno));

        // Get request time
        m_request_start_time_us = get_time_us();

        m_state = CONN_STATE_READING;
        //header_state = HDST_LINE1_PROTOCOL;

        return STATUS_OK;


}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t nconn::read_cb(void)
{

        uint32_t l_total_bytes_read = 0;
        int32_t l_bytes_read = 0;
        int32_t l_max_read = MAX_READ_BUF - m_read_buf_idx;
        bool l_should_give_up = false;

        do {

                if (m_scheme == URL_SCHEME_HTTPS)
                {
                        l_bytes_read = SSL_read(m_ssl, m_read_buf + m_read_buf_idx, l_max_read);
                }
                else
                {
                        do {
                                l_bytes_read = read(m_fd, m_read_buf + m_read_buf_idx, l_max_read);
                        } while((l_bytes_read < 0) && (errno == EAGAIN));
                }

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

                if((m_read_buf_idx == 0) && (m_stat.m_tt_first_read_us == 0))
                {
                        m_stat.m_tt_first_read_us = get_delta_time_us(m_request_start_time_us);
                }

                // Parse result
                size_t l_parse_status = 0;
                l_parse_status = http_parser_execute(&m_http_parser, &m_http_parser_settings, m_read_buf + m_read_buf_idx, l_bytes_read);
                if(l_parse_status < (size_t)l_bytes_read)
                {
                        if(m_verbose)
                        {
                                NDBG_PRINT("%s: Error: parse error.  Reason: %s: %s\n",
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
int32_t nconn::done_cb(void)
{

        // Shut down connection
        if (m_scheme == URL_SCHEME_HTTPS) {
                if(m_ssl) {
                        SSL_free(m_ssl);
                        m_ssl = NULL;
                }
        }

        //NDBG_PRINT("CLOSE[%lu--%d] %s--CONN--%s\n", m_id, m_fd, ANSI_COLOR_BG_RED, ANSI_COLOR_OFF);
        close(m_fd);
        m_fd = -1;

        // Reset all the values
        // TODO Make init function...
        // Set num use back to zero -we need reset method here?
        m_state = CONN_STATE_FREE;
        m_read_buf_idx = 0;
        m_num_reqs = 0;

        return STATUS_OK;

}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void nconn::reset_stats(void)
{
        // Initialize stats
        stat_init(m_stat);
}


//: ----------------------------------------------------------------------------
//:                          OpenSSL Support
//: ----------------------------------------------------------------------------

//: ----------------------------------------------------------------------------
//: Globals
//: ----------------------------------------------------------------------------
static pthread_mutex_t *g_lock_cs;
static long *g_lock_count;

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
static void pthreads_locking_callback(int mode, int type, const char *file, int line)
{
#if 0
        fprintf(stdout,"thread=%4d mode=%s lock=%s %s:%d\n",
                        (int)CRYPTO_thread_id(),
                        (mode&CRYPTO_LOCK)?"l":"u",
                                        (type&CRYPTO_READ)?"r":"w",file,line);
#endif

#if 0
        if (CRYPTO_LOCK_SSL_CERT == type)
                fprintf(stdout,"(t,m,f,l) %ld %d %s %d\n",
                                CRYPTO_thread_id(),
                                mode,file,line);
#endif

        if (mode & CRYPTO_LOCK)
        {

                pthread_mutex_lock(&(g_lock_cs[type]));

                g_lock_count[type]++;

        } else
        {

                pthread_mutex_unlock(&(g_lock_cs[type]));

        }

}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
static unsigned long pthreads_thread_id(void)
{
        unsigned long ret;

        ret=(unsigned long)pthread_self();
        return(ret);

}

//: ----------------------------------------------------------------------------
//:
//: ----------------------------------------------------------------------------
struct CRYPTO_dynlock_value
{
        pthread_mutex_t mutex;
};

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
static struct CRYPTO_dynlock_value* dyn_create_function(const char* /*file*/, int /*line*/)
{
        struct CRYPTO_dynlock_value* value = new CRYPTO_dynlock_value;
        if (!value) return NULL;

        pthread_mutex_init(&value->mutex, NULL);
        return value;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
static void dyn_lock_function(int mode,
                              struct CRYPTO_dynlock_value* l,
                              const char* a_file,
                              int a_line)
{
        if (mode & CRYPTO_LOCK)
        {
                pthread_mutex_lock(&l->mutex);
        }
        else
        {
                pthread_mutex_unlock(&l->mutex);
        }
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
static void dyn_destroy_function(struct CRYPTO_dynlock_value* l,
                                 const char* a_file,
                                 int a_line)
{
        pthread_mutex_destroy(&l->mutex);
        delete l;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void nconn_kill_locks(void)
{
        int i;

        CRYPTO_set_locking_callback(NULL);
        for (i=0; i<CRYPTO_num_locks(); i++)
        {
                pthread_mutex_destroy(&(g_lock_cs[i]));
        }

        OPENSSL_free(g_lock_cs);
}

//: ----------------------------------------------------------------------------
//: \details: OpenSSL can safely be used in multi-threaded applications provided
//:           that at least two callback functions are set, locking_function and
//:           threadid_func this function sets those two callbacks.
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
static void init_ssl_locking(void)
{
        int i;

        g_lock_cs = (pthread_mutex_t *)OPENSSL_malloc(CRYPTO_num_locks() * sizeof(pthread_mutex_t));
        g_lock_count = (long *)OPENSSL_malloc(CRYPTO_num_locks() * sizeof(long));

        for (i=0; i<CRYPTO_num_locks(); ++i)
        {

                g_lock_count[i]=0;

                pthread_mutex_init(&(g_lock_cs[i]),NULL);

        }

        CRYPTO_set_id_callback(pthreads_thread_id);
        CRYPTO_set_locking_callback(pthreads_locking_callback);

        CRYPTO_set_dynlock_create_callback(dyn_create_function);
        CRYPTO_set_dynlock_lock_callback(dyn_lock_function);
        CRYPTO_set_dynlock_destroy_callback(dyn_destroy_function);

}

//: ----------------------------------------------------------------------------
//: \details: Initialize OpenSSL
//: \return:  ctx on success, NULL on failure
//: \param:   TODO
//: ----------------------------------------------------------------------------
SSL_CTX* nconn_ssl_init(const std::string &a_cipher_list)
{
        SSL_CTX *server_ctx;

        // Initialize the OpenSSL library
        SSL_library_init();

        // Bring in and register error messages
        ERR_load_crypto_strings();
        SSL_load_error_strings();

        // TODO Deprecated???
        //SSLeay_add_ssl_algorithms();
        OpenSSL_add_all_algorithms();

        // Set up for thread safety
        init_ssl_locking();

        // We MUST have entropy, or else there's no point to crypto.
        if (!RAND_poll())
        {
                return NULL;
        }

        // TODO Old method???
#if 0
        // Random seed
        if (! RAND_status())
        {
                unsigned char bytes[1024];
                for (size_t i = 0; i < sizeof(bytes); ++i)
                        bytes[i] = random() % 0xff;
                RAND_seed(bytes, sizeof(bytes));
        }
#endif

        server_ctx = SSL_CTX_new(SSLv23_client_method()); /* Create new context */
        if (server_ctx == NULL)
        {
                ERR_print_errors_fp(stderr);
                NDBG_PRINT("SSL_CTX_new Error: %s\n", ERR_error_string(ERR_get_error(), NULL));
                return NULL;
        }

        if (a_cipher_list.size() > 0)
        {
                if (! SSL_CTX_set_cipher_list(server_ctx, a_cipher_list.c_str()))
                {
                        NDBG_PRINT("ERRRO: cannot set m_cipher list\n");
                        ERR_print_errors_fp(stderr);
                        //close_connection(con, nowP);
                        return NULL;
                }
        }


#if 0
        if (!SSL_CTX_load_verify_locations(ctx,
                        DEFAULT_PEM_FILE,
                        NULL))
        {
                fprintf(stderr, "Error loading trust store\n");
                ERR_print_errors_fp(stderr);
                SSL_CTX_free(ctx);
                ctx = NULL;
        }
#endif

        //NDBG_PRINT("SSL_CTX_new success\n");

        return server_ctx;

}


// -----------------------------------------------------------------------------
// TODO SCOTTS CODE TODO SCOTTS CODE TODO SCOTTS CODE TODO SCOTTS CODE
// -----------------------------------------------------------------------------
#if 0
void get_servers(subbuffer pop, subbuffer srvtype, subbuffer status, subbuffer indexes, buffer& servers)
{
        buffer cmd;
        servers.reset();

        cmd << "/EdgeCast/base/getServers --domain-root";
        if (!pop.empty())
        {
                cmd << " --pop='";
                cmd.append(pop, comma_to_pipe());
                cmd << "'";
        }
        if (!srvtype.empty())
                cmd << " --srvtype='" << srvtype << "'";
        if (!status.empty())
                cmd << " --status='" << status << "'";
        if (!indexes.empty())
                cmd << " --index='" << indexes << "'";
        run_cmd(cmd.b_str(), servers);
        servers.transform(lr_to_comma());

        if (s_verbose)
                fprintf(stderr, "cmd: %s, results: %s\n", cmd.b_str(), servers.b_str());
}
#endif


#if 0
struct CRYPTO_dynlock_value
{
        pthread_mutex_t mutex;
};

#define MUTEX_TYPE pthread_mutex_t
#define MUTEX_SETUP(x) pthread_mutex_init(&(x), NULL)
#define MUTEX_CLEANUP(x) pthread_mutex_destroy(&(x))
#define MUTEX_LOCK(x) pthread_mutex_lock(&(x))
#define MUTEX_UNLOCK(x) pthread_mutex_unlock(&(x))
#define THREAD_ID pthread_self( )
static MUTEX_TYPE* mutex_buf = NULL ;

static void locking_function(int mode, int n, const char* /*file*/, int /*line*/)
{
        if (mode & CRYPTO_LOCK)
                MUTEX_LOCK(mutex_buf[n]);
        else
                MUTEX_UNLOCK(mutex_buf[n]);
}

static unsigned long id_function(void)
{
        return ((unsigned long)THREAD_ID);
}

static struct CRYPTO_dynlock_value* dyn_create_function(const char* /*file*/, int /*line*/)
{
        struct CRYPTO_dynlock_value* value = new CRYPTO_dynlock_value;
        if (!value) return NULL;

        pthread_mutex_init(&value->mutex, NULL);
        return value;
}
static void dyn_lock_function(int mode, struct CRYPTO_dynlock_value* l, const char* /*file*/, int /*line*/)
{
        if (mode & CRYPTO_LOCK)
                pthread_mutex_lock(&l->mutex);
        else
                pthread_mutex_unlock(&l->mutex);
}

static void dyn_destroy_function(struct CRYPTO_dynlock_value* l, const char* /*file*/, int /*line*/)
{
        pthread_mutex_destroy(&l->mutex);
        delete l;
}

int THREAD_setup(void)
{
        int num_locks = CRYPTO_num_locks();
        mutex_buf = (MUTEX_TYPE*) malloc(num_locks * sizeof(MUTEX_TYPE));
        if(!mutex_buf) return 0;
        for (int i = 0; i < num_locks; i++)
                MUTEX_SETUP(mutex_buf[i]);
        CRYPTO_set_id_callback(id_function);
        CRYPTO_set_locking_callback(locking_function);
        CRYPTO_set_dynlock_create_callback(dyn_create_function);
        CRYPTO_set_dynlock_lock_callback(dyn_lock_function);
        CRYPTO_set_dynlock_destroy_callback(dyn_destroy_function);
        return 1;
}

int THREAD_cleanup(void)
{
        if (!mutex_buf) return 0;
        CRYPTO_set_id_callback(NULL);
        CRYPTO_set_locking_callback(NULL);
        for (int i = 0; i < CRYPTO_num_locks( ); i++)
                MUTEX_CLEANUP(mutex_buf[i]);
        free(mutex_buf);
        mutex_buf = NULL;
        return 1;
}

void init_OpenSSL(void)
{
        if (!THREAD_setup() || !SSL_library_init())
        {
                fprintf(stderr, "** OpenSSL initialization failed!\n");
                exit(-1);
        }
        SSLeay_add_ssl_algorithms();
        SSL_load_error_strings();
        SSL_library_init();
}
#endif


// -----------------------------------------------------------------------------
// TODO OLD CODE TODO OLD CODE TODO OLD CODE TODO OLD CODE TODO OLD CODE TODO
// -----------------------------------------------------------------------------
#if 0
//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
std::string get_file_wo_path(std::string &a_filename)
{
        std::string fName(a_filename);
        size_t pos = fName.rfind("/");

        if(pos == std::string::npos)  //No extension.
                return fName;
        if(pos == 0)    //. is at the front. Not an extension.
                return fName;

        return fName.substr(pos + 1, fName.length());
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
std::string get_file_path(std::string &a_filename)
{
        std::string fName(a_filename);
        size_t pos = fName.rfind("/");

        if(pos == std::string::npos)  //No extension.
                return fName;
        if(pos == 0)    //. is at the front. Not an extension.
                return fName;

        return fName.substr(0, pos);
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
std::string get_base_filename(std::string &a_filename)
{
        std::string fName(a_filename);
        size_t pos = fName.rfind(".");

        if(pos == std::string::npos)  //No extension.
                return fName;
        if(pos == 0)    //. is at the front. Not an extension.
                return fName;

        return fName.substr(0, pos);
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
std::string get_file_ext(std::string &a_filename)
{
        std::string fName(a_filename);
        size_t pos = fName.rfind(".");

        if(pos == std::string::npos)  //No extension.
                return NULL;
        if(pos == 0)    //. is at the front. Not an extension.
                return NULL;

        return fName.substr(pos + 1, fName.length());
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
std::string get_file_wo_ext(std::string &a_filename)
{
        std::string fName(a_filename);
        size_t pos = fName.rfind(".");

        if(pos == std::string::npos)  //No extension.
                return NULL;
        if(pos == 0)    //. is at the front. Not an extension.
                return NULL;

        return fName.substr(0, pos);
}

#endif


