//: ----------------------------------------------------------------------------
//: Copyright (C) 2014 Verizon.  All Rights Reserved.
//: All Rights Reserved
//:
//: \file:    api_server.h
//: \details: TODO
//: \author:  Reed P. Morrison
//: \date:    12/07/2014
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
#ifndef _API_SERVER_H
#define _API_SERVER_H


//: ----------------------------------------------------------------------------
//: Includes
//: ----------------------------------------------------------------------------
#include "http_parser.h"
#include "ndebug.h"
#include <pthread.h>
#include <signal.h>


//: ----------------------------------------------------------------------------
//: Constants
//: ----------------------------------------------------------------------------


//: ----------------------------------------------------------------------------
//: Macros
//: ----------------------------------------------------------------------------

//: ----------------------------------------------------------------------------
//: Enums
//: ----------------------------------------------------------------------------


//: ----------------------------------------------------------------------------
//: Types
//: ----------------------------------------------------------------------------

//: ----------------------------------------------------------------------------
//: Fwd Decl's
//: ----------------------------------------------------------------------------
namespace ns_hlx
{
        class hlx_client;
}


//: ----------------------------------------------------------------------------
//: \details: TODO
//: ----------------------------------------------------------------------------
class api_server
{
public:
        // -------------------------------------------------
        // Public methods
        // -------------------------------------------------
        api_server();
        ~api_server();

        // Settings...
        void set_verbose(bool a_val) { m_verbose = a_val;};
        void set_color(bool a_val) { m_color = a_val;};
        void set_stats(bool a_val) { m_stats = a_val;};
        void set_hlx_client(ns_hlx::hlx_client *a_hlx_client) { m_hlx_client = a_hlx_client;};

        // Running...
        int32_t start(uint16_t a_port);
        void *t_run(void *a_nothing);

        int32_t stop(void);
        int32_t wait_till_stopped(void);
        bool is_running(void) { return !m_stopped;};

        // -------------------------------------------------
        // Class methods
        // -------------------------------------------------

        // -------------------------------------------------
        // Public members
        // -------------------------------------------------

        // -------------------------------------------------
        // Public static methods
        // -------------------------------------------------
        static int hp_on_message_begin(http_parser* a_parser);
        static int hp_on_url(http_parser* a_parser, const char *a_at, size_t a_length);
        static int hp_on_header_field(http_parser* a_parser, const char *a_at, size_t a_length);
        static int hp_on_header_value(http_parser* a_parser, const char *a_at, size_t a_length);
        static int hp_on_headers_complete(http_parser* a_parser);
        static int hp_on_body(http_parser* a_parser, const char *a_at, size_t a_length);
        static int hp_on_message_complete(http_parser* a_parser);

private:
        // -------------------------------------------------
        // Private methods
        // -------------------------------------------------
        DISALLOW_COPY_AND_ASSIGN(api_server)

        //Helper for pthreads
        static void *t_run_static(void *a_context)
        {
                return reinterpret_cast<api_server *>(a_context)->t_run(NULL);
        }

        // -------------------------------------------------
        // Private members
        // -------------------------------------------------

        // -------------------------------------------------
        // Settings
        // -------------------------------------------------
        bool m_verbose;
        bool m_color;
        bool m_stats;
        uint16_t m_port;
        ns_hlx::hlx_client *m_hlx_client;

        http_parser_settings m_http_parser_settings;
        http_parser m_http_parser;

        // -------------------------------------------------
        // State
        // -------------------------------------------------
        sig_atomic_t m_stopped;
        pthread_t m_t_run_thread;
        int32_t m_server_fd;
        bool m_cur_msg_complete;

        // -------------------------------------------------
        // Class members
        // -------------------------------------------------

};


#endif


