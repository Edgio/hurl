//: ----------------------------------------------------------------------------
//: Copyright (C) 2014 Verizon.  All Rights Reserved.
//: All Rights Reserved
//:
//: \file:    t_client.h
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
#ifndef _T_CLIENT_H
#define _T_CLIENT_H


//: ----------------------------------------------------------------------------
//: Includes
//: ----------------------------------------------------------------------------
#include "hlp.h"
#include "evr.h"
#include "nconn.h"
#include "parsed_url.h"
#include "reqlet.h"
#include "ndebug.h"

#include <vector>
#include <map>
#include <list>
#include <unordered_set>

#include <string>

#include <pthread.h>

// Sockets...
#include <netinet/in.h>

// OpenSSL
#include <openssl/ssl.h>

#include <signal.h>

//: ----------------------------------------------------------------------------
//: Constants
//: ----------------------------------------------------------------------------
#define NCONN_MAX 16384

namespace ns_hlx {

//: ----------------------------------------------------------------------------
//: Enums
//: ----------------------------------------------------------------------------


//: ----------------------------------------------------------------------------
//: Types
//: ----------------------------------------------------------------------------

//: ----------------------------------------------------------------------------
//: Playback commands
//: ----------------------------------------------------------------------------
typedef struct pb_cmd_struct {
        std::string m_verb;
        std::string m_uri;
        std::string m_method;
        uint64_t m_timestamp_ms;
        std::string m_src_ip;
        std::string m_dest_ip;
        uint16_t m_src_port;
        uint16_t m_dest_port;
        header_map_t m_header_map;

        inline void init(void)
        {
                m_verb.clear();
                m_uri.clear();
                m_method.clear();
                m_timestamp_ms = 0;
                m_src_ip.clear();
                m_dest_ip.clear();
                m_src_port = 0;
                m_dest_port = 0;
                m_header_map.clear();
        }

        pb_cmd_struct():
                m_verb(),
                m_uri(),
                m_method(),
                m_timestamp_ms(0),
                m_src_ip(),
                m_dest_ip(),
                m_src_port(0),
                m_dest_port(0),
                m_header_map()

        {

        }

} pb_cmd_t;

class t_client;
typedef struct client_pb_cmd_struct {
        pb_cmd_t *m_pb_cmd;
        t_client *m_t_client;
} client_pb_cmd_t;


typedef struct {
        nconn *m_nconn;
        pthread_mutex_t m_mutex;
} nconn_lock_t;

typedef std::map <uint64_t, reqlet *> reqlet_map_t;

// connection types
typedef std::map<uint64_t, nconn_lock_t> nconn_lock_map_t;
typedef std::list<nconn_lock_t> nconn_lock_list_t;

//: ----------------------------------------------------------------------------
//: Fwd Decl's
//: ----------------------------------------------------------------------------


//: ----------------------------------------------------------------------------
//: \details: TODO
//: ----------------------------------------------------------------------------
class t_client
{
public:


        // -------------------------------------------------
        // Public methods
        // -------------------------------------------------
        t_client(bool a_verbose,
                bool a_color,
                uint32_t a_sock_opt_recv_buf_size,
                uint32_t a_sock_opt_send_buf_size,
                bool a_sock_opt_no_delay,
                const std::string & a_cipher_str,
                SSL_CTX *a_ctx,
                evr_type_t a_evr_type,
                uint32_t a_max_parallel_connections,
                int32_t a_timeout_s);

        ~t_client();

        int run(void);
        void *t_run(void *a_nothing);
        void *t_run_cmd(void *a_nothing);
        void stop(void) { m_run_cmd_stopped = true; m_stopped = true; }
        bool is_running(void) { return !m_stopped; }
        void get_stats_copy(tag_stat_map_t &ao_tag_stat_map);
        void set_scale(float a_scale) {m_scale = a_scale;};
        void set_first_timestamp_ms(uint64_t a_start_time_ms) {m_first_timestamp_ms = a_start_time_ms;};
        void set_timeout_s(int32_t a_val) {m_timeout_s = a_val;}

        uint32_t get_timeout_s(void) { return m_timeout_s;};

