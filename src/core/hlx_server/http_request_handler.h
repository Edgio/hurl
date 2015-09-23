//: ----------------------------------------------------------------------------
//: Copyright (C) 2014 Verizon.  All Rights Reserved.
//: All Rights Reserved
//:
//: \file:    http_request_handler.h
//: \details: TODO
//: \author:  Reed P. Morrison
//: \date:    08/28/2015
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
#ifndef _HTTP_REQUEST_HANDLER_H
#define _HTTP_REQUEST_HANDLER_H

//: ----------------------------------------------------------------------------
//: Includes
//: ----------------------------------------------------------------------------
#include <stdint.h>
#include "http.h"
#include "url_router.h"

namespace ns_hlx {

//: ----------------------------------------------------------------------------
//: http_request_handler
//: ----------------------------------------------------------------------------
class http_request_handler
{
public:
        // -------------------------------------------------
        // Public methods
        // -------------------------------------------------
        http_request_handler(void) {};
        virtual ~http_request_handler(){};

        virtual int32_t do_get(const url_param_map_t &a_url_param_map, http_req &a_request, http_resp &ao_response) = 0;
        virtual int32_t do_post(const url_param_map_t &a_url_param_map, http_req &a_request, http_resp &ao_response) = 0;
        virtual int32_t do_put(const url_param_map_t &a_url_param_map, http_req &a_request, http_resp &ao_response) = 0;
        virtual int32_t do_delete(const url_param_map_t &a_url_param_map, http_req &a_request, http_resp &ao_response) = 0;
        virtual int32_t do_default(const url_param_map_t &a_url_param_map, http_req &a_request, http_resp &ao_response) = 0;

        // -------------------------------------------------
        // Public members
        // -------------------------------------------------
        int32_t get_file(const std::string &a_path, const http_req &a_request, http_resp &ao_response);
        int32_t send_not_found(const http_req &a_request, http_resp &ao_response, const char *a_resp_str);
private:
        // -------------------------------------------------
        // Private methods
        // -------------------------------------------------
        DISALLOW_COPY_AND_ASSIGN(http_request_handler)
};

//: ----------------------------------------------------------------------------
//: default_http_request_handler
//: ----------------------------------------------------------------------------
class default_http_request_handler: public http_request_handler
{
public:
        // -------------------------------------------------
        // Public methods
        // -------------------------------------------------
        default_http_request_handler(void);
        ~default_http_request_handler();

        int32_t do_get(const url_param_map_t &a_url_param_map, http_req &a_request, http_resp &ao_response);
        int32_t do_post(const url_param_map_t &a_url_param_map, http_req &a_request, http_resp &ao_response);
        int32_t do_put(const url_param_map_t &a_url_param_map, http_req &a_request, http_resp &ao_response);
        int32_t do_delete(const url_param_map_t &a_url_param_map, http_req &a_request, http_resp &ao_response);
        int32_t do_default(const url_param_map_t &a_url_param_map, http_req &a_request, http_resp &ao_response);
private:
        // -------------------------------------------------
        // Private methods
        // -------------------------------------------------
        DISALLOW_COPY_AND_ASSIGN(default_http_request_handler)
};

} //namespace ns_hlx {

#endif // #ifndef _HLX_CLIENT_H





