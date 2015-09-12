//: ----------------------------------------------------------------------------
//: Copyright (C) 2014 Verizon.  All Rights Reserved.
//: All Rights Reserved
//:
//: \file:    hlx_server.h
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
#ifndef _HLX_SERVER_H
#define _HLX_SERVER_H

//: ----------------------------------------------------------------------------
//: Includes
//: ----------------------------------------------------------------------------
#include <pthread.h>
#include <signal.h>

#include <string>
#include <list>
#include <vector>
#include <map>
#include <stdint.h>
#include "http_request_handler.h"
#include "nbq.h"

//: ----------------------------------------------------------------------------
//: Constants
//: ----------------------------------------------------------------------------
#define HLX_SERVER_STATUS_OK 0
#define HLX_SERVER_STATUS_ERROR -1

//: ----------------------------------------------------------------------------
//: Macros
//: ----------------------------------------------------------------------------
#define HLX_SERVER_DISALLOW_ASSIGN(class_name)\
    class_name& operator=(const class_name &);
#define HLX_SERVER_DISALLOW_COPY(class_name)\
    class_name(const class_name &);
#define HLX_SERVER_DISALLOW_COPY_AND_ASSIGN(class_name)\
    HLX_SERVER_DISALLOW_COPY(class_name)\
    HLX_SERVER_DISALLOW_ASSIGN(class_name)

//: ----------------------------------------------------------------------------
//: Fwd decl's
//: ----------------------------------------------------------------------------

namespace ns_hlx {
//: ----------------------------------------------------------------------------
//: Types
//: ----------------------------------------------------------------------------
class t_server;
typedef std::list <t_server *> t_server_list_t;

//: ----------------------------------------------------------------------------
//: hlx_server
//: ----------------------------------------------------------------------------
class hlx_server
{
public:
        // -------------------------------------------------
        // Public methods
        // -------------------------------------------------
        hlx_server();
        ~hlx_server();

        // General
        void set_stats(bool a_val);
        void set_verbose(bool a_val);
        void set_color(bool a_val);

        // Settings
        void set_port(uint16_t a_port);
        void set_num_threads(uint32_t a_num_threads);
        void set_num_parallel(uint32_t a_num_parallel);

        void set_start_time_ms(uint64_t a_start_time_ms) {m_start_time_ms = a_start_time_ms;}

        // Running...
        int32_t run(void);
        int32_t stop(void);
        int32_t wait_till_stopped(void);
        bool is_running(void);
        int32_t add_endpoint(const std::string &a_endpoint, const http_request_handler *a_handler);

        // -------------------------------------------------
        // Class methods
        // -------------------------------------------------

private:
        // -------------------------------------------------
        // Private methods
        // -------------------------------------------------
        HLX_SERVER_DISALLOW_COPY_AND_ASSIGN(hlx_server)


        int init(void);
        int init_server_list(void);

        // -------------------------------------------------
        // Private members
        // -------------------------------------------------

        // -------------------------------------------------
        // Settings
        // -------------------------------------------------
        bool m_verbose;
        bool m_color;
        bool m_stats;

        // Run Settings
        uint16_t m_port;
        uint32_t m_num_threads;
        int32_t m_num_parallel;

        // Stats
        uint64_t m_start_time_ms;

        // t_client
        t_server_list_t m_t_server_list;
        int m_evr_loop_type;

        // router
        url_router m_url_router;

        // -------------------------------------------------
        // State
        // -------------------------------------------------
        int32_t m_fd;
        bool m_is_initd;


        // -------------------------------------------------
        // Class members
        // -------------------------------------------------
};


} //namespace ns_hlx {

#endif