        uint32_t get_num_conns(void) { return m_nconn_lock_map.size();}
        nconn *try_take_locked_nconn_w_hash(uint64_t a_hash);
        void give_lock(uint64_t a_id);

        void show_stats(void);

        // Playback
        void add_pb_cmd(const pb_cmd_t &a_cmd);
        void set_feeder_done(bool a_state) {m_feeder_done = a_state;}

        reqlet *evoke_reqlet(const pb_cmd_t &a_cmd);
        int32_t reassign_nconn(nconn *a_nconn, uint64_t a_new_id);
        int32_t remove_nconn(nconn *a_nconn);
        nconn *get_locked_conn(uint64_t a_id);

        // -------------------------------------------------
        // Public members
        // -------------------------------------------------
        // Needs to be public for now -to join externally
        pthread_t m_t_run_thread;
        pthread_t m_t_run_cmd_thread;
        int32_t m_timeout_s;

        // TODO add getters
        uint64_t m_num_cmds;
        uint64_t m_num_cmds_serviced;
        uint64_t m_num_cmds_dropped;
        uint64_t m_num_reqs_sent;
        uint64_t m_num_errors;

        // -------------------------------------------------
        // Static (class) methods
        // -------------------------------------------------
        static void *evr_loop_file_writeable_cb(void *a_data);
        static void *evr_loop_file_readable_cb(void *a_data);
        static void *evr_loop_file_error_cb(void *a_data);
        static void *evr_loop_file_timeout_cb(void *a_data);
        static void *evr_loop_timer_cb(void *a_data);

private:
        // -------------------------------------------------
        // Private methods
        // -------------------------------------------------
        DISALLOW_COPY_AND_ASSIGN(t_client)

        //Helper for pthreads
        static void *t_run_static(void *a_context)
        {
                return reinterpret_cast<t_client *>(a_context)->t_run(NULL);
        }

        //Helper for pthreads
        static void *t_run_cmd_static(void *a_context)
        {
                return reinterpret_cast<t_client *>(a_context)->t_run_cmd(NULL);
        }

        int32_t cleanup_connection(nconn *a_nconn, bool a_cancel_timer = true);
        int32_t create_request(nconn &ao_conn, reqlet &a_reqlet, const pb_cmd_t &a_cmd);
        nconn *create_new_nconn(const reqlet &a_reqlet);

        // TODO FIX!!!
        //int32_t delete_rconn_pb(uint64_t a_id);

        // -------------------------------------------------
        // Private members
        // -------------------------------------------------
        // client config
        bool m_verbose;
        bool m_color;

        // Socket options
        uint32_t m_sock_opt_recv_buf_size;
        uint32_t m_sock_opt_send_buf_size;
        bool m_sock_opt_no_delay;

        // SSL support
        std::string m_cipher_str;
        SSL_CTX *m_ssl_ctx;

        evr_type_t m_evr_type;

        sig_atomic_t m_stopped;
        uint32_t m_max_parallel_connections;

        int32_t m_start_time_s;
        reqlet_map_t m_reqlet_map;

        int64_t m_num_fetches;
        int64_t m_num_fetched;

        uint64_t m_last_get_req_us;

        header_map_t m_header_map;

        // Get evr_loop
        evr_loop *m_evr_loop;
        evr_loop *m_evr_cmd_loop;

        uint64_t m_first_timestamp_ms;
        pthread_mutex_t m_loop_mutex;
        bool m_feeder_done;
        float m_scale;

        sig_atomic_t m_run_cmd_stopped;

        uint64_t m_stat_cmd_fins;
        uint64_t m_stat_cmd_gets;
        uint64_t m_stat_rconn_created;
        uint64_t m_stat_rconn_reuse;
        uint64_t m_stat_rconn_deleted;
        uint64_t m_stat_bytes_read;
        uint64_t m_stat_err_connect;
        uint64_t m_stat_err_state;
        uint64_t m_stat_err_do_connect;
        uint64_t m_stat_err_send_request;
        uint64_t m_stat_err_read_cb;
        uint64_t m_stat_err_error_cb;

        nconn_lock_map_t m_nconn_lock_map;
        nconn_lock_list_t m_nconn_lock_free_list;
        pthread_mutex_t m_nconn_lock_map_mutex;

};

} //namespace ns_hlx {


#endif
