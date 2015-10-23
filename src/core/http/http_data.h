//: ----------------------------------------------------------------------------
//: Copyright (C) 2015 Verizon.  All Rights Reserved.
//: All Rights Reserved
//:
//: \file:    http_data.h
//: \details: TODO
//: \author:  Reed P. Morrison
//: \date:    07/20/2015
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
#ifndef _HTTP_DATA_H
#define _HTTP_DATA_H

//: ----------------------------------------------------------------------------
//: Includes
//: ----------------------------------------------------------------------------
#include "http_parser.h"
#include "evr.h"
#include "hlx/hlx.h"

namespace ns_hlx {

//: ----------------------------------------------------------------------------
//: Fwd Decl's
//: ----------------------------------------------------------------------------
class nconn;

//: ----------------------------------------------------------------------------
//:
//: ----------------------------------------------------------------------------
typedef enum type_enum {

        HTTP_DATA_TYPE_NONE = 0,
        HTTP_DATA_TYPE_CLIENT,
        HTTP_DATA_TYPE_SERVER,

} http_data_type_t;

//: ----------------------------------------------------------------------------
//: Connection data
//: ----------------------------------------------------------------------------
typedef struct http_data_struct {

        http_data_type_t m_type;
        nconn *m_nconn;
        http_req m_http_req;
        http_resp m_http_resp;
        void *m_ctx;
        evr_timer_event_t *m_timer_obj;
        http_parser_settings m_http_parser_settings;
        http_parser m_http_parser;
        bool m_save;
        bool m_supports_keep_alives;
        uint16_t m_status_code;
        url_router *m_url_router;
        uint64_t m_idx;

        uint64_t get_idx(void) {return m_idx;}
        void set_idx(uint64_t a_idx) {m_idx = a_idx;}

        http_data_struct(void):
                m_type(HTTP_DATA_TYPE_NONE),
                m_nconn(NULL),
                m_http_req(),
                m_http_resp(),
                m_ctx(NULL),
                m_timer_obj(NULL),
                m_http_parser_settings(),
                m_http_parser(),
                m_save(false),
                m_supports_keep_alives(false),
                m_status_code(0),
                m_url_router(NULL),
                m_idx(0)
        {};

private:
        // Disallow copy/assign
        http_data_struct& operator=(const http_data_struct &);
        http_data_struct(const http_data_struct &);

} http_data_t;

} // ns_hlx

#endif






