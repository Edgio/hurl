//: ----------------------------------------------------------------------------
//: Copyright (C) 2016 Verizon.  All Rights Reserved.
//: All Rights Reserved
//:
//: \file:    tls_util.h
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
#ifndef _SSL_UTIL_H
#define _SSL_UTIL_H
//: ----------------------------------------------------------------------------
//: includes
//: ----------------------------------------------------------------------------
#include <string>
#include <vector>
//: ----------------------------------------------------------------------------
//: ext fwd decl's
//: ----------------------------------------------------------------------------
typedef struct ssl_ctx_st SSL_CTX;
typedef struct ssl_st SSL;
typedef struct x509_store_ctx_st X509_STORE_CTX;
typedef struct x509_st X509;
namespace ns_hurl {
//: ----------------------------------------------------------------------------
//: globals
//: ----------------------------------------------------------------------------
extern __thread char gts_last_tls_error[256];
//: ----------------------------------------------------------------------------
//: prototypes
//: ----------------------------------------------------------------------------
void tls_init(void);
void tls_kill_locks(void);
int32_t tls_cleanup(void);
int32_t get_tls_options_str_val(const std::string a_options_str, long &ao_val);
const char *get_tls_info_cipher_str(SSL *a_ssl);
const char *get_tls_info_protocol_str(int32_t a_version);
int32_t get_tls_info_protocol_num(SSL *a_ssl);
int32_t validate_server_certificate(SSL *a_tls, const char* a_host, bool a_disallow_self_signed);
int tls_cert_verify_callback_allow_self_signed(int ok, X509_STORE_CTX* store);
int tls_cert_verify_callback(int ok, X509_STORE_CTX* store);
bool tls_x509_get_ids(X509* x509, std::vector<std::string>& ids);
} //namespace ns_hurl {
#endif
