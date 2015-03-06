//: ----------------------------------------------------------------------------
//: Copyright (C) 2014 Verizon.  All Rights Reserved.
//: All Rights Reserved
//:
//: \file:    ssl_util.h
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
//: Includes
//: ----------------------------------------------------------------------------
#include <string>
#include <openssl/ssl.h>

//: ----------------------------------------------------------------------------
//: Fwd Decl's
//: ----------------------------------------------------------------------------
SSL_CTX* ssl_init(const std::string &a_cipher_list,
		  long a_options = 0,
		  const std::string &a_ca_file = "",
		  const std::string &a_ca_path = "");
void ssl_kill_locks(void);
int32_t get_ssl_options_str_val(const std::string a_options_str, long &ao_val);
int32_t get_ssl_session_info(SSL *a_ssl, std::string &ao_protocol, std::string &ao_cipher);
#endif
