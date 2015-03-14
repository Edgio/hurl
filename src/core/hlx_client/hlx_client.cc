//: ----------------------------------------------------------------------------
//: Copyright (C) 2014 Verizon.  All Rights Reserved.
//: All Rights Reserved
//:
//: \file:    hlx_client.cc
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

//: ----------------------------------------------------------------------------
//: Includes
//: ----------------------------------------------------------------------------
#include "hlx_client.h"
#include "util.h"
#include "ssl_util.h"
#include "ndebug.h"
#include "resolver.h"
#include "reqlet.h"
#include "nconn_ssl.h"
#include "nconn_tcp.h"

#include <string.h>

// getrlimit
#include <sys/time.h>
#include <sys/resource.h>

// signal
#include <signal.h>

// Shared pointer
//#include <tr1/memory>

#include <list>
#include <algorithm>
#include <unordered_set>

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <getopt.h> // For getopt_long
#include <termios.h>
#include <errno.h>
#include <string.h>
#include <stdint.h>
#ifndef __STDC_FORMAT_MACROS
#define __STDC_FORMAT_MACROS
#endif
#include <inttypes.h>

//: ----------------------------------------------------------------------------
//: Constants
//: ----------------------------------------------------------------------------

//: ----------------------------------------------------------------------------
//: Macros
//: ----------------------------------------------------------------------------
#define T_CLIENT_CONN_CLEANUP(a_t_client, a_conn, a_reqlet, a_status, a_response) \
        do { \
                a_reqlet->set_response(a_status, a_response); \
                if(a_t_client->m_settings.m_show_summary) a_t_client->m_settings.m_hlx_client->append_summary(a_reqlet);\
                if(a_status >= 500) a_t_client->m_settings.m_hlx_client->up_done(true); \
                else a_t_client->m_settings.m_hlx_client->up_done(false); \
                a_t_client->cleanup_connection(a_conn); \
        }while(0)

#define HLX_CLIENT_SET_NCONN_OPT(_conn, _opt, _buf, _len) \
        do { \
                int _status = 0; \
                _status = _conn.set_opt((_opt), (_buf), (_len)); \
                if (_status != STATUS_OK) { \
                        NDBG_PRINT("STATUS_ERROR: Failed to set_opt %d.  Status: %d.\n", _opt, _status); \
                        return NULL;\
                } \
        } while(0)

#define HLX_CLIENT_GET_NCONN_OPT(_conn, _opt, _buf, _len) \
        do { \
                int _status = 0; \
                _status = _conn.get_opt((_opt), (_buf), (_len)); \
                if (_status != STATUS_OK) { \
                        NDBG_PRINT("STATUS_ERROR: Failed to get_opt %d.  Status: %d.\n", _opt, _status); \
                        return NULL;\
                } \
        } while(0)

namespace ns_hlx {
//: ----------------------------------------------------------------------------
//: Types
//: ----------------------------------------------------------------------------
typedef std::vector<nconn *> nconn_vector_t;
typedef std::list<uint32_t> conn_id_list_t;
typedef std::unordered_set<uint32_t> conn_id_set_t;

//: ----------------------------------------------------------------------------
//: Settings
//: ----------------------------------------------------------------------------
typedef struct settings_struct
{
        bool m_verbose;
        bool m_color;
        bool m_quiet;
        bool m_show_summary;

        // request options
        std::string m_url;
        header_map_t m_header_map;

        // run options
        t_client_list_t m_t_client_list;
        evr_loop_type_t m_evr_loop_type;
        int32_t m_num_parallel;
        uint32_t m_num_threads;
        uint32_t m_timeout_s;
        bool m_connect_only;

        // tcp options
        uint32_t m_sock_opt_recv_buf_size;
        uint32_t m_sock_opt_send_buf_size;
        bool m_sock_opt_no_delay;

        // SSL options
        SSL_CTX* m_ssl_ctx;
        std::string m_ssl_cipher_list;
        std::string m_ssl_options_str;
        long m_ssl_options;
        bool m_ssl_verify;
        bool m_ssl_sni;
        std::string m_ssl_ca_file;
        std::string m_ssl_ca_path;

        hlx_client *m_hlx_client;

        // ---------------------------------
        // Defaults...
        // ---------------------------------
        settings_struct(void) :
                m_verbose(false),
                m_color(false),
                m_quiet(false),
                m_show_summary(false),
                m_url(),
                m_header_map(),
                m_t_client_list(),
                m_evr_loop_type(EVR_LOOP_EPOLL),
                m_num_parallel(64),
                m_num_threads(8),
                m_timeout_s(10),
                m_connect_only(false),
                m_sock_opt_recv_buf_size(0),
                m_sock_opt_send_buf_size(0),
                m_sock_opt_no_delay(false),
                m_ssl_ctx(NULL),
                m_ssl_cipher_list(""),
                m_ssl_options_str(""),
                m_ssl_options(0),
                m_ssl_verify(false),
                m_ssl_sni(false),
                m_ssl_ca_file(""),
                m_ssl_ca_path(""),
                m_hlx_client(NULL)
        {}

private:
        DISALLOW_COPY_AND_ASSIGN(settings_struct);

} settings_struct_t;


//: ----------------------------------------------------------------------------
//: t_client
//: ----------------------------------------------------------------------------
class t_client
{
public:
        // -------------------------------------------------
        // Public methods
        // -------------------------------------------------
        t_client(const settings_struct_t &a_settings);

        ~t_client();

        int run(void);
        void *t_run(void *a_nothing);
        void stop(void);
        bool is_running(void) { return !m_stopped; }
        int32_t set_header(const std::string &a_header_key, const std::string &a_header_val);
        void set_ssl_ctx(SSL_CTX * a_ssl_ctx) { m_settings.m_ssl_ctx = a_ssl_ctx;};
        uint32_t get_timeout_s(void) { return m_settings.m_timeout_s;};

        // -------------------------------------------------
        // Public members
        // -------------------------------------------------
        // Needs to be public for now -to join externally
        pthread_t m_t_run_thread;
        settings_struct_t m_settings;

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
        nconn *create_new_nconn(uint32_t a_id, const reqlet &a_reqlet);

        // -------------------------------------------------
        // Private members
        // -------------------------------------------------
        // client config

        sig_atomic_t m_stopped;

        nconn_vector_t m_nconn_vector;
        conn_id_list_t m_conn_free_list;
        conn_id_set_t m_conn_used_set;

        int64_t m_num_fetches;
        int64_t m_num_fetched;
        int64_t m_num_pending;

        //uint64_t m_unresolved_count;

