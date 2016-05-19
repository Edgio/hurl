//: ----------------------------------------------------------------------------
//: Copyright (C) 2016 Verizon.  All Rights Reserved.
//: All Rights Reserved
//:
//: \file:    access.h
//: \details: TODO
//: \author:  Reed P. Morrison
//: \date:    04/09/2016
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
#ifndef _ACCESS_H
#define _ACCESS_H

//: ----------------------------------------------------------------------------
//: Includes
//: ----------------------------------------------------------------------------
#include "hlx/scheme.h"
#include "hlx/http_status.h"
#include <sys/socket.h>
#include <string>

namespace ns_hlx {

//: ----------------------------------------------------------------------------
//: Info
//: ----------------------------------------------------------------------------
class access_info {

public:
        // -------------------------------------------------
        // Public members
        // -------------------------------------------------
        // -----------------------------------
        // Connection info
        // -----------------------------------
        struct sockaddr_storage m_conn_clnt_sa;
        socklen_t m_conn_clnt_sa_len;

        struct sockaddr_storage m_conn_upsv_sa;
        socklen_t m_conn_upsv_sa_len;

        // -----------------------------------
        // Request info
        // -----------------------------------
        std::string m_rqst_host;
        scheme_t m_rqst_scheme;
        const char *m_rqst_method;
        uint8_t m_rqst_http_major;
        uint8_t m_rqst_http_minor;
        std::string m_rqst_request;
        std::string m_rqst_query_string;
        std::string m_rqst_http_user_agent;
        std::string m_rqst_http_referer;
        uint64_t m_rqst_request_length;

        // TODO reuse/expose method enumeration from http-parser
        //uint16_t m_rqst_request_method;

        // -----------------------------------
        // Response info
        // -----------------------------------
        // local time in the "Common Log Format"
        std::string m_resp_time_local;
        http_status_t m_resp_status;
        uint64_t m_bytes_in;
        uint64_t m_bytes_out;
        uint64_t m_start_time_ms;
        uint64_t m_total_time_ms;

        // -------------------------------------------------
        // Public methods
        // -------------------------------------------------
        access_info(void);
        ~access_info(void);
private:
        // -------------------------------------------------
        // Private methods
        // -------------------------------------------------
        // Disallow copy/assign
        access_info& operator=(const access_info &);
        access_info(const access_info &);
};

}

#endif

