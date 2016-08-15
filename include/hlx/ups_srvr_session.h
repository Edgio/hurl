//: ----------------------------------------------------------------------------
;//: Copyright (C) 2016 Verizon.  All Rights Reserved.
//: All Rights Reserved
//:
//: \file:    ups_srvr_session.h
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
#ifndef _UPS_SRVR_SESSION_H
#define _UPS_SRVR_SESSION_H

//: ----------------------------------------------------------------------------
//: Includes
//: ----------------------------------------------------------------------------
#include "nconn.h"
#include "hlx/evr.h"
#include "hlx/resp.h"
#include "hlx/http_status.h"
#include <stdint.h>

namespace ns_hlx {

//: ----------------------------------------------------------------------------
//: Fwd Decl's
//: ----------------------------------------------------------------------------
class nconn;
class t_srvr;
class subr;

//: ----------------------------------------------------------------------------
//: Connection data
//: ----------------------------------------------------------------------------
class ups_srvr_session {

public:
        // -------------------------------------------------
        // Public members
        // -------------------------------------------------
        nconn *m_nconn;
        t_srvr *m_t_srvr;
        evr_timer_t *m_timer_obj;

        resp *m_resp;
        bool m_rqst_resp_logging;
        bool m_rqst_resp_logging_color;
        nbq *m_in_q;
        nbq *m_out_q;
        bool m_in_q_detached;
        subr *m_subr;
        uint64_t m_idx;

        // -------------------------------------------------
        // Public Static (class) methods
        // -------------------------------------------------
        static int32_t evr_fd_readable_cb(void *a_data);
        static int32_t evr_fd_writeable_cb(void *a_data);
        static int32_t evr_fd_error_cb(void *a_data);
        static int32_t evr_fd_timeout_cb(void *a_ctx, void *a_data);

        // -------------------------------------------------
        // Public methods
        // -------------------------------------------------
        ups_srvr_session(void);
        uint64_t get_idx(void) {return m_idx;}
        void set_idx(uint64_t a_idx) {m_idx = a_idx;}
        void clear(void);
        int32_t subr_error(http_status_t a_status);
        int32_t cancel_timer(void *a_timer);
        uint32_t get_timeout_ms(void);
        void set_timeout_ms(uint32_t a_t_ms);
        uint64_t get_last_active_ms(void);
        void set_last_active_ms(uint64_t a_time_ms);
        int32_t teardown(http_status_t a_status);

private:
        // -------------------------------------------------
        // Private methods
        // -------------------------------------------------
        // Disallow copy/assign
        ups_srvr_session& operator=(const ups_srvr_session &);
        ups_srvr_session(const ups_srvr_session &);
        bool subr_complete(void);
        void subr_log_status(uint16_t a_status = 0);

        // -------------------------------------------------
        // Private Static (class) methods
        // -------------------------------------------------
        static int32_t run_state_machine(void *a_data, evr_mode_t a_conn_mode);

        // -------------------------------------------------
        // Private members
        // -------------------------------------------------
        uint64_t m_last_active_ms;
        uint32_t m_timeout_ms;
};

} // ns_hlx

#endif






