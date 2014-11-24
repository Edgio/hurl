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
#include "evr.h"
#include "parsed_url.h"

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
                if(l_conn->m_collect_stats_flag)
                {
                        l_conn->m_stat.m_tt_header_completion_us = get_delta_time_us(l_conn->m_request_start_time_us);
                }

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
                if(l_conn->m_collect_stats_flag)
                {
                        l_conn->m_stat.m_body_bytes += a_length;
                }

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
                // Stats
                if(l_conn->m_collect_stats_flag)
                {
                        l_conn->m_stat.m_tt_completion_us = get_delta_time_us(l_conn->m_request_start_time_us);
                }

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
int32_t nconn::setup_socket(const host_info_t &a_host_info)
{
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

        // -------------------------------------------
        // Initalize the http response parser
        // -------------------------------------------
        if (m_scheme == SCHEME_HTTP)
        {
                m_http_parser.data = this;
                http_parser_init(&m_http_parser, HTTP_RESPONSE);
        }
        else if(m_scheme == SCHEME_HTTPS)
        {
                m_http_parser.data = this;
                http_parser_init(&m_http_parser, HTTP_RESPONSE);

                // Create SSL Context
                m_ssl = SSL_new(m_ssl_ctx);
                // TODO Check for NULL

                SSL_set_fd(m_ssl, m_fd);
                // TODO Check for Errors

        }

        return STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t nconn::ssl_connect_cb(const host_info_t &a_host_info)
{
        // -------------------------------------------
        // HTTPS
        // -------------------------------------------

        int l_status;
        m_state = CONN_STATE_SSL_CONNECTING;

        l_status = SSL_connect(m_ssl);
        // TODO REMOVE
        //NDBG_PRINT("%sSSL_CON%s[%3d]: Status %3d. Reason: %s\n",
        //                ANSI_COLOR_BG_GREEN, ANSI_COLOR_OFF, m_fd, l_status, strerror(errno));
        //NDBG_PRINT_BT();
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
                {
                        //NDBG_PRINT("%sSSL_CON%s[%3d]: SSL_ERROR_WANT_READ\n", ANSI_COLOR_BG_GREEN, ANSI_COLOR_OFF, m_fd);
                        m_state = CONN_STATE_SSL_CONNECTING_WANT_READ;
                        return EAGAIN;

                }
                case SSL_ERROR_WANT_WRITE:
                {
                        //NDBG_PRINT("%sSSL_CON%s[%3d]: CONN_STATE_SSL_CONNECTING_WANT_WRITE\n", ANSI_COLOR_BG_GREEN, ANSI_COLOR_OFF, m_fd);
                        m_state = CONN_STATE_SSL_CONNECTING_WANT_WRITE;
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
        if(m_collect_stats_flag)
        {
                m_stat.m_tt_ssl_connect_us = get_delta_time_us(m_connect_start_time_us);
        }

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

        //did_connect = 1;

        return STATUS_OK;

}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t nconn::send_request(bool is_reuse)
{

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

                // -----------------------------------------
                //
                // -----------------------------------------
                if(m_scheme == SCHEME_HTTP)
                {
                        l_status = write(m_fd, m_req_buf + l_bytes_written, m_req_buf_len - l_bytes_written);
                        if(l_status < 0)
                        {
                                NDBG_PRINT("Error: performing write.  Reason: %s.\n", strerror(errno));
                                return STATUS_ERROR;
                        }
                        l_bytes_written += l_status;

                }
                // -----------------------------------------
                //
                // -----------------------------------------
                else if(m_scheme == SCHEME_HTTPS)
                {
                        l_status = SSL_write(m_ssl, m_req_buf + l_bytes_written, m_req_buf_len - l_bytes_written);
                        if(l_status < 0)
                        {
                                NDBG_PRINT("Error: performing SSL_write.\n");
                                return STATUS_ERROR;
                        }
                        l_bytes_written += l_status;
                }

        }

        // TODO REMOVE
        //NDBG_OUTPUT("%sWRITE  %s[%3d]: Status %3d. Reason: %s\n",
        //                ANSI_COLOR_BG_CYAN, ANSI_COLOR_OFF, m_fd, l_status, strerror(errno));

        // Get request time
        if(m_collect_stats_flag)
        {
                m_request_start_time_us = get_time_us();
        }

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
                // -----------------------------------------
                // HTTP
                // -----------------------------------------
                if (m_scheme == SCHEME_HTTP)
                {
                        do {
                                l_bytes_read = read(m_fd, m_read_buf + m_read_buf_idx, l_max_read);
                                //NDBG_PRINT("%sHOST%s: %s fd[%3d] READ: %d bytes. Reason: %s\n",
                                //                ANSI_COLOR_FG_RED, ANSI_COLOR_OFF, m_host.c_str(), m_fd,
                                //                l_bytes_read,
                                //                strerror(errno));

                        } while((l_bytes_read < 0) && (errno == EAGAIN));
                }
                // -----------------------------------------
                // HTTPS
                // -----------------------------------------
                else if (m_scheme == SCHEME_HTTPS)
                {

                        l_bytes_read = SSL_read(m_ssl, m_read_buf + m_read_buf_idx, l_max_read);
                        if(l_bytes_read < 0)
                        {
                                int l_ssl_error = SSL_get_error(m_ssl, l_bytes_read);
                                //NDBG_PRINT("%sSSL_READ%s[%3d] l_bytes_read: %d error: %d\n",
                                //                ANSI_COLOR_BG_RED, ANSI_COLOR_OFF,
                                //                SSL_get_fd(m_ssl), l_bytes_read,
                                //                l_ssl_error);
                                if(l_ssl_error == SSL_ERROR_WANT_READ)
                                {
                                        return STATUS_OK;
                                }
                        }
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
                if(m_collect_stats_flag)
                {
                        if((m_read_buf_idx == 0) && (m_stat.m_tt_first_read_us == 0))
                        {
                                m_stat.m_tt_first_read_us = get_delta_time_us(m_request_start_time_us);
                        }
                }

                // -----------------------------------------
                // HTTP(S) Parse result
                // -----------------------------------------
                if ((m_scheme == SCHEME_HTTP) ||
                    (m_scheme == SCHEME_HTTPS))
                {
                        size_t l_parse_status = 0;
                        //NDBG_PRINT("%sHTTP_PARSER%s: m_read_buf: %p, m_read_buf_idx: %d, l_bytes_read: %d\n",
                        //                ANSI_COLOR_BG_YELLOW, ANSI_COLOR_OFF,
                        //                m_read_buf, (int)m_read_buf_idx, (int)l_bytes_read);
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
        if (m_scheme == SCHEME_HTTP)
        {

        }
        else if (m_scheme == SCHEME_HTTPS)
        {
                if(m_ssl)
                {
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
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t nconn::run_state_machine(evr_loop *a_evr_loop, const host_info_t &a_host_info)
{

        uint32_t l_retry_connect_count = 0;

        // Cancel last timer
        a_evr_loop->cancel_timer(&(m_timer_obj));

        //NDBG_PRINT("%sRUN_STATE_MACHINE%s: STATE[%d] --START\n", ANSI_COLOR_BG_YELLOW, ANSI_COLOR_OFF, m_state);
state_top:
        //NDBG_PRINT("%sRUN_STATE_MACHINE%s: STATE[%d]\n", ANSI_COLOR_BG_YELLOW, ANSI_COLOR_OFF, m_state);
        switch (m_state)
        {

        // -------------------------------------------------
        // STATE: FREE
        // -------------------------------------------------
        case CONN_STATE_FREE:
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
                        NDBG_PRINT("Error: Couldn't add socket file descriptor\n");
                        return STATUS_ERROR;
                }

                m_state = CONN_STATE_CONNECTING;
                goto state_top;

        }

        // -------------------------------------------------
        // STATE: CONNECTING
        // -------------------------------------------------
        case CONN_STATE_CONNECTING:
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
                                m_state = CONN_STATE_CONNECTED;
                                break;
                        }
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
                        }
                        case ECONNREFUSED:
                        {
                                NDBG_PRINT("Error Connection refused for host: %s. Reason: %s\n", m_host.c_str(), strerror(errno));
                                return STATUS_ERROR;
                        }
                        case EAGAIN:
                        case EINPROGRESS:
                        {
                                //NDBG_PRINT("Error Connection in progress. Reason: %s\n", strerror(errno));
                                m_state = CONN_STATE_CONNECTING;
                                //l_in_progress = true;

                                // Set to writeable and try again
                                if (0 != a_evr_loop->mod_fd(m_fd,
                                                            EVR_FILE_ATTR_MASK_WRITE|EVR_FILE_ATTR_MASK_STATUS_ERROR,
                                                            this))
                                {
                                        NDBG_PRINT("Error: Couldn't add socket file descriptor\n");
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
                                        NDBG_PRINT("Error connect() for host: %s.  Reason: %s\n", m_host.c_str(), strerror(errno));
                                        return STATUS_ERROR;
                                }
                                break;
                        }
                        default:
                        {
                                NDBG_PRINT("Error Unkown. Reason: %s\n", strerror(errno));
                                return STATUS_ERROR;
                        }
                        }
                }

                m_state = CONN_STATE_CONNECTED;

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
                // HTTPS
                // -------------------------------------------
                if (m_scheme == SCHEME_HTTP)
                {
                        // Nuttin...
                }
                else if(m_scheme == SCHEME_HTTPS)
                {
                        m_state = CONN_STATE_SSL_CONNECTING;
                }

                // -------------------------------------------
                // Add to event handler
                // -------------------------------------------
                if (0 != a_evr_loop->mod_fd(m_fd,
                                            EVR_FILE_ATTR_MASK_READ|EVR_FILE_ATTR_MASK_STATUS_ERROR,
                                            this))
                {
                        NDBG_PRINT("Error: Couldn't add socket file descriptor\n");
                        return STATUS_ERROR;
                }

                goto state_top;
        }

        // -------------------------------------------------
        // STATE: SSL_CONNECTING
        // -------------------------------------------------
        case CONN_STATE_SSL_CONNECTING:
        case CONN_STATE_SSL_CONNECTING_WANT_WRITE:
        case CONN_STATE_SSL_CONNECTING_WANT_READ:
        {
                int l_status;
                //NDBG_PRINT("%sSSL_CONNECTING%s\n", ANSI_COLOR_BG_RED, ANSI_COLOR_OFF);
                l_status = ssl_connect_cb(a_host_info);
                if(EAGAIN == l_status)
                {
                        if(CONN_STATE_SSL_CONNECTING_WANT_READ == m_state)
                        {
                                if (0 != a_evr_loop->mod_fd(m_fd,
                                                            EVR_FILE_ATTR_MASK_READ|EVR_FILE_ATTR_MASK_STATUS_ERROR,
                                                            this))
                                {
                                        NDBG_PRINT("Error: Couldn't add socket file descriptor\n");
                                        return STATUS_ERROR;
                                }
                        }
                        else if(CONN_STATE_SSL_CONNECTING_WANT_WRITE == m_state)
                        {
                                if (0 != a_evr_loop->mod_fd(m_fd,
                                                            EVR_FILE_ATTR_MASK_WRITE|EVR_FILE_ATTR_MASK_STATUS_ERROR,
                                                            this))
                                {
                                        NDBG_PRINT("Error: Couldn't add socket file descriptor\n");
                                        return STATUS_ERROR;
                                }
                        }
                        return STATUS_OK;
                }
                else if(STATUS_OK != l_status)
                {
                        NDBG_PRINT("Error: performing connect_cb\n");
                        return STATUS_ERROR;
                }

                // -------------------------------------------
                // Add to event handler
                // -------------------------------------------
                if (0 != a_evr_loop->mod_fd(m_fd,
                                            EVR_FILE_ATTR_MASK_READ|EVR_FILE_ATTR_MASK_STATUS_ERROR,
                                            this))
                {
                        NDBG_PRINT("Error: Couldn't add socket file descriptor\n");
                        return STATUS_ERROR;
                }

                goto state_top;
        }

        // -------------------------------------------------
        // STATE: CONNECTED
        // -------------------------------------------------
        case CONN_STATE_CONNECTED:
        {
                // -------------------------------------------
                // Send request
                // -------------------------------------------
                int32_t l_request_status = STATUS_OK;
                //NDBG_PRINT("%sSEND_REQUEST%s\n", ANSI_COLOR_BG_CYAN, ANSI_COLOR_OFF);
                l_request_status = send_request(false);
                if(l_request_status != STATUS_OK)
                {
                        NDBG_PRINT("Error: performing send_request\n");
                        return STATUS_ERROR;
                }
                break;
        }

        // -------------------------------------------------
        // STATE: READING
        // -------------------------------------------------
        case CONN_STATE_READING:
        {
                int l_read_status = 0;
                //NDBG_PRINT("%sCNST_READING%s\n", ANSI_COLOR_BG_CYAN, ANSI_COLOR_OFF);
                l_read_status = read_cb();
                //NDBG_PRINT("%sCNST_READING%s: read_cb(): total: %d\n", ANSI_COLOR_BG_CYAN, ANSI_COLOR_OFF, (int)m_stat.m_total_bytes);
                if(l_read_status < 0)
                {
                        NDBG_PRINT("Error: performing read_cb\n");
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
//:                          OpenSSL Support
//: ----------------------------------------------------------------------------

//: ----------------------------------------------------------------------------
//: Globals
//: ----------------------------------------------------------------------------
static pthread_mutex_t *g_lock_cs;

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
static void pthreads_locking_callback(int a_mode, int a_type, const char *a_file, int a_line)
{
#if 0
        fprintf(stdout,"thread=%4d mode=%s lock=%s %s:%d\n",
                        (int)CRYPTO_thread_id(),
                        (mode&CRYPTO_LOCK)?"l":"u",
                                        (type&CRYPTO_READ)?"r":"w",a_file,a_line);
#endif

#if 0
        if (CRYPTO_LOCK_SSL_CERT == type)
                fprintf(stdout,"(t,m,f,l) %ld %d %s %d\n",
                                CRYPTO_thread_id(),
                                a_mode,a_file,a_line);
#endif

        if (a_mode & CRYPTO_LOCK)
        {
                pthread_mutex_lock(&(g_lock_cs[a_type]));
        } else
        {
                pthread_mutex_unlock(&(g_lock_cs[a_type]));

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
static struct CRYPTO_dynlock_value* dyn_create_function(const char* a_file, int a_line)
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
static void dyn_lock_function(int a_mode,
                              struct CRYPTO_dynlock_value* a_l,
                              const char* a_file,
                              int a_line)
{
        if (a_mode & CRYPTO_LOCK)
        {
                pthread_mutex_lock(&a_l->mutex);
        }
        else
        {
                pthread_mutex_unlock(&a_l->mutex);
        }
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
static void dyn_destroy_function(struct CRYPTO_dynlock_value* a_l,
                                 const char* a_file,
                                 int a_line)
{
        if(a_l)
        {
                pthread_mutex_destroy(&a_l->mutex);
                free(a_l);
        }
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void nconn_kill_locks(void)
{
        CRYPTO_set_id_callback(NULL);
        CRYPTO_set_locking_callback(NULL);
        if(g_lock_cs)
        {
                for (int i=0; i<CRYPTO_num_locks(); ++i)
                {
                        pthread_mutex_destroy(&(g_lock_cs[i]));
                }
        }

        OPENSSL_free(g_lock_cs);
        g_lock_cs = NULL;
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
        int l_num_locks = CRYPTO_num_locks();
        g_lock_cs = (pthread_mutex_t *)OPENSSL_malloc(l_num_locks * sizeof(pthread_mutex_t));
        //g_lock_cs =(pthread_mutex_t*)malloc(        l_num_locks * sizeof(pthread_mutex_t));

        for (int i=0; i<l_num_locks; ++i)
        {
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