        // Get evr_loop
        evr_loop *m_evr_loop;

};

//: ----------------------------------------------------------------------------
//: Thread local global
//: ----------------------------------------------------------------------------
__thread t_client *g_t_client = NULL;

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
#define COPY_SETTINGS(_field) m_settings._field = a_settings._field
t_client::t_client(const settings_struct_t &a_settings):
        m_t_run_thread(),
        m_settings(),
        m_stopped(false),
        m_nconn_vector(a_settings.m_num_parallel),
        m_conn_free_list(),
        m_conn_used_set(),
        m_num_fetches(-1),
        m_num_fetched(0),
        m_num_pending(0),
        m_evr_loop(NULL)
{
        for(int32_t i_conn = 0; i_conn < a_settings.m_num_parallel; ++i_conn)
        {
                m_nconn_vector[i_conn] = NULL;
                m_conn_free_list.push_back(i_conn);
        }

        // Friggin effc++
        COPY_SETTINGS(m_verbose);
        COPY_SETTINGS(m_color);
        COPY_SETTINGS(m_quiet);
        COPY_SETTINGS(m_show_summary);
        COPY_SETTINGS(m_url);
        COPY_SETTINGS(m_header_map);
        COPY_SETTINGS(m_t_client_list);
        COPY_SETTINGS(m_evr_loop_type);
        COPY_SETTINGS(m_num_parallel);
        COPY_SETTINGS(m_num_threads);
        COPY_SETTINGS(m_timeout_s);
        COPY_SETTINGS(m_connect_only);
        COPY_SETTINGS(m_sock_opt_recv_buf_size);
        COPY_SETTINGS(m_sock_opt_send_buf_size);
        COPY_SETTINGS(m_sock_opt_no_delay);
        COPY_SETTINGS(m_ssl_ctx);
        COPY_SETTINGS(m_ssl_cipher_list);
        COPY_SETTINGS(m_ssl_options_str);
        COPY_SETTINGS(m_ssl_options);
        COPY_SETTINGS(m_ssl_verify);
        COPY_SETTINGS(m_ssl_sni);
        COPY_SETTINGS(m_ssl_ca_file);
        COPY_SETTINGS(m_ssl_ca_path);
        COPY_SETTINGS(m_hlx_client);

}


//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
t_client::~t_client()
{
        for(uint32_t i_conn = 0; i_conn < m_nconn_vector.size(); ++i_conn)
        {
                if(m_nconn_vector[i_conn])
                {
                        delete m_nconn_vector[i_conn];
                        m_nconn_vector[i_conn] = NULL;
                }
        }

        if(m_evr_loop)
        {
                delete m_evr_loop;
        }
}


//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int t_client::run(void)
{

        int32_t l_pthread_error = 0;

        l_pthread_error = pthread_create(&m_t_run_thread,
                        NULL,
                        t_run_static,
                        this);
        if (l_pthread_error != 0)
        {
                // failed to create thread

                NDBG_PRINT("Error: creating thread.  Reason: %s\n.", strerror(l_pthread_error));
                return STATUS_ERROR;

        }

        return STATUS_OK;

}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void t_client::stop(void)
{
        m_stopped = true;
        int32_t l_status;
        l_status = m_evr_loop->stop();
        if(l_status != STATUS_OK)
        {
                NDBG_PRINT("Error performing stop.\n");
        }
}


//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void *t_client::evr_loop_file_writeable_cb(void *a_data)
{
        if(!a_data)
        {
                //NDBG_PRINT("a_data == NULL\n");
                return NULL;
        }

        nconn* l_nconn = static_cast<nconn*>(a_data);
        reqlet *l_reqlet = static_cast<reqlet *>(l_nconn->get_data1());
        t_client *l_t_client = g_t_client;

        // Cancel last timer
        l_t_client->m_evr_loop->cancel_timer(&(l_nconn->m_timer_obj));

        int32_t l_status = STATUS_OK;
        l_status = l_nconn->run_state_machine(l_t_client->m_evr_loop, l_reqlet->m_host_info);
        if(STATUS_ERROR == l_status)
        {
                if(l_nconn->m_verbose)
                {
                        NDBG_PRINT("Error: performing run_state_machine\n");
                }
                T_CLIENT_CONN_CLEANUP(l_t_client, l_nconn, l_reqlet, 901, l_nconn->m_last_error.c_str());
                return NULL;
        }

        if(l_nconn->is_done())
        {
                if(l_t_client->m_settings.m_connect_only)
                {
                        T_CLIENT_CONN_CLEANUP(l_t_client, l_nconn, l_reqlet, 200, "Connected Successfully");
                }
                else
                {
                        T_CLIENT_CONN_CLEANUP(l_t_client, l_nconn, l_reqlet, 0, "");
                }
                return NULL;
        }


        // Add idle timeout
        l_t_client->m_evr_loop->add_timer( l_t_client->get_timeout_s()*1000, evr_loop_file_timeout_cb, l_nconn, &(l_nconn->m_timer_obj));

        return NULL;

}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void *t_client::evr_loop_file_readable_cb(void *a_data)
{
        if(!a_data)
        {
                //NDBG_PRINT("a_data == NULL\n");
                return NULL;
        }

        nconn* l_nconn = static_cast<nconn*>(a_data);
        reqlet *l_reqlet = static_cast<reqlet *>(l_nconn->get_data1());
        t_client *l_t_client = g_t_client;

        // Cancel last timer
        l_t_client->m_evr_loop->cancel_timer(&(l_nconn->m_timer_obj));

        int32_t l_status = STATUS_OK;
        l_status = l_nconn->run_state_machine(l_t_client->m_evr_loop, l_reqlet->m_host_info);
        if(STATUS_ERROR == l_status)
        {
                if(l_nconn->m_verbose)
                {
                        NDBG_PRINT("Error: performing run_state_machine\n");
                }
                T_CLIENT_CONN_CLEANUP(l_t_client, l_nconn, l_reqlet, 901, l_nconn->m_last_error.c_str());
                return NULL;
        }

        if(l_status >= 0)
        {
                l_reqlet->m_stat_agg.m_num_bytes_read += l_status;
        }

        // Check for done...
        if((l_nconn->is_done()) ||
           (l_status == STATUS_ERROR))
        {
                if(l_status == STATUS_ERROR)
                {
                        T_CLIENT_CONN_CLEANUP(l_t_client, l_nconn, l_reqlet, 901, l_nconn->m_last_error.c_str());
                }
                else
                {
                        if(l_t_client->m_settings.m_connect_only)
                        {
                                T_CLIENT_CONN_CLEANUP(l_t_client, l_nconn, l_reqlet, 200, "Connected Successfully");
                        }
                        else
                        {
                                T_CLIENT_CONN_CLEANUP(l_t_client, l_nconn, l_reqlet, 0, "");
                        }
                }
                return NULL;
        }

        // Add idle timeout
        l_t_client->m_evr_loop->add_timer( l_t_client->get_timeout_s()*1000, evr_loop_file_timeout_cb, l_nconn, &(l_nconn->m_timer_obj));

        return NULL;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void *g_completion_timer;
void *t_client::evr_loop_timer_completion_cb(void *a_data)
{
        return NULL;
}


//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void *t_client::evr_loop_file_error_cb(void *a_data)
{
        //NDBG_PRINT("%sSTATUS_ERRORS%s\n", ANSI_COLOR_FG_RED, ANSI_COLOR_OFF);
        return NULL;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void *t_client::evr_loop_file_timeout_cb(void *a_data)
{
        if(!a_data)
        {
                //NDBG_PRINT("a_data == NULL\n");
                return NULL;
        }

        nconn* l_nconn = static_cast<nconn*>(a_data);
        reqlet *l_reqlet = static_cast<reqlet *>(l_nconn->get_data1());
        t_client *l_t_client = g_t_client;

        //printf("%sT_O%s: %p\n",ANSI_COLOR_FG_BLUE, ANSI_COLOR_OFF,
        //                l_rconn->m_timer_obj);

        // Add stats
        //add_stat_to_agg(l_reqlet->m_stat_agg, l_nconn->get_stats());
        if(l_t_client->m_settings.m_verbose)
        {
                NDBG_PRINT("%sTIMING OUT CONN%s: i_conn: %lu HOST: %s THIS: %p\n",
                                ANSI_COLOR_BG_RED, ANSI_COLOR_OFF,
                                l_nconn->get_id(),
                                l_reqlet->m_url.m_host.c_str(),
                                l_t_client);
        }

        // Cleanup
        T_CLIENT_CONN_CLEANUP(l_t_client, l_nconn, l_reqlet, 902, "Connection timed out");

        return NULL;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void *t_client::evr_loop_timer_cb(void *a_data)
{
        return NULL;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void *t_client::t_run(void *a_nothing)
{

        m_stopped = false;

        // Set thread local
        g_t_client = this;

        // Create loop
        m_evr_loop = new evr_loop(
                        evr_loop_file_readable_cb,
                        evr_loop_file_writeable_cb,
                        evr_loop_file_error_cb,
                        m_settings.m_evr_loop_type,
                        m_settings.m_num_parallel);

        // Get hlx ptr
        hlx_client *l_hlx_client = m_settings.m_hlx_client;

        // -------------------------------------------
        // Main loop.
        // -------------------------------------------
        //NDBG_PRINT("starting main loop\n");
        while(!m_stopped &&
                        !l_hlx_client->done())
        {

                // -------------------------------------------
                // Start Connections
                // -------------------------------------------
                //NDBG_PRINT("%sSTART_CONNECTIONS%s\n", ANSI_COLOR_BG_MAGENTA, ANSI_COLOR_OFF);
                if(!l_hlx_client->done())
                {
                        int32_t l_status;
                        l_status = start_connections();
                        if(l_status != STATUS_OK)
                        {
                                //NDBG_PRINT("%sSTART_CONNECTIONS%s ERROR!\n", ANSI_COLOR_BG_RED, ANSI_COLOR_OFF);
                                return NULL;
                        }
                }

                // Run loop
                m_evr_loop->run();

        }

        //NDBG_PRINT("%sFINISHING_CONNECTIONS%s -done: %d -- m_stopped: %d\n", ANSI_COLOR_BG_MAGENTA, ANSI_COLOR_OFF, l_hlx_client->done(), m_stopped);

        // Still awaiting responses -wait...
        uint64_t l_cur_time = get_time_s();
        uint64_t l_end_time = l_cur_time + m_settings.m_timeout_s;
        while(!m_stopped && (m_num_pending > 0) && (l_cur_time < l_end_time))
        {
                // Run loop
                //NDBG_PRINT("waiting: m_num_pending: %d --time-left: %d\n", (int)m_num_pending, int(l_end_time - l_cur_time));
                m_evr_loop->run();

                // TODO -this is pretty hard polling -make option???
                usleep(10000);
                l_cur_time = get_time_s();

        }
        //NDBG_PRINT("%sDONE_CONNECTIONS%s\n", ANSI_COLOR_BG_YELLOW, ANSI_COLOR_OFF);

        m_stopped = true;

        return NULL;

}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
nconn *t_client::create_new_nconn(uint32_t a_id, const reqlet &a_reqlet)
{
        nconn *l_nconn = NULL;

        if(a_reqlet.m_url.m_scheme == nconn::SCHEME_TCP)
        {
                // TODO SET OPTIONS!!!
                l_nconn = new nconn_tcp(m_settings.m_verbose,
                                        m_settings.m_color,
                                        1,
                                        true,
                                        false,
                                        m_settings.m_connect_only);
        }
        else if(a_reqlet.m_url.m_scheme == nconn::SCHEME_SSL)
        {
                // TODO SET OPTIONS!!!
                l_nconn = new nconn_ssl(m_settings.m_verbose,
                                        m_settings.m_color,
                                        1,
                                        true,
                                        false,
                                        m_settings.m_connect_only);
        }

        // -------------------------------------------
        // Set options
        // -------------------------------------------
        // Set generic options
        HLX_CLIENT_SET_NCONN_OPT((*l_nconn), nconn_tcp::OPT_TCP_RECV_BUF_SIZE, NULL, m_settings.m_sock_opt_recv_buf_size);
        HLX_CLIENT_SET_NCONN_OPT((*l_nconn), nconn_tcp::OPT_TCP_SEND_BUF_SIZE, NULL, m_settings.m_sock_opt_send_buf_size);
        HLX_CLIENT_SET_NCONN_OPT((*l_nconn), nconn_tcp::OPT_TCP_NO_DELAY, NULL, m_settings.m_sock_opt_no_delay);

        // Set ssl options
        if(a_reqlet.m_url.m_scheme == nconn::SCHEME_SSL)
        {
                HLX_CLIENT_SET_NCONN_OPT((*l_nconn), nconn_ssl::OPT_SSL_CIPHER_STR,
                                              m_settings.m_ssl_cipher_list.c_str(),
                                              m_settings.m_ssl_cipher_list.length());

                HLX_CLIENT_SET_NCONN_OPT((*l_nconn), nconn_ssl::OPT_SSL_CTX,
                                             m_settings.m_ssl_ctx,
                                             sizeof(m_settings.m_ssl_ctx));

                HLX_CLIENT_SET_NCONN_OPT((*l_nconn), nconn_ssl::OPT_SSL_VERIFY,
                                              &(m_settings.m_ssl_verify),
                                              sizeof(m_settings.m_ssl_verify));

                //HLX_CLIENT_SET_NCONN_OPT((*l_nconn), nconn_ssl::OPT_SSL_OPTIONS,
                //                              &(m_settings.m_ssl_options),
                //                              sizeof(m_settings.m_ssl_options));

        }

        return l_nconn;

}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t t_client::start_connections(void)
{
        int32_t l_status;
        reqlet *l_reqlet = NULL;

        // Get hlx ptr
        hlx_client *l_hlx_client = m_settings.m_hlx_client;

        // Find an empty client slot.
        //NDBG_PRINT("m_conn_free_list.size(): %Zu\n", m_conn_free_list.size());
        for (conn_id_list_t::iterator i_conn = m_conn_free_list.begin();
               (i_conn != m_conn_free_list.end()) &&
               (!l_hlx_client->done()) &&
               !m_stopped;
             )
        {

                // Loop trying to get reqlet
                l_reqlet = NULL;
                while(((l_reqlet = l_hlx_client->try_get_resolved()) == NULL) && (!l_hlx_client->done()));
                if((l_reqlet == NULL) && l_hlx_client->done())
                {
                        // Bail out
                        return STATUS_OK;
                }

                // Start client for this reqlet
                //NDBG_PRINT("i_conn: %d\n", *i_conn);
                nconn *l_nconn = m_nconn_vector[*i_conn];
                // TODO Check for NULL

                if(l_nconn &&
                   (l_nconn->m_scheme != l_reqlet->m_url.m_scheme))
                {
                        // Destroy nconn and recreate
                        delete l_nconn;
                        l_nconn = NULL;
                }


                if(!l_nconn)
                {
                        // Create nconn
                        l_nconn = create_new_nconn(*i_conn, *l_reqlet);
                        if(!l_nconn)
                        {
                                NDBG_PRINT("Error performing create_new_nconn\n");
                                return STATUS_ERROR;
                        }
                }

                // Assign the reqlet for this client
                l_nconn->set_data1(l_reqlet);

                // Bump stats
                ++(l_reqlet->m_stat_agg.m_num_conn_started);

                // Create request
                if(!m_settings.m_connect_only)
                {
                        create_request(*l_nconn, *l_reqlet);
                }

                m_conn_used_set.insert(*i_conn);
                m_conn_free_list.erase(i_conn++);

                // TODO Make configurable
                m_evr_loop->add_timer(m_settings.m_timeout_s*1000, evr_loop_file_timeout_cb, l_nconn, &(l_nconn->m_timer_obj));

                // Add to num pending
                ++m_num_pending;

                //NDBG_PRINT("%sCONNECT%s: %s\n", ANSI_COLOR_BG_MAGENTA, ANSI_COLOR_OFF, l_reqlet->m_url.m_host.c_str());
                l_nconn->set_host(l_reqlet->m_url.m_host);
                l_status = l_nconn->run_state_machine(m_evr_loop, l_reqlet->m_host_info);
                if(STATUS_OK != l_status)
                {
                        NDBG_PRINT("Error: Performing do_connect\n");
                        T_CLIENT_CONN_CLEANUP(this, l_nconn, l_reqlet, 500, "Performing do_connect");
                        continue;
                }
        }

        return STATUS_OK;

}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t t_client::create_request(nconn &ao_conn,
                                 reqlet &a_reqlet)
{
        // Get client
        char *l_req_buf = NULL;
        uint32_t l_req_buf_len = 0;
        uint32_t l_max_buf_len = nconn_tcp::m_max_req_buf;

        GET_NCONN_OPT(ao_conn, nconn_tcp::OPT_TCP_REQ_BUF, (void **)(&l_req_buf), &l_req_buf_len);

        // -------------------------------------------
        // Request.
        // -------------------------------------------
        const std::string &l_path_ref = a_reqlet.get_path(NULL);
        //NDBG_PRINT("HOST: %s PATH: %s\n", a_reqlet.m_url.m_host.c_str(), l_path_ref.c_str());
        if(l_path_ref.length())
        {
                l_req_buf_len = snprintf(l_req_buf, l_max_buf_len,
                                "GET %.500s HTTP/1.1\r\n", l_path_ref.c_str());
        } else {
                l_req_buf_len = snprintf(l_req_buf, l_max_buf_len,
                                "GET / HTTP/1.1\r\n");
        }

        // -------------------------------------------
        // Add repo headers
        // -------------------------------------------
        bool l_specd_host = false;

        // Loop over reqlet map
        for(header_map_t::const_iterator i_header = m_settings.m_header_map.begin();
                        i_header != m_settings.m_header_map.end();
                        ++i_header)
        {
                if(!i_header->first.empty() && !i_header->second.empty())
                {
                        //printf("Adding HEADER: %s: %s\n", i_header->first.c_str(), i_header->second.c_str());
                        l_req_buf_len += snprintf(l_req_buf + l_req_buf_len, l_max_buf_len - l_req_buf_len,
                                        "%s: %s\r\n", i_header->first.c_str(), i_header->second.c_str());

                        if (strcasecmp(i_header->first.c_str(), "host") == 0)
                        {
                                l_specd_host = true;
                        }
                }
        }

        // -------------------------------------------
        // Default Host if unspecified
        // -------------------------------------------
        if (!l_specd_host)
        {
                l_req_buf_len += snprintf(l_req_buf + l_req_buf_len, l_max_buf_len - l_req_buf_len,
                                "Host: %s\r\n", a_reqlet.m_url.m_host.c_str());
        }

        // -------------------------------------------
        // End of request terminator...
        // -------------------------------------------
        l_req_buf_len += snprintf(l_req_buf + l_req_buf_len, l_max_buf_len - l_req_buf_len, "\r\n");

        // Set len
        SET_NCONN_OPT(ao_conn, nconn_tcp::OPT_TCP_REQ_BUF_LEN, NULL, l_req_buf_len);
        return STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t t_client::cleanup_connection(nconn *a_nconn, bool a_cancel_timer)
{

        uint64_t l_conn_id = a_nconn->get_id();

        // Cancel last timer
        if(a_cancel_timer)
        {
                m_evr_loop->cancel_timer(&(a_nconn->m_timer_obj));
        }
        a_nconn->reset_stats();
        a_nconn->cleanup(m_evr_loop);

        // Add back to free list
        m_conn_free_list.push_back(l_conn_id);
        m_conn_used_set.erase(l_conn_id);

        // Reduce num pending
        ++m_num_fetched;
        --m_num_pending;

        return STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t t_client::set_header(const std::string &a_header_key, const std::string &a_header_val)
{
        int32_t l_retval = STATUS_OK;
        m_settings.m_header_map[a_header_key] = a_header_val;
        return l_retval;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int hlx_client::init(void)
{

        // -------------------------------------------
        // Init resolver with cache
        // -------------------------------------------
        int32_t l_ldb_init_status;
        l_ldb_init_status = resolver::get()->init(m_ai_cache, m_use_ai_cache);
        if(STATUS_OK != l_ldb_init_status)
        {
                return HLX_CLIENT_STATUS_ERROR;
        }

        // -------------------------------------------
        // SSL init...
        // -------------------------------------------
        m_ssl_ctx = ssl_init(m_ssl_cipher_list, // ctx cipher list str
                             m_ssl_options,     // ctx options
                             m_ssl_ca_file,     // ctx ca file
                             m_ssl_ca_path);    // ctx ca path
        if(NULL == m_ssl_ctx)
        {
                NDBG_PRINT("Error: performing ssl_init with cipher_list: %s\n", m_ssl_cipher_list.c_str());
                return HLX_CLIENT_STATUS_ERROR;
        }

        m_is_initd = true;
        return HLX_CLIENT_STATUS_OK;

}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int hlx_client::run(void)
{
        int l_status = 0;
        if(!m_is_initd)
        {
                l_status = init();
                if(HLX_CLIENT_STATUS_OK != l_status)
                {
                        return HLX_CLIENT_STATUS_ERROR;
                }
        }

        // -------------------------------------------
        // Bury the config into a settings struct
        // -------------------------------------------
        settings_struct_t l_settings;
        l_settings.m_verbose = m_verbose;
        l_settings.m_color = m_color;
        l_settings.m_quiet = m_quiet;
        l_settings.m_show_summary = m_show_summary;
        l_settings.m_url = m_url;
        l_settings.m_header_map = m_header_map;
        l_settings.m_evr_loop_type = (evr_loop_type_t)m_evr_loop_type;
        l_settings.m_num_parallel = m_num_parallel;
        l_settings.m_num_threads = m_num_threads;
        l_settings.m_timeout_s = m_timeout_s;
        l_settings.m_connect_only = m_connect_only;
        l_settings.m_sock_opt_recv_buf_size = m_sock_opt_recv_buf_size;
        l_settings.m_sock_opt_send_buf_size = m_sock_opt_send_buf_size;
        l_settings.m_sock_opt_no_delay = m_sock_opt_no_delay;
        l_settings.m_ssl_ctx = m_ssl_ctx;
        l_settings.m_ssl_cipher_list = m_ssl_cipher_list;
        l_settings.m_ssl_options_str = m_ssl_options_str;
        l_settings.m_ssl_options = m_ssl_options;
        l_settings.m_ssl_verify = m_ssl_verify;
        l_settings.m_ssl_sni = m_ssl_sni;
        l_settings.m_ssl_ca_file = m_ssl_ca_file;
        l_settings.m_ssl_ca_path = m_ssl_ca_path;
        l_settings.m_hlx_client = this;

        // -------------------------------------------
        // Create t_client list...
        // -------------------------------------------
        for(uint32_t i_client_idx = 0; i_client_idx < m_num_threads; ++i_client_idx)
        {

                //if(a_settings.m_verbose)
                //{
                //        NDBG_PRINT("Creating...\n");
                //}

                // Construct with settings...
                t_client *l_t_client = new t_client(l_settings);
                for(header_map_t::iterator i_header = m_header_map.begin();
                    i_header != m_header_map.end();
                    ++i_header)
                {
                        l_t_client->set_header(i_header->first, i_header->second);
                }

                m_t_client_list.push_back(l_t_client);
        }

        // -------------------------------------------
        // Run...
        // -------------------------------------------
        for(t_client_list_t::iterator i_t_client = m_t_client_list.begin();
                        i_t_client != m_t_client_list.end();
                        ++i_t_client)
        {
                //if(a_settings.m_verbose)
                //{
                //        NDBG_PRINT("Running...\n");
                //}
                (*i_t_client)->run();
        }

        return HLX_CLIENT_STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int hlx_client::stop(void)
{
        int32_t l_retval = HLX_CLIENT_STATUS_OK;

        for (t_client_list_t::iterator i_t_client = m_t_client_list.begin();
                        i_t_client != m_t_client_list.end();
                        ++i_t_client)
        {
                (*i_t_client)->stop();
        }

        return l_retval;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void hlx_client::wait_till_stopped(void)
{
        //int32_t l_retval = HLX_CLIENT_STATUS_OK;

        // -------------------------------------------
        // Join all threads before exit
        // -------------------------------------------
        for(t_client_list_t::iterator i_client = m_t_client_list.begin();
            i_client != m_t_client_list.end();
            ++i_client)
        {
                //if(m_verbose)
                //{
                //      NDBG_PRINT("joining...\n");
                //}
                pthread_join(((*i_client)->m_t_run_thread), NULL);

        }
        //return l_retval;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
bool hlx_client::is_running(void)
{
        for (t_client_list_t::iterator i_t_client = m_t_client_list.begin();
                        i_t_client != m_t_client_list.end();
                        ++i_t_client)
        {
                if((*i_t_client)->is_running())
                        return true;
        }
        return false;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void hlx_client::set_quiet(bool a_val)
{
        m_quiet = a_val;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void hlx_client::set_verbose(bool a_val)
{
        m_verbose = a_val;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void hlx_client::set_color(bool a_val)
{
        m_color = a_val;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void hlx_client::set_url(const std::string &a_url)
{
        m_url = a_url;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int hlx_client::set_host_list(host_list_t &a_host_list)
{
        // Create the reqlet list
        uint32_t l_reqlet_num = 0;
        for(host_list_t::const_iterator i_host = a_host_list.begin();
                        i_host != a_host_list.end();
                        ++i_host, ++l_reqlet_num)
        {
                // Create a re
                reqlet *l_reqlet = new reqlet(l_reqlet_num, 1);
                l_reqlet->init_with_url(m_url);

                // Get host and port if exist
                parsed_url l_url;

                //TODO REMOVE!!!
                //NDBG_PRINT("HOST: %s\n", i_host->m_host.c_str());

                l_url.parse(i_host->m_host);

                if(strchr(i_host->m_host.c_str(), (int)':'))
                {
                        l_reqlet->set_host(l_url.m_host);
                        l_reqlet->set_port(l_url.m_port);
                }
                else
                {
                        // TODO make set host take const
                        l_reqlet->set_host(i_host->m_host);
                }

                if(!i_host->m_hostname.empty())
                {
                     l_reqlet->m_url.m_hostname = i_host->m_hostname;
                }
                if(!i_host->m_id.empty())
                {
                     l_reqlet->m_url.m_id = i_host->m_id;
                }
                if(!i_host->m_where.empty())
                {
                     l_reqlet->m_url.m_where = i_host->m_where;
                }

                // Add to list
                add_reqlet(l_reqlet);

        }

        return HLX_CLIENT_STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int hlx_client::set_server_list(server_list_t &a_server_list)
{
        // Create the reqlet list
        uint32_t l_reqlet_num = 0;
        for(server_list_t::const_iterator i_server = a_server_list.begin();
            i_server != a_server_list.end();
            ++i_server, ++l_reqlet_num)
        {
                // Create a re
                reqlet *l_reqlet = new reqlet(l_reqlet_num, 1);
                l_reqlet->init_with_url(m_url);

                // Get host and port if exist
                parsed_url l_url;
                l_url.parse(*i_server);

                if(strchr(i_server->c_str(), (int)':'))
                {
                        l_reqlet->set_host(l_url.m_host);
                        l_reqlet->set_port(l_url.m_port);
                }
                else
                {
                        // TODO make set host take const
                        l_reqlet->set_host(*i_server);
                }

                // Add to list
                add_reqlet(l_reqlet);

        }

        return HLX_CLIENT_STATUS_OK;

}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void hlx_client::set_connect_only(bool a_val)
{
        m_connect_only = a_val;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void hlx_client::set_use_ai_cache(bool a_val)
{
        m_use_ai_cache = a_val;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void hlx_client::set_ai_cache(const std::string &a_ai_cache)
{
        m_ai_cache = a_ai_cache;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void hlx_client::set_timeout_s(uint32_t a_timeout_s)
{
        m_timeout_s = a_timeout_s;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void hlx_client::set_num_threads(uint32_t a_num_threads)
{
        m_num_threads = a_num_threads;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void hlx_client::set_num_parallel(uint32_t a_num_parallel)
{
        m_num_parallel = a_num_parallel;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void hlx_client::set_show_summary(bool a_val)
{
        m_show_summary = a_val;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void hlx_client::set_sock_opt_no_delay(bool a_val)
{
        m_sock_opt_no_delay = a_val;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void hlx_client::set_sock_opt_send_buf_size(uint32_t a_send_buf_size)
{
        m_sock_opt_send_buf_size = a_send_buf_size;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void hlx_client::set_sock_opt_recv_buf_size(uint32_t a_recv_buf_size)
{
        m_sock_opt_recv_buf_size = a_recv_buf_size;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int hlx_client::set_header(const std::string &a_header)
{
        int32_t l_status;
        std::string l_header_key;
        std::string l_header_val;
        l_status = break_header_string(a_header, l_header_key, l_header_val);
        if(l_status != 0)
        {
                // If verbose???
                //printf("Error header string[%s] is malformed\n", a_header.c_str());
                return HLX_CLIENT_STATUS_ERROR;
        }
        set_header(l_header_key, l_header_val);

        return HLX_CLIENT_STATUS_OK;

}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int hlx_client::set_header(const std::string &a_key, const std::string &a_val)
{
        m_header_map[a_key] = a_val;
        return HLX_CLIENT_STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void hlx_client::clear_headers(void)
{
        m_header_map.clear();
}


//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void hlx_client::set_ssl_cipher_list(const std::string &a_cipher_list)
{
        m_ssl_cipher_list = a_cipher_list;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int hlx_client::set_ssl_options(const std::string &a_ssl_options_str)
{
        int32_t l_status;
        l_status = get_ssl_options_str_val(a_ssl_options_str, m_ssl_options);
        if(l_status != HLX_CLIENT_STATUS_OK)
        {
                return HLX_CLIENT_STATUS_ERROR;
        }
        return HLX_CLIENT_STATUS_OK;

}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int hlx_client::set_ssl_options(long a_ssl_options)
{
        m_ssl_options = a_ssl_options;
        return HLX_CLIENT_STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void hlx_client::set_ssl_ca_path(const std::string &a_ssl_ca_path)
{
        m_ssl_ca_path = a_ssl_ca_path;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void hlx_client::set_ssl_ca_file(const std::string &a_ssl_ca_file)
{
        m_ssl_ca_file = a_ssl_ca_file;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void hlx_client::set_ssl_sni_verify(bool a_val)
{
        m_ssl_sni = a_val;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void hlx_client::set_ssl_verify(bool a_val)
{
        m_ssl_verify = a_val;
}

//: ----------------------------------------------------------------------------
//: \details: Constructor
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
hlx_client::hlx_client(void):

        // General
        m_verbose(false),
        m_color(false),
        m_quiet(false),

        // Run settings
        m_url(),
        m_header_map(),
        m_use_ai_cache(true),
        m_ai_cache(),
        // TODO Make define
        m_num_parallel(64),
        // TODO Make define
        m_num_threads(8),
        // TODO Make define
        m_timeout_s(10),
        m_connect_only(false),
        m_show_summary(false),

        // Socket options
        m_sock_opt_recv_buf_size(0),
        m_sock_opt_send_buf_size(0),
        m_sock_opt_no_delay(false),

        // SSL
        m_ssl_ctx(NULL),
        m_ssl_cipher_list(),
        m_ssl_options_str(),
        m_ssl_options(0),
        m_ssl_verify(false),
        m_ssl_sni(false),
        m_ssl_ca_file(),
        m_ssl_ca_path(),

        // t_client
        m_t_client_list(),
        m_evr_loop_type(EVR_LOOP_EPOLL),

        m_reqlet_vector(),
        m_reqlet_vector_idx(0),
        m_mutex(),
        m_num_reqlets(0),
        m_num_get(0),
        m_num_done(0),
        m_num_resolved(0),
        m_num_error(0),
        m_summary_success(0),
        m_summary_error_addr(0),
        m_summary_error_conn(0),
        m_summary_error_unknown(0),
        m_summary_ssl_error_self_signed(0),
        m_summary_ssl_error_expired(0),
        m_summary_ssl_error_other(0),
        m_summary_ssl_protocols(),
        m_summary_ssl_ciphers(),
        m_is_initd(false)
{
        // Init mutex
        pthread_mutex_init(&m_mutex, NULL);

};

//: ----------------------------------------------------------------------------
//: \details: Constructor
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
hlx_client::~hlx_client(void)
{
        // Delete reqlets

        for(size_t i_reqlet = 0;
            i_reqlet < m_reqlet_vector.size();
            ++i_reqlet)
        {
                reqlet *i_reqlet_ptr = m_reqlet_vector[i_reqlet];
                if(i_reqlet_ptr)
                {
                        delete i_reqlet_ptr;
                        i_reqlet_ptr = NULL;
                }
        }

        // Delete t_client list...
        for(t_client_list_t::iterator i_client_hle = m_t_client_list.begin();
                        i_client_hle != m_t_client_list.end(); )
        {
                t_client *l_t_client_ptr = *i_client_hle;
                if(l_t_client_ptr)
                {
                        delete l_t_client_ptr;
                        m_t_client_list.erase(i_client_hle++);
                        l_t_client_ptr = NULL;
                }
        }

        // SSL Cleanup
        ssl_kill_locks();

        // TODO Deprecated???
        //EVP_cleanup();

}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
#define ARESP(_str) l_responses_str += _str
std::string hlx_client::dump_all_responses(bool a_color, bool a_pretty, output_type_t a_output_type, int a_part_map)
{
        std::string l_responses_str = "";
        std::string l_host_color = "";
        std::string l_server_color = "";
        std::string l_id_color = "";
        std::string l_status_color = "";
        std::string l_header_color = "";
        std::string l_body_color = "";
        std::string l_off_color = "";
        char l_buf[1024*1024];
        if(a_color)
        {
                l_host_color = ANSI_COLOR_FG_BLUE;
                l_server_color = ANSI_COLOR_FG_RED;
                l_id_color = ANSI_COLOR_FG_CYAN;
                l_status_color = ANSI_COLOR_FG_MAGENTA;
                l_header_color = ANSI_COLOR_FG_GREEN;
                l_body_color = ANSI_COLOR_FG_YELLOW;
                l_off_color = ANSI_COLOR_OFF;
        }

        if(a_output_type == OUTPUT_JSON)
        {
                ARESP("[\n");
        }

        int l_cur_reqlet = 0;
        int l_reqlet_num = m_reqlet_vector.size();
        for(reqlet_vector_t::iterator i_reqlet = m_reqlet_vector.begin();
            i_reqlet != m_reqlet_vector.end();
            ++i_reqlet, ++l_cur_reqlet)
        {


                bool l_fbf = false;
                if(a_output_type == OUTPUT_JSON)
                {
                        if(a_pretty) if(a_output_type == OUTPUT_JSON) ARESP("  ");
                        ARESP("{");
                        if(a_pretty) if(a_output_type == OUTPUT_JSON) ARESP("\n");
                }

                // Host
                if(a_part_map & PART_HOST)
                {
                        if(a_pretty) if(a_output_type == OUTPUT_JSON) ARESP("    ");
                        sprintf(l_buf, "\"%shost%s\": \"%s\"",
                                        l_host_color.c_str(), l_off_color.c_str(),
                                        (*i_reqlet)->m_url.m_host.c_str());
                        ARESP(l_buf);
                        l_fbf = true;
                }

                // Server
                if(a_part_map & PART_SERVER)
                {

                        if(l_fbf) {ARESP(", "); l_fbf = false;}
                        if(a_pretty) if(a_output_type == OUTPUT_JSON) ARESP("    ");
                        sprintf(l_buf, "\"%sserver%s\": \"%s:%d\"",
                                        l_server_color.c_str(), l_server_color.c_str(),
                                        (*i_reqlet)->m_url.m_host.c_str(),
                                        (*i_reqlet)->m_url.m_port
                                        );
                        ARESP(l_buf);
                        l_fbf = true;

                        if(!(*i_reqlet)->m_url.m_id.empty())
                        {
                                if(l_fbf) {ARESP(", "); l_fbf = false;}
                                if(a_pretty) if(a_output_type == OUTPUT_JSON) ARESP("    ");
                                sprintf(l_buf, "\"%sid%s\": \"%s\"",
                                                l_id_color.c_str(), l_id_color.c_str(),
                                                (*i_reqlet)->m_url.m_id.c_str()
                                                );
                                ARESP(l_buf);
                                l_fbf = true;
                        }

                        if(!(*i_reqlet)->m_url.m_where.empty())
                        {
                                if(l_fbf) {ARESP(", "); l_fbf = false;}
                                if(a_pretty) if(a_output_type == OUTPUT_JSON) ARESP("    ");
                                sprintf(l_buf, "\"%swhere%s\": \"%s\"",
                                                l_id_color.c_str(), l_id_color.c_str(),
                                                (*i_reqlet)->m_url.m_where.c_str()
                                                );
                                ARESP(l_buf);
                                l_fbf = true;
                        }


                        l_fbf = true;
                }

                // Status Code
                if(a_part_map & PART_STATUS_CODE)
                {
                        if(l_fbf) {ARESP(", "); l_fbf = false;}
                        if(a_pretty) if(a_output_type == OUTPUT_JSON) ARESP("\n    ");
                        const char *l_status_val_color = "";
                        if(a_color)
                        {
                                if((*i_reqlet)->m_response_status == 200) l_status_val_color = ANSI_COLOR_FG_GREEN;
                                else l_status_val_color = ANSI_COLOR_FG_RED;
                        }
                        sprintf(l_buf, "\"%sstatus-code%s\": %s%d%s",
                                        l_status_color.c_str(), l_off_color.c_str(),
                                        l_status_val_color, (*i_reqlet)->m_response_status, l_off_color.c_str());
                        ARESP(l_buf);
                        l_fbf = true;
                }

                // Headers
                // TODO -only in json mode for now
                if(a_output_type == OUTPUT_JSON)
                if(a_part_map & PART_HEADERS)
                {
                        for(header_map_t::iterator i_header = (*i_reqlet)->m_response_headers.begin();
                                        i_header != (*i_reqlet)->m_response_headers.end();
                            ++i_header)
                        {
                                if(l_fbf) {ARESP(", "); l_fbf = false;}
                                if(a_pretty) if(a_output_type == OUTPUT_JSON) NDBG_OUTPUT("\n    ");
                                sprintf(l_buf, "\"%s%s%s\": \"%s\"",
                                                l_header_color.c_str(), i_header->first.c_str(), l_off_color.c_str(),
                                                i_header->second.c_str());
                                ARESP(l_buf);
                                l_fbf = true;
                        }
                }

                // Headers
                // TODO -only in json mode for now
                //if(a_output_type == OUTPUT_JSON)
                //if(a_part_map & PART_HEADERS)
                if(1)
                {
                        for(header_map_t::iterator i_header = (*i_reqlet)->m_conn_info.begin();
                                        i_header != (*i_reqlet)->m_conn_info.end();
                            ++i_header)
                        {
                                if(l_fbf) {ARESP(", "); l_fbf = false;}
                                if(a_pretty) if(a_output_type == OUTPUT_JSON) NDBG_OUTPUT("\n    ");
                                sprintf(l_buf, "\"%s%s%s\": \"%s\"",
                                                l_header_color.c_str(), i_header->first.c_str(), l_off_color.c_str(),
                                                i_header->second.c_str());
                                ARESP(l_buf);
                                l_fbf = true;
                        }
                }

                // Body
                if(a_part_map & PART_BODY)
                {
                        if(l_fbf) {ARESP(", "); l_fbf = false;}
                        if(a_pretty) if(a_output_type == OUTPUT_JSON) ARESP("\n    ");

                        //NDBG_PRINT("RESPONSE SIZE: %ld\n", (*i_reqlet)->m_response_body.length());
                        if(!(*i_reqlet)->m_response_body.empty())
                        {
                                sprintf(l_buf, "\"%sbody%s\": %s",
                                                l_body_color.c_str(), l_off_color.c_str(),
                                                (*i_reqlet)->m_response_body.c_str());
                        }
                        else
                        {
                                sprintf(l_buf, "\"%sbody%s\": \"NO_RESPONSE\"",
                                                l_body_color.c_str(), l_off_color.c_str());
                        }
                        ARESP(l_buf);
                        l_fbf = true;
                }

                if(a_output_type == OUTPUT_JSON)
                {
                        if(a_pretty) ARESP("\n  ");
                        ARESP("}");
                        if(l_cur_reqlet < (l_reqlet_num - 1)) ARESP(", ");
                }
                ARESP("\n");
        }

        if(a_output_type == OUTPUT_JSON)
        {
                ARESP("]\n");
        }

        return l_responses_str;

}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
bool hlx_client::done(void)
{
        return (m_num_get >= m_num_reqlets);
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void hlx_client::up_done(bool a_error)
{
        ++m_num_done;
        if(a_error)++m_num_error;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t hlx_client::append_summary(reqlet *a_reqlet)
{
        // Examples:
        //
        // Address resolution
        //{"status-code": 900, "body": "Address resolution failed."},
        //
        // Connectivity
        //
        //{"status-code": 0, "body": "NO_RESPONSE"
        //
        //{"status-code": 902, "body": "Connection timed out"
        //
        //{"status-code": 901, "body": "Error Connection refused. Reason: Connection refused"
        //{"status-code": 901, "body": "Error Unkown. Reason: No route to host"
        //
        //{"status-code": 901, "body": "SSL_ERROR_SYSCALL 0: error:00000000:lib(0):func(0):reason(0). Connection reset by peer"
        //{"status-code": 901, "body": "SSL_ERROR_SYSCALL 0: error:00000000:lib(0):func(0):reason(0). An EOF was observed that violates the protocol"
        //{"status-code": 901, "body": "SSL_ERROR_SSL 0: error:140770FC:SSL routines:SSL23_GET_SERVER_HELLO:unknown protocol."
        //{"status-code": 901, "body": "SSL_ERROR_SSL 0: error:14077410:SSL routines:SSL23_GET_SERVER_HELLO:sslv3 alert handshake failure."}
        //{"status-code": 901, "body": "SSL_ERROR_SSL 0: error:14077438:SSL routines:SSL23_GET_SERVER_HELLO:tlsv1 alert internal error."
        //{"status-code": 901, "body": "SSL_ERROR_SSL 0: error:14077458:SSL routines:SSL23_GET_SERVER_HELLO:tlsv1 unrecognized name."

        pthread_mutex_lock(&m_mutex);

        if(a_reqlet->m_response_status == 900)
        {
                ++m_summary_error_addr;
        }
        else if((a_reqlet->m_response_status == 0) ||
                (a_reqlet->m_response_status == 901) ||
                (a_reqlet->m_response_status == 902))
        {
                // Missing ca
                if(a_reqlet->m_response_body.find("unable to get local issuer certificate") != std::string::npos)
                {
                        ++m_summary_ssl_error_other;
                }
                // expired
                if(a_reqlet->m_response_body.find("certificate has expired") != std::string::npos)
                {
                        ++m_summary_ssl_error_expired;
                }
                // expired
                if(a_reqlet->m_response_body.find("self signed certificate") != std::string::npos)
                {
                        ++m_summary_ssl_error_self_signed;
                }

                ++m_summary_error_conn;
        }
        else if(a_reqlet->m_response_status == 200)
        {
                ++m_summary_success;

                header_map_t::iterator i_h;
                if((i_h = a_reqlet->m_conn_info.find("Protocol")) != a_reqlet->m_conn_info.end())
                {
                        ++m_summary_ssl_protocols[i_h->second];
                }
                if((i_h = a_reqlet->m_conn_info.find("Cipher")) != a_reqlet->m_conn_info.end())
                {
                        ++m_summary_ssl_ciphers[i_h->second];
                }
        }
        else
        {
                ++m_summary_error_unknown;
        }

        pthread_mutex_unlock(&m_mutex);

        // TODO
        return STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void hlx_client::display_summary(bool a_color)
{
        std::string l_header_str = "";
        std::string l_protocol_str = "";
        std::string l_cipher_str = "";
        std::string l_off_color = "";

        if(a_color)
        {
                l_header_str = ANSI_COLOR_FG_CYAN;
                l_protocol_str = ANSI_COLOR_FG_GREEN;
                l_cipher_str = ANSI_COLOR_FG_YELLOW;
                l_off_color = ANSI_COLOR_OFF;
        }

        NDBG_OUTPUT("****************** %sSUMMARY%s ****************** \n", l_header_str.c_str(), l_off_color.c_str());
        NDBG_OUTPUT("| total hosts:                     %u\n",m_num_reqlets);
        NDBG_OUTPUT("| success:                         %u\n",m_summary_success);
        NDBG_OUTPUT("| error address lookup:            %u\n",m_summary_error_addr);
        NDBG_OUTPUT("| error connectivity:              %u\n",m_summary_error_conn);
        NDBG_OUTPUT("| error unknown:                   %u\n",m_summary_error_unknown);
        NDBG_OUTPUT("| ssl error cert expired           %u\n",m_summary_ssl_error_expired);
        NDBG_OUTPUT("| ssl error cert self-signed       %u\n",m_summary_ssl_error_self_signed);
        NDBG_OUTPUT("| ssl error other                  %u\n",m_summary_ssl_error_other);

        // Sort
        typedef std::map<uint32_t, std::string> _sorted_map_t;
        _sorted_map_t l_sorted_map;
        NDBG_OUTPUT("+--------------- %sSSL PROTOCOLS%s -------------- \n", l_protocol_str.c_str(), l_off_color.c_str());
        l_sorted_map.clear();
        for(summary_map_t::iterator i_s = m_summary_ssl_protocols.begin(); i_s != m_summary_ssl_protocols.end(); ++i_s)
        l_sorted_map[i_s->second] = i_s->first;
        for(_sorted_map_t::reverse_iterator i_s = l_sorted_map.rbegin(); i_s != l_sorted_map.rend(); ++i_s)
        NDBG_OUTPUT("| %-32s %u\n", i_s->second.c_str(), i_s->first);
        NDBG_OUTPUT("+--------------- %sSSL CIPHERS%s ---------------- \n", l_cipher_str.c_str(), l_off_color.c_str());
        l_sorted_map.clear();
        for(summary_map_t::iterator i_s = m_summary_ssl_ciphers.begin(); i_s != m_summary_ssl_ciphers.end(); ++i_s)
        l_sorted_map[i_s->second] = i_s->first;
        for(_sorted_map_t::reverse_iterator i_s = l_sorted_map.rbegin(); i_s != l_sorted_map.rend(); ++i_s)
        NDBG_OUTPUT("| %-32s %u\n", i_s->second.c_str(), i_s->first);

}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
reqlet *hlx_client::get_reqlet(void)
{
        reqlet *l_reqlet = NULL;

        //NDBG_PRINT("%sREQLST%s[%lu]: .\n", ANSI_COLOR_FG_CYAN, ANSI_COLOR_OFF, m_reqlet_list.size());

        pthread_mutex_lock(&m_mutex);
        if(!m_reqlet_vector.empty() &&
           (m_num_get < m_num_reqlets))
        {
                l_reqlet = m_reqlet_vector[m_reqlet_vector_idx];
                ++m_num_get;
                ++m_reqlet_vector_idx;
        }
        pthread_mutex_unlock(&m_mutex);

        //NDBG_PRINT("%sREQLET%s[%p]: Gettin to repo.\n", ANSI_COLOR_BG_GREEN, ANSI_COLOR_OFF, l_reqlet);
        //NDBG_PRINT("%sREQLET%s[%p]: Gettin to repo.\n", ANSI_COLOR_BG_RED, ANSI_COLOR_OFF, *m_reqlet_list_iter);

        return l_reqlet;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
reqlet *hlx_client::try_get_resolved(void)
{
        reqlet *l_reqlet = NULL;
        int32_t l_status;

        l_reqlet = get_reqlet();
        if(NULL == l_reqlet)
        {
                return NULL;
        }

        // Try resolve
        l_status = l_reqlet->resolve();
        if(l_status != STATUS_OK)
        {
                // TODO Set response and error
                up_resolved(true);
                append_summary(l_reqlet);
                return NULL;
        }

        up_resolved(false);
        return l_reqlet;

}


//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t hlx_client::add_reqlet(reqlet *a_reqlet)
{

        //NDBG_PRINT("%sREQLST%s[%lu]: .\n", ANSI_COLOR_FG_CYAN, ANSI_COLOR_OFF, m_reqlet_list.size());

        bool l_was_empty = false;
        if(m_reqlet_vector.empty())
        {
                l_was_empty = true;
        }

        //NDBG_PRINT("%sREQLET%s[%p]: Adding to repo.\n", ANSI_COLOR_FG_GREEN, ANSI_COLOR_OFF, a_reqlet);
        m_reqlet_vector.push_back(a_reqlet);
        ++m_num_reqlets;

        if(l_was_empty)
        {
                m_reqlet_vector_idx = 0;
                //NDBG_PRINT("%sREQLET%s[%p]: Adding to repo.\n", ANSI_COLOR_BG_RED, ANSI_COLOR_OFF, *m_reqlet_list_iter);
        }

        //NDBG_PRINT("%sREQLET%s[%p]: Adding to repo.\n", ANSI_COLOR_FG_RED, ANSI_COLOR_OFF, *m_reqlet_list_iter);

        return STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void hlx_client::display_status_line(bool a_color)
{
        if(a_color)
        {
                printf("Done/Resolved/Req'd/Total/Error %s%8u%s / %s%8u%s / %s%8u%s / %s%8u%s / %s%8u%s\n",
                                ANSI_COLOR_FG_GREEN, m_num_done, ANSI_COLOR_OFF,
                                ANSI_COLOR_FG_MAGENTA, m_num_resolved, ANSI_COLOR_OFF,
                                ANSI_COLOR_FG_YELLOW, m_num_get, ANSI_COLOR_OFF,
                                ANSI_COLOR_FG_BLUE, m_num_reqlets, ANSI_COLOR_OFF,
                                ANSI_COLOR_FG_RED, m_num_error, ANSI_COLOR_OFF);
        }
        else
        {
                printf("Done/Resolved/Req'd/Total/Error %8u / %8u / %8u / %8u / %8u\n",m_num_done, m_num_resolved, m_num_get, m_num_reqlets, m_num_error);
        }
}

} //namespace ns_hlx {


