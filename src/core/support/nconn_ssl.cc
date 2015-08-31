//: ----------------------------------------------------------------------------
//: Copyright (C) 2014 Verizon.  All Rights Reserved.
//: All Rights Reserved
//:
//: \file:    nconn_ssl.cc
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
#include "nconn_ssl.h"
#include "util.h"
#include "evr.h"
#include "hostcheck.h"
#include "reqlet.h"
#include "ssl_util.h"
#include "ndebug.h"

#include <errno.h>
#include <string.h>
#include <unistd.h>

#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/bio.h>
#include <openssl/rand.h>
#include <openssl/crypto.h>
#include <openssl/x509v3.h>

//: ----------------------------------------------------------------------------
//: Macros
//: ----------------------------------------------------------------------------

namespace ns_hlx {


//: ----------------------------------------------------------------------------
//: Globals
//: ----------------------------------------------------------------------------
__thread char gts_last_ssl_error[256] = "\0";

//: ----------------------------------------------------------------------------
//: Fwd Decl's
//: ----------------------------------------------------------------------------
static int ssl_cert_verify_callback(int ok, X509_STORE_CTX* store);
static int ssl_cert_verify_callback_allow_self_signed(int ok, X509_STORE_CTX* store);
static bool ssl_x509_get_ids(X509* x509, std::vector<std::string>& ids);

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t nconn_ssl::ssl_connect(const host_info_t &a_host_info)
{
        // -------------------------------------------
        // HTTPSf
        // -------------------------------------------

        int l_status;
        m_ssl_state = SSL_STATE_SSL_CONNECTING;

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
                        if(gts_last_ssl_error[0] != '\0')
                        {
                                NCONN_ERROR("HOST[%s]: SSL_ERROR_SSL %lu: %s. Reason: %s",
                                                m_host.c_str(),
                                                ERR_get_error(),
                                                ERR_error_string(ERR_get_error(),NULL),
                                                gts_last_ssl_error);
                                // Set back
                                gts_last_ssl_error[0] = '\0';
                        }
                        else
                        {
                                NCONN_ERROR("HOST[%s]: SSL_ERROR_SSL %lu: %s.",
                                                m_host.c_str(),
                                                ERR_get_error(),
                                                ERR_error_string(ERR_get_error(),NULL));
                        }

                        break;
                }
                case SSL_ERROR_WANT_READ:
                {
                        //NDBG_PRINT("%sSSL_CON%s[%3d]: SSL_ERROR_WANT_READ\n", ANSI_COLOR_BG_GREEN, ANSI_COLOR_OFF, m_fd);
                        m_ssl_state = SSL_STATE_SSL_CONNECTING_WANT_READ;
                        return EAGAIN;

                }
                case SSL_ERROR_WANT_WRITE:
                {
                        //NDBG_PRINT("%sSSL_CON%s[%3d]: SSL_STATE_SSL_CONNECTING_WANT_WRITE\n", ANSI_COLOR_BG_GREEN, ANSI_COLOR_OFF, m_fd);
                        m_ssl_state = SSL_STATE_SSL_CONNECTING_WANT_WRITE;
                        return EAGAIN;
                }

                case SSL_ERROR_WANT_X509_LOOKUP:
                {
                        NCONN_ERROR("HOST[%s]: SSL_ERROR_WANT_X509_LOOKUP", m_host.c_str());
                        break;
                }

