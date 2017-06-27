//: ----------------------------------------------------------------------------
//: Copyright (C) 2017 Verizon.  All Rights Reserved.
//: All Rights Reserved
//:
//: \file:    h2url.cc
//: \details: TODO
//: \author:  Reed P. Morrison
//: \date:    04/08/2017
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
//: includes
//: ----------------------------------------------------------------------------
#include "http_parser/http_parser.h"
#include "nghttp2/nghttp2.h"
#include "hurl/status.h"
#include "hurl/nconn/scheme.h"
#include "hurl/nconn/host_info.h"
#include "hurl/nconn/nconn.h"
#include "hurl/nconn/nconn_tcp.h"
#include "hurl/nconn/nconn_tls.h"
#include "hurl/support/kv_map_list.h"
#include "hurl/support/string_util.h"
#include "hurl/support/tls_util.h"
#include "hurl/support/nbq.h"
#include "hurl/support/trace.h"
#include "hurl/dns/nlookup.h"
#include "hurl/http/http_status.h"
#include "hurl/http/resp.h"
#include "hurl/http/cb.h"
#include "hurl/http/api_resp.h"
// internal
#include "support/ndebug.h"
#include "support/file_util.h"
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <stdint.h>
#ifndef __STDC_FORMAT_MACROS
#define __STDC_FORMAT_MACROS
#endif
#include <inttypes.h>
// openssl
#include <openssl/err.h>
#include <openssl/bio.h>
#include <openssl/rand.h>
#include <openssl/crypto.h>
#include <openssl/ssl.h>
// For errx
#include <err.h>
// For sleep
#include <unistd.h>
// socket support
#include <netdb.h>
#include <string.h>
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/tcp.h>
#include <string>
//: ----------------------------------------------------------------------------
//: constants
//: ----------------------------------------------------------------------------
#ifndef STATUS_OK
#define STATUS_OK 0
#endif
#ifndef STATUS_ERROR
#define STATUS_ERROR -1
#endif
// ---------------------------------------------------------
// ALPN/NPN support borrowed from curl...
// ---------------------------------------------------------
#define ALPN_HTTP_1_1_LENGTH 8
#define ALPN_HTTP_1_1 "http/1.1"
// Check for OpenSSL 1.0.2 which has ALPN support.
#undef HAS_ALPN
#if OPENSSL_VERSION_NUMBER >= 0x10002000L \
    && !defined(OPENSSL_NO_TLSEXT)
#  define HAS_ALPN 1
#endif
// Check for OpenSSL 1.0.1 which has NPN support.
#undef HAS_NPN
#if OPENSSL_VERSION_NUMBER >= 0x10001000L \
    && !defined(OPENSSL_NO_TLSEXT) \
    && !defined(OPENSSL_NO_NEXTPROTONEG)
#  define HAS_NPN 1
#endif
//: ----------------------------------------------------------------------------
//: macros
//: ----------------------------------------------------------------------------
#ifndef _U_
#define _U_ __attribute__((unused))
#endif
#ifndef ARRLEN
#define ARRLEN(x) (sizeof(x) / sizeof(x[0]))
#endif
#ifndef _SET_NCONN_OPT
#define _SET_NCONN_OPT(_conn, _opt, _buf, _len) do { \
                int _status = 0; \
                _status = _conn->set_opt((_opt), (_buf), (_len)); \
                if (_status != ns_hurl::nconn::NC_STATUS_OK) { \
                        delete _conn;\
                        _conn = NULL;\
                        return STATUS_ERROR;\
                } \
        } while(0)
#endif
#ifndef CHECK_FOR_NULL_ERROR_DEBUG
#define CHECK_FOR_NULL_ERROR_DEBUG(_data) \
        do {\
                if(!_data) {\
                        NDBG_PRINT("Error.\n");\
                        return STATUS_ERROR;\
                }\
        } while(0);
#endif
#ifndef CHECK_FOR_NULL_ERROR
#define CHECK_FOR_NULL_ERROR(_data) \
        do {\
                if(!_data) {\
                        return STATUS_ERROR;\
                }\
        } while(0);
#endif
//: ----------------------------------------------------------------------------
//: fwd decl's
//: ----------------------------------------------------------------------------
static int npn_select_next_proto_cb(SSL *a_ssl,
                                    unsigned char **a_out,
                                    unsigned char *a_outlen,
                                    const unsigned char *a_in,
                                    unsigned int a_inlen,
                                    void *a_arg);
