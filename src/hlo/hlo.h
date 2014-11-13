//: ----------------------------------------------------------------------------
//: Copyright (C) 2014 Verizon.  All Rights Reserved.
//: All Rights Reserved
//:
//: \file:    hlo.h
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
#ifndef _HLO_H
#define _HLO_H


//: ----------------------------------------------------------------------------
//: Includes
//: ----------------------------------------------------------------------------
#include <string>
#include <list>
#include <map>
#include <stdint.h>

#include "ndebug.h"
#include "reqlet.h"

//: ----------------------------------------------------------------------------
//: Constants
//: ----------------------------------------------------------------------------
// Version
#define HLO_VERSION_MAJOR 0
#define HLO_VERSION_MINOR 0
#define HLO_VERSION_MACRO 1
#define HLO_VERSION_PATCH "alpha"

#define HLO_DEFAULT_CONN_TIMEOUT_S 5

//: ----------------------------------------------------------------------------
//: Macros
//: ----------------------------------------------------------------------------


//: ----------------------------------------------------------------------------
//: Enums
//: ----------------------------------------------------------------------------
typedef enum evr_type
{
        EV_EPOLL,
        EV_SELECT
} evr_type_t;

typedef enum {

        REQLET_MODE_SEQUENTIAL = 0,
        REQLET_MODE_RANDOM,
        REQLET_MODE_ROUND_ROBIN

} reqlet_mode_t;

//: ----------------------------------------------------------------------------
//: Types
//: ----------------------------------------------------------------------------


//: ----------------------------------------------------------------------------
//: Fwd Decl's
//: ----------------------------------------------------------------------------
struct ssl_ctx_st;
typedef ssl_ctx_st SSL_CTX;

class t_client;
typedef std::list <t_client *> t_client_list_t;

class t_client_pb;

//: ----------------------------------------------------------------------------
//: \details: TODO
//: ----------------------------------------------------------------------------
class hlo
{
public:
        // -------------------------------------------------
        // Public methods
        // -------------------------------------------------
        int32_t init();

        ~hlo();

        // Settings...
        int32_t set_header(const std::string &a_header_key, const std::string &a_header_val);

        void set_verbose(bool a_val) { m_verbose = a_val;}
        void set_color(bool a_val) { m_color = a_val;}
        void set_quiet(bool a_val) { m_quiet = a_val;}

        void set_cipher_list(std::string a_val) {m_cipher_list = a_val;}
        void set_sock_opt_recv_buf_size(uint32_t a_val) {m_sock_opt_recv_buf_size = a_val;}
        void set_sock_opt_send_buf_size(uint32_t a_val) {m_sock_opt_send_buf_size = a_val;}
        void set_sock_opt_no_delay(bool a_val) {m_sock_opt_no_delay = a_val;}
        void set_event_handler_type(evr_type_t a_val) {m_evr_type = a_val;}
        void set_start_parallel(int32_t a_val) {m_start_parallel = a_val;}
        void set_run_time_s(int32_t a_val) {m_run_time_s = a_val;}
        void set_num_threads(uint32_t a_val) {m_num_threads = a_val;}

        void set_timeout_s(int32_t a_val) {m_timeout_s = a_val;}
        void set_end_fetches(int32_t a_val);
        void set_max_reqs_per_conn(int64_t a_val);
        void set_rate(int32_t a_val);
        void set_reqlet_mode(reqlet_mode_t a_mode);

        int32_t set_url(const std::string &a_url);
        int32_t set_url_file(const std::string &a_url_file);
        void set_wildcarding(bool a_val) { m_wildcarding = a_val;}

        // Running...
        int32_t run(void);

        int32_t stop(void);
        int32_t wait_till_stopped(void);
        bool is_running(void);

        // Stats
        void display_results_line_desc(bool a_color_flag);
        void display_results_line(bool a_color_flag);
        void display_results_line_playback(bool a_color_flag);

        void display_results_http_load_style(double a_elapsed_time,
                        uint32_t a_max_parallel,
                        bool a_show_breakdown_flag = false, bool a_one_line_flag = false);
        void display_results(double a_elapsed_time,
                        uint32_t a_max_parallel,
                        bool a_show_breakdown_flag = false);

        int32_t get_stats_json(char *l_json_buf, uint32_t l_json_buf_max_len);

        // -------------------------------------------------
        // Class methods
        // -------------------------------------------------
    // Get the singleton instance
    static hlo *get(void);

    // -------------------------------------------------
        // Public members
        // -------------------------------------------------
        t_client_list_t m_t_client_list;

private:
        // -------------------------------------------------
        // Private methods
        // -------------------------------------------------
        DISALLOW_COPY_AND_ASSIGN(hlo)
        hlo();

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
        evr_type_t m_evr_type;
        int32_t m_start_parallel;
        int32_t m_end_fetches;
        int64_t m_max_reqs_per_conn;
        int32_t m_run_time_s;
        int32_t m_timeout_s;
        uint32_t m_num_threads;
        std::string m_url;
        std::string m_url_file;
        header_map_t m_header_map;
        int32_t m_rate;
        reqlet_mode_t m_reqlet_mode;
        bool m_wildcarding;

        // -------------------------------------------------
        // Class members
        // -------------------------------------------------
        // the pointer to the singleton for the instance 
        static hlo *m_singleton_ptr;

};


#endif


