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
#include "nconn.h"

// TODO TEST
#include "file.h"

#include "hlx/default_rqst_h.h"
#include "hlx/hmsg.h"
#include "hlx/stat.h"
#include "ndebug.h"

//: ----------------------------------------------------------------------------
//: Constants
//: ----------------------------------------------------------------------------
// TODO -make enum
#ifndef STATUS_AGAIN
#define STATUS_AGAIN -2
#endif

namespace ns_hlx {

//: ----------------------------------------------------------------------------
//: Fwd Decl's
//: ----------------------------------------------------------------------------
class nconn;
class t_hlx;
class subr;

//: ----------------------------------------------------------------------------
//:
//: ----------------------------------------------------------------------------
typedef enum hconn_ev_cb_enum {

        HCONN_EV_CB_READABLE = 0,
        HCONN_EV_CB_WRITEABLE,
        HCONN_EV_CB_TIMEOUT,
        HCONN_EV_CB_ERROR,

} hconn_ev_cb_t;

//: ----------------------------------------------------------------------------
//:
//: ----------------------------------------------------------------------------
typedef enum hconn_type_enum {

        HCONN_TYPE_NONE = 0,
        HCONN_TYPE_CLIENT,
        HCONN_TYPE_UPSTREAM,

} hconn_type_t;

//: ----------------------------------------------------------------------------
//: HL_HTTP_CLIENT_STATE
//: ----------------------------------------------------------------------------
typedef enum http_client_state_enum {

        HTTP_CLIENT_STATE_NONE = 0,
        HTTP_CLIENT_STATE_READ_RQST,
        HTTP_CLIENT_STATE_UPST_RQST,
        HTTP_CLIENT_STATE_SEND_RESP,

} http_client_state_t;

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
        //uint16_t m_status_code;
        bool m_verbose;
        bool m_color;
        url_router *m_url_router;
        nbq *m_in_q;
        nbq *m_out_q;
        subr *m_subr;
        uint64_t m_idx;

        // TODO Test
        filesender *m_fs;

        // -------------------------------------------------
        // Public methods
        // -------------------------------------------------
        static default_rqst_h s_default_rqst_h;

        // -------------------------------------------------
        // Public methods
        // -------------------------------------------------
        hconn(void);
        uint64_t get_idx(void) {return m_idx;}
        void set_idx(uint64_t a_idx) {m_idx = a_idx;}
        int32_t run_state_machine(nconn::mode_t a_conn_mode, int32_t a_conn_status);
        int32_t subr_error(http_status_t a_status);
        bool subr_complete(void);
        void clear(void);
private:
        // -------------------------------------------------
        // Private methods
        // -------------------------------------------------
        // Disallow copy/assign
        hconn& operator=(const hconn &);
        hconn(const hconn &);
        int32_t handle_req(void);
        int32_t run_state_machine_cln(nconn::mode_t a_conn_mode, int32_t a_conn_status);
        int32_t run_state_machine_ups(nconn::mode_t a_conn_mode, int32_t a_conn_status);

};

} // ns_hlx

#endif






