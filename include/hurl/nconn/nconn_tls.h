//: ----------------------------------------------------------------------------
//: Copyright (C) 2016 Verizon.  All Rights Reserved.
//: All Rights Reserved
//:
//: \file:    nconn_tls.h
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
#ifndef _NCONN_TLS_H
#define _NCONN_TLS_H
//: ----------------------------------------------------------------------------
//: includes
//: ----------------------------------------------------------------------------
#include "nconn_tcp.h"
//: ----------------------------------------------------------------------------
//: ext fwd decl's
//: ----------------------------------------------------------------------------
typedef struct ssl_ctx_st SSL_CTX;
typedef struct ssl_st SSL;
namespace ns_hurl {
//: ----------------------------------------------------------------------------
//: \details: TODO
//: ----------------------------------------------------------------------------
class nconn_tls: public nconn_tcp
{
public:
        // ---------------------------------------
        // Connection state
        // ---------------------------------------
        typedef enum tls_state
        {
                TLS_STATE_NONE,
                TLS_STATE_LISTENING,
                TLS_STATE_CONNECTING,
                TLS_STATE_ACCEPTING,
                // Connecting
                TLS_STATE_TLS_CONNECTING,
                TLS_STATE_TLS_CONNECTING_WANT_READ,
                TLS_STATE_TLS_CONNECTING_WANT_WRITE,
                // Accepting
                TLS_STATE_TLS_ACCEPTING,
                TLS_STATE_TLS_ACCEPTING_WANT_READ,
                TLS_STATE_TLS_ACCEPTING_WANT_WRITE,
                TLS_STATE_CONNECTED,
                TLS_STATE_READING,
                TLS_STATE_WRITING,
                TLS_STATE_DONE
        } tls_state_t;
        // ---------------------------------------
        // Options
        // ---------------------------------------
        typedef enum tls_opt_enum
        {
                OPT_TLS_CTX = 1000,
                // Settings
                OPT_TLS_CIPHER_STR = 1001,
                OPT_TLS_OPTIONS = 1002,
                OPT_TLS_SSL = 1003,
                OPT_TLS_SSL_LAST_ERR = 1004,
                // Verify options
                OPT_TLS_VERIFY = 1100,
                OPT_TLS_SNI = 1102,
                OPT_TLS_HOSTNAME = 1103,
                OPT_TLS_VERIFY_ALLOW_SELF_SIGNED = 1105,
                OPT_TLS_VERIFY_NO_HOST_CHECK = 1106,
                // CA options
                OPT_TLS_CA_FILE = 1201,
                OPT_TLS_CA_PATH = 1202,
                // Server config
                OPT_TLS_TLS_KEY = 1301,
                OPT_TLS_TLS_CRT = 1302,
                // sentinel
                OPT_TLS_SENTINEL = 1999
        } tls_opt_t;
        // ---------------------------------------
        // Public methods
        // ---------------------------------------
        nconn_tls():
          nconn_tcp(),
          m_tls_ctx(NULL),
          m_tls(NULL),
          m_tls_opt_verify(false),
          m_tls_opt_sni(false),
          m_tls_opt_verify_allow_self_signed(false),
          m_tls_opt_verify_no_host_check(false),
          m_tls_opt_hostname(""),
          m_tls_opt_ca_file(""),
          m_tls_opt_ca_path(""),
          m_tls_opt_options(0),
          m_tls_opt_cipher_str(""),
          m_tls_key(""),
          m_tls_crt(""),
          m_tls_state(TLS_STATE_NONE),
          m_last_err(0)
        {
                m_scheme = SCHEME_TLS;
        };
        // Destructor
        ~nconn_tls() {};
        int32_t set_opt(uint32_t a_opt, const void *a_buf, uint32_t a_len);
        int32_t get_opt(uint32_t a_opt, void **a_buf, uint32_t *a_len);
        bool is_listening(void) {return (m_tls_state == TLS_STATE_LISTENING);};
        bool is_connecting(void) {return ((m_tls_state == TLS_STATE_CONNECTING) ||
                                          (m_tls_state == TLS_STATE_TLS_CONNECTING) ||
                                          (m_tls_state == TLS_STATE_TLS_CONNECTING_WANT_READ) ||
                                          (m_tls_state == TLS_STATE_TLS_CONNECTING_WANT_WRITE));};
        bool is_accepting(void) {return ((m_tls_state == TLS_STATE_ACCEPTING) ||
                                         (m_tls_state == TLS_STATE_TLS_ACCEPTING) ||
                                         (m_tls_state == TLS_STATE_TLS_ACCEPTING_WANT_READ) ||
                                         (m_tls_state == TLS_STATE_TLS_ACCEPTING_WANT_WRITE));};
        // -------------------------------------------------
        // virtual methods
        // -------------------------------------------------
        int32_t ncsetup();
        int32_t ncread(char *a_buf, uint32_t a_buf_len);
        int32_t ncwrite(char *a_buf, uint32_t a_buf_len);
        int32_t ncaccept();
        int32_t ncconnect();
        int32_t nccleanup();
        int32_t ncset_listening(int32_t a_val);
        int32_t ncset_listening_nb(int32_t a_val);
        int32_t ncset_accepting(int a_fd);
        int32_t ncset_connected(void);
private:
        // -------------------------------------------------
        // Private methods
        // -------------------------------------------------
        nconn_tls& operator=(const nconn_tls &);
        nconn_tls(const nconn_tls &);
        int32_t tls_connect(void);
        int32_t tls_accept(void);
        int32_t init(void);
        // -------------------------------------------------
        // Private members
        // -------------------------------------------------
        SSL_CTX * m_tls_ctx;
        SSL *m_tls;
        bool m_tls_opt_verify;
        bool m_tls_opt_sni;
        bool m_tls_opt_verify_allow_self_signed;
        bool m_tls_opt_verify_no_host_check;
        std::string m_tls_opt_hostname;
        std::string m_tls_opt_ca_file;
        std::string m_tls_opt_ca_path;
        long m_tls_opt_options;
        std::string m_tls_opt_cipher_str;
        std::string m_tls_key;
        std::string m_tls_crt;
        tls_state_t m_tls_state;
        long m_last_err;
        // -------------------------------------------------
        // Private static methods
        // -------------------------------------------------
        static int alpn_select_next_proto_cb(SSL *a_ssl,
                                             unsigned char **a_out,
                                             unsigned char *a_outlen,
                                             const unsigned char *a_in,
                                             unsigned int a_inlen,
                                             void *a_arg);
};
//: ----------------------------------------------------------------------------
//: \prototypes:
//: ----------------------------------------------------------------------------
SSL_CTX* tls_init_ctx(const std::string &a_cipher_list,
                      long a_options = 0,
                      const std::string &a_ca_file = "",
                      const std::string &a_ca_path = "",
                      bool a_server_flag = false,
                      const std::string &a_tls_key_file = "",
                      const std::string &a_tls_crt_file = "");
int32_t show_tls_info(nconn *a_nconn);
} //namespace ns_hurl {

#endif
