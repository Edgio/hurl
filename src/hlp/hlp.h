//: ----------------------------------------------------------------------------
//: Copyright (C) 2014 Verizon.  All Rights Reserved.
//: All Rights Reserved
//:
//: \file:    hlp.h
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
#ifndef _HLP_H
#define _HLP_H


//: ----------------------------------------------------------------------------
//: Includes
//: ----------------------------------------------------------------------------
#include <string>
#include <vector>
#include <map>
#include <stdint.h>

#include "ndebug.h"

//: ----------------------------------------------------------------------------
//: Constants
//: ----------------------------------------------------------------------------
// Version
#define HLP_VERSION_MAJOR 0
#define HLP_VERSION_MINOR 0
#define HLP_VERSION_MACRO 1
#define HLP_VERSION_PATCH "alpha"

#define HLP_DEFAULT_CONN_TIMEOUT_S 5

//: ----------------------------------------------------------------------------
//: Macros
//: ----------------------------------------------------------------------------


//: ----------------------------------------------------------------------------
//: Fwd Decl's
//: ----------------------------------------------------------------------------
struct ssl_ctx_st;
typedef ssl_ctx_st SSL_CTX;

namespace ns_hlx {

//: ----------------------------------------------------------------------------
//: Enums
//: ----------------------------------------------------------------------------
typedef enum evr_type
{
        EV_EPOLL,
        EV_SELECT
} evr_type_t;

//: ----------------------------------------------------------------------------
//: Types
//: ----------------------------------------------------------------------------
typedef std::map<std::string, std::string> header_map_t;

//: ----------------------------------------------------------------------------
//: Fwd Decl's
//: ----------------------------------------------------------------------------

class t_client;
typedef std::vector <t_client *> t_client_list_t;

//: ----------------------------------------------------------------------------
//: \details: TODO
//: ----------------------------------------------------------------------------
class hlp
{
public:
        // -------------------------------------------------
        // Public methods
        // -------------------------------------------------
        int32_t init();

        ~hlp();

        // Settings...
        int32_t set_header(const std::string &a_header_key, const std::string &a_header_val);

        void set_verbose(bool a_val) { m_verbose = a_val;}
        void set_extra_info(uint32_t a_val) { m_extra_info = a_val;}
        void set_color(bool a_val) { m_color = a_val;}
        void set_quiet(bool a_val) { m_quiet = a_val;}
        void set_cipher_list(std::string a_val) {m_cipher_list = a_val;}
        void set_sock_opt_recv_buf_size(uint32_t a_val) {m_sock_opt_recv_buf_size = a_val;}
        void set_sock_opt_send_buf_size(uint32_t a_val) {m_sock_opt_send_buf_size = a_val;}
        void set_sock_opt_no_delay(bool a_val) {m_sock_opt_no_delay = a_val;}
        void set_event_handler_type(evr_type_t a_val) {m_evr_type = a_val;}
        void set_scale(float a_scale) {m_scale = a_scale;};
        void set_num_threads(uint32_t a_num_threads) {m_num_threads = a_num_threads;};
        void set_timeout_s(int32_t a_val) {m_timeout_s = a_val;}

        // Running...
        int32_t run(const std::string &a_playback_file, const std::string &a_pb_dest_addr, int32_t a_pb_dest_port);

        int32_t stop(void);
        int32_t wait_till_stopped(void);
        bool is_running(void);

        // Stats
        void display_results_line_desc(bool a_color_flag);
        void display_results_line(bool a_color_flag);
        void display_results(double a_elapsed_time,
                        uint32_t a_max_parallel,
                        bool a_show_breakdown_flag = false);

        int32_t get_stats_json(char *l_json_buf, uint32_t l_json_buf_max_len);

        // -------------------------------------------------
        // Class methods
        // -------------------------------------------------
    // Get the singleton instance
    static hlp *get(void);

        //Helper for pthreads
        static void *t_feed_playback_static(void *a_context)
        {
                return reinterpret_cast<hlp *>(a_context)->t_feed_playback(NULL);
        }
        void *t_feed_playback(void *a_nothing);
        //t_client *get_playback_client(void) { return m_playback_client;}
        uint64_t get_num_cmds(void);
        uint64_t get_num_cmds_serviced(void);
        uint32_t get_num_conns(void);
        uint64_t get_num_reqs_sent(void);

        // -------------------------------------------------
        // Public members
        // -------------------------------------------------
        pthread_t m_t_feed_playback_thread;
        t_client_list_t m_t_client_list;


private:
        // -------------------------------------------------
        // Private methods
        // -------------------------------------------------
        DISALLOW_COPY_AND_ASSIGN(hlp)
        hlp();

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
        std::string m_playback_file;
        header_map_t m_header_map;
        int32_t m_timeout_s;

        // Playback info
        float m_playback_stat_percent_complete;
        std::string m_pb_dest_addr;
        int32_t m_pb_dest_port;
        float m_scale;
        uint32_t m_extra_info;
        uint32_t m_num_threads;

        // -------------------------------------------------
        // Class members
        // -------------------------------------------------
        // the pointer to the singleton for the instance 
        static hlp *m_singleton_ptr;

};

} //namespace ns_hlx {

#endif
