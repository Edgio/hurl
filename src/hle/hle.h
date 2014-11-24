//: ----------------------------------------------------------------------------
//: Copyright (C) 2014 Verizon.  All Rights Reserved.
//: All Rights Reserved
//:
//: \file:    hle.h
//: \details: TODO
//: \author:  Reed P. Morrison
//: \date:    02/07/2014
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
#ifndef _HLE_H
#define _HLE_H


//: ----------------------------------------------------------------------------
//: Includes
//: ----------------------------------------------------------------------------
#include "evr.h"
#include "ndebug.h"

#include <stdint.h>
#include <string>
#include <list>
#include <map>


//: ----------------------------------------------------------------------------
//: Constants
//: ----------------------------------------------------------------------------
// Version
#define HLE_VERSION_MAJOR 0
#define HLE_VERSION_MINOR 0
#define HLE_VERSION_MACRO 1
#define HLE_VERSION_PATCH "alpha"

#define HLE_DEFAULT_CONN_TIMEOUT_S 10


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
class t_client;
typedef std::list <std::string> host_list_t;
typedef std::list <t_client *> t_client_list_t;
typedef std::map <std::string, std::string> header_map_t;

struct ssl_ctx_st;
typedef ssl_ctx_st SSL_CTX;

//: ----------------------------------------------------------------------------
//: \details: TODO
//: ----------------------------------------------------------------------------
class hle
{
public:
        // -------------------------------------------------
        // Public methods
        // -------------------------------------------------
        int32_t init();

        ~hle();

        // Settings...
        int32_t set_header(const std::string &a_header_key, const std::string &a_header_val);

        void set_verbose(bool a_val) { m_verbose = a_val;}
        void set_color(bool a_val) { m_color = a_val;}
        void set_quiet(bool a_val) { m_quiet = a_val;}
        void set_cipher_list(std::string a_val) {m_cipher_list = a_val;}
        void set_url(std::string a_val) {m_url = a_val;}
        void set_sock_opt_recv_buf_size(uint32_t a_val) {m_sock_opt_recv_buf_size = a_val;}
        void set_sock_opt_send_buf_size(uint32_t a_val) {m_sock_opt_send_buf_size = a_val;}
        void set_sock_opt_no_delay(bool a_val) {m_sock_opt_no_delay = a_val;}
        void set_event_handler_type(evr_loop_type_t a_val) {m_evr_loop_type = a_val;}
        void set_start_parallel(int32_t a_val) {m_start_parallel = a_val;}
        void set_num_threads(uint32_t a_val) {m_num_threads = a_val;}
        void set_timeout_s(int32_t a_val) {m_timeout_s = a_val;}

        // Running...
        int32_t run(host_list_t &a_host_list);

        int32_t stop(void);
        int32_t wait_till_stopped(void);
        bool is_running(void);

        // -------------------------------------------------
        // Class methods
        // -------------------------------------------------
        // Get the singleton instance
        static hle *get(void);

        // -------------------------------------------------
        // Public members
        // -------------------------------------------------
        t_client_list_t m_t_client_list;

private:
        // -------------------------------------------------
        // Private methods
        // -------------------------------------------------
        DISALLOW_COPY_AND_ASSIGN(hle)
        hle();

        // -------------------------------------------------
        // Private members
        // -------------------------------------------------
        SSL_CTX* m_ssl_ctx;
        bool m_is_initd;

        // -------------------------------------------------
        // Settings
        // -------------------------------------------------
        std::string m_cipher_list;
        bool m_verbose;
        bool m_color;
        bool m_quiet;
        uint32_t m_sock_opt_recv_buf_size;
        uint32_t m_sock_opt_send_buf_size;
        bool m_sock_opt_no_delay;
        evr_loop_type_t m_evr_loop_type;
        int32_t m_start_parallel;
        uint32_t m_num_threads;
        std::string m_url;
        header_map_t m_header_map;
        uint32_t m_timeout_s;

        // -------------------------------------------------
        // Class members
        // -------------------------------------------------
        // the pointer to the singleton for the instance 
        static hle *m_singleton_ptr;

};


#endif