int32_t http2_parse(void *a_data, char *a_buf, uint32_t a_len, uint64_t a_off);
//: ----------------------------------------------------------------------------
//: request object/meta
//: ----------------------------------------------------------------------------
class request {
public:
        typedef enum {
                HTTP_PROTOCOL_V1_TCP = 0,
                HTTP_PROTOCOL_V1_TLS,
                HTTP_PROTOCOL_V2_TLS
        } http_protocol_type_t;
        // -------------------------------------------------
        // public methods
        // -------------------------------------------------
        request():
                m_scheme(ns_hurl::SCHEME_TCP),
                m_host(),
                m_url(),
                m_url_path(),
                m_url_query(),
                m_verb("GET"),
                m_headers(),
                m_body_data(NULL),
                m_body_data_len(0),
                m_port(0),
                m_expect_resp_body_flag(true),
                m_evr_loop(NULL),
                m_tls_ctx(NULL),
                m_tls(NULL),
                m_nconn(NULL),
                m_timer_obj(NULL),
                m_resp(NULL),
                m_in_q(NULL),
                m_out_q(NULL),
                m_ngxxx_session(NULL),
                m_ngxxx_session_stream_id(-1),
                m_ngxxx_session_stream_closed(false),
                m_conf_hp_type(HTTP_PROTOCOL_V2_TLS),
                m_conf_tls_cipher_list(),
                m_conf_tls_options(),
                m_conf_tls_verify(),
                m_conf_tls_sni(),
                m_conf_tls_self_ok(),
                m_conf_tls_no_host_check(),
                m_conf_tls_ca_file(),
                m_conf_tls_ca_path()
        {};
        ~request()
        {
                if(m_tls)
                {
                        SSL_shutdown(m_tls);
                        m_tls = NULL;
                }
                if(m_tls_ctx)
                {
                        SSL_CTX_free(m_tls_ctx);
                        m_tls_ctx = NULL;
                }
        }
        int32_t set_header(const std::string &a_key, const std::string &a_val);
        int32_t init_with_url(const std::string &a_url);
        int32_t create_request(void);
        // -------------------------------------------------
        // Public Static (class) methods
        // -------------------------------------------------
        static int32_t evr_fd_readable_cb(void *a_data) {return run_state_machine(a_data, ns_hurl::EVR_MODE_READ);}
        static int32_t evr_fd_writeable_cb(void *a_data){return run_state_machine(a_data, ns_hurl::EVR_MODE_WRITE);}
        static int32_t evr_fd_error_cb(void *a_data) {return run_state_machine(a_data, ns_hurl::EVR_MODE_ERROR);}
        static int32_t evr_fd_timeout_cb(void *a_data){return run_state_machine(a_data, ns_hurl::EVR_MODE_TIMEOUT);}
        // connected cb
        static int32_t connected_cb(ns_hurl::nconn *a_nconn, void *a_data);
        // -------------------------------------------------
        // public members
        // -------------------------------------------------
        ns_hurl::scheme_t m_scheme;
        std::string m_host;
        std::string m_url;
        std::string m_url_path;
        std::string m_url_query;
        std::string m_verb;
        ns_hurl::kv_map_list_t m_headers;
        char *m_body_data;
        uint32_t m_body_data_len;
        uint16_t m_port;
        bool m_expect_resp_body_flag;
        ns_hurl::evr_loop *m_evr_loop;
        SSL_CTX *m_tls_ctx;
        SSL *m_tls;
        ns_hurl::nconn *m_nconn;
        ns_hurl::evr_event_t *m_timer_obj;
        ns_hurl::resp *m_resp;
        ns_hurl::nbq *m_in_q;
        ns_hurl::nbq *m_out_q;
        nghttp2_session *m_ngxxx_session;
        int32_t m_ngxxx_session_stream_id;
        bool m_ngxxx_session_stream_closed;
        // conf options
        http_protocol_type_t m_conf_hp_type;
        std::string m_conf_tls_cipher_list;
        long m_conf_tls_options;
        bool m_conf_tls_verify;
        bool m_conf_tls_sni;
        bool m_conf_tls_self_ok;
        bool m_conf_tls_no_host_check;
        std::string m_conf_tls_ca_file;
        std::string m_conf_tls_ca_path;
private:
        // -------------------------------------------------
        // private methods
        // -------------------------------------------------
        // Disallow copy/assign
        request(const request &);
        request& operator=(const request &);
        int32_t teardown(ns_hurl::http_status_t a_status);
        // -------------------------------------------------
        // Private Static (class) methods
        // -------------------------------------------------
        static int32_t run_state_machine(void *a_data, ns_hurl::evr_mode_t a_conn_mode);
};
//: ****************************************************************************
//: ******************** N G H T T P 2   S U P P O R T *************************
//: ****************************************************************************
//: ----------------------------------------------------------------------------
//: Types
//: ----------------------------------------------------------------------------
//: ----------------------------------------------------------------------------
//: \details: nghttp2_send_callback. Here we transmit the |data|, |length| bytes,
//:           to the network. Because we are using libevent bufferevent, we just
//:           write those bytes into bufferevent buffer
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
static ssize_t ngxxx_send_cb(nghttp2_session *a_session _U_,
                             const uint8_t *a_data,
                             size_t a_length,
                             int a_flags _U_,
                             void *a_user_data)
{
        request *l_request = (request *)a_user_data;
        //NDBG_PRINT("SEND_CB\n");
        //ns_hurl::mem_display(a_data, a_length, true);
        int64_t l_s;
        l_s = l_request->m_out_q->write((const char *)a_data,(uint64_t)a_length);
        //NDBG_PRINT("%sWRITE%s: l_s: %d\n", ANSI_COLOR_FG_BLUE, ANSI_COLOR_OFF, (int)l_s);
        return (ssize_t)l_s;
}
//: ----------------------------------------------------------------------------
//: \details: nghttp2_on_frame_recv_callback: Called when nghttp2 library
//:           received a complete frame from the remote peer.
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
static int ngxxx_frame_recv_cb(nghttp2_session *a_session,
                               const nghttp2_frame *a_frame,
                               void *a_user_data)
{
        //NDBG_PRINT("%sFRAME%s: TYPE[%6u]\n", ANSI_COLOR_BG_MAGENTA, ANSI_COLOR_OFF, a_frame->hd.type);
        request *l_request = (request *)a_user_data;
        switch (a_frame->hd.type)
        {
        case NGHTTP2_HEADERS:
        {
                if((a_frame->headers.cat == NGHTTP2_HCAT_RESPONSE) &&
                   (l_request->m_ngxxx_session_stream_id == a_frame->hd.stream_id))
                {
                        //fprintf(stderr, "All headers received\n");
                }
                break;
        }
        }
        return 0;
}
//: ----------------------------------------------------------------------------
//: \details: nghttp2_on_data_chunk_recv_callback: Called when DATA frame is
//:           received from the remote peer. In this implementation, if the frame
//:           is meant to the stream we initiated, print the received data in
//:           stdout, so that the user can redirect its output to the file
//:           easily.
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
static int ngxxx_data_chunk_recv_cb(nghttp2_session *a_session _U_,
                                    uint8_t a_flags _U_,
                                    int32_t a_stream_id,
                                    const uint8_t *a_data,
                                    size_t a_len,
                                    void *a_user_data)
{
        //NDBG_PRINT("%sCHUNK%s: \n", ANSI_COLOR_BG_BLUE, ANSI_COLOR_OFF);
        request *l_request = (request *)a_user_data;
        if(l_request->m_ngxxx_session_stream_id == a_stream_id)
        {
                fwrite(a_data, a_len, 1, stdout);
        }
        return 0;
}
//: ----------------------------------------------------------------------------
//: \details: nghttp2_on_stream_close_callback: Called when a stream is about to
//:           closed. This example program only deals with 1 HTTP request (1
//:           stream), if it is closed, we send GOAWAY and tear down the
//:           session
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
static int ngxxx_stream_close_cb(nghttp2_session *a_session,
                                 int32_t a_stream_id,
                                 uint32_t a_error_code,
                                 void *a_user_data)
{
        //NDBG_PRINT("%sCLOSE%s: \n", ANSI_COLOR_BG_RED, ANSI_COLOR_OFF);
        request *l_request = (request *)a_user_data;
        int l_rv;
        l_request->m_ngxxx_session_stream_closed = true;
        if(l_request->m_ngxxx_session_stream_id == a_stream_id)
        {
                //fprintf(stderr, "Stream %d closed with error_code=%d\n", a_stream_id, a_error_code);
                l_rv = nghttp2_session_terminate_session(a_session, NGHTTP2_NO_ERROR);
                if (l_rv != 0)
                {
                        return NGHTTP2_ERR_CALLBACK_FAILURE;
                }
        }
        return 0;

}
//: ----------------------------------------------------------------------------
//: \details: nghttp2_on_header_callback: Called when nghttp2 library emits
//:           single header name/value pair
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
static int ngxxx_header_cb(nghttp2_session *a_session _U_,
                           const nghttp2_frame *a_frame,
                           const uint8_t *a_name,
                           size_t a_namelen,
                           const uint8_t *a_value,
                           size_t a_valuelen,
                           uint8_t a_flags _U_,
                           void *a_user_data)
{
        //NDBG_PRINT("%sHEADER%s: \n", ANSI_COLOR_BG_YELLOW, ANSI_COLOR_OFF);
        request *l_request = (request *)a_user_data;
        switch (a_frame->hd.type)
        {
        case NGHTTP2_HEADERS:
                if((a_frame->headers.cat == NGHTTP2_HCAT_RESPONSE) &&
                   (l_request->m_ngxxx_session_stream_id == a_frame->hd.stream_id))
                {
                        // Print response headers for the initiated request.
                        fprintf(stdout, "%s%.*s%s: %s%.*s%s\n",
                                ANSI_COLOR_FG_BLUE, (int)a_namelen, a_name, ANSI_COLOR_OFF,
                                ANSI_COLOR_FG_GREEN, (int)a_valuelen, a_value, ANSI_COLOR_OFF);
                        break;
                }
        }
        return 0;
}
//: ----------------------------------------------------------------------------
//: \details: nghttp2_on_begin_headers_callback:
//:           Called when nghttp2 library gets started to receive header block.
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
static int ngxxx_begin_headers_cb(nghttp2_session *a_session _U_,
                                  const nghttp2_frame *a_frame,
                                  void *a_user_data)
{
        //NDBG_PRINT("%sBEGIN_HEADERS%s: \n", ANSI_COLOR_BG_WHITE, ANSI_COLOR_OFF);
        request *l_request = (request *)a_user_data;
        switch (a_frame->hd.type)
        {
        case NGHTTP2_HEADERS:
                if((a_frame->headers.cat == NGHTTP2_HCAT_RESPONSE) &&
                  (l_request->m_ngxxx_session_stream_id == a_frame->hd.stream_id))
                {
                        //fprintf(stderr, "Response headers for stream ID=%d:\n", a_frame->hd.stream_id);
                }
                break;
        }
        return 0;
}
//: ----------------------------------------------------------------------------
//: support routines
//: ----------------------------------------------------------------------------
//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t request::set_header(const std::string &a_key, const std::string &a_val)
{
        ns_hurl::kv_map_list_t::iterator i_obj = m_headers.find(a_key);
        if(i_obj != m_headers.end())
        {
                // Special handling for Host/User-agent/referer
                bool l_replace = false;
                bool l_remove = false;
                if(!strcasecmp(a_key.c_str(), "User-Agent") ||
                   !strcasecmp(a_key.c_str(), "Referer") ||
                   !strcasecmp(a_key.c_str(), "Accept") ||
                   !strcasecmp(a_key.c_str(), "Host"))
                {
                        l_replace = true;
                        if(a_val.empty())
                        {
                                l_remove = true;
                        }
                }
                if(l_replace)
                {
                        i_obj->second.pop_front();
                        if(!l_remove)
                        {
                                i_obj->second.push_back(a_val);
                        }
                }
                else
                {
                        i_obj->second.push_back(a_val);
                }
        }
        else
        {
                ns_hurl::str_list_t l_list;
                l_list.push_back(a_val);
                m_headers[a_key] = l_list;
        }
        return STATUS_OK;
}
//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t request::connected_cb(ns_hurl::nconn *a_nconn, void *a_data)
{
        //NDBG_PRINT("%sconnected%s...\n", ANSI_COLOR_FG_GREEN, ANSI_COLOR_OFF);
        if(!a_data)
        {
                NDBG_PRINT("error\n");
                return HURL_STATUS_ERROR;
        }
        request *l_request = (request *)a_data;
        if(l_request->m_nconn &&
           ns_hurl::nconn_get_SSL(*(l_request->m_nconn)))
        {
                SSL *l_tls = ns_hurl::nconn_get_SSL(*(l_request->m_nconn));
                X509* l_cert = NULL;
                l_cert = SSL_get_peer_certificate(l_tls);
                if(l_cert == NULL)
                {
                        NDBG_PRINT("SSL_get_peer_certificate error.  tls: %p\n", l_tls);
                        return HURL_STATUS_ERROR;
                }
                printf("%s", ANSI_COLOR_FG_MAGENTA);
                printf("+------------------------------------------------------------------------------+\n");
                printf("| *************** T L S   S E R V E R   C E R T I F I C A T E **************** |\n");
                printf("+------------------------------------------------------------------------------+\n");
                printf("%s", ANSI_COLOR_OFF);
                X509_print_fp(stdout, l_cert);
                SSL_SESSION *m_tls_session = SSL_get_session(l_tls);
                printf("%s", ANSI_COLOR_FG_YELLOW);
                printf("+------------------------------------------------------------------------------+\n");
                printf("|                      T L S   S E S S I O N   I N F O                         |\n");
                printf("+------------------------------------------------------------------------------+\n");
                printf("%s", ANSI_COLOR_OFF);
                SSL_SESSION_print_fp(stdout, m_tls_session);
                //int32_t l_protocol_num = ns_hurl::get_tls_info_protocol_num(l_tls);
                //std::string l_cipher = ns_hurl::get_tls_info_cipher_str(l_tls);
                //std::string l_protocol = ns_hurl::get_tls_info_protocol_str(l_protocol_num);
                //printf(" cipher:     %s\n", l_cipher.c_str());
                //printf(" l_protocol: %s\n", l_protocol.c_str());
        }
        int32_t l_s;
        l_s = l_request->create_request();
        if(l_s != STATUS_OK)
        {
                NDBG_PRINT("error\n");
                return HURL_STATUS_ERROR;
        }
        return HURL_STATUS_OK;
}
//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t request::init_with_url(const std::string &a_url)
{
        // -------------------------------------------------
        // parse url
        // -------------------------------------------------
        std::string l_url_fixed = a_url;
        // Find scheme prefix "://"
        if(a_url.find("://", 0) == std::string::npos)
        {
                l_url_fixed = "http://" + a_url;
        }
        http_parser_url l_url;
        http_parser_url_init(&l_url);
        // silence bleating memory sanitizers...
        //memset(&l_url, 0, sizeof(l_url));
        int l_status;
        l_status = http_parser_parse_url(l_url_fixed.c_str(), l_url_fixed.length(), 0, &l_url);
        if(l_status != 0)
        {
                NDBG_PRINT("Error parsing url: %s\n", l_url_fixed.c_str());
                // TODO get error msg from http_parser
                return STATUS_ERROR;
        }
        // Set no port
        m_port = 0;
        for(uint32_t i_part = 0; i_part < UF_MAX; ++i_part)
        {
                //NDBG_PRINT("i_part: %d offset: %d len: %d\n", i_part, l_url.field_data[i_part].off, l_url.field_data[i_part].len);
                //NDBG_PRINT("len+off: %d\n",       l_url.field_data[i_part].len + l_url.field_data[i_part].off);
                //NDBG_PRINT("a_url.length(): %d\n", (int)a_url.length());
                if(l_url.field_data[i_part].len &&
                  // TODO Some bug with parser -parsing urls like "http://127.0.0.1" sans paths
                  ((l_url.field_data[i_part].len + l_url.field_data[i_part].off) <= l_url_fixed.length()))
                {
                        switch(i_part)
                        {
                        case UF_SCHEMA:
                        {
                                std::string l_part = l_url_fixed.substr(l_url.field_data[i_part].off, l_url.field_data[i_part].len);
                                //NDBG_PRINT("l_part: %s\n", l_part.c_str());
                                if(l_part == "http")
                                {
                                        m_scheme = ns_hurl::SCHEME_TCP;
                                }
                                else if(l_part == "https")
                                {
                                        m_scheme = ns_hurl::SCHEME_TLS;
                                }
                                else
                                {
                                        NDBG_PRINT("Error schema[%s] is unsupported\n", l_part.c_str());
                                        return STATUS_ERROR;
                                }
                                break;
                        }
                        case UF_HOST:
                        {
                                std::string l_part = l_url_fixed.substr(l_url.field_data[i_part].off, l_url.field_data[i_part].len);
                                //NDBG_PRINT("l_part[UF_HOST]: %s\n", l_part.c_str());
                                m_host = l_part;
                                break;
                        }
                        case UF_PORT:
                        {
                                std::string l_part = l_url_fixed.substr(l_url.field_data[i_part].off, l_url.field_data[i_part].len);
                                //NDBG_PRINT("l_part[UF_PORT]: %s\n", l_part.c_str());
                                m_port = (uint16_t)strtoul(l_part.c_str(), NULL, 10);
                                break;
                        }
                        case UF_PATH:
                        {
                                std::string l_part = l_url_fixed.substr(l_url.field_data[i_part].off, l_url.field_data[i_part].len);
                                //NDBG_PRINT("l_part[UF_PATH]: %s\n", l_part.c_str());
                                m_url_path = l_part;
                                break;
                        }
                        case UF_QUERY:
                        {
                                std::string l_part = l_url_fixed.substr(l_url.field_data[i_part].off, l_url.field_data[i_part].len);
                                //NDBG_PRINT("l_part[UF_QUERY]: %s\n", l_part.c_str());
                                m_url_query = l_part;
                                break;
                        }
                        case UF_FRAGMENT:
                        {
                                //std::string l_part = l_url_fixed.substr(l_url.field_data[i_part].off, l_url.field_data[i_part].len);
                                //NDBG_PRINT("l_part[UF_FRAGMENT]: %s\n", l_part.c_str());
                                //m_fragment = l_part;
                                break;
                        }
                        case UF_USERINFO:
                        {
                                //std::string l_part = l_url_fixed.substr(l_url.field_data[i_part].off, l_url.field_data[i_part].len);
                                //sNDBG_PRINT("l_part[UF_USERINFO]: %s\n", l_part.c_str());
                                //m_userinfo = l_part;
                                break;
                        }
                        default:
                        {
                                break;
                        }
                        }
                }
        }
        // -------------------------------------------------
        // ports
        // -------------------------------------------------
        if(!m_port)
        {
                switch(m_scheme)
                {
                case ns_hurl::SCHEME_TCP:
                {
                        m_port = 80;
                        break;
                }
                case ns_hurl::SCHEME_TLS:
                {
                        m_port = 443;
                        break;
                }
                default:
                {
                        m_port = 80;
                        break;
                }
                }
        }
        // -------------------------------------------------
        // int path if empty
        // -------------------------------------------------
        if(m_url_path.empty())
        {
                m_url_path = "/";
        }
        // -------------------------------------------------
        // setup ctx
        // -------------------------------------------------
        //m_num_to_req = m_path_vector.size();
        //NDBG_PRINT("Showing parsed url.\n");
        //m_url.show();
        //NDBG_PRINT("Parsed url: %s\n", l_url_fixed.c_str());
        m_conf_hp_type = HTTP_PROTOCOL_V2_TLS;
        if(m_scheme == ns_hurl::SCHEME_TCP)
        {
                m_conf_hp_type = HTTP_PROTOCOL_V1_TCP;
        }
        // -------------------------------------------------
        // init tls...
        // -------------------------------------------------
        if(m_scheme == ns_hurl::SCHEME_TLS)
        {
                m_conf_hp_type = HTTP_PROTOCOL_V1_TLS;
                ns_hurl::tls_init();
                std::string l_unused;
                m_tls_ctx = ns_hurl::tls_init_ctx(m_conf_tls_cipher_list, // ctx cipher list str
                                                  m_conf_tls_options,     // ctx options
                                                  m_conf_tls_ca_file,     // ctx ca file
                                                  m_conf_tls_ca_path,     // ctx ca path
                                                  false,                  // is server?
                                                  l_unused,               // tls key
                                                  l_unused);              // tls crt
                // modes from nghttp2 client example
                SSL_CTX_set_mode(m_tls_ctx, SSL_MODE_AUTO_RETRY);
                SSL_CTX_set_mode(m_tls_ctx, SSL_MODE_RELEASE_BUFFERS);
#ifdef HAS_NPN
                // set npn callback
                SSL_CTX_set_next_proto_select_cb(m_tls_ctx, npn_select_next_proto_cb, this);
#endif
        }
        // -------------------------------------------------
        // evr loop
        // -------------------------------------------------
        // TODO -make loop configurable
#if defined(__linux__)
        m_evr_loop = new ns_hurl::evr_loop(ns_hurl::EVR_LOOP_EPOLL, 512);
#elif defined(__FreeBSD__) || defined(__APPLE__)
        m_evr_loop = new ns_hurl::evr_loop(ns_hurl::EVR_LOOP_SELECT, 512);
#else
        m_evr_loop = new ns_hurl::evr_loop(ns_hurl::EVR_LOOP_SELECT, 512);
#endif
        if(!m_evr_loop)
        {
                return STATUS_ERROR;
        }
        // -------------------------------------------------
        // create connection
        // -------------------------------------------------
        if(m_scheme == ns_hurl::SCHEME_TLS)
        {
                m_nconn = new ns_hurl::nconn_tls();
                _SET_NCONN_OPT(m_nconn, ns_hurl::nconn_tls::OPT_TLS_CTX, m_tls_ctx,sizeof(m_tls_ctx));
                _SET_NCONN_OPT(m_nconn, ns_hurl::nconn_tls::OPT_TLS_CIPHER_STR,m_conf_tls_cipher_list.c_str(),m_conf_tls_cipher_list.length());
                _SET_NCONN_OPT(m_nconn, ns_hurl::nconn_tls::OPT_TLS_VERIFY, &(m_conf_tls_verify), sizeof(bool));
                _SET_NCONN_OPT(m_nconn, ns_hurl::nconn_tls::OPT_TLS_VERIFY_ALLOW_SELF_SIGNED, &(m_conf_tls_self_ok), sizeof(bool));
                _SET_NCONN_OPT(m_nconn, ns_hurl::nconn_tls::OPT_TLS_VERIFY_NO_HOST_CHECK, &(m_conf_tls_no_host_check), sizeof(bool));
                _SET_NCONN_OPT(m_nconn, ns_hurl::nconn_tls::OPT_TLS_SNI, &(m_conf_tls_sni), sizeof(bool));
                _SET_NCONN_OPT(m_nconn, ns_hurl::nconn_tls::OPT_TLS_HOSTNAME, m_host.c_str(), m_host.length());
        }
        else if(m_scheme == ns_hurl::SCHEME_TCP)
        {
                m_nconn = new ns_hurl::nconn_tcp();
        }
        else
        {
                return STATUS_ERROR;
        }
        m_nconn->set_data(this);
        m_nconn->set_num_reqs_per_conn(1);
        m_nconn->set_collect_stats(false);
        m_nconn->set_connect_only(false);
        m_nconn->set_evr_loop(m_evr_loop);
        m_nconn->setup_evr_fd(request::evr_fd_readable_cb,
                              request::evr_fd_writeable_cb,
                              request::evr_fd_error_cb);
        m_nconn->set_label(m_host);
        m_nconn->set_connected_cb(connected_cb);
        m_nconn->set_read_cb(ns_hurl::http_parse);
        // -------------------------------------------------
        // setup resp
        // -------------------------------------------------
        m_resp = new ns_hurl::resp();
        m_resp->init(true);
        m_resp->m_http_parser->data = m_resp;
        m_nconn->set_read_cb_data(m_resp);
        // -------------------------------------------------
        // setup q's
        // -------------------------------------------------
        m_in_q = new ns_hurl::nbq(8*1024);
        m_resp->set_q(m_in_q);
        m_out_q = new ns_hurl::nbq(8*1024);
        // -------------------------------------------------
        // nghttp2 setup
        // -------------------------------------------------
        if(m_scheme == ns_hurl::SCHEME_TLS)
        {
                // -------------------------------------------------
                // create session/stream
                // -------------------------------------------------
                m_ngxxx_session_stream_id = -1;
                m_ngxxx_session_stream_closed = false;
                // -------------------------------------------------
                // init session...
                // -------------------------------------------------
                nghttp2_session_callbacks *l_cb;
                nghttp2_session_callbacks_new(&l_cb);
                nghttp2_session_callbacks_set_send_callback(l_cb, ngxxx_send_cb);
                nghttp2_session_callbacks_set_on_frame_recv_callback(l_cb, ngxxx_frame_recv_cb);
                nghttp2_session_callbacks_set_on_data_chunk_recv_callback(l_cb, ngxxx_data_chunk_recv_cb);
                nghttp2_session_callbacks_set_on_stream_close_callback(l_cb, ngxxx_stream_close_cb);
                nghttp2_session_callbacks_set_on_header_callback(l_cb, ngxxx_header_cb);
                nghttp2_session_callbacks_set_on_begin_headers_callback(l_cb, ngxxx_begin_headers_cb);
                nghttp2_session_client_new(&(m_ngxxx_session), l_cb, this);
                nghttp2_session_callbacks_del(l_cb);
        }
        // -------------------------------------------------
        // done
        // -------------------------------------------------
        return STATUS_OK;
}
//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t request::create_request(void)
{
        printf("%s", ANSI_COLOR_FG_WHITE);
        printf("+------------------------------------------------------------------------------+\n");
        printf("|                                R E Q U E S T                                 |\n");
        printf("+------------------------------------------------------------------------------+\n");
        printf("%s", ANSI_COLOR_OFF);
        if(m_conf_hp_type == HTTP_PROTOCOL_V2_TLS)
        {
                // -------------------------------------------------
                // send connection header
                // -------------------------------------------------
                nghttp2_settings_entry l_iv[1] = {
                        { NGHTTP2_SETTINGS_MAX_CONCURRENT_STREAMS, 100 }
                };
                int32_t l_s;
                // client 24 bytes magic string will be sent by nghttp2 library
                l_s = nghttp2_submit_settings(m_ngxxx_session, NGHTTP2_FLAG_NONE, l_iv, ARRLEN(l_iv));
                if(l_s != 0)
                {
                        errx(1, "Could not submit SETTINGS: %s", nghttp2_strerror(l_s));
                }
                // -------------------------------------------------
                // send request
                // -------------------------------------------------
                int32_t l_id;
                //printf("[INFO] path      = %s\n", a_path.c_str());
                //printf("[INFO] authority = %s\n", a_host.c_str());
                // -------------------------------------------------
                // authority note:
                // -------------------------------------------------
                // is the concatenation of host and port with ":" in
                // between.
                // -------------------------------------------------
#define MAKE_NV(NAME, VALUE, VALUELEN) {(uint8_t *) NAME, (uint8_t *)VALUE, sizeof(NAME) - 1, VALUELEN, NGHTTP2_NV_FLAG_NONE}
#define MAKE_NV2(NAME, VALUE)          {(uint8_t *) NAME, (uint8_t *)VALUE, sizeof(NAME) - 1, sizeof(VALUE) - 1, NGHTTP2_NV_FLAG_NONE}

                nghttp2_nv l_hdrs[] = {
                        MAKE_NV(  ":method", m_verb.c_str(), m_verb.length()),
                        MAKE_NV(  ":path",   m_url_path.c_str(), m_url_path.length()),
                        MAKE_NV2( ":scheme", "https"),
                        MAKE_NV(  ":authority", m_host.c_str(), m_host.length()),
                        MAKE_NV2( "accept", "*/*"),
                        MAKE_NV2( "user-agent", "nghttp2/" NGHTTP2_VERSION)
                };
                // print headers
                for(size_t i_h = 0; i_h < ARRLEN(l_hdrs); ++i_h)
                {
                        fprintf(stdout, "%s%.*s%s: %s%.*s%s\n",
                                ANSI_COLOR_FG_BLUE, (int)l_hdrs[i_h].namelen, l_hdrs[i_h].name, ANSI_COLOR_OFF,
                                ANSI_COLOR_FG_GREEN, (int)l_hdrs[i_h].valuelen, l_hdrs[i_h].value, ANSI_COLOR_OFF);
                }
                fprintf(stdout, "\n");
                //fprintf(stderr, "Request headers:\n");
                l_id = nghttp2_submit_request(m_ngxxx_session, NULL, l_hdrs, ARRLEN(l_hdrs), NULL, this);
                if (l_id < 0)
                {
                        errx(1, "Could not submit HTTP request: %s", nghttp2_strerror(l_id));
                }
                //printf("[INFO] Stream ID = %d\n", l_id);
                m_ngxxx_session_stream_id = l_id;
                // -----------------------------------------
                // session send???
                // -----------------------------------------
                l_s = nghttp2_session_send(m_ngxxx_session);
                if (l_s != 0)
                {
                        warnx("Fatal error: %s", nghttp2_strerror(l_s));
                        // TODO
                        //delete_http2_session_data(session_data);
                        return -1;
                }
                m_nconn->set_read_cb(http2_parse);
                m_nconn->set_read_cb_data(this);
        }
        else
        {
                // -----------------------------------------
                // path
                // -----------------------------------------
                // TODO grab from path...
                char l_buf[2048];
                //if(!(a_request.m_url_query.empty()))
                //{
                //        l_path_ref += "?";
                //        l_path_ref += a_request.m_url_query;
                //}
                //NDBG_PRINT("HOST: %s PATH: %s\n", a_reqlet.m_url.m_host.c_str(), l_path_ref.c_str());
                int l_len;
                if(!m_url_query.empty())
                {
                        l_len = snprintf(l_buf, sizeof(l_buf),
                                         "%s %s?%s HTTP/1.1",
                                         m_verb.c_str(),
                                         m_url_path.c_str(),
                                         m_url_query.c_str());
                }
                else
                {
                        l_len = snprintf(l_buf, sizeof(l_buf),
                                         "%s %s HTTP/1.1",
                                         m_verb.c_str(),
                                         m_url_path.c_str());

                }
                ns_hurl::nbq_write_request_line(*m_out_q, l_buf, l_len);
                // -----------------------------------------
                // Add repo headers
                // -----------------------------------------
                bool l_specd_host = false;
                // Loop over reqlet map
                for(ns_hurl::kv_map_list_t::const_iterator i_hl = m_headers.begin();
                    i_hl != m_headers.end();
                    ++i_hl)
                {
                        if(i_hl->first.empty() || i_hl->second.empty()) { continue;}
                        for(ns_hurl::str_list_t::const_iterator i_v = i_hl->second.begin();
                            i_v != i_hl->second.end();
                            ++i_v)
                        {
                                ns_hurl::nbq_write_header(*m_out_q, i_hl->first.c_str(), i_hl->first.length(), i_v->c_str(), i_v->length());
                                if (strcasecmp(i_hl->first.c_str(), "host") == 0)
                                {
                                        l_specd_host = true;
                                }
                        }
                }
                // -----------------------------------------
                // Default Host if unspecified
                // -----------------------------------------
                if(!l_specd_host)
                {
                        ns_hurl::nbq_write_header(*m_out_q,
                                                  "Host", strlen("Host"),
                                                  m_host.c_str(), m_host.length());
                }
                // -----------------------------------------
                // body
                // -----------------------------------------
#if 0
                if(g_conf_body_data && g_conf_body_data_len)
                {
                        //NDBG_PRINT("Write: buf: %p len: %d\n", l_buf, l_len);
                        ns_hurl::nbq_write_body(a_nbq, g_conf_body_data, g_conf_body_data_len);
                }
#else
                if(0){}
#endif
                else
                {
                        ns_hurl::nbq_write_body(*m_out_q, NULL, 0);
                }
                TRC_OUTPUT("%s", ANSI_COLOR_FG_GREEN);
                m_out_q->print();
                TRC_OUTPUT("%s", ANSI_COLOR_OFF);
        }
        printf("%s", ANSI_COLOR_FG_CYAN);
        printf("+------------------------------------------------------------------------------+\n");
        printf("|                              R E S P O N S E                                 |\n");
        printf("+------------------------------------------------------------------------------+\n");
        printf("%s", ANSI_COLOR_OFF);
        return STATUS_OK;
}
//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t request::teardown(ns_hurl::http_status_t a_status)
{
        // -------------------------------------------------
        // TODO PUT BACK!!!
        // -------------------------------------------------
        if(m_timer_obj)
        {
                m_evr_loop->cancel_event(m_timer_obj);
                m_timer_obj = NULL;
        }
#if 0
        if(a_status >= 500)
        {
                ++g_req_num_errors;
                ++g_sum_error;
                // on error

                if(m_nconn)
                {
                        ns_hurl::conn_status_t l_conn_status = ns_hurl::nconn_get_status(*m_nconn);
                        //printf("%s.%s.%d: host:          %s\n",__FILE__,__FUNCTION__,__LINE__,a_subr.get_host().c_str());
                        //printf("%s.%s.%d: m_error_str:   %s\n",__FILE__,__FUNCTION__,__LINE__,l_resp->m_error_str.c_str());
                        //printf("%s.%s.%d: l_conn_status: %d\n",__FILE__,__FUNCTION__,__LINE__,l_conn_status);
                        switch(l_conn_status)
                        {
                        case ns_hurl::CONN_STATUS_CANCELLED:
                        {
                                ++(g_sum_error_conn);
                                break;
                        }
                        case ns_hurl::CONN_STATUS_ERROR_ADDR_LOOKUP_FAILURE:
                        {
                                ++(g_sum_error_addr);
                                break;
                        }
                        case ns_hurl::CONN_STATUS_ERROR_ADDR_LOOKUP_TIMEOUT:
                        {
                                ++(g_sum_error_addr);
                                break;
                        }
                        case ns_hurl::CONN_STATUS_ERROR_CONNECT_TLS:
                        {
                                ++(g_sum_error_conn);
                                // Get last error
                                if(m_nconn)
                                {
                                        SSL *l_tls = ns_hurl::nconn_get_SSL(*m_nconn);
                                        long l_tls_vr = SSL_get_verify_result(l_tls);
                                        if ((l_tls_vr == X509_V_ERR_DEPTH_ZERO_SELF_SIGNED_CERT) ||
                                            (l_tls_vr == X509_V_ERR_SELF_SIGNED_CERT_IN_CHAIN))
                                        {
                                                ++(g_sum_error_tls_self_signed);
                                        }
                                        else if(l_tls_vr == X509_V_ERR_CERT_HAS_EXPIRED)
                                        {
                                                ++(g_sum_error_tls_expired);
                                        }
                                        else if((l_tls_vr == X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT) ||
                                                (l_tls_vr == X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT_LOCALLY))
                                        {
                                                ++(g_sum_error_tls_issuer);
                                        }
                                        else
                                        {
                                                //long l_err = ns_hurl::nconn_get_last_SSL_err(a_nconn);
                                                //printf("ERRORXXXX: %ld\n", l_err);
                                                //printf("ERRORXXXX: %s\n", ERR_error_string(l_err,NULL));
                                                //printf("ERRORXXXX: %s\n", l_resp->m_error_str.c_str());
                                                ++(g_sum_error_tls_other);
                                        }
                                }
                                break;

                        }
                        case ns_hurl::CONN_STATUS_ERROR_CONNECT_TLS_HOST:
                        {
                                ++(g_sum_error_conn);
                                ++(g_sum_error_tls_hostname);
                                break;
                        }
                        default:
                        {
                                //printf("CONN STATUS: %d\n", l_conn_status);
                                ++(g_sum_error_conn);
                                ++(g_sum_error_unknown);
                                break;
                        }
                        }
                }
        }
        else
        {
                ++g_sum_success;
                // on success
                if(m_nconn)
                {
                        SSL *l_SSL = ns_hurl::nconn_get_SSL(*m_nconn);
                        if(l_SSL)
                        {
                                int32_t l_protocol_num = ns_hurl::get_tls_info_protocol_num(l_SSL);
                                m_tls_info_cipher_str = ns_hurl::get_tls_info_cipher_str(l_SSL);
                                m_tls_info_protocol_str = ns_hurl::get_tls_info_protocol_str(l_protocol_num);

                                pthread_mutex_lock(&g_sum_info_mutex);
                                ++(g_sum_info_tls_ciphers[m_tls_info_cipher_str]);
                                ++(g_sum_info_tls_protocols[m_tls_info_protocol_str]);
                                pthread_mutex_unlock(&g_sum_info_mutex);

#if 0
                                SSL_SESSION *m_tls_session = SSL_get_session(m_tls);
                                SSL_SESSION_print_fp(stdout, m_tls_session);
                                X509* l_cert = NULL;
                                l_cert = SSL_get_peer_certificate(m_tls);
                                if(NULL == l_cert)
                                {
                                        NDBG_PRINT("SSL_get_peer_certificate error.  tls: %p\n", m_tls);
                                        return NC_STATUS_ERROR;
                                }
                                X509_print_fp(stdout, l_cert);
#endif
                        }
                }
        }
#endif
        if(m_nconn)
        {
                m_nconn->nc_cleanup();
                delete m_nconn;
                m_nconn = NULL;
        }
        return STATUS_OK;
}
//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t request::run_state_machine(void *a_data, ns_hurl::evr_mode_t a_conn_mode)
{
        //NDBG_PRINT("RUN a_conn_mode: %d a_data: %p\n", a_conn_mode, a_data);
        //CHECK_FOR_NULL_ERROR(a_data);
        // TODO -return OK for a_data == NULL
        if(!a_data)
        {
                return STATUS_OK;
        }
        ns_hurl::nconn* l_nconn = static_cast<ns_hurl::nconn*>(a_data);
        request *l_rx = static_cast<request *>(l_nconn->get_data());
        // -------------------------------------------------
        // ERROR
        // -------------------------------------------------
        if(a_conn_mode == ns_hurl::EVR_MODE_ERROR)
        {
                // ignore errors for free connections
                if(l_nconn->is_free())
                {
                        //TRC_ERROR("call back for free connection\n");
                        return STATUS_OK;
                }
                if(l_rx)
                {
                        l_rx->m_evr_loop->cancel_event(l_rx->m_timer_obj);
                        // TODO Check status
                        l_rx->m_timer_obj = NULL;
                        // TODO FIX!!!
                        return l_rx->teardown(ns_hurl::HTTP_STATUS_BAD_GATEWAY);
                        return STATUS_OK;
                }
                else
                {
                        return STATUS_ERROR;
                }
        }
        // -------------------------------------------------
        // TIMEOUT
        // -------------------------------------------------
        else if(a_conn_mode == ns_hurl::EVR_MODE_TIMEOUT)
        {
                //NDBG_PRINT("%sTIMEOUT%s --HOST: %s\n", ANSI_COLOR_FG_RED, ANSI_COLOR_OFF, l_rx->m_host.c_str());
                // ignore timeout for free connections
                if(l_nconn->is_free())
                {
                        //TRC_ERROR("call back for free connection\n");
                        return STATUS_OK;
                }
                if(l_rx)
                {
                        return l_rx->teardown(ns_hurl::HTTP_STATUS_GATEWAY_TIMEOUT);
                        return STATUS_OK;
                }
                return STATUS_ERROR;
        }
        else if(a_conn_mode == ns_hurl::EVR_MODE_READ)
        {
                // ignore readable for free connections
                if(l_nconn->is_free())
                {
                        //TRC_ERROR("call back for free connection\n");
                        return STATUS_OK;
                }
        }
        // -------------------------------------------------
        // TODO unknown conn mode???
        // -------------------------------------------------
        else if((a_conn_mode != ns_hurl::EVR_MODE_READ) &&
                (a_conn_mode != ns_hurl::EVR_MODE_WRITE))
        {
                //TRC_ERROR("unknown a_conn_mode: %d\n", a_conn_mode);
                return STATUS_OK;
        }
        // -------------------------------------------------
        // in/out q's
        // -------------------------------------------------
        ns_hurl::nbq *l_in_q = NULL;
        ns_hurl::nbq *l_out_q = NULL;
        if(l_rx)
        {
                l_in_q = l_rx->m_in_q;
                l_out_q = l_rx->m_out_q;
        }
        // -------------------------------------------------
        // conn loop
        // -------------------------------------------------
        int32_t l_s = HURL_STATUS_OK;
        do {
                uint32_t l_read = 0;
                uint32_t l_written = 0;
                l_s = l_nconn->nc_run_state_machine(a_conn_mode, l_in_q, l_read, l_out_q, l_written);
                //NDBG_PRINT("l_nconn->nc_run_state_machine(%d): status: %d\n", a_conn_mode, l_s);
                if(!l_rx ||
                   (l_s == ns_hurl::nconn::NC_STATUS_EOF) ||
                   (l_s == ns_hurl::nconn::NC_STATUS_ERROR) ||
                   l_nconn->is_done())
                {
                        goto check_conn_status;
                }
                // -----------------------------------------
                // READABLE
                // -----------------------------------------
                if(a_conn_mode == ns_hurl::EVR_MODE_READ)
                {
                        // -----------------------------------------
                        // Handle completion
                        // -----------------------------------------
                        if(l_rx->m_resp &&
                           l_rx->m_resp->m_complete)
                        {
                                // Cancel timer
                                l_rx->m_evr_loop->cancel_event(l_rx->m_timer_obj);
                                // TODO Check status
                                l_rx->m_timer_obj = NULL;
                                // Get request time
                                if(l_nconn->get_collect_stats_flag())
                                {
                                        //l_nconn->set_stat_tt_completion_us(get_delta_time_us(l_nconn->get_connect_start_time_us()));
                                }
                                l_s = ns_hurl::nconn::NC_STATUS_EOF;
                                goto check_conn_status;
                        }
                }
                // -----------------------------------------
                // STATUS_OK
                // -----------------------------------------
                else if(l_s == ns_hurl::nconn::NC_STATUS_OK)
                {
                        l_s = ns_hurl::nconn::NC_STATUS_BREAK;
                        goto check_conn_status;
                }
check_conn_status:
                if(l_nconn->is_free())
                {
                        return STATUS_OK;
                }
                if(!l_rx)
                {
                        //TRC_ERROR("no ups_srvr_session associated with nconn mode: %d\n", a_conn_mode);
                        return STATUS_ERROR;
                }
                switch(l_s)
                {
                case ns_hurl::nconn::NC_STATUS_BREAK:
                {
                        goto done;
                }
                case ns_hurl::nconn::NC_STATUS_EOF:
                {
                        return l_rx->teardown(ns_hurl::HTTP_STATUS_OK);
                }
                case ns_hurl::nconn::NC_STATUS_ERROR:
                {
                        return l_rx->teardown(ns_hurl::HTTP_STATUS_BAD_GATEWAY);
                }
                default:
                {
                        break;
                }
                }
                if(l_nconn->is_done())
                {
                        return l_rx->teardown(ns_hurl::HTTP_STATUS_OK);
                }
        } while((l_s != ns_hurl::nconn::NC_STATUS_AGAIN));
done:
        return STATUS_OK;
}
//: ****************************************************************************
//: ************************** H 2   P A R S E *********************************
//: ****************************************************************************
//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t http2_parse(void *a_data, char *a_buf, uint32_t a_len, uint64_t a_off)
{
        if(!a_data)
        {
                return HURL_STATUS_ERROR;
        }
        request *l_request = (request *)a_data;
        //NDBG_PRINT("%sREAD%s: a_len: %d\n", ANSI_COLOR_FG_RED, ANSI_COLOR_OFF, a_len);
        //if(a_len > 0) ns_hurl::mem_display((uint8_t *)a_buf, a_len, true);
        ssize_t l_rl;
        l_rl = nghttp2_session_mem_recv(l_request->m_ngxxx_session, (const uint8_t *)a_buf, a_len);
        if(l_rl < 0)
        {
                warnx("Fatal error: %s", nghttp2_strerror((int) l_rl));
                // TODO
                //delete_http2_session_data(session_data);
                NDBG_PRINT("Fatal error: %s", nghttp2_strerror((int) l_rl));;
                return HURL_STATUS_ERROR;
        }
        if(l_request->m_ngxxx_session_stream_closed)
        {
                //NDBG_PRINT("%sREAD%s: marking complete\n", ANSI_COLOR_BG_RED, ANSI_COLOR_OFF);
                l_request->m_resp->m_complete = true;
        }
        return HURL_STATUS_OK;
}
//: ****************************************************************************
//: ************************ N P N  S U P P O R T ******************************
//: ****************************************************************************
//: ----------------------------------------------------------------------------
//: \details: NPN TLS extension client callback. We check that server advertised
//:           the HTTP/2 protocol the nghttp2 library supports. If not, exit
//:           the program.
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
static int npn_select_next_proto_cb(SSL *a_ssl _U_,
                                    unsigned char **a_out,
                                    unsigned char *a_outlen,
                                    const unsigned char *a_in,
                                    unsigned int a_inlen,
                                    void *a_arg)
{
        printf("%s", ANSI_COLOR_FG_WHITE);
        printf("+------------------------------------------------------------------------------+\n");
        printf("|                               T L S   N P N                                  |\n");
        printf("+------------------------------------------------------------------------------+\n");
        printf("%s", ANSI_COLOR_OFF);
        ns_hurl::mem_display((const uint8_t *)a_in, a_inlen, true);
        if(nghttp2_select_next_protocol(a_out, a_outlen, a_in, a_inlen) <= 0)
        {
                //TRC_DEBUG("Server did not advertise %s\n", NGHTTP2_PROTO_VERSION_ID);
        }
        else
        {
                if(a_arg)
                {
                        //NDBG_PRINT("setting HTTP_PROTOCOL_V2_TLS\n");
                        ((request *)a_arg)->m_conf_hp_type = request::HTTP_PROTOCOL_V2_TLS;
                }
        }
        return SSL_TLSEXT_ERR_OK;
}
//: ----------------------------------------------------------------------------
//: \details: Print the version.
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void print_version(FILE* a_stream, int a_exit_code)
{
        fprintf(a_stream, "h2url: http2 curl utility\n");
        fprintf(a_stream, "Copyright (C) 2017 Verizon Digital Media.\n");
        fprintf(a_stream, "               Version: %s\n", HURL_VERSION);
        exit(a_exit_code);
}
//: ----------------------------------------------------------------------------
//: \details: Print the command line help.
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void print_usage(FILE* a_stream, int a_exit_code)
{
        fprintf(a_stream, "Usage: nghttp2_client_ex [http[s]://]hostname[:port]/path [options]\n");
        fprintf(a_stream, "Options are:\n");
        fprintf(a_stream, "  -h, --help           Display this help and exit.\n");
        fprintf(a_stream, "  -V, --version        Display the version number and exit.\n");
        fprintf(a_stream, "  \n");
        fprintf(a_stream, "Settings:\n");
#if 0
        fprintf(a_stream, "  -d, --data           HTTP body data -supports curl style @ file specifier\n");
#endif
#if 0
        fprintf(a_stream, "  -H, --header         Request headers -can add multiple ie -H<> -H<>...\n");
#endif
        fprintf(a_stream, "  -X, --verb           Request command -HTTP verb to use -GET/PUT/etc. Default GET\n");
        fprintf(a_stream, "  \n");
#if 0
        fprintf(a_stream, "TLS Settings:\n");
        fprintf(a_stream, "  -y, --cipher         Cipher --see \"openssl ciphers\" for list.\n");
        fprintf(a_stream, "  -O, --tls_options    SSL Options string.\n");
        fprintf(a_stream, "  -K, --tls_verify     Verify server certificate.\n");
        fprintf(a_stream, "  -N, --tls_sni        Use SSL SNI.\n");
        fprintf(a_stream, "  -B, --tls_self_ok    Allow self-signed certificates.\n");
        fprintf(a_stream, "  -M, --tls_no_host    Skip host name checking.\n");
        fprintf(a_stream, "  -F, --tls_ca_file    SSL CA File.\n");
        fprintf(a_stream, "  -L, --tls_ca_path    SSL CA Path.\n");
        fprintf(a_stream, "  \n");
#endif
#if 0
        fprintf(a_stream, "Output Options: -defaults to line delimited\n");
        fprintf(a_stream, "  -o, --output         File to write output to. Defaults to stdout\n");
        fprintf(a_stream, "  \n");
#endif
#if 0
        fprintf(a_stream, "Print Options:\n");
        fprintf(a_stream, "  -v, --verbose        Verbose logging\n");
        fprintf(a_stream, "  -c, --no_color       Turn off colors\n");
        fprintf(a_stream, "  \n");
#endif
#if 0
        fprintf(a_stream, "Debug Options:\n");
        fprintf(a_stream, "  -r, --trace          Turn on tracing (error/warn/debug/verbose/all)\n");
        fprintf(a_stream, "  \n");
#endif
        exit(a_exit_code);
}
//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int main(int argc, char** argv)
{
        request *l_request = new request();
        bool l_conf_verbose = false;
        bool l_conf_color = true;
        std::string l_output_file = "";
        // TODO REMOVE!!!
        UNUSED(l_conf_verbose);
        UNUSED(l_conf_color);
        // TODO REMOVE!!!
        //ns_hurl::trc_log_level_set(ns_hurl::TRC_LOG_LEVEL_ALL);
        //sns_hurl::trc_log_file_open("/dev/stdout");
        // -------------------------------------------------
        // tty???
        // -------------------------------------------------
        if(isatty(fileno(stdout)) == 0)
        {
                l_conf_color = false;
        }
        // -------------------------------------------------
        // defaults
        // -------------------------------------------------
        // defaults from nghttp2 client example
        l_request->m_conf_tls_options =
                SSL_OP_ALL |
                SSL_OP_NO_SSLv2 |
                SSL_OP_NO_SSLv3 |
                SSL_OP_NO_COMPRESSION |
                SSL_OP_NO_SESSION_RESUMPTION_ON_RENEGOTIATION;
        // -------------------------------------------------
        // Get args...
        // -------------------------------------------------
        char l_opt;
        std::string l_arg;
        int l_option_index = 0;
        struct option l_long_options[] =
                {
                { "help",           0, 0, 'h' },
                { "version",        0, 0, 'V' },
                { "data",           1, 0, 'd' },
                { "header",         1, 0, 'H' },
                { "verb",           1, 0, 'X' },
                { "cipher",         1, 0, 'y' },
                { "tls_options",    1, 0, 'O' },
                { "tls_verify",     0, 0, 'K' },
                { "tls_sni",        0, 0, 'N' },
                { "tls_self_ok",    0, 0, 'B' },
                { "tls_no_host",    0, 0, 'M' },
                { "tls_ca_file",    1, 0, 'F' },
                { "tls_ca_path",    1, 0, 'L' },
                { "output",         1, 0, 'o' },
                { "verbose",        0, 0, 'v' },
                { "no_color",       0, 0, 'c' },
                { "trace",          1, 0, 'r' },
                // list sentinel
                { 0, 0, 0, 0 }
        };
        // -------------------------------------------------
        // Assume unspecified arg url...
        // TODO Unsure if good way to allow unspecified
        // arg...
        // -------------------------------------------------
        bool is_opt = false;
        std::string l_url;
        for(int i_arg = 1; i_arg < argc; ++i_arg) {
                if(argv[i_arg][0] == '-') {
                        is_opt = true;
                }
                else if(argv[i_arg][0] != '-' && is_opt == false) {
                        l_url = std::string(argv[i_arg]);
                        break;
                } else {
                        is_opt = false;
                }
        }
        // -------------------------------------------------
        // Args...
        // -------------------------------------------------
        char l_short_arg_list[] = "hVd:H:X:y:O:KNBMF:L:vcr:";
        while ((l_opt = getopt_long_only(argc, argv, l_short_arg_list, l_long_options, &l_option_index)) != -1)
        {

                if (optarg)
                {
                        l_arg = std::string(optarg);
                }
                else
                {
                        l_arg.clear();
                }
                //printf("arg[%c=%d]: %s\n", l_opt, l_option_index, l_arg.c_str());
                switch (l_opt)
                {
                // -----------------------------------------
                // help
                // -----------------------------------------
                case 'h':
                {
                        print_usage(stdout, 0);
                        break;
                }
                // -----------------------------------------
                // version
                // -----------------------------------------
                case 'V':
                {
                        print_version(stdout, 0);
                        break;
                }
#if 0
                // -----------------------------------------
                // data
                // -----------------------------------------
                case 'd':
                {
                        // TODO Size limits???
                        int32_t l_s;
                        // If a_data starts with @ assume file
                        if(l_arg[0] == '@')
                        {
                                char *l_buf;
                                uint32_t l_len;
                                l_s = ns_hurl::read_file(l_arg.data() + 1, &(l_buf), &(l_len));
                                if(l_s != 0)
                                {
                                        printf("Error reading body data from file: %s\n", l_arg.c_str() + 1);
                                        return STATUS_ERROR;
                                }
                                l_request->m_body_data = l_buf;
                                l_request->m_body_data_len = l_len;
                        }
                        else
                        {
                                char *l_buf;
                                uint32_t l_len;
                                l_len = l_arg.length() + 1;
                                l_buf = (char *)malloc(sizeof(char)*l_len);
                                l_request->m_body_data = l_buf;
                                l_request->m_body_data_len = l_len;
                        }

                        // Add content length
                        char l_len_str[64];
                        sprintf(l_len_str, "%u", l_request->m_body_data_len);
                        l_request->set_header("Content-Length", l_len_str);
                        break;
                }
#endif
#if 0
                // -----------------------------------------
                // header
                // -----------------------------------------
                case 'H':
                {
                        int32_t l_s;
                        std::string l_key;
                        std::string l_val;
                        l_s = ns_hurl::break_header_string(l_arg, l_key, l_val);
                        if (l_s != 0)
                        {
                                printf("Error breaking header string: %s -not in <HEADER>:<VAL> format?\n", l_arg.c_str());
                                return STATUS_ERROR;
                        }
                        l_s = l_request->set_header(l_key, l_val);
                        if (l_s != 0)
                        {
                                printf("Error performing set_header: %s\n", l_arg.c_str());
                                return STATUS_ERROR;
                        }
                        break;
                }
#endif
                // -----------------------------------------
                // verb
                // -----------------------------------------
                case 'X':
                {
                        if(l_arg.length() > 64)
                        {
                                printf("Error verb string: %s too large try < 64 chars\n", l_arg.c_str());
                                return STATUS_ERROR;
                        }
                        l_request->m_verb = l_arg;
                        break;
                }
#if 0
                // -----------------------------------------
                // cipher
                // -----------------------------------------
                case 'y':
                {
                        l_request->m_conf_tls_cipher_list = l_arg;
                        break;
                }
                // -----------------------------------------
                // tls options
                // -----------------------------------------
                case 'O':
                {
                        int32_t l_s;
                        long l_tls_options;
                        l_s = ns_hurl::get_tls_options_str_val(l_arg, l_tls_options);
                        if(l_s != HURL_STATUS_OK)
                        {
                                return STATUS_ERROR;
                        }
                        l_request->m_conf_tls_options = l_tls_options;
                        break;
                }
                // -----------------------------------------
                // tls verify
                // -----------------------------------------
                case 'K':
                {
                        l_request->m_conf_tls_verify = true;
                        break;
                }
                // -----------------------------------------
                // tls sni
                // -----------------------------------------
                case 'N':
                {
                        l_request->m_conf_tls_sni = true;
                        break;
                }
                // -----------------------------------------
                // tls self signed
                // -----------------------------------------
                case 'B':
                {
                        l_request->m_conf_tls_self_ok = true;
                        break;
                }
                // -----------------------------------------
                // tls skip host check
                // -----------------------------------------
                case 'M':
                {
                        l_request->m_conf_tls_no_host_check = true;
                        break;
                }
                // -----------------------------------------
                // tls ca file
                // -----------------------------------------
                case 'F':
                {
                        l_request->m_conf_tls_ca_file = l_arg;
                        break;
                }
                // -----------------------------------------
                // tls ca path
                // -----------------------------------------
                case 'L':
                {
                        l_request->m_conf_tls_ca_path = l_arg;
                        break;
                }
#endif
#if 0
                // -----------------------------------------
                // output file
                // -----------------------------------------
                case 'o':
                {
                        l_output_file = l_arg;
                        break;
                }
#endif
#if 0
                // -----------------------------------------
                // verbose
                // -----------------------------------------
                case 'v':
                {
                        l_conf_verbose = true;
                        break;
                }
#endif
#if 0
                // -----------------------------------------
                // color
                // -----------------------------------------
                case 'c':
                {
                        l_conf_color = false;
                        break;
                }
#endif
#if 0
                // -----------------------------------------
                // trace
                // -----------------------------------------
                case 'r':
                {
#define ELIF_TRACE_STR(_level) else if(strncasecmp(_level, l_arg.c_str(), sizeof(_level)) == 0)
                        bool l_trace = false;
                        if(0) {}
                        ELIF_TRACE_STR("error") { ns_hurl::trc_log_level_set(ns_hurl::TRC_LOG_LEVEL_ERROR); l_trace = true; }
                        ELIF_TRACE_STR("warn") { ns_hurl::trc_log_level_set(ns_hurl::TRC_LOG_LEVEL_WARN); l_trace = true; }
                        ELIF_TRACE_STR("debug") { ns_hurl::trc_log_level_set(ns_hurl::TRC_LOG_LEVEL_DEBUG); l_trace = true; }
                        ELIF_TRACE_STR("verbose") { ns_hurl::trc_log_level_set(ns_hurl::TRC_LOG_LEVEL_VERBOSE); l_trace = true; }
                        ELIF_TRACE_STR("all") { ns_hurl::trc_log_level_set(ns_hurl::TRC_LOG_LEVEL_ALL); l_trace = true; }
                        else
                        {
                                ns_hurl::trc_log_level_set(ns_hurl::TRC_LOG_LEVEL_NONE);
                        }
                        if(l_trace)
                        {
                                ns_hurl::trc_log_file_open("/dev/stdout");
                        }
                        break;
                }
#endif
                // -----------------------------------------
                // What???
                // -----------------------------------------
                case '?':
                {
                        // Required argument was missing
                        // '?' is provided when the 3rd arg to getopt_long does not begin with a ':', and is preceeded
                        // by an automatic error message.
                        printf("exiting...\n");
                        print_usage(stdout, STATUS_ERROR);
                        break;
                }
                // -----------------------------------------
                // Huh???
                // -----------------------------------------
                default:
                {
                        printf("unrecognized option.\n");
                        print_usage(stdout, STATUS_ERROR);
                        break;
                }
                }
        }
        // -------------------------------------------------
        // verify input
        // -------------------------------------------------
        if(l_url.empty())
        {
                printf("error: url required.");
                print_usage(stdout, STATUS_ERROR);
        }
        int32_t l_s;
        l_s = l_request->init_with_url(l_url);
        if(l_s != HURL_STATUS_OK)
        {
                printf("error: performing init_with_url.");
                print_usage(stdout, STATUS_ERROR);
        }
        // -------------------------------------------------
        // resolve
        // -------------------------------------------------
        ns_hurl::host_info l_host_info;
        l_s = ns_hurl::nlookup(l_request->m_host, l_request->m_port, l_host_info);
        if(l_s != HURL_STATUS_OK)
        {
                printf("error: resolving: %s:%d\n", l_request->m_host.c_str(), l_request->m_port);
                return HURL_STATUS_ERROR;
        }
        l_request->m_nconn->set_host_info(l_host_info);
        // -------------------------------------------------
        // create request
        // -------------------------------------------------
        request::evr_fd_writeable_cb(l_request->m_nconn);
        // -------------------------------------------------
        // run
        // -------------------------------------------------
        //NDBG_PRINT("Running.\n");
        while(l_request->m_resp->m_complete == false)
        {
                //NDBG_PRINT("Running.\n");
                l_s = l_request->m_evr_loop->run();
                if(l_s != HURL_STATUS_OK)
                {
                        // TODO log run failure???
                        //NDBG_PRINT("error performing l_request->m_evr_loop->run\n");
                }
        }
        // -------------------------------------------------
        // response
        // -------------------------------------------------
        if((l_request->m_resp) &&
           (l_request->m_conf_hp_type != request::HTTP_PROTOCOL_V2_TLS))
        {
                l_request->m_resp->show();
        }
        // -------------------------------------------------
        // Cleanup...
        // -------------------------------------------------
        if(l_request) {delete l_request; l_request = NULL;}
        //printf("Cleaning up...\n");
        return 0;
}