                // look at error stack/return value/errno
                case SSL_ERROR_SYSCALL:
                {
                        if(l_status == 0) {
                                NCONN_ERROR("HOST[%s]: SSL_ERROR_SYSCALL %lu: %s. An EOF was observed that violates the protocol",
                                                m_host.c_str(),
                                                ERR_get_error(), ERR_error_string(ERR_get_error(), NULL));
                        } else if(l_status == -1) {
                                NCONN_ERROR("HOST[%s]: SSL_ERROR_SYSCALL %lu: %s. %s",
                                                m_host.c_str(),
                                                ERR_get_error(), ERR_error_string(ERR_get_error(), NULL),
                                                strerror(errno));
                        }
                        break;
                }
                case SSL_ERROR_ZERO_RETURN:
                {
                        NCONN_ERROR("HOST[%s]: SSL_ERROR_ZERO_RETURN", m_host.c_str());
                        break;
                }
                case SSL_ERROR_WANT_CONNECT:
                {
                        NCONN_ERROR("HOST[%s]: SSL_ERROR_WANT_CONNECT", m_host.c_str());
                        break;
                }
                case SSL_ERROR_WANT_ACCEPT:
                {
                        NCONN_ERROR("HOST[%s]: SSL_ERROR_WANT_ACCEPT", m_host.c_str());
                        break;
                }
                }


                //ERR_print_errors_fp(stderr);
                return STATUS_ERROR;
        }
        else if(1 == l_status)
        {
                m_ssl_state = SSL_STATE_CONNECTED;
        }

        // Stats
        if(m_collect_stats_flag)
        {
                m_stat.m_tt_ssl_connect_us = get_delta_time_us(m_connect_start_time_us);
                //NDBG_PRINT("m_stat.m_tt_ssl_connect_us: %lud\n", m_stat.m_tt_ssl_connect_us);
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
int32_t nconn_ssl::send_request(bool is_reuse)
{

        m_ssl_state = SSL_STATE_CONNECTED;

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
                        NDBG_OUTPUT("HOST[%s]: REQUEST-->\n%s%.*s%s\n", m_host.c_str(), ANSI_COLOR_FG_MAGENTA, (int)m_req_buf_len, m_req_buf, ANSI_COLOR_OFF);
                else
                        NDBG_OUTPUT("HOST[%s]: REQUEST-->\n%.*s\n", m_host.c_str(), (int)m_req_buf_len, m_req_buf);

        }

        // TODO Wrap with while and check for errors
        uint32_t l_bytes_written = 0;
        int l_status;
        while(l_bytes_written < m_req_buf_len)
        {
                l_status = SSL_write(m_ssl, m_req_buf + l_bytes_written, m_req_buf_len - l_bytes_written);
                if(l_status < 0)
                {
                        NCONN_ERROR("HOST[%s]: Error: performing SSL_write.\n", m_host.c_str());
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

        m_ssl_state = SSL_STATE_READING;
        //header_state = HDST_LINE1_PROTOCOL;

        return STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t nconn_ssl::receive_response(void)
{

        uint32_t l_total_bytes_read = 0;
        int32_t l_bytes_read = 0;
        int32_t l_max_read = m_max_read_buf - m_read_buf_idx;
        bool l_should_give_up = false;

        do {
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

                //NDBG_PRINT("%sHOST%s: %s fd[%3d]\n", ANSI_COLOR_FG_RED, ANSI_COLOR_OFF, m_host.c_str(), m_fd);
                //ns_hlo::mem_display((uint8_t *)(m_read_buf + m_read_buf_idx), l_bytes_read);

                // TODO Handle EOF -close connection...
                if ((l_bytes_read <= 0) && (errno != EAGAIN))
                {
                        if(m_verbose)
                        {
                                NCONN_ERROR("HOST[%s]: %sl_bytes_read%s[%d] <= 0 total = %u--error: %s\n",
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
int32_t nconn_ssl::cleanup(void)
{
        // Shut down connection
        if(m_ssl)
        {
                SSL_free(m_ssl);
                m_ssl = NULL;
        }

        // Reset all the values
        // TODO Make init function...
        // Set num use back to zero -we need reset method here?
        m_ssl_state = SSL_STATE_FREE;

        // Super
        nconn_tcp::cleanup();

        return STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t nconn_ssl::init(void)
{

        // Create SSL Context
        m_ssl = SSL_new(m_ssl_ctx);
        // TODO Check for NULL

        SSL_set_fd(m_ssl, m_fd);
        // TODO Check for Errors

        const long l_ssl_options = m_ssl_opt_options;

        //NDBG_PRINT("l_ssl_options: 0x%08lX\n", l_ssl_options);

        if (l_ssl_options)
        {
                // clear all options and set specified options
                SSL_clear_options(m_ssl, 0x11111111L);
                long l_result = SSL_set_options(m_ssl, l_ssl_options);
                if (l_ssl_options != l_result)
                {
                        //NDBG_PRINT("Failed to set SSL options: 0x%08lX -set to: 0x%08lX \n", l_result, l_ssl_options);
                        //return STATUS_ERROR;
                }
        }

        if (!m_ssl_opt_cipher_str.empty())
        {
                if (1 != SSL_set_cipher_list(m_ssl, m_ssl_opt_cipher_str.c_str()))
                {
                        NCONN_ERROR("HOST[%s]: Failed to set ssl cipher list: %s\n", m_host.c_str(), m_ssl_opt_cipher_str.c_str());
                        return STATUS_ERROR;
                }
        }

        // Set tls sni extension
        if (!m_ssl_opt_tlsext_hostname.empty())
        {
                // const_cast to work around SSL's use of arg -this call does not change buffer argument
                if (1 != SSL_set_tlsext_host_name(m_ssl, m_ssl_opt_tlsext_hostname.c_str()))
                {
                        NCONN_ERROR("HOST[%s]: Failed to set tls hostname: %s\n", m_host.c_str(), m_ssl_opt_tlsext_hostname.c_str());
                        return false;
                }
        }

        // Set SSL Cert verify callback ...
        if (m_ssl_opt_verify)
        {
                if (m_ssl_opt_verify_allow_self_signed)
                {
                        SSL_set_verify(m_ssl, SSL_VERIFY_PEER, ssl_cert_verify_callback_allow_self_signed);
                }
                else
                {
                        SSL_set_verify(m_ssl, SSL_VERIFY_PEER, ssl_cert_verify_callback);
                }
        }

	return STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t nconn_ssl::run_state_machine(evr_loop *a_evr_loop, const host_info_t &a_host_info)
{

        uint32_t l_retry_connect_count = 0;

        // Cancel last timer if was not in free state
        if(m_ssl_state != SSL_STATE_FREE)
        {
                a_evr_loop->cancel_timer(&(m_timer_obj));
        }

        //NDBG_PRINT("%sRUN_STATE_MACHINE%s: STATE[%d] --START\n",
        //                ANSI_COLOR_BG_YELLOW, ANSI_COLOR_OFF,
        //                m_ssl_state);
state_top:
        //NDBG_PRINT("%sRUN_STATE_MACHINE%s: STATE[%d]\n",
        //                ANSI_COLOR_BG_YELLOW, ANSI_COLOR_OFF,
        //                m_ssl_state);
        switch (m_ssl_state)
        {

        // -------------------------------------------------
        // STATE: FREE
        // -------------------------------------------------
        case SSL_STATE_FREE:
        {
                int32_t l_status;
                l_status = setup_socket(a_host_info);
                if(l_status != STATUS_OK)
                {
                        return STATUS_ERROR;
                }

                //NDBG_PRINT("m_ssl_ctx: %p\n", m_ssl_ctx);

                l_status = init();
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
                        NCONN_ERROR("HOST[%s]: Error: Couldn't add socket file descriptor", m_host.c_str());
                        return STATUS_ERROR;
                }

                m_ssl_state = SSL_STATE_CONNECTING;
                goto state_top;

        }

        // -------------------------------------------------
        // STATE: CONNECTING
        // -------------------------------------------------
        case SSL_STATE_CONNECTING:
        {
                //int l_status;
                //NDBG_PRINT("%sRUN_STATE_MACHINE%s: %sCNST_CONNECTING%s\n",
                //                ANSI_COLOR_BG_YELLOW, ANSI_COLOR_OFF,
                //                ANSI_COLOR_BG_RED, ANSI_COLOR_OFF);
                int l_connect_status = 0;
                l_connect_status = connect(m_fd,
                                           (struct sockaddr*) &(a_host_info.m_sa),
                                           (a_host_info.m_sa_len));

                // TODO REMOVE
                //++l_retry;
                //NDBG_PRINT_BT();
                //NDBG_PRINT("%sRUN_STATE_MACHINE%s: %sCONNECT%s[%3d]: Retry: %d Status %3d. Reason[%d]: %s\n",
                //           ANSI_COLOR_BG_YELLOW, ANSI_COLOR_OFF,
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
                                m_ssl_state = SSL_STATE_CONNECTED;
                                break;
                        }
                        case EINVAL:
                        {
                                int l_err;
                                socklen_t l_errlen;
                                l_errlen = sizeof(l_err);
                                if (getsockopt(m_fd, SOL_SOCKET, SO_ERROR, (void*) &l_err, &l_errlen) < 0)
                                {
                                        NCONN_ERROR("HOST[%s]: unknown connect error",
                                                        m_host.c_str());
                                }
                                else
                                {
                                        NCONN_ERROR("HOST[%s]: %s", m_host.c_str(),
                                                        strerror(l_err));
                                }
                                return STATUS_ERROR;
                        }
                        case ECONNREFUSED:
                        {
                                NCONN_ERROR("HOST[%s]: Error Connection refused. Reason: %s", m_host.c_str(), strerror(errno));
                                return STATUS_ERROR;
                        }
                        case EAGAIN:
                        case EINPROGRESS:
                        {
                                //NDBG_PRINT("Error Connection in progress. Reason: %s\n", strerror(errno));
                                m_ssl_state = SSL_STATE_CONNECTING;
                                //l_in_progress = true;

                                // Set to writeable and try again
                                if (0 != a_evr_loop->mod_fd(m_fd,
                                                            EVR_FILE_ATTR_MASK_WRITE|EVR_FILE_ATTR_MASK_STATUS_ERROR,
                                                            this))
                                {
                                        NCONN_ERROR("HOST[%s]: Error: Couldn't add socket file descriptor", m_host.c_str());
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
                                        NCONN_ERROR("HOST[%s]: Error connect(). Reason: %s", m_host.c_str(), strerror(errno));
                                        return STATUS_ERROR;
                                }
                                break;
                        }
                        default:
                        {
                                NCONN_ERROR("HOST[%s]: Error Unkown. Reason: %s", m_host.c_str(), strerror(errno));
                                return STATUS_ERROR;
                        }
                        }
                }

                m_ssl_state = SSL_STATE_CONNECTED;

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

                m_ssl_state = SSL_STATE_SSL_CONNECTING;

                // -------------------------------------------
                // Add to event handler
                // -------------------------------------------
                if (0 != a_evr_loop->mod_fd(m_fd,
                                            EVR_FILE_ATTR_MASK_READ|EVR_FILE_ATTR_MASK_STATUS_ERROR,
                                            this))
                {
                        NCONN_ERROR("HOST[%s]: Error: Couldn't add socket file descriptor", m_host.c_str());
                        return STATUS_ERROR;
                }

                goto state_top;
        }

        // -------------------------------------------------
        // STATE: SSL_CONNECTING
        // -------------------------------------------------
        case SSL_STATE_SSL_CONNECTING:
        case SSL_STATE_SSL_CONNECTING_WANT_WRITE:
        case SSL_STATE_SSL_CONNECTING_WANT_READ:
        {
                int l_status;
                //NDBG_PRINT("%sSSL_CONNECTING%s\n", ANSI_COLOR_BG_RED, ANSI_COLOR_OFF);
                l_status = ssl_connect(a_host_info);
                if(EAGAIN == l_status)
                {
                        if(SSL_STATE_SSL_CONNECTING_WANT_READ == m_ssl_state)
                        {
                                if (0 != a_evr_loop->mod_fd(m_fd,
                                                            EVR_FILE_ATTR_MASK_READ|EVR_FILE_ATTR_MASK_STATUS_ERROR,
                                                            this))
                                {
                                        NCONN_ERROR("HOST[%s]: Error: Couldn't add socket file descriptor", m_host.c_str());
                                        return STATUS_ERROR;
                                }
                        }
                        else if(SSL_STATE_SSL_CONNECTING_WANT_WRITE == m_ssl_state)
                        {
                                if (0 != a_evr_loop->mod_fd(m_fd,
                                                            EVR_FILE_ATTR_MASK_WRITE|EVR_FILE_ATTR_MASK_STATUS_ERROR,
                                                            this))
                                {
                                        NCONN_ERROR("HOST[%s]: Error: Couldn't add socket file descriptor", m_host.c_str());
                                        return STATUS_ERROR;
                                }
                        }
                        return STATUS_OK;
                }
                else if(STATUS_OK != l_status)
                {
                        return STATUS_ERROR;
                }

                // -------------------------------------------
                // Add to event handler
                // -------------------------------------------
                if (0 != a_evr_loop->mod_fd(m_fd,
                                            EVR_FILE_ATTR_MASK_READ|EVR_FILE_ATTR_MASK_STATUS_ERROR,
                                            this))
                {
                        NCONN_ERROR("HOST[%s]: Error: Couldn't add socket file descriptor", m_host.c_str());
                        return STATUS_ERROR;
                }

                goto state_top;
        }

        // -------------------------------------------------
        // STATE: CONNECTED
        // -------------------------------------------------
        case SSL_STATE_CONNECTED:
        {

#if 0
                SSL_SESSION *m_ssl_session = SSL_get_session(m_ssl);
                SSL_SESSION_print_fp(stdout, m_ssl_session);

                // TODO REMOVE
                X509* l_cert = NULL;
                l_cert = SSL_get_peer_certificate(m_ssl);
                if(NULL == l_cert)
                {
                        NDBG_PRINT("SSL_get_peer_certificate error.  ssl: %p\n", m_ssl);
                        return STATUS_ERROR;
                }
                X509_print_fp(stdout, l_cert);
#endif

                // -----------------------------------------
                // Store protocol and cipher
                // -----------------------------------------
                // SSL-Session:
                //    Protocol  : TLSv1
                //    Cipher    : ECDHE-RSA-AES256-SHA
                // -----------------------------------------
                // TODO Only store in some mode???
                // might be slow
                if(1)
                {
                        reqlet *l_reqlet = static_cast<reqlet *>(m_data1);
                        if(l_reqlet)
                        {
                                // Get protocol
                                std::string l_protocol;
                                std::string l_cipher;
                                int32_t l_status;
                                l_status = get_ssl_session_info(m_ssl, l_protocol, l_cipher);
                                if(l_status != STATUS_OK)
                                {
                                        // do nothing
                                }
                                l_reqlet->m_conn_info["Protocol"] = l_protocol;
                                l_reqlet->m_conn_info["Cipher"] = l_cipher;
                        }
                }

                if(m_ssl_opt_verify)
                {
                        int32_t l_status = 0;
                        // Do verify
                        char *l_hostname = NULL;
                        l_status = validate_server_certificate(l_hostname, (!m_ssl_opt_verify_allow_self_signed));
                        if(l_status != STATUS_OK)
                        {
                                return STATUS_ERROR;
                        }
                }

                // -------------------------------------------
                // Send request
                // -------------------------------------------
        	if(!m_connect_only)
        	{
			int32_t l_request_status = STATUS_OK;
			//NDBG_PRINT("%sSEND_REQUEST%s\n", ANSI_COLOR_BG_CYAN, ANSI_COLOR_OFF);
			l_request_status = send_request(false);
			if(l_request_status != STATUS_OK)
			{
			        NCONN_ERROR("HOST[%s]: Error: performing send_request", m_host.c_str());
				return STATUS_ERROR;
			}
        	}
        	// connect only -we outtie!
        	else
        	{
        		m_ssl_state = SSL_STATE_DONE;
        	}
                break;
        }

        // -------------------------------------------------
        // STATE: READING
        // -------------------------------------------------
        case SSL_STATE_READING:
        {
                int l_read_status = 0;
                //NDBG_PRINT("%sCNST_READING%s\n", ANSI_COLOR_BG_CYAN, ANSI_COLOR_OFF);
                l_read_status = receive_response();
                //NDBG_PRINT("%sCNST_READING%s: receive_response(): total: %d\n", ANSI_COLOR_BG_CYAN, ANSI_COLOR_OFF, (int)m_stat.m_total_bytes);
                if(l_read_status < 0)
                {
                        NCONN_ERROR("HOST[%s]: Error: performing receive_response", m_host.c_str());
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
int32_t nconn_ssl::set_opt(uint32_t a_opt, const void *a_buf, uint32_t a_len)
{

        //NDBG_PRINT("HERE: a_opt: %d a_buf: %p\n", a_opt, a_buf);

        // TODO RUN SUPER
        int32_t l_status;
        l_status = nconn_tcp::set_opt(a_opt, a_buf, a_len);
        if((l_status != STATUS_OK) &&
           (l_status != m_opt_unhandled))
        {
                return STATUS_ERROR;
        }
        if(l_status == STATUS_OK)
        {
                return STATUS_OK;
        }

        switch(a_opt)
        {
        case OPT_SSL_CIPHER_STR:
        {
                m_ssl_opt_cipher_str.assign((char *)a_buf, a_len);
                break;
        }
        case OPT_SSL_OPTIONS:
        {
                memcpy(&m_ssl_opt_options, a_buf, sizeof(long));
                break;
        }
        case OPT_SSL_VERIFY:
        {
                memcpy(&m_ssl_opt_verify, a_buf, sizeof(bool));
                break;
        }
        case OPT_SSL_VERIFY_ALLOW_SELF_SIGNED:
        {
                memcpy(&m_ssl_opt_verify_allow_self_signed, a_buf, sizeof(bool));
                break;
        }
        case OPT_SSL_CA_FILE:
        {
                m_ssl_opt_ca_file.assign((char *)a_buf, a_len);
                break;
        }
        case OPT_SSL_CA_PATH:
        {
                m_ssl_opt_ca_path.assign((char *)a_buf, a_len);
                break;
        }
        case OPT_SSL_CTX:
        {
                m_ssl_ctx = (SSL_CTX *)a_buf;
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
int32_t nconn_ssl::get_opt(uint32_t a_opt, void **a_buf, uint32_t *a_len)
{

        // TODO RUN SUPER
        int32_t l_status;
        l_status = nconn_tcp::get_opt(a_opt, a_buf, a_len);
        if((l_status != STATUS_OK) &&
           (l_status != m_opt_unhandled))
        {
                return STATUS_ERROR;
        }
        if(l_status == STATUS_OK)
        {
                return STATUS_OK;
        }

        switch(a_opt)
        {
        default:
        {
                //NDBG_PRINT("Error unsupported option: %d\n", a_opt);
                return m_opt_unhandled;
        }
        }

        return STATUS_OK;
}

// -------------------------------------------
// Check host name
// Based on example from:
// "Network Security with OpenSSL" pg. 135-136
// Returns 0 on Success, -1 on Failure
// -------------------------------------------
static int validate_server_certificate_hostname(X509* a_cert, const char* a_host)
{
        typedef std::vector <std::string> cert_name_list_t;
        cert_name_list_t l_cert_name_list;
        bool l_get_ids_status = false;

        l_get_ids_status = ssl_x509_get_ids(a_cert, l_cert_name_list);
        if(!l_get_ids_status)
        {
                // No names found bail out
                NDBG_PRINT("HOST[%s]: ssl_x509_get_ids returned no names.\n", a_host);
                return -1;
        }

        for(uint32_t i_name = 0; i_name < l_cert_name_list.size(); ++i_name)
        {
                if(Curl_cert_hostcheck(l_cert_name_list[i_name].c_str(), a_host))
                {
                        return 0;
                }
        }

        NDBG_PRINT("HOST[%s]: Error Hostname match failed.\n", a_host);
        return -1;

}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: \notes:   Based on example from "Network Security with OpenSSL" pg. 132
//: ----------------------------------------------------------------------------
int32_t nconn_ssl::validate_server_certificate(const char* a_host, bool a_disallow_self_signed)
{
        X509* l_cert = NULL;

        // Get certificate
        l_cert = SSL_get_peer_certificate(m_ssl);
        if(NULL == l_cert)
        {
                NCONN_ERROR("HOST[%s]: SSL_get_peer_certificate error.  ssl: %p", a_host, m_ssl);
                return STATUS_ERROR;
        }

        // Example of displaying cert
        //X509_print_fp(stdout, l_cert);

        // Check host name
        if(a_host)
        {
                int l_status = 0;
                l_status = validate_server_certificate_hostname(l_cert, a_host);
                if(0 != l_status)
                {
                        if(NULL != l_cert)
                        {
                                X509_free(l_cert);
                                l_cert = NULL;
                        }
                        return STATUS_ERROR;
                }
        }

        if(NULL != l_cert)
        {
                X509_free(l_cert);
                l_cert = NULL;
        }

        long l_ssl_verify_result;
        l_ssl_verify_result = SSL_get_verify_result(m_ssl);
        if(X509_V_OK != l_ssl_verify_result)
        {

                // Check for self-signed failures
                //a_disallow_self_signed
                if(false == a_disallow_self_signed)
                {
                        if ((l_ssl_verify_result == X509_V_ERR_DEPTH_ZERO_SELF_SIGNED_CERT) ||
                            (l_ssl_verify_result == X509_V_ERR_SELF_SIGNED_CERT_IN_CHAIN))
                        {
                                // No errors return success(0)
                                if(NULL != l_cert)
                                {
                                        X509_free(l_cert);
                                        l_cert = NULL;
                                }
                                return 0;
                        }
                }

                NCONN_ERROR("HOST[%s]: SSL_get_verify_result[%ld]: %s",
                      a_host,
                      l_ssl_verify_result,
                      X509_verify_cert_error_string(l_ssl_verify_result));
                if(NULL != l_cert)
                {
                        X509_free(l_cert);
                        l_cert = NULL;
                }
                return STATUS_ERROR;
        }

        // No errors return success(0)
        return STATUS_OK;

}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: \notes:   Based on example from "Network Security with OpenSSL" pg. 132
//: ----------------------------------------------------------------------------
int ssl_cert_verify_callback_allow_self_signed(int ok, X509_STORE_CTX* store)
{
        if (!ok)
        {
                if(store)
                {
                        // TODO Can add check for depth here.
                        //int depth = X509_STORE_CTX_get_error_depth(store);

                        int err = X509_STORE_CTX_get_error(store);
                        if ((err == X509_V_ERR_DEPTH_ZERO_SELF_SIGNED_CERT) ||
                            (err == X509_V_ERR_SELF_SIGNED_CERT_IN_CHAIN))
                        {
                                // Return success despite self-signed
                                return 1;
                        }
                        else
                        {
                                sprintf(gts_last_ssl_error, "ssl_cert_verify_callback_allow_self_signed Error[%d].  Reason: %s",
                                      err, X509_verify_cert_error_string(err));
                                //NDBG_PRINT("ssl_cert_verify_callback_allow_self_signed Error[%d].  Reason: %s\n",
                                //      err, X509_verify_cert_error_string(err));
                        }
                }
        }
        return ok;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: \notes:   Based on example from "Network Security with OpenSSL" pg. 132
//: ----------------------------------------------------------------------------
int ssl_cert_verify_callback(int ok, X509_STORE_CTX* store)
{
        if (!ok)
        {
                if(store)
                {
                        // TODO Can add check for depth here.
                        //int depth = X509_STORE_CTX_get_error_depth(store);
                        int err = X509_STORE_CTX_get_error(store);
                        sprintf(gts_last_ssl_error, "ssl_cert_verify_callback Error[%d].  Reason: %s",
                              err, X509_verify_cert_error_string(err));
                        //NDBG_PRINT("ssl_cert_verify_callback Error[%d].  Reason: %s\n",
                        //      err, X509_verify_cert_error_string(err));
                }
        }
        return ok;
}


//: ----------------------------------------------------------------------------
//: \details: Return an array of (RFC 6125 coined) DNS-IDs and CN-IDs in a x509
//:           certificate
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
bool ssl_x509_get_ids(X509* x509, std::vector<std::string>& ids)
{
        if (!x509)
                return false;

        // First, the DNS-IDs (dNSName entries in the subjectAltName extension)
        GENERAL_NAMES* names =
                (GENERAL_NAMES*)X509_get_ext_d2i(x509, NID_subject_alt_name, NULL, NULL);
        if (names)
        {
                std::string san;
                for (int i = 0; i < sk_GENERAL_NAME_num(names); i++)
                {
                        GENERAL_NAME* name = sk_GENERAL_NAME_value(names, i);

                        if (name->type == GEN_DNS)
                        {
                                san.assign(reinterpret_cast<char*>(ASN1_STRING_data(name->d.uniformResourceIdentifier)),
                                           ASN1_STRING_length(name->d.uniformResourceIdentifier));
                                if (!san.empty())
                                        ids.push_back(san);
                        }
                }
        }

        if (names)
                sk_GENERAL_NAME_pop_free(names, GENERAL_NAME_free);

        // Second, the CN-IDs (commonName attributes in the subject DN)
        X509_NAME* subj = X509_get_subject_name(x509);
        int i = -1;
        while ((i = X509_NAME_get_index_by_NID(subj, NID_commonName, i)) != -1)
        {
                ASN1_STRING* name = X509_NAME_ENTRY_get_data(X509_NAME_get_entry(subj, i));

                std::string dn(reinterpret_cast<char*>(ASN1_STRING_data(name)),
                               ASN1_STRING_length(name));
                if (!dn.empty())
                        ids.push_back(dn);
        }

        return ids.empty() ? false : true;
}

} //namespace ns_hlx {


