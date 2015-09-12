//: ----------------------------------------------------------------------------
//: Copyright (C) 2014 Verizon.  All Rights Reserved.
//: All Rights Reserved
//:
//: \file:    t_server.h
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
#ifndef _T_SERVER_H
#define _T_SERVER_H

//: ----------------------------------------------------------------------------
//: Includes
//: ----------------------------------------------------------------------------
#include "hlx_server.h"
#include "nconn_pool.h"
#include "settings.h"
#include "ndebug.h"
#include "evr.h"

// signal
#include <signal.h>

//: ----------------------------------------------------------------------------
//: Constants
//: ----------------------------------------------------------------------------

//: ----------------------------------------------------------------------------
//: Macros
//: ----------------------------------------------------------------------------


//: ----------------------------------------------------------------------------
//: Fwd decl's
//: ----------------------------------------------------------------------------


namespace ns_hlx {

//: ----------------------------------------------------------------------------
//: t_server
//: ----------------------------------------------------------------------------
class t_server
{
public:
        // -------------------------------------------------
        // Public methods
        // -------------------------------------------------
        t_server(const settings_struct_t &a_settings,
                 url_router *a_url_router);
        ~t_server();

        int run(void);
        void *t_run(void *a_nothing);
        void stop(void);
        bool is_running(void) { return !m_stopped; }

        // -------------------------------------------------
        // Public members
        // -------------------------------------------------
        // Needs to be public for now -to join externally
        pthread_t m_t_run_thread;
        settings_struct_t m_settings;
        url_router *m_url_router;

        // -----------------------------
        // Summary info
        // -----------------------------

        // -------------------------------------------------
        // Public members
        // -------------------------------------------------

        // -------------------------------------------------
        // Public Static (class) methods
        // -------------------------------------------------
        static int32_t evr_loop_file_writeable_cb(void *a_data);
        static int32_t evr_loop_file_readable_cb(void *a_data);
        static int32_t evr_loop_file_error_cb(void *a_data);
        static int32_t evr_loop_file_timeout_cb(void *a_data);
        static int32_t evr_loop_timer_cb(void *a_data);
        static int32_t evr_loop_timer_completion_cb(void *a_data);

private:
        // -------------------------------------------------
        // Private methods
        // -------------------------------------------------
        DISALLOW_COPY_AND_ASSIGN(t_server)

        //Helper for pthreads
        static void *t_run_static(void *a_context)
        {
                return reinterpret_cast<t_server *>(a_context)->t_run(NULL);
        }

        int32_t cleanup_connection(nconn *a_nconn, bool a_cancel_timer = true, int32_t a_status = 0);

        int32_t get_response(nconn &ao_conn);

        // -------------------------------------------------
        // Private members
        // -------------------------------------------------
        nconn_pool m_nconn_pool;
        sig_atomic_t m_stopped;
        int32_t m_start_time_s;
        evr_loop *m_evr_loop;
        nconn::scheme_t m_scheme;
        nconn *m_nconn;

};


} //namespace ns_hlx {

#endif // #ifndef _HLX_CLIENT_H


