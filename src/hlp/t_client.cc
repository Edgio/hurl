//: ----------------------------------------------------------------------------
//: Copyright (C) 2014 Verizon.  All Rights Reserved.
//: All Rights Reserved
//:
//: \file:    t_client.cc
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

//: ----------------------------------------------------------------------------
//: Includes
//: ----------------------------------------------------------------------------
#include "t_client.h"
#include "city.h"
#include "hlp.h"
#include "util.h"
#include "nconn_ssl.h"
#include "nconn_tcp.h"
#include "resolver.h"

#include "ndebug.h"

#include <unistd.h>

// inet_aton
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <errno.h>
#include <string.h>
#include <string>

#include <stdint.h>
#ifndef __STDC_FORMAT_MACROS
#define __STDC_FORMAT_MACROS
#endif
#include <inttypes.h>

#include "tinymt64.h"

//: ----------------------------------------------------------------------------
//: Macros
//: ----------------------------------------------------------------------------
#define HLP_SET_NCONN_OPT(_conn, _opt, _buf, _len) \
        do { \
                int _status = 0; \
                _status = _conn.set_opt((_opt), (_buf), (_len)); \
                if (_status != STATUS_OK) { \
                        NDBG_PRINT("STATUS_ERROR: Failed to set_opt %d.  Status: %d.\n", _opt, _status); \
                        return NULL;\
                } \
        } while(0)

#define HLP_GET_NCONN_OPT(_conn, _opt, _buf, _len) \
        do { \
                int _status = 0; \
                _status = _conn.get_opt((_opt), (_buf), (_len)); \
                if (_status != STATUS_OK) { \
                        NDBG_PRINT("STATUS_ERROR: Failed to get_opt %d.  Status: %d.\n", _opt, _status); \
                        return NULL;\
                } \
        } while(0)

//: ----------------------------------------------------------------------------
//: Fwd Decl's
//: ----------------------------------------------------------------------------
uint64_t get_cmd_hash(const pb_cmd_t &a_cmd);

//: ----------------------------------------------------------------------------
//: Thread local global
//: ----------------------------------------------------------------------------
__thread t_client *g_t_client = NULL;

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
t_client::t_client(
        bool a_verbose,
        bool a_color,
        uint32_t a_sock_opt_recv_buf_size,
        uint32_t a_sock_opt_send_buf_size,
        bool a_sock_opt_no_delay,
        const std::string & a_cipher_str,
        SSL_CTX *a_ssl_ctx,
        evr_type_t a_evr_type,
        uint32_t a_max_parallel_connections,
        int32_t a_timeout_s):
        m_t_run_thread(),
        m_t_run_cmd_thread(),
        m_timeout_s(a_timeout_s),
        m_num_cmds(0),
        m_num_cmds_serviced(0),
        m_num_cmds_dropped(0),
        m_num_reqs_sent(0),
        m_num_errors(0),
        m_verbose(a_verbose),
        m_color(a_color),
        m_sock_opt_recv_buf_size(a_sock_opt_recv_buf_size),
        m_sock_opt_send_buf_size(a_sock_opt_send_buf_size),
        m_sock_opt_no_delay(a_sock_opt_no_delay),
        m_cipher_str(a_cipher_str),
        m_ssl_ctx(a_ssl_ctx),
        m_evr_type(a_evr_type),
        m_stopped(false),
        m_max_parallel_connections(a_max_parallel_connections),
        m_start_time_s(0),
        m_reqlet_map(),
        m_num_fetches(-1),
        m_num_fetched(0),
        m_last_get_req_us(0),
        m_header_map(),
        m_evr_loop(NULL),
        m_evr_cmd_loop(NULL),
        m_first_timestamp_ms(0),
        m_loop_mutex(),
        m_feeder_done(false),
        m_scale(1.0),
        m_run_cmd_stopped(false),
        m_stat_cmd_fins(0),
        m_stat_cmd_gets(0),
        m_stat_rconn_created(0),
        m_stat_rconn_reuse(0),
        m_stat_rconn_deleted(0),
        m_stat_bytes_read(0),
        m_stat_err_connect(0),
        m_stat_err_state(0),
        m_stat_err_do_connect(0),
        m_stat_err_send_request(0),
        m_stat_err_read_cb(0),
        m_stat_err_error_cb(0),
        m_nconn_lock_map(),
        m_nconn_lock_free_list(),
        m_nconn_lock_map_mutex()
{

        pthread_mutex_init(&m_loop_mutex, NULL);
        pthread_mutex_init(&m_nconn_lock_map_mutex, NULL);

        evr_loop_type_t l_evr_loop_type = EVR_LOOP_EPOLL;
        // -------------------------------------------
        // Get the event handler...
        // -------------------------------------------
        if (m_evr_type == EV_SELECT)
        {
                l_evr_loop_type = EVR_LOOP_SELECT;
                //NDBG_PRINT("Using evr_select\n");
        }
        // Default to epoll
        else
        {
                l_evr_loop_type = EVR_LOOP_EPOLL;

                //NDBG_PRINT("Using evr_epoll\n");
        }

        // Create loop
        m_evr_loop = new evr_loop(evr_loop_file_readable_cb,
                        evr_loop_file_writeable_cb,
                        evr_loop_file_error_cb,
                        l_evr_loop_type,
                        m_max_parallel_connections, false);

        // Create loop
        m_evr_cmd_loop = new evr_loop(NULL,
                        NULL,
                        NULL,
                        l_evr_loop_type,
                        m_max_parallel_connections, true);

}


