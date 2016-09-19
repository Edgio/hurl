//: ----------------------------------------------------------------------------
;//: Copyright (C) 2016 Verizon.  All Rights Reserved.
//: All Rights Reserved
//:
//: \file:    clnt_session.h
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
#ifndef _CLNT_SESSION_H
#define _CLNT_SESSION_H

//: ----------------------------------------------------------------------------
//: Includes
//: ----------------------------------------------------------------------------
#include "hlx/evr.h"
#include "hlx/srvr.h"
#include "hlx/default_rqst_h.h"
#include "hlx/hmsg.h"
#include "hlx/stat.h"
#include "hlx/access.h"
#include "hlx/base_u.h"
#include <stdint.h>

namespace ns_hlx {

//: ----------------------------------------------------------------------------
//: Fwd Decl's
//: ----------------------------------------------------------------------------
class nconn;
class t_srvr;
class subr;
class lsnr;
class clnt_session;

#ifndef resp_done_cb_t
// TODO move to handler specific resp cb...
typedef int32_t (*resp_done_cb_t)(clnt_session &);
#endif

//: ----------------------------------------------------------------------------
//: Connection data
//: ----------------------------------------------------------------------------
class clnt_session {

public:
        // -------------------------------------------------
        // Public members
        // -------------------------------------------------
        nconn *m_nconn;
        t_srvr *m_t_srvr;
        evr_timer_t *m_timer_obj;

        rqst *m_rqst;
        bool m_rqst_resp_logging;
        bool m_rqst_resp_logging_color;
        lsnr *m_lsnr;
        nbq *m_in_q;
        nbq *m_out_q;
        uint64_t m_idx;

        // upstream
        base_u *m_ups;

        access_info m_access_info;
        resp_done_cb_t m_resp_done_cb;

        // -------------------------------------------------
        // Public static default
        // -------------------------------------------------
        static default_rqst_h s_default_rqst_h;

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
        clnt_session(void);
        ~clnt_session(void);
        uint64_t get_idx(void) {return m_idx;}
        void set_idx(uint64_t a_idx) {m_idx = a_idx;}
        void clear(void);
        evr_loop *get_evr_loop(void);
        nbq *get_nbq(void);
        void release_nbq(nbq *a_nbq);
        srvr *get_srvr(void);
        int32_t queue_output(void);
        int32_t add_timer(uint32_t a_ms,
                          timer_cb_t a_cb,
                          void *a_data,
                          void **ao_timer);
        uint32_t get_timeout_ms(void);
        void set_timeout_ms(uint32_t a_t_ms);
        uint64_t get_last_active_ms(void);
        void set_last_active_ms(uint64_t a_time_ms);
        subr *create_subr(const subr *a_subr);
        int32_t queue_subr(subr &a_subr);
private:
        // -------------------------------------------------
        // Private methods
        // -------------------------------------------------
        // Disallow copy/assign
        clnt_session& operator=(const clnt_session &);
        clnt_session(const clnt_session &);

        int32_t handle_req(void);
        void log_status(uint16_t a_status = 0);
        int32_t teardown(void);
        int32_t cancel_timer(void *a_timer);

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






