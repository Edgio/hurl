//: ----------------------------------------------------------------------------
;//: Copyright (C) 2015 Verizon.  All Rights Reserved.
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
#include "http_parser/http_parser.h"
#include "evr.h"

// TODO TEST
#include "file.h"

#include "hlx/hlx.h"
#include "ndebug.h"

namespace ns_hlx {

//: ----------------------------------------------------------------------------
//: Fwd Decl's
//: ----------------------------------------------------------------------------
class nconn;

//: ----------------------------------------------------------------------------
//:
//: ----------------------------------------------------------------------------
typedef enum type_enum {

        DATA_TYPE_NONE = 0,
        DATA_TYPE_CLIENT,
        DATA_TYPE_SERVER,

} hconn_type_t;

//: ----------------------------------------------------------------------------
//: Connection data
//: ----------------------------------------------------------------------------
class hconn {

public:
        // -------------------------------------------------
        // Publice members
        // -------------------------------------------------
        hconn_type_t m_type;

        nconn *m_nconn;
        t_hlx *m_t_hlx;
        evr_timer_event_t *m_timer_obj;

        hmsg *m_hmsg;
        http_parser_settings m_http_parser_settings;
        http_parser m_http_parser;
        uint64_t m_cur_off;
        char * m_cur_buf;

        bool m_save;
        bool m_supports_keep_alives;
        uint16_t m_status_code;
        url_router *m_url_router;
        nbq *m_in_q;
        nbq *m_out_q;
        subr *m_subr;
        uint64_t m_idx;

        // TODO Test
        filesender *m_fs;

        // -------------------------------------------------
        // Publice methods
        // -------------------------------------------------
        uint64_t get_idx(void) {return m_idx;}
        void set_idx(uint64_t a_idx) {m_idx = a_idx;}

        hconn(void):
                m_type(DATA_TYPE_NONE),
                m_nconn(NULL),
                m_t_hlx(NULL),
                m_timer_obj(NULL),
                m_hmsg(NULL),
                m_http_parser_settings(),
                m_http_parser(),
                m_cur_off(0),
                m_cur_buf(NULL),
                m_save(false),
                m_supports_keep_alives(false),
                m_status_code(0),
                m_url_router(NULL),
                m_in_q(NULL),
                m_out_q(NULL),
                m_subr(NULL),
                m_idx(0),

                // TODO TEST
                m_fs(NULL)
        {};

private:
        // -------------------------------------------------
        // Private methods
        // -------------------------------------------------
        // Disallow copy/assign
        hconn& operator=(const hconn &);
        hconn(const hconn &);

};

} // ns_hlx

#endif