//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
t_client::~t_client()
{

}


//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int t_client::run(void)
{
        int32_t l_pthread_error = 0;

        l_pthread_error = pthread_create(&m_t_run_cmd_thread,
                        NULL,
                        t_run_cmd_static,
                        this);
        if (l_pthread_error != 0)
        {
                // failed to create thread

                NDBG_PRINT("Error: creating cmd thread.  Reason: %s\n.", strerror(l_pthread_error));
                return STATUS_ERROR;

        }

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
void *t_client::evr_loop_file_writeable_cb(void *a_data)
{

        //NDBG_PRINT("%sWRITEABLE%s %p\n", ANSI_COLOR_FG_BLUE, ANSI_COLOR_OFF, a_data);

        if(!a_data)
        {
                //NDBG_PRINT("%sWRITEABLE%s ERROR\n", ANSI_COLOR_FG_BLUE, ANSI_COLOR_OFF);
                return NULL;
        }

        t_client *l_t_client = g_t_client;
        nconn *l_nconn = (nconn *)a_data;
        if(!l_nconn)
        {
                //NDBG_PRINT("%sWRITEABLE%s ERROR\n", ANSI_COLOR_FG_BLUE, ANSI_COLOR_OFF);
                return NULL;
        }

        l_nconn = l_t_client->get_locked_conn(l_nconn->get_id());
        if(!l_nconn)
        {
                //NDBG_PRINT("%sWRITEABLE%s ERROR fd[%" PRIu64 "]\n", ANSI_COLOR_FG_BLUE, ANSI_COLOR_OFF, l_nconn->get_id());
                return NULL;
        }

        //NDBG_PRINT("%sWRITEABLE%s %p\n", ANSI_COLOR_FG_BLUE, ANSI_COLOR_OFF, l_nconn);

        reqlet *l_reqlet = static_cast<reqlet *>(l_nconn->get_data1());

        int32_t l_status = STATUS_OK;
        l_status = l_nconn->run_state_machine(l_t_client->m_evr_loop, l_reqlet->m_host_info);
        if(STATUS_ERROR == l_status)
        {
                NDBG_PRINT("Error: performing run_state_machine\n");
                // TODO FIX!!!
                //T_CLIENT_CONN_CLEANUP(l_t_client, l_nconn, l_reqlet, 500, "Error performing connect_cb");
                l_t_client->cleanup_connection(l_nconn);
                return NULL;
        }

        // Add idle timeout
        l_t_client->m_evr_loop->add_timer( l_t_client->get_timeout_s()*1000, evr_loop_file_timeout_cb, l_nconn, &(l_nconn->m_timer_obj));
        l_t_client->give_lock(l_nconn->get_id());
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
                //NDBG_PRINT("%sREADABLE%s ERROR\n", ANSI_COLOR_FG_RED, ANSI_COLOR_OFF);
                return NULL;
        }

        nconn* l_nconn = static_cast<nconn*>(a_data);
        if(!l_nconn)
        {
                //NDBG_PRINT("%sREADABLE%s ERROR\n", ANSI_COLOR_FG_RED, ANSI_COLOR_OFF);
                return NULL;
        }

        reqlet *l_reqlet = static_cast<reqlet *>(l_nconn->get_data1());
        t_client *l_t_client = g_t_client;

        //NDBG_PRINT("%sREADABLE%s %p\n", ANSI_COLOR_FG_RED, ANSI_COLOR_OFF, l_nconn);

        // Cancel last timer
        l_t_client->m_evr_loop->cancel_timer(&(l_nconn->m_timer_obj));

        int32_t l_status = STATUS_OK;
        l_status = l_nconn->run_state_machine(l_t_client->m_evr_loop, l_reqlet->m_host_info);
        if(STATUS_ERROR == l_status)
        {
                NDBG_PRINT("Error: performing run_state_machine\n");
                // TODO FIX!!!
                //T_CLIENT_CONN_CLEANUP(l_t_client, l_nconn, l_reqlet, 500, "Error performing connect_cb");
                l_t_client->cleanup_connection(l_nconn);
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
                // Add stats
                add_stat_to_agg(l_reqlet->m_stat_agg, l_nconn->get_stats());
                l_nconn->reset_stats();

                l_reqlet->m_stat_agg.m_num_conn_completed++;
                l_t_client->m_num_fetched++;

                // Bump stats
                if(l_status == STATUS_ERROR)
                {
                        ++(l_reqlet->m_stat_agg.m_num_errors);
                }

                // Give back reqlet
                bool l_can_reuse = false;
                l_can_reuse = (l_nconn->can_reuse());

                //NDBG_PRINT("CONN[%d] %sREUSE%s: %d\n",
                //              l_nconn->m_fd,
                //              ANSI_COLOR_BG_RED, ANSI_COLOR_OFF,
                //              l_can_reuse
                //              );

                if(!l_can_reuse) {
                        //NDBG_PRINT("DONE: l_reqlet: %sHOST%s: %d / %d\n",
                        //              ANSI_COLOR_BG_RED, ANSI_COLOR_OFF,
                        //              l_can_reuse,
                        //              l_nconn->can_reuse());

                        l_t_client->cleanup_connection(l_nconn, false);
                        return NULL;
                }
        }

        // Add idle timeout
        l_t_client->m_evr_loop->add_timer( l_t_client->get_timeout_s()*1000, evr_loop_file_timeout_cb, l_nconn, &(l_nconn->m_timer_obj));
        l_t_client->give_lock(l_nconn->get_id());
        return NULL;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void *t_client::evr_loop_file_error_cb(void *a_data)
{
        if(!a_data)
        {
                return NULL;
        }

        t_client *l_t_client = g_t_client;
        nconn *l_nconn = (nconn *)a_data;
        if(!l_nconn)
        {
                return NULL;
        }

        l_nconn = l_t_client->get_locked_conn(l_nconn->get_id());
        if(!l_nconn)
        {
                return NULL;
        }

        //NDBG_PRINT("%sSTATUS_ERRORS%s\n", ANSI_COLOR_BG_RED, ANSI_COLOR_OFF);
        l_t_client->cleanup_connection(l_nconn, true);
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
                return NULL;
        }

        t_client *l_t_client = g_t_client;
        nconn *l_nconn = (nconn *)a_data;
        if(!l_nconn)
        {
                return NULL;
        }

        l_nconn = l_t_client->get_locked_conn(l_nconn->get_id());
        if(!l_nconn)
        {
                return NULL;
        }

        if(l_t_client->m_verbose)
        {
                NDBG_PRINT("%sTIMING OUT CONN%s: i_conn: %" PRIu64 "\n",
                        ANSI_COLOR_BG_RED, ANSI_COLOR_OFF,
                        l_nconn->get_id());
        }

        // TODO
        // Add stats
        //l_conn->m_stat.m_status_code = 599;
        //add_stat_to_agg(l_reqlet->m_stat_agg, l_conn->get_stats());

        l_t_client->cleanup_connection(l_nconn, true);
        return NULL;

}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
nconn *t_client::try_take_locked_nconn_w_hash(uint64_t a_hash)
{

        //NDBG_PRINT("ID[%" PRIu64 "]\n", a_hash);

        nconn_lock_t l_nconn_lock;

        // ---------------------------------
        // Look up
        // ---------------------------------
        pthread_mutex_lock(&m_nconn_lock_map_mutex);
        nconn_lock_map_t::iterator i_nconn = m_nconn_lock_map.find(a_hash);
        if(i_nconn != m_nconn_lock_map.end())
        {
                // Found it!!! try take lock
                if(pthread_mutex_trylock(&(i_nconn->second.m_mutex)) == 0)
                {
                        pthread_mutex_unlock(&m_nconn_lock_map_mutex);
                        return i_nconn->second.m_nconn;
                }

                // else was busy
                pthread_mutex_unlock(&m_nconn_lock_map_mutex);
                return NULL;

        }
        pthread_mutex_unlock(&m_nconn_lock_map_mutex);

        // ---------------------------------
        // Create new
        // ---------------------------------
        // Try take from unused list
        if(!m_nconn_lock_free_list.empty())
        {
                l_nconn_lock = m_nconn_lock_free_list.front();
                m_nconn_lock_free_list.pop_front();
        }
        else
        {
                nconn *l_nconn = NULL;

                // Assume tcp -fix later
                l_nconn = new nconn_tcp(m_verbose, m_color, -1, false, false);

                // Set generic options
                HLP_SET_NCONN_OPT((*l_nconn), nconn_tcp::OPT_TCP_RECV_BUF_SIZE, NULL, m_sock_opt_recv_buf_size);
                HLP_SET_NCONN_OPT((*l_nconn), nconn_tcp::OPT_TCP_SEND_BUF_SIZE, NULL, m_sock_opt_send_buf_size);
                HLP_SET_NCONN_OPT((*l_nconn), nconn_tcp::OPT_TCP_NO_DELAY, NULL, m_sock_opt_no_delay);

                l_nconn_lock.m_nconn = l_nconn;
                pthread_mutex_init(&l_nconn_lock.m_mutex, NULL);
        }

        // Set id
        l_nconn_lock.m_nconn->set_id(a_hash);

        pthread_mutex_lock(&m_nconn_lock_map_mutex);
        m_nconn_lock_map[a_hash] = l_nconn_lock;
        pthread_mutex_unlock(&m_nconn_lock_map_mutex);

        //NDBG_PRINT("CREATE_ID[%" PRIu64 "]\n", a_hash);

        // Take lock
        pthread_mutex_lock(&l_nconn_lock.m_mutex);
        return l_nconn_lock.m_nconn;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void t_client::give_lock(uint64_t a_id)
{

}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void *t_client::evr_loop_timer_cb(void *a_data)
{

        //NDBG_PRINT("%sHERE%s: \n", ANSI_COLOR_FG_GREEN, ANSI_COLOR_OFF);

        // Get refs
        client_pb_cmd_t *l_client_pb_cmd = static_cast<client_pb_cmd_t *>(a_data);
        pb_cmd_t *l_pb_cmd = l_client_pb_cmd->m_pb_cmd;
        t_client *l_t_client = l_client_pb_cmd->m_t_client;

        // Stats
        ++(l_t_client->m_num_cmds_serviced);

        // Get command hash
        uint64_t l_cmd_hash = 0;
        l_cmd_hash = get_cmd_hash(*l_pb_cmd);

        // Try take
        nconn *l_nconn = NULL;
        l_nconn = l_t_client->try_take_locked_nconn_w_hash(l_cmd_hash);
        if(NULL == l_nconn)
        {
                // Failed to get because was busy -log and drop.
                ++(l_t_client->m_num_cmds_dropped);

                // Clean up
                delete l_client_pb_cmd;
                delete l_pb_cmd;
                return NULL;
        }


        // If connection active -reuse for request
        if(l_pb_cmd->m_verb == "FIN")
        {
                //++(l_t_client->m_stat_cmd_fins);

                l_t_client->cleanup_connection(l_nconn);
                delete l_client_pb_cmd;
                delete l_pb_cmd;
                return NULL;
        }
        else
        {

                // Stats
                //++(l_t_client->m_stat_cmd_gets);

                // ---------------------------------------
                // Get reqlet
                // ---------------------------------------
                reqlet *l_reqlet = NULL;
                l_reqlet = l_t_client->evoke_reqlet(*l_pb_cmd);
                if(!l_reqlet)
                {
                        NDBG_PRINT("Error: performing evoke_reqlet.  Something's busted.\n");
                        l_t_client->give_lock(l_nconn->get_id());
                        delete l_client_pb_cmd;
                        delete l_pb_cmd;
                        return NULL;
                }

                // ---------------------------------------
                // Create request
                // ---------------------------------------
                l_nconn->set_data1(l_reqlet);
                l_t_client->create_request(*l_nconn, *l_reqlet, *l_pb_cmd);
                // -note doesn't really ever return error

                // ---------------------------------------
                // Connection DONE
                // available for reuse ?
                // ---------------------------------------

                //NDBG_PRINT("%sHERE%s: l_state %d\n", ANSI_COLOR_FG_GREEN, ANSI_COLOR_OFF, l_state);

                if(l_nconn->is_done())
                {
                        // Can reuse???
                        if(l_nconn->can_reuse())
                        {
                                //NDBG_PRINT("%sREUSE%s: l_rconn_pb[%" PRIu64 "]\n", ANSI_COLOR_BG_MAGENTA, ANSI_COLOR_OFF, l_rconn_pb->m_hash);

                                int32_t l_send_status;
                                l_send_status = l_nconn->send_request(true);
                                if(l_send_status != STATUS_OK)
                                {
                                        //++(l_t_client->m_stat_err_send_request);
                                }
                                //++(l_t_client->m_num_reqs_sent);
                                l_t_client->m_evr_loop->add_timer(l_t_client->get_timeout_s()*1000, evr_loop_file_timeout_cb, l_nconn, &(l_nconn->m_timer_obj));

                        }
                        // You complete me...
                        else
                        {
                                //NDBG_PRINT("NOT FREE but can't reuse???\n");

                                ++(l_reqlet->m_stat_agg.m_num_errors);
                                ++(l_t_client->m_stat_err_state);

                                l_t_client->cleanup_connection(l_nconn);
                                delete l_client_pb_cmd;
                                delete l_pb_cmd;
                                return NULL;
                        }

                }
                // ---------------------------------------
                // Connection Free
                // ---------------------------------------
                else if(l_nconn->is_free())
                {

                        int l_status;

                        // Check if scheme is same...
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
                                l_nconn = l_t_client->create_new_nconn(*l_reqlet);
                                if(!l_nconn)
                                {
                                        NDBG_PRINT("Error performing create_new_nconn\n");
                                        return NULL;
                                }
                        }

                        l_t_client->reassign_nconn(l_nconn, l_cmd_hash);

                        // Bump stats
                        ++(l_reqlet->m_stat_agg.m_num_conn_started);

                        l_t_client->m_evr_loop->add_timer(l_t_client->get_timeout_s()*1000, evr_loop_file_timeout_cb, l_nconn, &(l_nconn->m_timer_obj));

                        //NDBG_PRINT("%sCONNECT%s: %s\n", ANSI_COLOR_BG_MAGENTA, ANSI_COLOR_OFF, l_reqlet->m_url.m_host.c_str());
                        l_status = l_nconn->run_state_machine(l_t_client->m_evr_loop, l_reqlet->m_host_info);
                        if(l_status != STATUS_OK)
                        {
                                NDBG_PRINT("Error: Performing do_connect: status: %d\n", l_status);
                                ++(l_reqlet->m_stat_agg.m_num_errors);
                                l_t_client->cleanup_connection(l_nconn);
                                delete l_client_pb_cmd;
                                delete l_pb_cmd;
                                return NULL;
                        }

                }
                // ---------------------------------------
                // Connection NOT available...
                // TODO Dropping for now
                // ---------------------------------------
                else
                {

                        //NDBG_PRINT("Error: Connection in weird state[%d]\n", l_rconn_pb->m_conn->m_state);
                        // TODO assert!?
                        ++(l_t_client->m_stat_err_state);
                        ++(l_reqlet->m_stat_agg.m_num_errors);

                }

        }

        // -------------------------------------------
        // Cleanup
        // -------------------------------------------
        // Give back lock
        l_t_client->give_lock(l_nconn->get_id());
        delete l_client_pb_cmd;
        delete l_pb_cmd;

        return NULL;

}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
nconn *t_client::create_new_nconn(const reqlet &a_reqlet)
{
        nconn *l_nconn = NULL;


        if(a_reqlet.m_url.m_scheme == nconn::SCHEME_TCP)
        {
                // TODO SET OPTIONS!!!
                l_nconn = new nconn_tcp(m_verbose, m_color, 1, true, false);
        }
        else if(a_reqlet.m_url.m_scheme == nconn::SCHEME_SSL)
        {
                // TODO SET OPTIONS!!!
                l_nconn = new nconn_ssl(m_verbose, m_color, 1, true, false);
        }

        // -------------------------------------------
        // Set options
        // -------------------------------------------
        // Set generic options
        HLP_SET_NCONN_OPT((*l_nconn), nconn_tcp::OPT_TCP_RECV_BUF_SIZE, NULL, m_sock_opt_recv_buf_size);
        HLP_SET_NCONN_OPT((*l_nconn), nconn_tcp::OPT_TCP_SEND_BUF_SIZE, NULL, m_sock_opt_send_buf_size);
        HLP_SET_NCONN_OPT((*l_nconn), nconn_tcp::OPT_TCP_NO_DELAY, NULL, m_sock_opt_no_delay);

        // Set ssl options
        if(a_reqlet.m_url.m_scheme == nconn::SCHEME_SSL)
        {
                HLP_SET_NCONN_OPT((*l_nconn), nconn_ssl::OPT_SSL_CIPHER_STR, m_cipher_str.c_str(), m_cipher_str.length());
                HLP_SET_NCONN_OPT((*l_nconn), nconn_ssl::OPT_SSL_CTX, m_ssl_ctx, sizeof(m_ssl_ctx));
        }

        return l_nconn;

}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
#define REQLET_MAX_URL_LEN 1024
reqlet *t_client::evoke_reqlet(const pb_cmd_t &a_cmd)
{
        char l_url[REQLET_MAX_URL_LEN];

        // Create Request URL
        snprintf(l_url, REQLET_MAX_URL_LEN, "%s:%d%s", a_cmd.m_dest_ip.c_str(), a_cmd.m_dest_port, a_cmd.m_uri.c_str());

        //NDBG_PRINT("EVSTATUS_OKE: %s\n", l_url);

        // Find reqlet by url...
        uint32_t l_url_len = strnlen(l_url, REQLET_MAX_URL_LEN);
        uint64_t l_hash;
        l_hash = CityHash64(l_url, l_url_len);

        reqlet_map_t::iterator i_reqlet;
        if((i_reqlet = m_reqlet_map.find(l_hash)) != m_reqlet_map.end())
        {
                return i_reqlet->second;
        }

        // Create reqlet
        reqlet *l_reqlet = new reqlet(m_reqlet_map.size(), -1);
        std::string l_url_str = l_url;
        l_reqlet->init_with_url(l_url_str);

        resolver l_resolver;
        l_resolver.init();

        l_reqlet->resolve(l_resolver);

        // Add to map
        m_reqlet_map[l_hash] = l_reqlet;

        return l_reqlet;
}


