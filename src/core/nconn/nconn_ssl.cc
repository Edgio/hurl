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
#include "time_util.h"
#include "evr.h"
#include "hostcheck.h"
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
int32_t nconn_ssl::init(void)
{

        //NDBG_PRINT("INIT'ing: m_ssl_ctx: %p\n", m_ssl_ctx);

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
                        //return NC_STATUS_ERROR;
                }
        }

        if (!m_ssl_opt_cipher_str.empty())
        {
                if (1 != SSL_set_cipher_list(m_ssl, m_ssl_opt_cipher_str.c_str()))
                {
                        NCONN_ERROR("HOST[%s]: Failed to set ssl cipher list: %s\n", m_host.c_str(), m_ssl_opt_cipher_str.c_str());
                        return NC_STATUS_ERROR;
                }
        }

        // Set tls sni extension
        if (!m_ssl_opt_tlsext_hostname.empty())
        {
                // const_cast to work around SSL's use of arg -this call does not change buffer argument
                if (1 != SSL_set_tlsext_host_name(m_ssl, m_ssl_opt_tlsext_hostname.c_str()))
                {
                        NCONN_ERROR("HOST[%s]: Failed to set tls hostname: %s\n", m_host.c_str(), m_ssl_opt_tlsext_hostname.c_str());
                        return NC_STATUS_ERROR;
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

        return NC_STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t nconn_ssl::ssl_connect(void)
{
        int l_status;
        m_ssl_state = SSL_STATE_SSL_CONNECTING;
        //NDBG_PRINT("Connect: %s\n", m_host.c_str());
        l_status = SSL_connect(m_ssl);
        //NDBG_PRINT("Connect: %d\n", l_status);
        if (l_status <= 0)
        {
                //NDBG_PRINT("SSL connection failed - %d\n", l_status);
                //NDBG_PRINT("Showing error.\n");
                int l_ssl_error = SSL_get_error(m_ssl, l_status);
                //NDBG_PRINT("l_ssl_error: %d.\n", l_ssl_error);
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
                                //NDBG_PRINT("HOST[%s]: SSL_ERROR_SSL %lu: %s. Reason: %s\n",
                                //                m_host.c_str(),
                                //                ERR_get_error(),
                                //                ERR_error_string(ERR_get_error(),NULL),
                                //                gts_last_ssl_error);

                                // Set back
                                gts_last_ssl_error[0] = '\0';
                        }
                        else
                        {
                                NCONN_ERROR("HOST[%s]: SSL_ERROR_SSL %lu: %s.",
                                                m_host.c_str(),
                                                ERR_get_error(),
                                                ERR_error_string(ERR_get_error(),NULL));
                                //NDBG_PRINT("HOST[%s]: SSL_ERROR_SSL %lu: %s.\n",
                                //                m_host.c_str(),
                                //                ERR_get_error(),
                                //                ERR_error_string(ERR_get_error(),NULL));
                        }

                        break;
                }
                case SSL_ERROR_WANT_READ:
                {
                        //NDBG_PRINT("%sSSL_CON%s[%3d]: SSL_ERROR_WANT_READ\n", ANSI_COLOR_BG_GREEN, ANSI_COLOR_OFF, m_fd);
                        m_ssl_state = SSL_STATE_SSL_CONNECTING_WANT_READ;
                        return NC_STATUS_AGAIN;

                }
                case SSL_ERROR_WANT_WRITE:
                {
                        //NDBG_PRINT("%sSSL_CON%s[%3d]: SSL_STATE_SSL_CONNECTING_WANT_WRITE\n", ANSI_COLOR_BG_GREEN, ANSI_COLOR_OFF, m_fd);
                        m_ssl_state = SSL_STATE_SSL_CONNECTING_WANT_WRITE;
                        return NC_STATUS_AGAIN;
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
                return NC_STATUS_ERROR;
        }
        else if(l_status == 1)
        {
                //NDBG_PRINT("CONNECTED\n");
                m_ssl_state = SSL_STATE_CONNECTED;
        }

        //did_connect = 1;

        return NC_STATUS_OK;

}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t nconn_ssl::ssl_accept(void)
{
        int l_status;
        m_ssl_state = SSL_STATE_SSL_ACCEPTING;
        l_status = SSL_accept(m_ssl);
        if (l_status <= 0)
        {
                //NDBG_PRINT("SSL connection failed - %d\n", l_status);
                int l_ssl_error = SSL_get_error(m_ssl, l_status);
                //NDBG_PRINT("Showing error: %d.\n", l_ssl_error);
                switch(l_ssl_error) {
                case SSL_ERROR_SSL:
                {
                        // TODO REMOVE
                        NDBG_PRINT("HOST[%s]: SSL_ERROR_SSL %lu: %s.\n",
                                        m_host.c_str(),
                                        ERR_get_error(),
                                        ERR_error_string(ERR_get_error(),NULL));
                        if(gts_last_ssl_error[0] != '\0')
                        {
                                NCONN_ERROR("HOST[%s]: SSL_ERROR_SSL %lu: %s. Reason: %s\n",
                                                m_host.c_str(),
                                                ERR_get_error(),
                                                ERR_error_string(ERR_get_error(),NULL),
                                                gts_last_ssl_error);
                                // Set back
                                gts_last_ssl_error[0] = '\0';
                        }
                        else
                        {
                                NCONN_ERROR("HOST[%s]: SSL_ERROR_SSL %lu: %s.\n",
                                                m_host.c_str(),
                                                ERR_get_error(),
                                                ERR_error_string(ERR_get_error(),NULL));
                        }
                        break;
                }
                case SSL_ERROR_WANT_READ:
                {
                        //NDBG_PRINT("%sSSL_CON%s[%3d]: SSL_ERROR_WANT_READ\n", ANSI_COLOR_BG_GREEN, ANSI_COLOR_OFF, m_fd);
                        m_ssl_state = SSL_STATE_SSL_ACCEPTING_WANT_READ;
                        return NC_STATUS_AGAIN;
                }
                case SSL_ERROR_WANT_WRITE:
                {
                        //NDBG_PRINT("%sSSL_CON%s[%3d]: SSL_STATE_SSL_CONNECTING_WANT_WRITE\n", ANSI_COLOR_BG_GREEN, ANSI_COLOR_OFF, m_fd);
                        m_ssl_state = SSL_STATE_SSL_ACCEPTING_WANT_WRITE;
                        return NC_STATUS_AGAIN;
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
                return NC_STATUS_ERROR;
        }
        else if(1 == l_status)
        {
                m_ssl_state = SSL_STATE_CONNECTED;
        }

        //did_connect = 1;

        return NC_STATUS_OK;
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
        if((l_status != NC_STATUS_OK) && (l_status != NC_STATUS_UNSUPPORTED))
        {
                return NC_STATUS_ERROR;
        }
        if(l_status == NC_STATUS_OK)
        {
                return NC_STATUS_OK;
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
        case OPT_SSL_TLS_KEY:
        {
                m_tls_key.assign((char *)a_buf, a_len);
                break;
        }
        case OPT_SSL_TLS_CRT:
        {
                m_tls_crt.assign((char *)a_buf, a_len);
                break;
        }
        case OPT_SSL_CTX:
        {
                m_ssl_ctx = (SSL_CTX *)a_buf;
                break;
        }
        default:
        {
                NDBG_PRINT("Error unsupported option: %d\n", a_opt);
                return NC_STATUS_UNSUPPORTED;
        }
        }

        return NC_STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t nconn_ssl::get_opt(uint32_t a_opt, void **a_buf, uint32_t *a_len)
{
        int32_t l_status;
        l_status = nconn_tcp::get_opt(a_opt, a_buf, a_len);
        if((l_status != NC_STATUS_OK) && (l_status != NC_STATUS_UNSUPPORTED))
        {
                return NC_STATUS_ERROR;
        }
        if(l_status == NC_STATUS_OK)
        {
                return NC_STATUS_OK;
        }

        switch(a_opt)
        {
        case OPT_SSL_TLS_KEY:
        {
                // TODO
                break;
        }
        case OPT_SSL_TLS_CRT:
        {
                // TODO
                break;
        }
        case OPT_SSL_INFO_CIPHER_STR:
        {
                *a_buf = (void *)m_ssl_info_cipher_str;
                break;
        }
        case OPT_SSL_INFO_PROTOCOL_STR:
        {
                *a_buf = (void *)m_ssl_info_protocol_str;
                break;
        }
        default:
        {
                //NDBG_PRINT("Error unsupported option: %d\n", a_opt);
                return NC_STATUS_ERROR;
        }
        }

        return NC_STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t nconn_ssl::ncset_listening(evr_loop *a_evr_loop, int32_t a_val)
{
        int32_t l_status;
        l_status = nconn_tcp::ncset_listening(a_evr_loop, a_val);
        if(l_status != NC_STATUS_OK)
        {
                NDBG_PRINT("Error performing nconn_tcp::ncset_listening.\n");
                return NC_STATUS_ERROR;
        }
        m_ssl_state = SSL_STATE_LISTENING;
        return NC_STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t nconn_ssl::ncset_accepting(evr_loop *a_evr_loop, int a_fd)
{
        int32_t l_status;
        l_status = nconn_tcp::ncset_accepting(a_evr_loop, a_fd);
        if(l_status != NC_STATUS_OK)
        {
                return NC_STATUS_ERROR;
        }

        // setup ssl
        init();

        // set connection socket to SSL state
        SSL_set_fd(m_ssl, a_fd);

        // set to accepting state
        m_ssl_state = SSL_STATE_ACCEPTING;

        // Add to event handler
        if (0 != a_evr_loop->mod_fd(m_fd,
                                    EVR_FILE_ATTR_MASK_READ|EVR_FILE_ATTR_MASK_RD_HUP|EVR_FILE_ATTR_MASK_WRITE|EVR_FILE_ATTR_MASK_ET,
                                    this))
        {
                NDBG_PRINT("Error: Couldn't add socket file descriptor\n");
                return NC_STATUS_ERROR;
        }

        return NC_STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t nconn_ssl::ncread(char *a_buf, uint32_t a_buf_len)
{
        ssize_t l_status;
        int32_t l_bytes_read = 0;

        l_status = SSL_read(m_ssl, a_buf, a_buf_len);
        //NDBG_PRINT("%sHOST%s: %s ssl[%p] READ: %ld bytes. Reason: %s\n",
        //                ANSI_COLOR_FG_RED, ANSI_COLOR_OFF,
        //                m_host.c_str(),
        //                m_ssl,
        //                l_status,
        //                strerror(errno));
        if(l_status < 0)
        {
                int l_ssl_error = SSL_get_error(m_ssl, l_status);
                //NDBG_PRINT("%sSSL_READ%s[%3d] l_bytes_read: %d error: %d\n",
                //                ANSI_COLOR_BG_RED, ANSI_COLOR_OFF,
                //                SSL_get_fd(m_ssl), l_bytes_read,
                //                l_ssl_error);
                if(l_ssl_error == SSL_ERROR_WANT_READ)
                {
                        return NC_STATUS_AGAIN;
                }
        }
        else
        {
                l_bytes_read += l_status;
        }
        if(l_status > 0)
        {
                return l_bytes_read;
        }
        if(l_status == 0)
        {
                return NC_STATUS_EOF;
        }

        // TODO Translate status...
        switch(errno)
        {
                case EAGAIN:
                {
                        return NC_STATUS_AGAIN;
                }
                default:
                {
                        return NC_STATUS_ERROR;
                }
        }

        return NC_STATUS_OK;

}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t nconn_ssl::ncwrite(char *a_buf, uint32_t a_buf_len)
{
        int l_status;
        l_status = SSL_write(m_ssl, a_buf, a_buf_len);
        //NDBG_PRINT("%sHOST%s: %s ssl[%p] WRITE: %d bytes. Reason: %s\n",
        //                ANSI_COLOR_FG_CYAN, ANSI_COLOR_OFF,
        //                m_host.c_str(),
        //                m_ssl,
        //                l_status,
        //                strerror(errno));
        if(l_status < 0)
        {
                NCONN_ERROR("HOST[%s]: Error: performing SSL_write.\n", m_host.c_str());
                return NC_STATUS_ERROR;
        }
        return l_status;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t nconn_ssl::ncsetup(evr_loop *a_evr_loop)
{
        int32_t l_status;
        //NDBG_PRINT("m_ssl_ctx: %p\n", m_ssl_ctx);
        l_status = nconn_tcp::ncsetup(a_evr_loop);
        if(l_status != NC_STATUS_OK)
        {
                return NC_STATUS_ERROR;
        }

        //NDBG_PRINT("m_ssl_ctx: %p\n", m_ssl_ctx);

        l_status = init();
        if(l_status != NC_STATUS_OK)
        {
                return NC_STATUS_ERROR;
        }

        // TODO Stats???
        //if(m_collect_stats_flag)
        //{
        //        m_connect_start_time_us = get_time_us();
        //}

        m_ssl_state = SSL_STATE_CONNECTING;

        return NC_STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t nconn_ssl::ncaccept(evr_loop *a_evr_loop)
{
        //NDBG_PRINT("%sSSL_ACCEPT%s: STATE[%d] --START\n",
        //                ANSI_COLOR_BG_BLUE, ANSI_COLOR_OFF, m_ssl_state);
ncaccept_state_top:
        //NDBG_PRINT("%sSSL_ACCEPT%s: STATE[%d]\n",
        //                ANSI_COLOR_BG_BLUE, ANSI_COLOR_OFF, m_ssl_state);
        switch (m_ssl_state)
        {
        // -------------------------------------------------
        // STATE: LISTENING
        // -------------------------------------------------
        case SSL_STATE_LISTENING:
        {
                int32_t l_status;
                l_status = nconn_tcp::ncaccept(a_evr_loop);
                return l_status;
        }
        // -------------------------------------------------
        // STATE: SSL_ACCEPTING
        // -------------------------------------------------
        case SSL_STATE_ACCEPTING:
        case SSL_STATE_SSL_ACCEPTING:
        case SSL_STATE_SSL_ACCEPTING_WANT_READ:
        case SSL_STATE_SSL_ACCEPTING_WANT_WRITE:
        {
                int l_status;
                l_status = ssl_accept();
                if(l_status == NC_STATUS_AGAIN)
                {
                        if(SSL_STATE_SSL_ACCEPTING_WANT_READ == m_ssl_state)
                        {
                                if (0 != a_evr_loop->mod_fd(m_fd,
                                                            EVR_FILE_ATTR_MASK_READ|EVR_FILE_ATTR_MASK_RD_HUP|EVR_FILE_ATTR_MASK_ET,
                                                            this))
                                {
                                        NCONN_ERROR("HOST[%s]: Error: Couldn't add socket file descriptor", m_host.c_str());
                                        return NC_STATUS_ERROR;
                                }
                        }
                        else if(SSL_STATE_SSL_ACCEPTING_WANT_WRITE == m_ssl_state)
                        {
                                if (0 != a_evr_loop->mod_fd(m_fd,
                                                            EVR_FILE_ATTR_MASK_WRITE|EVR_FILE_ATTR_MASK_ET,
                                                            this))
                                {
                                        NCONN_ERROR("HOST[%s]: Error: Couldn't add socket file descriptor", m_host.c_str());
                                        return NC_STATUS_ERROR;
                                }
                        }
                        return NC_STATUS_OK;
                }
                else if(l_status != NC_STATUS_OK)
                {
                        return NC_STATUS_ERROR;
                }

                // -------------------------------------------
                // Add to event handler
                // -------------------------------------------
                if (0 != a_evr_loop->mod_fd(m_fd,
                                            EVR_FILE_ATTR_MASK_READ|EVR_FILE_ATTR_MASK_RD_HUP|EVR_FILE_ATTR_MASK_ET,
                                            this))
                {
                        NCONN_ERROR("HOST[%s]: Error: Couldn't add socket file descriptor", m_host.c_str());
                        return NC_STATUS_ERROR;
                }
                goto ncaccept_state_top;
        }
        // -------------------------------------------------
        // STATE: CONNECTED
        // -------------------------------------------------
        case SSL_STATE_CONNECTED:
        {
                //...
                break;
        }
        // -------------------------------------------------
        // STATE: ???
        // -------------------------------------------------
        default:
        {
                NDBG_PRINT("State error: %d\n", m_ssl_state);
                return NC_STATUS_ERROR;
        }
        }

        return NC_STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t nconn_ssl::ncconnect(evr_loop *a_evr_loop)
{
        //NDBG_PRINT("%sSSL_CONNECT%s: STATE[%d] --START\n",
        //                ANSI_COLOR_BG_BLUE, ANSI_COLOR_OFF, m_ssl_state);
ncconnect_state_top:
        //NDBG_PRINT("%sSSL_CONNECT%s: STATE[%d]\n",
        //                ANSI_COLOR_BG_BLUE, ANSI_COLOR_OFF, m_ssl_state);
        switch (m_ssl_state)
        {
        // -------------------------------------------------
        // STATE: CONNECTING
        // -------------------------------------------------
        case SSL_STATE_CONNECTING:
        {
                int32_t l_status;
                l_status = nconn_tcp::ncconnect(a_evr_loop);
                if(l_status == NC_STATUS_ERROR)
                {
                        return NC_STATUS_ERROR;
                }

                if(nconn_tcp::is_connecting())
                {
                        //NDBG_PRINT("Still connecting...\n");
                        return NC_STATUS_OK;
                }

                m_ssl_state = SSL_STATE_SSL_CONNECTING;
                goto ncconnect_state_top;
        }
        // -------------------------------------------------
        // STATE: SSL_CONNECTING
        // -------------------------------------------------
        case SSL_STATE_SSL_CONNECTING:
        case SSL_STATE_SSL_CONNECTING_WANT_READ:
        case SSL_STATE_SSL_CONNECTING_WANT_WRITE:
        {
                int l_status;
                l_status = ssl_connect();
                //NDBG_PRINT("%sSSL_CONNECTING%s status = %d m_ssl_state = %d\n", ANSI_COLOR_BG_RED, ANSI_COLOR_OFF, l_status, m_ssl_state);
                if(l_status == NC_STATUS_AGAIN)
                {
                        if(SSL_STATE_SSL_CONNECTING_WANT_READ == m_ssl_state)
                        {
                                if (0 != a_evr_loop->mod_fd(m_fd,
                                                            EVR_FILE_ATTR_MASK_READ|EVR_FILE_ATTR_MASK_RD_HUP|EVR_FILE_ATTR_MASK_ET,
                                                            this))
                                {
                                        NCONN_ERROR("HOST[%s]: Error: Couldn't add socket file descriptor", m_host.c_str());
                                        return NC_STATUS_ERROR;
                                }
                        }
                        else if(SSL_STATE_SSL_CONNECTING_WANT_WRITE == m_ssl_state)
                        {
                                if (0 != a_evr_loop->mod_fd(m_fd,
                                                            EVR_FILE_ATTR_MASK_WRITE|EVR_FILE_ATTR_MASK_ET,
                                                            this))
                                {
                                        NCONN_ERROR("HOST[%s]: Error: Couldn't add socket file descriptor", m_host.c_str());
                                        return NC_STATUS_ERROR;
                                }
                        }
                        return NC_STATUS_OK;
                }
                else if(l_status != NC_STATUS_OK)
                {
                        return NC_STATUS_ERROR;
                }

                // -------------------------------------------
                // Add to event handler
                // -------------------------------------------
                if (0 != a_evr_loop->mod_fd(m_fd,
                                            EVR_FILE_ATTR_MASK_READ|EVR_FILE_ATTR_MASK_RD_HUP|EVR_FILE_ATTR_MASK_ET,
                                            this))
                {
                        NCONN_ERROR("HOST[%s]: Error: Couldn't add socket file descriptor", m_host.c_str());
                        return NC_STATUS_ERROR;
                }
                goto ncconnect_state_top;
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
                        return NC_STATUS_ERROR;
                }
                X509_print_fp(stdout, l_cert);
#endif

                int32_t l_protocol_num = get_ssl_info_protocol_num(m_ssl);
                m_ssl_info_cipher_str = get_ssl_info_cipher_str(m_ssl);
                m_ssl_info_protocol_str = get_ssl_info_protocol_str(l_protocol_num);

                //NDBG_PRINT("m_ssl_info_cipher_str:   %s\n", m_ssl_info_cipher_str);
                //NDBG_PRINT("m_ssl_info_protocol_str: %s\n", m_ssl_info_protocol_str);

                if(m_ssl_opt_verify)
                {
                        int32_t l_status = 0;
                        // Do verify
                        char *l_hostname = NULL;
                        l_status = validate_server_certificate(l_hostname, (!m_ssl_opt_verify_allow_self_signed));
                        if(l_status != NC_STATUS_OK)
                        {
                                return NC_STATUS_ERROR;
                        }
                }
                break;
        }
        // -------------------------------------------------
        // STATE: ???
        // -------------------------------------------------
        default:
        {
                NDBG_PRINT("State error: %d\n", m_ssl_state);
                return NC_STATUS_ERROR;
        }
        }

        return NC_STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t nconn_ssl::nccleanup(void)
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
        nconn_tcp::nccleanup();

        return NC_STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: Check host name
//: Based on example from:
//: "Network Security with OpenSSL" pg. 135-136
//: Returns 0 on Success, -1 on Failure
//: ----------------------------------------------------------------------------
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
                NCONN_ERROR("HOST[%s]: SSL_get_peer_certificate error.  ssl: %p", a_host, (void *)m_ssl);
                return NC_STATUS_ERROR;
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
                        return NC_STATUS_ERROR;
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
                                return NC_STATUS_ERROR;
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
                return NC_STATUS_ERROR;
        }

        // No errors return success(0)
        return NC_STATUS_OK;

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
