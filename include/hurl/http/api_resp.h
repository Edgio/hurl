//: ----------------------------------------------------------------------------
//: Copyright (C) 2016 Verizon.  All Rights Reserved.
//: All Rights Reserved
//:
//: \file:    api_resp.h
//: \details: TODO
//: \author:  Reed P. Morrison
//: \date:    03/11/2015
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
#ifndef _API_RESP_H
#define _API_RESP_H

//: ----------------------------------------------------------------------------
//: Includes
//: ----------------------------------------------------------------------------
#include "hurl/http/http_status.h"
#include "hurl/support/kv_map_list.h"

// For fixed size types
#include <stdint.h>
#include <string>

namespace ns_hurl {

//: ----------------------------------------------------------------------------
//: Fwd Decl's
//: ----------------------------------------------------------------------------
class nbq;
class rqst;
class srvr;

//: ----------------------------------------------------------------------------
//: api_resp
//: ----------------------------------------------------------------------------
class api_resp
{
public:
        // -------------------------------------------------
        // Public methods
        // -------------------------------------------------
        api_resp(void);
        ~api_resp();

        // Request Parts
        // Getters
        const kv_map_list_t &get_headers(void);
        http_status_t get_status(void);

        // Setters
        void set_status(http_status_t a_status);
        int set_header(const std::string &a_header);
        int set_header(const std::string &a_key, const std::string &a_val);
        void set_headers(const kv_map_list_t &a_headers_list);
        int set_headerf(const std::string &a_key, const char* fmt, ...) __attribute__((format(__printf__,3,4)));;
        void set_body_data(const char *a_ptr, uint32_t a_len);
        void add_std_headers(http_status_t a_status,
                             const char *a_content_type,
                             uint64_t a_len,
                             bool a_keep_alive,
                             const std::string &a_server_name);
        // Clear
        void clear_headers(void);

        // Serialize to q for sending.
        int32_t serialize(nbq &ao_q);

private:
        // -------------------------------------------------
        // Private methods
        // -------------------------------------------------
        // Disallow copy/assign
        api_resp& operator=(const api_resp &);
        api_resp(const api_resp &);

        // -------------------------------------------------
        // Private members
        // -------------------------------------------------
        http_status_t m_status;
        kv_map_list_t m_headers;
        const char *m_body_data;
        uint32_t m_body_data_len;
};

//: ----------------------------------------------------------------------------
//: helpers
//: ----------------------------------------------------------------------------
const char *get_resp_status_str(http_status_t a_status);

//: ----------------------------------------------------------------------------
//: nbq_utils
//: ----------------------------------------------------------------------------
int32_t nbq_write_request_line(nbq &ao_q, const char *a_buf, uint32_t a_len);
int32_t nbq_write_status(nbq &ao_q, http_status_t a_status);
int32_t nbq_write_header(nbq &ao_q,
                         const char *a_key_buf, uint32_t a_key_len,
                         const char *a_val_buf, uint32_t a_val_len);
int32_t nbq_write_header(nbq &ao_q, const char *a_key_buf, const char *a_val_buf);
int32_t nbq_write_body(nbq &ao_q, const char *a_buf, uint32_t a_len);
void create_json_resp_str(ns_hurl::http_status_t a_status, std::string &ao_resp_str);

}

#endif

