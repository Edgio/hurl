//: ----------------------------------------------------------------------------
//: Copyright (C) 2014 Verizon.  All Rights Reserved.
//: All Rights Reserved
//:
//: \file:    nconn_tls.cc
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
#include "nconn_tls.h"
#include "time_util.h"
#include "evr.h"
#include "tls_util.h"
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
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t nconn_tls::init(void)
{

        //NDBG_PRINT("INIT'ing: m_tls_ctx: %p\n", m_tls_ctx);

        // Create TLS Context
        m_tls = SSL_new(m_tls_ctx);
        // TODO Check for NULL

        SSL_set_fd(m_tls, m_fd);
        // TODO Check for Errors

        const long l_tls_options = m_tls_opt_options;

        //NDBG_PRINT("l_tls_options: 0x%08lX\n", l_tls_options);

        if (l_tls_options)
        {
                // clear all options and set specified options
                SSL_clear_options(m_tls, 0x11111111L);
                long l_result = SSL_set_options(m_tls, l_tls_options);
                if (l_tls_options != l_result)
                {
                        //NDBG_PRINT("Failed to set tls options: 0x%08lX -set to: 0x%08lX \n", l_result, l_tls_options);
                        //return NC_STATUS_ERROR;
                }
        }

        if (!m_tls_opt_cipher_str.empty())
        {
                if (1 != SSL_set_cipher_list(m_tls, m_tls_opt_cipher_str.c_str()))
                {
                        NCONN_ERROR("LABEL[%s]: Failed to set tls cipher list: %s\n", m_label.c_str(), m_tls_opt_cipher_str.c_str());
                        return NC_STATUS_ERROR;
                }
        }

        // Set tls sni extension
        if (!m_tls_opt_tlsext_hostname.empty())
        {
                // const_cast to work around SSL's use of arg -this call does not change buffer argument
                if (1 != SSL_set_tlsext_host_name(m_tls, m_tls_opt_tlsext_hostname.c_str()))
                {
                        NCONN_ERROR("LABEL[%s]: Failed to set tls hostname: %s\n", m_label.c_str(), m_tls_opt_tlsext_hostname.c_str());
                        return NC_STATUS_ERROR;
                }
        }

        // Set tls Cert verify callback ...
        if (m_tls_opt_verify)
        {
                if (m_tls_opt_verify_allow_self_signed)
                {
                        SSL_set_verify(m_tls, SSL_VERIFY_PEER, tls_cert_verify_callback_allow_self_signed);
                }
                else
                {
                        SSL_set_verify(m_tls, SSL_VERIFY_PEER, tls_cert_verify_callback);
                }
        }

        return NC_STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t nconn_tls::tls_connect(void)
{
        int l_status;
        m_tls_state = TLS_STATE_TLS_CONNECTING;
        //NDBG_PRINT("Connect: %s\n", m_label.c_str());
        l_status = SSL_connect(m_tls);
        //NDBG_PRINT("Connect: %d\n", l_status);
        if (l_status <= 0)
        {
                //NDBG_PRINT("SSL connection failed - %d\n", l_status);
                //NDBG_PRINT("Showing error.\n");
                int l_tls_error = SSL_get_error(m_tls, l_status);
                //NDBG_PRINT("l_tls_error: %d.\n", l_tls_error);
                switch(l_tls_error) {
                case SSL_ERROR_SSL:
                {
                        if(gts_last_tls_error[0] != '\0')
                        {
                                NCONN_ERROR("LABEL[%s]: SSL_ERROR_SSL %lu: %s. Reason: %s",
                                                m_label.c_str(),
                                                ERR_get_error(),
                                                ERR_error_string(ERR_get_error(),NULL),
                                                gts_last_tls_error);
                                //NDBG_PRINT("LABEL[%s]: SSL_ERROR_SSL %lu: %s. Reason: %s\n",
                                //                m_label.c_str(),
                                //                ERR_get_error(),
                                //                ERR_error_string(ERR_get_error(),NULL),
                                //                gts_last_tls_error);

                                // Set back
                                gts_last_tls_error[0] = '\0';
                        }
                        else
                        {
                                NCONN_ERROR("LABEL[%s]: SSL_ERROR_SSL %lu: %s.",
                                                m_label.c_str(),
                                                ERR_get_error(),
                                                ERR_error_string(ERR_get_error(),NULL));
                                //NDBG_PRINT("LABEL[%s]: SSL_ERROR_SSL %lu: %s.\n",
                                //                m_label.c_str(),
                                //                ERR_get_error(),
                                //                ERR_error_string(ERR_get_error(),NULL));
                        }

                        break;
                }
                case SSL_ERROR_WANT_READ:
                {
                        //NDBG_PRINT("%sSSL_CON%s[%3d]: SSL_ERROR_WANT_READ\n", ANSI_COLOR_BG_GREEN, ANSI_COLOR_OFF, m_fd);
                        m_tls_state = TLS_STATE_TLS_CONNECTING_WANT_READ;
                        return NC_STATUS_AGAIN;

                }
                case SSL_ERROR_WANT_WRITE:
                {
                        //NDBG_PRINT("%sSSL_CON%s[%3d]: TLS_STATE_TLS_CONNECTING_WANT_WRITE\n", ANSI_COLOR_BG_GREEN, ANSI_COLOR_OFF, m_fd);
                        m_tls_state = TLS_STATE_TLS_CONNECTING_WANT_WRITE;
                        return NC_STATUS_AGAIN;
                }

                case SSL_ERROR_WANT_X509_LOOKUP:
                {
                        NCONN_ERROR("LABEL[%s]: SSL_ERROR_WANT_X509_LOOKUP", m_label.c_str());
                        //NDBG_PRINT("LABEL[%s]: SSL_ERROR_WANT_X509_LOOKUP\n", m_label.c_str());
                        break;
                }

                // look at error stack/return value/errno
                case SSL_ERROR_SYSCALL:
                {
                        if(l_status == 0)
                        {
                                NCONN_ERROR("LABEL[%s]: SSL_ERROR_SYSCALL %lu: %s. An EOF was observed that violates the protocol",
                                                m_label.c_str(),
                                                ERR_get_error(), ERR_error_string(ERR_get_error(), NULL));
                                //NDBG_PRINT("LABEL[%s]: SSL_ERROR_SYSCALL %lu: %s. An EOF was observed that violates the protocol\n",
                                //                m_label.c_str(),
                                //                ERR_get_error(), ERR_error_string(ERR_get_error(), NULL));

                        }
                        else if(l_status == -1)
                        {
                                NCONN_ERROR("LABEL[%s]: SSL_ERROR_SYSCALL %lu: %s. %s",
                                                m_label.c_str(),
                                                ERR_get_error(), ERR_error_string(ERR_get_error(), NULL),
                                                strerror(errno));
                                //NDBG_PRINT("LABEL[%s]: SSL_ERROR_SYSCALL %lu: %s. %s\n",
                                //                m_label.c_str(),
                                //                ERR_get_error(), ERR_error_string(ERR_get_error(), NULL),
                                //                strerror(errno));
                        }
                        break;
                }
                case SSL_ERROR_ZERO_RETURN:
                {
                        NCONN_ERROR("LABEL[%s]: SSL_ERROR_ZERO_RETURN", m_label.c_str());
                        //NDBG_PRINT("LABEL[%s]: SSL_ERROR_ZERO_RETURN\n", m_label.c_str());
                        break;
                }
                case SSL_ERROR_WANT_CONNECT:
                {
                        NCONN_ERROR("LABEL[%s]: SSL_ERROR_WANT_CONNECT", m_label.c_str());
                        //NDBG_PRINT("LABEL[%s]: SSL_ERROR_WANT_CONNECT\n", m_label.c_str());
                        break;
                }
                case SSL_ERROR_WANT_ACCEPT:
                {
                        NCONN_ERROR("LABEL[%s]: SSL_ERROR_WANT_ACCEPT", m_label.c_str());
                        //sNDBG_PRINT("LABEL[%s]: SSL_ERROR_WANT_ACCEPT\n", m_label.c_str());
                        break;
                }
                }

                //ERR_print_errors_fp(stderr);
                return NC_STATUS_ERROR;
        }
        else if(l_status == 1)
        {
                //NDBG_PRINT("CONNECTED\n");
                m_tls_state = TLS_STATE_CONNECTED;
        }

        //did_connect = 1;

        return NC_STATUS_OK;

}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t nconn_tls::tls_accept(void)
{
        int l_status;
        m_tls_state = TLS_STATE_TLS_ACCEPTING;
        l_status = SSL_accept(m_tls);
        if (l_status <= 0)
        {
                //NDBG_PRINT("SSL connection failed - %d\n", l_status);
                int l_tls_error = SSL_get_error(m_tls, l_status);
                //NDBG_PRINT("Showing error: %d.\n", l_tls_error);
                switch(l_tls_error) {
                case SSL_ERROR_SSL:
                {
                        //NDBG_PRINT("LABEL[%s]: SSL_ERROR_SSL %lu: %s.\n",
                        //                m_label.c_str(),
                        //                ERR_get_error(),
                        //                ERR_error_string(ERR_get_error(),NULL));
                        if(gts_last_tls_error[0] != '\0')
                        {
                                NCONN_ERROR("LABEL[%s]: SSL_ERROR_SSL %lu: %s. Reason: %s\n",
                                                m_label.c_str(),
                                                ERR_get_error(),
                                                ERR_error_string(ERR_get_error(),NULL),
                                                gts_last_tls_error);
                                // Set back
                                gts_last_tls_error[0] = '\0';
                        }
                        else
                        {
                                NCONN_ERROR("LABEL[%s]: SSL_ERROR_SSL %lu: %s.\n",
                                                m_label.c_str(),
                                                ERR_get_error(),
                                                ERR_error_string(ERR_get_error(),NULL));
                        }
                        break;
                }
                case SSL_ERROR_WANT_READ:
                {
                        //NDBG_PRINT("%sSSL_CON%s[%3d]: SSL_ERROR_WANT_READ\n", ANSI_COLOR_BG_GREEN, ANSI_COLOR_OFF, m_fd);
                        m_tls_state = TLS_STATE_TLS_ACCEPTING_WANT_READ;
                        return NC_STATUS_AGAIN;
                }
                case SSL_ERROR_WANT_WRITE:
                {
                        //NDBG_PRINT("%sSSL_CON%s[%3d]: TLS_STATE_TLS_CONNECTING_WANT_WRITE\n", ANSI_COLOR_BG_GREEN, ANSI_COLOR_OFF, m_fd);
                        m_tls_state = TLS_STATE_TLS_ACCEPTING_WANT_WRITE;
                        return NC_STATUS_AGAIN;
                }

                case SSL_ERROR_WANT_X509_LOOKUP:
                {
                        NCONN_ERROR("LABEL[%s]: SSL_ERROR_WANT_X509_LOOKUP", m_label.c_str());
                        break;
                }

                // look at error stack/return value/errno
                case SSL_ERROR_SYSCALL:
                {
                        if(l_status == 0) {
                                NCONN_ERROR("LABEL[%s]: SSL_ERROR_SYSCALL %lu: %s. An EOF was observed that violates the protocol",
                                                m_label.c_str(),
                                                ERR_get_error(), ERR_error_string(ERR_get_error(), NULL));
                        } else if(l_status == -1) {
                                NCONN_ERROR("LABEL[%s]: SSL_ERROR_SYSCALL %lu: %s. %s",
                                                m_label.c_str(),
                                                ERR_get_error(), ERR_error_string(ERR_get_error(), NULL),
                                                strerror(errno));
                        }
                        break;
                }
                case SSL_ERROR_ZERO_RETURN:
                {
                        NCONN_ERROR("LABEL[%s]: SSL_ERROR_ZERO_RETURN", m_label.c_str());
                        break;
                }
                case SSL_ERROR_WANT_CONNECT:
                {
                        NCONN_ERROR("LABEL[%s]: SSL_ERROR_WANT_CONNECT", m_label.c_str());
                        break;
                }
                case SSL_ERROR_WANT_ACCEPT:
                {
                        NCONN_ERROR("LABEL[%s]: SSL_ERROR_WANT_ACCEPT", m_label.c_str());
                        break;
                }
                }

                //ERR_print_errors_fp(stderr);
                return NC_STATUS_ERROR;
        }
        else if(1 == l_status)
        {
                m_tls_state = TLS_STATE_CONNECTED;
        }

        //did_connect = 1;

        return NC_STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t nconn_tls::set_opt(uint32_t a_opt, const void *a_buf, uint32_t a_len)
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
        case OPT_TLS_CIPHER_STR:
        {
                m_tls_opt_cipher_str.assign((char *)a_buf, a_len);
                break;
        }
        case OPT_TLS_OPTIONS:
        {
                memcpy(&m_tls_opt_options, a_buf, sizeof(long));
                break;
        }
        case OPT_TLS_VERIFY:
        {
                memcpy(&m_tls_opt_verify, a_buf, sizeof(bool));
                break;
        }
        case OPT_TLS_VERIFY_ALLOW_SELF_SIGNED:
        {
                memcpy(&m_tls_opt_verify_allow_self_signed, a_buf, sizeof(bool));
                break;
        }
        case OPT_TLS_SNI:
        {
                m_tls_opt_tlsext_hostname.assign((char *)a_buf, a_len);
                break;
        }
        case OPT_TLS_CA_FILE:
        {
                m_tls_opt_ca_file.assign((char *)a_buf, a_len);
                break;
        }
        case OPT_TLS_CA_PATH:
        {
                m_tls_opt_ca_path.assign((char *)a_buf, a_len);
                break;
        }
        case OPT_TLS_TLS_KEY:
        {
                m_tls_key.assign((char *)a_buf, a_len);
                break;
        }
        case OPT_TLS_TLS_CRT:
        {
                m_tls_crt.assign((char *)a_buf, a_len);
                break;
        }
        case OPT_TLS_CTX:
        {
                m_tls_ctx = (SSL_CTX *)a_buf;
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
int32_t nconn_tls::get_opt(uint32_t a_opt, void **a_buf, uint32_t *a_len)
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
        case OPT_TLS_TLS_KEY:
        {
                // TODO
                break;
        }
        case OPT_TLS_TLS_CRT:
        {
                // TODO
                break;
        }
        case OPT_TLS_SSL:
        {
                *a_buf = (void *)m_tls;
                *a_len = sizeof(m_tls);
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
int32_t nconn_tls::ncset_listening(evr_loop *a_evr_loop, int32_t a_val)
{
        int32_t l_status;
        l_status = nconn_tcp::ncset_listening(a_evr_loop, a_val);
        if(l_status != NC_STATUS_OK)
        {
                NDBG_PRINT("Error performing nconn_tcp::ncset_listening.\n");
                return NC_STATUS_ERROR;
        }
        m_tls_state = TLS_STATE_LISTENING;
        return NC_STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t nconn_tls::ncset_listening_nb(evr_loop *a_evr_loop, int32_t a_val)
{
        int32_t l_status;
        l_status = nconn_tcp::ncset_listening_nb(a_evr_loop, a_val);
        if(l_status != NC_STATUS_OK)
        {
                NDBG_PRINT("Error performing nconn_tcp::ncset_listening.\n");
                return NC_STATUS_ERROR;
        }
        m_tls_state = TLS_STATE_LISTENING;
        return NC_STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t nconn_tls::ncset_accepting(evr_loop *a_evr_loop, int a_fd)
{
        int32_t l_status;
        l_status = nconn_tcp::ncset_accepting(a_evr_loop, a_fd);
        if(l_status != NC_STATUS_OK)
        {
                return NC_STATUS_ERROR;
        }

        // setup tls
        init();

        // set connection socket to tls state
        SSL_set_fd(m_tls, a_fd);

        // set to accepting state
        m_tls_state = TLS_STATE_ACCEPTING;

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
int32_t nconn_tls::ncread(evr_loop *a_evr_loop, char *a_buf, uint32_t a_buf_len)
{
        ssize_t l_status;
        int32_t l_bytes_read = 0;

        l_status = SSL_read(m_tls, a_buf, a_buf_len);
        //NDBG_PRINT("%sHOST%s: %s tls[%p] READ: %ld bytes. Reason: %s\n",
        //                ANSI_COLOR_FG_RED, ANSI_COLOR_OFF,
        //                m_label.c_str(),
        //                m_tls,
        //                l_status,
        //                strerror(errno));
        if(l_status < 0)
        {
                int l_tls_error = SSL_get_error(m_tls, l_status);
                //NDBG_PRINT("%sSSL_READ%s[%3d] l_bytes_read: %d error: %d\n",
                //                ANSI_COLOR_BG_RED, ANSI_COLOR_OFF,
                //                SSL_get_fd(m_tls), l_bytes_read,
                //                l_tls_error);
                if(l_tls_error == SSL_ERROR_WANT_READ)
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
int32_t nconn_tls::ncwrite(evr_loop *a_evr_loop, char *a_buf, uint32_t a_buf_len)
{
        int l_status;
        l_status = SSL_write(m_tls, a_buf, a_buf_len);
        //NDBG_PRINT("%sHOST%s: %s tls[%p] WRITE: %d bytes. Reason: %s\n",
        //                ANSI_COLOR_FG_CYAN, ANSI_COLOR_OFF,
        //                m_label.c_str(),
        //                m_tls,
        //                l_status,
        //                strerror(errno));
        if(l_status < 0)
        {
                NCONN_ERROR("LABEL[%s]: Error: performing SSL_write.\n", m_label.c_str());
                return NC_STATUS_ERROR;
        }
        return l_status;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t nconn_tls::ncsetup(evr_loop *a_evr_loop)
{
        int32_t l_status;
        //NDBG_PRINT("m_tls_ctx: %p\n", m_tls_ctx);
        l_status = nconn_tcp::ncsetup(a_evr_loop);
        if(l_status != NC_STATUS_OK)
        {
                return NC_STATUS_ERROR;
        }

        //NDBG_PRINT("m_tls_ctx: %p\n", m_tls_ctx);

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

        m_tls_state = TLS_STATE_CONNECTING;

        return NC_STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t nconn_tls::ncaccept(evr_loop *a_evr_loop)
{
        //NDBG_PRINT("%sSSL_ACCEPT%s: STATE[%d] --START\n",
        //                ANSI_COLOR_BG_BLUE, ANSI_COLOR_OFF, m_tls_state);
ncaccept_state_top:
        //NDBG_PRINT("%sSSL_ACCEPT%s: STATE[%d]\n",
        //                ANSI_COLOR_BG_BLUE, ANSI_COLOR_OFF, m_tls_state);
        switch (m_tls_state)
        {
        // -------------------------------------------------
        // STATE: LISTENING
        // -------------------------------------------------
        case TLS_STATE_LISTENING:
        {
                int32_t l_status;
                l_status = nconn_tcp::ncaccept(a_evr_loop);
                return l_status;
        }
        // -------------------------------------------------
        // STATE: SSL_ACCEPTING
        // -------------------------------------------------
        case TLS_STATE_ACCEPTING:
        case TLS_STATE_TLS_ACCEPTING:
        case TLS_STATE_TLS_ACCEPTING_WANT_READ:
        case TLS_STATE_TLS_ACCEPTING_WANT_WRITE:
        {
                int l_status;
                l_status = tls_accept();
                if(l_status == NC_STATUS_AGAIN)
                {
                        if(TLS_STATE_TLS_ACCEPTING_WANT_READ == m_tls_state)
                        {
                                if (0 != a_evr_loop->mod_fd(m_fd,
                                                            EVR_FILE_ATTR_MASK_READ|EVR_FILE_ATTR_MASK_RD_HUP|EVR_FILE_ATTR_MASK_ET,
                                                            this))
                                {
                                        NCONN_ERROR("LABEL[%s]: Error: Couldn't add socket file descriptor", m_label.c_str());
                                        return NC_STATUS_ERROR;
                                }
                        }
                        else if(TLS_STATE_TLS_ACCEPTING_WANT_WRITE == m_tls_state)
                        {
                                if (0 != a_evr_loop->mod_fd(m_fd,
                                                            EVR_FILE_ATTR_MASK_WRITE|EVR_FILE_ATTR_MASK_ET,
                                                            this))
                                {
                                        NCONN_ERROR("LABEL[%s]: Error: Couldn't add socket file descriptor", m_label.c_str());
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
                        NCONN_ERROR("LABEL[%s]: Error: Couldn't add socket file descriptor", m_label.c_str());
                        return NC_STATUS_ERROR;
                }
                goto ncaccept_state_top;
        }
        // -------------------------------------------------
        // STATE: CONNECTED
        // -------------------------------------------------
        case TLS_STATE_CONNECTED:
        {
                //...
                break;
        }
        // -------------------------------------------------
        // STATE: ???
        // -------------------------------------------------
        default:
        {
                NDBG_PRINT("State error: %d\n", m_tls_state);
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
int32_t nconn_tls::ncconnect(evr_loop *a_evr_loop)
{
        //NDBG_PRINT("%stls_connect%s: STATE[%d] --START\n",
        //                ANSI_COLOR_BG_BLUE, ANSI_COLOR_OFF, m_tls_state);
ncconnect_state_top:
        //NDBG_PRINT("%stls_connect%s: STATE[%d]\n",
        //                ANSI_COLOR_BG_BLUE, ANSI_COLOR_OFF, m_tls_state);
        switch (m_tls_state)
        {
        // -------------------------------------------------
        // STATE: CONNECTING
        // -------------------------------------------------
        case TLS_STATE_CONNECTING:
        {
                int32_t l_status;
                l_status = nconn_tcp::ncconnect(a_evr_loop);
                if(l_status == NC_STATUS_ERROR)
                {
                        //NDBG_PRINT("Error performing nconn_tcp::ncconnect\n");
                        return NC_STATUS_ERROR;
                }

                if(nconn_tcp::is_connecting())
                {
                        //NDBG_PRINT("Still connecting...\n");
                        return NC_STATUS_OK;
                }

                m_tls_state = TLS_STATE_TLS_CONNECTING;
                goto ncconnect_state_top;
        }
        // -------------------------------------------------
        // STATE: tls_connectING
        // -------------------------------------------------
        case TLS_STATE_TLS_CONNECTING:
        case TLS_STATE_TLS_CONNECTING_WANT_READ:
        case TLS_STATE_TLS_CONNECTING_WANT_WRITE:
        {
                int l_status;
                l_status = tls_connect();
                //NDBG_PRINT("%stls_connectING%s status = %d m_tls_state = %d\n", ANSI_COLOR_BG_RED, ANSI_COLOR_OFF, l_status, m_tls_state);
                if(l_status == NC_STATUS_AGAIN)
                {
                        if(TLS_STATE_TLS_CONNECTING_WANT_READ == m_tls_state)
                        {
                                if (0 != a_evr_loop->mod_fd(m_fd,
                                                            EVR_FILE_ATTR_MASK_READ|EVR_FILE_ATTR_MASK_RD_HUP|EVR_FILE_ATTR_MASK_ET,
                                                            this))
                                {
                                        NCONN_ERROR("LABEL[%s]: Error: Couldn't add socket file descriptor", m_label.c_str());
                                        //NDBG_PRINT("LABEL[%s]: Error: Couldn't add socket file descriptor", m_label.c_str());
                                        return NC_STATUS_ERROR;
                                }
                        }
                        else if(TLS_STATE_TLS_CONNECTING_WANT_WRITE == m_tls_state)
                        {
                                if (0 != a_evr_loop->mod_fd(m_fd,
                                                            EVR_FILE_ATTR_MASK_WRITE|EVR_FILE_ATTR_MASK_ET,
                                                            this))
                                {
                                        NCONN_ERROR("LABEL[%s]: Error: Couldn't add socket file descriptor", m_label.c_str());
                                        //NDBG_PRINT("LABEL[%s]: Error: Couldn't add socket file descriptor", m_label.c_str());
                                        return NC_STATUS_ERROR;
                                }
                        }
                        return NC_STATUS_OK;
                }
                else if(l_status != NC_STATUS_OK)
                {
                        //NDBG_PRINT("Error performing tls_connect\n");
                        return NC_STATUS_ERROR;
                }

                // -------------------------------------------
                // Add to event handler
                // -------------------------------------------
                if (0 != a_evr_loop->mod_fd(m_fd,
                                            EVR_FILE_ATTR_MASK_READ|EVR_FILE_ATTR_MASK_RD_HUP|EVR_FILE_ATTR_MASK_ET,
                                            this))
                {
                        NCONN_ERROR("LABEL[%s]: Error: Couldn't add socket file descriptor", m_label.c_str());
                        return NC_STATUS_ERROR;
                }
                goto ncconnect_state_top;
        }
        // -------------------------------------------------
        // STATE: CONNECTED
        // -------------------------------------------------
        case TLS_STATE_CONNECTED:
        {
                if(m_tls_opt_verify)
                {
                        int32_t l_status = 0;
                        // Do verify
                        l_status = validate_server_certificate(m_tls, NULL, (!m_tls_opt_verify_allow_self_signed));
                        if(l_status != 0)
                        {
                                //NDBG_PRINT("Error validate_server_certificate failed\n");
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
                //NDBG_PRINT("State error: %d\n", m_tls_state);
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
int32_t nconn_tls::nccleanup(void)
{
        // Shut down connection
        if(m_tls)
        {
                SSL_free(m_tls);
                m_tls = NULL;
        }

        // Reset all the values
        // TODO Make init function...
        // Set num use back to zero -we need reset method here?
        m_tls_state = TLS_STATE_FREE;

        // Super
        nconn_tcp::nccleanup();

        return NC_STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: nconn_utils
//: ----------------------------------------------------------------------------
//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
SSL *nconn_get_SSL(nconn &a_nconn)
{
        SSL *l_ssl;
        uint32_t l_len;
        int l_status;
        l_status = a_nconn.get_opt(nconn_tls::OPT_TLS_SSL, (void **)&l_ssl, &l_len);
        if(l_status != nconn::NC_STATUS_OK)
        {
                return NULL;
        }
        return l_ssl;
}

} //namespace ns_hlx {