//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
uint64_t get_cmd_hash(const pb_cmd_t &a_cmd)
{
        // -------------------------------------------
        // src ip/port dest ip/prt
        // Create hash
        // -------------------------------------------
        uint64_t l_hash = 0;
        l_hash += CityHash64(a_cmd.m_src_ip.data(), a_cmd.m_src_ip.length());
        l_hash += CityHash64((const char *)(&(a_cmd.m_src_port)), sizeof(a_cmd.m_src_port));
        l_hash += CityHash64(a_cmd.m_dest_ip.data(), a_cmd.m_dest_ip.length());
        l_hash += CityHash64((const char *)(&(a_cmd.m_dest_port)), sizeof(a_cmd.m_dest_port));

        return l_hash;
}


//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void *t_client::t_run(void *a_nothing)
{

        // Set start time
        m_start_time_s = get_time_s();

        // Set thread local
        g_t_client = this;

        // -------------------------------------------
        // Main loop.
        // -------------------------------------------
        //NDBG_PRINT("starting main loop: m_run_time_s: %d start_time_s: %" PRIu64 "\n", m_run_time_s, get_time_s() - m_start_time_s);
        while(!m_stopped)
        {
                m_evr_loop->run();
        }

        m_stopped = true;

        return NULL;

}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void *t_client::t_run_cmd(void *a_nothing)
{
        // Set start time
        m_start_time_s = get_time_s();

        while(!m_run_cmd_stopped)
        {
                if(m_feeder_done && (m_num_cmds_serviced >= m_num_cmds))
                {
                        break;
                }
                m_evr_cmd_loop->run();
        }

        if(m_feeder_done && (m_num_cmds_serviced >= m_num_cmds))
        {
                sleep(1);
                m_stopped = true;
        }

        m_run_cmd_stopped = true;

        return NULL;

}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t t_client::create_request(nconn &ao_conn,
                reqlet &a_reqlet,
                const pb_cmd_t &a_cmd)
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
        //NDBG_PRINT("PATH: %s\n", l_path_ref.c_str());
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
        for(header_map_t::const_iterator i_header = a_cmd.m_header_map.begin();
                        i_header != a_cmd.m_header_map.end();
                        ++i_header)
        {
                //NDBG_PRINT("Adding HEADER: %s: %s\n", i_header->first.c_str(), i_header->second.c_str());
                l_req_buf_len += snprintf(l_req_buf + l_req_buf_len, l_max_buf_len - l_req_buf_len,
                                  "%s: %s\r\n", i_header->first.c_str(), i_header->second.c_str());

                 if (strcasecmp(i_header->first.c_str(), "host") == 0)
                 {
                         l_specd_host = true;
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
        // Cancel last timer
        if(a_cancel_timer)
        {
                m_evr_loop->cancel_timer(&(a_nconn->m_timer_obj));
        }

        // stats
        //++m_stat_rconn_deleted;
        //NDBG_PRINT("FD[%d] Cleanup\n", l_fd);
        a_nconn->cleanup(m_evr_cmd_loop);
        a_nconn->set_data1(NULL);
        remove_nconn(a_nconn);
        return STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t t_client::reassign_nconn(nconn *a_nconn, uint64_t a_new_id)
{
        uint64_t l_old_id = a_nconn->get_id();
        //NDBG_PRINT("OLD_ID[%" PRIu64 "] --> NEW_ID[%" PRIu64 "]\n", l_old_id, a_new_id);
        if(l_old_id == a_new_id)
        {
                // Do nothing...
                return STATUS_OK;
        }

        nconn_lock_map_t::iterator i_nconn_lock = m_nconn_lock_map.find(l_old_id);
        if(i_nconn_lock != m_nconn_lock_map.end())
        {
                nconn_lock_t l_nconn_lock = i_nconn_lock->second;
                pthread_mutex_unlock(&l_nconn_lock.m_mutex);
                pthread_mutex_lock(&m_nconn_lock_map_mutex);
                m_nconn_lock_map.erase(l_old_id);
                m_nconn_lock_map[a_new_id] = l_nconn_lock;
                pthread_mutex_unlock(&m_nconn_lock_map_mutex);
        }
        else
        {
                NDBG_PRINT("Error could not find old nconn id: %" PRIu64 "\n", l_old_id);
                return STATUS_ERROR;
        }

        return STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t t_client::remove_nconn(nconn *a_nconn)
{

        //NDBG_PRINT("REMOVING\n");
        uint64_t l_id = a_nconn->get_id();
        nconn_lock_map_t::iterator i_nconn_lock = m_nconn_lock_map.find(l_id);
        if(i_nconn_lock != m_nconn_lock_map.end())
        {
                nconn_lock_t l_nconn_lock = i_nconn_lock->second;
                pthread_mutex_unlock(&l_nconn_lock.m_mutex);
                pthread_mutex_lock(&m_nconn_lock_map_mutex);
                m_nconn_lock_map.erase(l_id);
                m_nconn_lock_free_list.push_back(l_nconn_lock);
                pthread_mutex_unlock(&m_nconn_lock_map_mutex);
        }
        else
        {
                NDBG_PRINT("Error could not find nconn id: %" PRIu64 "\n", l_id);
                return STATUS_ERROR;
        }
        return STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
nconn *t_client::get_locked_conn(uint64_t a_id)
{
        // TODO REMOVE
        //NDBG_PRINT("%sGET_LOCK%s: a_fd: %" PRIu64 "\n", ANSI_COLOR_BG_BLUE, ANSI_COLOR_OFF, a_id);

        nconn_lock_map_t::iterator i_nconn_lock = m_nconn_lock_map.find(a_id);
        if(i_nconn_lock != m_nconn_lock_map.end())
        {
                nconn_lock_t l_nconn_lock = i_nconn_lock->second;
                pthread_mutex_lock(&l_nconn_lock.m_mutex);
                return l_nconn_lock.m_nconn;
        }
        else
        {
                NDBG_PRINT("Error could not find nconn id: %" PRIu64 "\n", a_id);
                return NULL;
        }

        return NULL;
}


//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void t_client::get_stats_copy(tag_stat_map_t &ao_tag_stat_map)
{
        // TODO Need to make this threadsafe -spinlock perhaps...
        for(reqlet_map_t::iterator i_reqlet = m_reqlet_map.begin(); i_reqlet != m_reqlet_map.end(); ++i_reqlet)
        {
                ao_tag_stat_map[i_reqlet->second->get_label()] = i_reqlet->second->m_stat_agg;
        }
}


//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void t_client::show_stats(void)
{

        NDBG_OUTPUT("* %sSTATS%s: fins: %lu gets: %lu conn_created: %lu deleted: %lu reused: %lu\n",
                        ANSI_COLOR_FG_GREEN, ANSI_COLOR_OFF,
                        m_stat_cmd_fins,
                        m_stat_cmd_gets,
                        m_stat_rconn_created,
                        m_stat_rconn_deleted,
                        m_stat_rconn_reuse
                        );
        NDBG_OUTPUT("* %sSTATS%s: bytes_read: %lu tq = %lu eq = %lu free_size = %lu map_size = %lu\n",
                        ANSI_COLOR_FG_GREEN, ANSI_COLOR_OFF,
                        m_stat_bytes_read,
                        m_evr_cmd_loop->get_pq_size(),
                        m_evr_loop->get_pq_size(),
                        m_nconn_lock_free_list.size(),
                        m_nconn_lock_map.size()
                        );
        NDBG_OUTPUT("* %sSTATUS_ERROR%s: stat: %lu connect: %lu do_connect: %lu send_request: %lu read_cb: %lu error_cb: %lu\n",
                        ANSI_COLOR_FG_RED, ANSI_COLOR_OFF,
                        m_stat_err_state,
                        m_stat_err_connect,
                        m_stat_err_do_connect,
                        m_stat_err_send_request,
                        m_stat_err_read_cb,
                        m_stat_err_error_cb
                        );
}


//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void t_client::add_pb_cmd(const pb_cmd_t &a_cmd)
{
        client_pb_cmd_t *l_client_pb_cmd = new client_pb_cmd_t();
        l_client_pb_cmd->m_pb_cmd = new pb_cmd_t(a_cmd);
        l_client_pb_cmd->m_t_client = this;
        uint64_t l_ms = 0;
        void *l_unused;
        l_ms = (uint64_t)((double)(a_cmd.m_timestamp_ms - m_first_timestamp_ms))/m_scale;

        // Slow down feeder if too far ahead -memory optimization
        while((get_time_s() - m_start_time_s + 60) < (l_ms/1000))
        {
                sleep(1);
        }

        //NDBG_PRINT("ADDING CMD delta = %lu --scale = %f.\n", l_ms, m_scale);

        ++m_num_cmds;
        m_evr_cmd_loop->add_timer(l_ms, evr_loop_timer_cb, l_client_pb_cmd, &l_unused);
}
