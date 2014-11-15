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
#include "hle.h"
#include "evr.h"
#include "nconn.h"
#include "parsed_url.h"
#include "reqlet.h"
#include "ndebug.h"
#include "util.h"


#include <map>
#include <vector>
#include <list>
#include <unordered_set>

#include <string>

#include <pthread.h>

// Sockets...
#include <netinet/in.h>

// OpenSSL
#include <openssl/ssl.h>

#include <signal.h>

// For usleep
#include <unistd.h>


//: ----------------------------------------------------------------------------
//: Constants
//: ----------------------------------------------------------------------------

//: ----------------------------------------------------------------------------
//: Enums
//: ----------------------------------------------------------------------------


//: ----------------------------------------------------------------------------
//: Types
//: ----------------------------------------------------------------------------
class t_client;

typedef std::vector<nconn *> nconn_vector_t;
typedef std::list<uint32_t> conn_id_list_t;
typedef std::unordered_set<uint32_t> conn_id_set_t;
typedef std::map <std::string, std::string> header_map_t;

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
        t_client(
                bool a_verbose,
                bool a_color,
                uint32_t a_sock_opt_recv_buf_size,
                uint32_t a_sock_opt_send_buf_size,
                bool a_sock_opt_no_delay,
                uint32_t a_timeout_s,
                const std::string & a_cipher_str,
                SSL_CTX *a_ctx,
                evr_loop_type_t a_evr_loop_type,
                uint32_t a_max_parallel_connections
                );

        ~t_client();

        int run(void);
        void *t_run(void *a_nothing);
        void stop(void);
        bool is_running(void) { return !m_stopped; }
        int32_t set_header(const std::string &a_header_key, const std::string &a_header_val);
        void set_ssl_ctx(SSL_CTX * a_ssl_ctx) { m_ssl_ctx = a_ssl_ctx;};
        uint32_t get_timeout_s(void) { return m_timeout_s;};

        // -------------------------------------------------
        // Public members
        // -------------------------------------------------
        // Needs to be public for now -to join externally
        pthread_t m_t_run_thread;

        // -------------------------------------------------
        // Static (class) methods
        // -------------------------------------------------
        static void *evr_loop_file_writeable_cb(void *a_data);
        static void *evr_loop_file_readable_cb(void *a_data);
        static void *evr_loop_file_error_cb(void *a_data);
        static void *evr_loop_file_timeout_cb(void *a_data);
        static void *evr_loop_timer_cb(void *a_data);
        static void *evr_loop_timer_completion_cb(void *a_data);

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

        int32_t start_connections(void);
        int32_t cleanup_connection(nconn *a_nconn, bool a_cancel_timer = true);
        int32_t create_request(nconn &ao_conn, reqlet &a_reqlet);

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
        uint32_t m_timeout_s;

        // SSL support
        std::string m_cipher_str;
        SSL_CTX *m_ssl_ctx;

        evr_loop_type_t m_evr_loop_type;

        sig_atomic_t m_stopped;

        uint32_t m_max_parallel_connections;
        nconn_vector_t m_nconn_vector;
        conn_id_list_t m_conn_free_list;
        conn_id_set_t m_conn_used_set;

        int64_t m_num_fetches;
        int64_t m_num_fetched;
        int64_t m_num_pending;

        header_map_t m_header_map;

        //uint64_t m_unresolved_count;

        // Get evr_loop
        evr_loop *m_evr_loop;

};

#endif
