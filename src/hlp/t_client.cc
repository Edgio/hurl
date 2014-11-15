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
        uint32_t a_max_parallel_connections):
        m_t_run_thread(),
        m_t_run_cmd_thread(),
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
        m_nconn_vector(NCONN_MAX),
        m_nconn_map(),
        m_nconn_free_list(),
        m_nconn_map_mutex()
{

        pthread_mutex_init(&m_loop_mutex, NULL);
        pthread_mutex_init(&m_nconn_map_mutex, NULL);

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

        if(!a_data)
        {
                return NULL;
        }

        t_client *l_t_client = g_t_client;
        int32_t l_fd = (intptr_t)a_data;
        nconn *l_nconn = l_t_client->get_locked_conn(l_fd);
        if(!l_nconn)
        {
                return NULL;
        }

        // Get state
        conn_state_t l_state = l_nconn->get_state();

        //NDBG_PRINT("%sWRITEABLE%s\n", ANSI_COLOR_BG_BLUE, ANSI_COLOR_OFF);

        switch (l_state)
        {
        case CONN_STATE_CONNECTING:
        {
                int l_status;
                //NDBG_PRINT("%sCNST_CONNECTING%s\n", ANSI_COLOR_BG_RED, ANSI_COLOR_OFF);
                reqlet *l_reqlet = (reqlet *)l_nconn->get_data1();
                if(!l_reqlet)
                {
                        NDBG_PRINT("Error: getting reqlet\n");
                        l_t_client->cleanup_connection(l_nconn);
                        l_nconn->give_lock();
                        return NULL;
                }

                l_status = l_nconn->connect_cb(l_reqlet->m_host_info);
                if(STATUS_OK != l_status)
                {
                        NDBG_PRINT("Error: performing connect_cb\n");
                        l_t_client->cleanup_connection(l_nconn);
                        l_nconn->give_lock();
                        return NULL;
                }
                ++(l_t_client->m_num_reqs_sent);

                // -------------------------------------------
                // Add to event handler
                // -------------------------------------------
                void *l_fd_ptr = (void *) (intptr_t)l_fd;
                if (0 != l_t_client->m_evr_loop->add_file(l_fd, EVR_FILE_ATTR_MASK_READ, l_fd_ptr))
                {
                        NDBG_PRINT("Error: Couldn't add socket file descriptor\n");
                        l_t_client->cleanup_connection(l_nconn);
                        l_nconn->give_lock();
                        return NULL;
                }

                // TODO Add timer for callback
                l_t_client->m_evr_loop->add_timer(HLP_DEFAULT_CONN_TIMEOUT_MS, evr_loop_file_timeout_cb, l_fd_ptr, &(l_nconn->m_timer_obj));
                break;
        }
        default:
                break;
        }

        l_nconn->give_lock();
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
                return NULL;
        }

        t_client *l_t_client = g_t_client;
        int32_t l_fd = (intptr_t)a_data;
        nconn *l_nconn = l_t_client->get_locked_conn(l_fd);
        if(!l_nconn)
        {
                return NULL;
        }

        conn_state_t l_state = l_nconn->get_state();
        switch (l_state)
        {
        case CONN_STATE_READING:
        {

                reqlet *l_reqlet = (reqlet *)l_nconn->get_data1();
                if(!l_reqlet)
                {
                        NDBG_PRINT("Error: getting reqlet\n");
                        l_t_client->cleanup_connection(l_nconn);
                        l_nconn->give_lock();
                        return NULL;
                }

                int l_read_status = 0;
                l_read_status = l_nconn->read_cb();
                if(l_read_status == STATUS_ERROR)
                {
                        if(l_t_client->m_verbose)
                        {
                                NDBG_PRINT("Error: performing read for l_rconn_pb[%" PRIu64 "]\n", l_nconn->get_id());
                        }
                        l_t_client->cleanup_connection(l_nconn);
                        l_nconn->give_lock();
                        return NULL;
                        //++(l_reqlet->m_stat_agg.m_num_errors);
                        //++(l_t_client->m_stat_err_read_cb);
                }
                else
                {
                        //l_t_client->m_stat_bytes_read += l_read_status;
                        //++l_t_client->m_num_fetched;
                        l_reqlet->m_stat_agg.m_num_bytes_read += l_read_status;
                }

                // Check for done...
                l_state = l_nconn->get_state();
                if(l_state == CONN_STATE_DONE)
                {
                        ++(l_reqlet->m_stat_agg.m_num_conn_completed);

                        // Cancel timer
                        l_t_client->m_evr_loop->cancel_timer(&(l_nconn->m_timer_obj));

                        // Add stats
                        add_stat_to_agg(l_reqlet->m_stat_agg, l_nconn->get_stats());
                        l_nconn->reset_stats();

                        // You complete me...
                        if(!l_nconn->can_reuse())
                        {
                                l_t_client->cleanup_connection(l_nconn, false);
                                l_nconn->give_lock();
                                return NULL;
                        }
                }

                break;
        }
        default:
        {
                //NDBG_OUTPUT("* %sBADX%s[%6d]\n",
                //                ANSI_COLOR_BG_RED, ANSI_COLOR_OFF,
                //                l_fd);

                l_t_client->cleanup_connection(l_nconn);
                l_nconn->give_lock();
                return NULL;

                break;
        }
        }

        l_nconn->give_lock();
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
        int32_t l_fd = (intptr_t)a_data;
        nconn *l_nconn = l_t_client->get_locked_conn(l_fd);
        if(!l_nconn)
        {
                return NULL;
        }
        //NDBG_PRINT("%sSTATUS_ERRORS%s\n", ANSI_COLOR_BG_RED, ANSI_COLOR_OFF);
        l_t_client->cleanup_connection(l_nconn, true);
        l_nconn->give_lock();
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
        int32_t l_fd = (intptr_t)a_data;
        nconn *l_nconn = l_t_client->get_locked_conn(l_fd);
        if(!l_nconn)
        {
                return NULL;
        }

        if(l_t_client->m_verbose)
        {
                NDBG_PRINT("%sTIMING OUT CONN%s: i_conn: %d\n",
                        ANSI_COLOR_BG_RED, ANSI_COLOR_OFF,
                        l_fd);
        }

        // TODO
        // Add stats
        //l_conn->m_stat.m_status_code = 599;
        //add_stat_to_agg(l_reqlet->m_stat_agg, l_conn->get_stats());

        l_t_client->cleanup_connection(l_nconn, true);
        l_nconn->give_lock();
        return NULL;

}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
nconn *t_client::try_take_locked_nconn_w_hash(uint64_t a_hash)
{

        // ---------------------------------
        // Look up
        // ---------------------------------
        pthread_mutex_lock(&m_nconn_map_mutex);
        nconn_map_t::iterator i_nconn = m_nconn_map.find(a_hash);
        if(i_nconn != m_nconn_map.end())
        {
                // Found it!!! try take lock
                if(i_nconn->second->try_lock() == 0)
                {
                        pthread_mutex_unlock(&m_nconn_map_mutex);
                        return i_nconn->second;
                }

                // else was busy
                pthread_mutex_unlock(&m_nconn_map_mutex);
                return NULL;

        }
        pthread_mutex_unlock(&m_nconn_map_mutex);

        // ---------------------------------
        // Create new
        // ---------------------------------
        // Try take from unused list
        nconn *l_conn = NULL;
        if(!m_nconn_free_list.empty())
        {
                l_conn = m_nconn_free_list.front();
                m_nconn_free_list.pop_front();
        }
        else
        {
                // Allocate new connection
                l_conn = new nconn(m_verbose,
                                m_color,
                                m_sock_opt_recv_buf_size,
                                m_sock_opt_send_buf_size,
                                m_sock_opt_no_delay,
                                false,
                                false,
                                -1,
                                NULL);

        }

        pthread_mutex_lock(&m_nconn_map_mutex);
        m_nconn_map[a_hash] = l_conn;
        pthread_mutex_unlock(&m_nconn_map_mutex);


        // Take lock
        l_conn->take_lock();

        return l_conn;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void *t_client::evr_loop_timer_cb(void *a_data)
{

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
                l_nconn->give_lock();
                delete l_client_pb_cmd;
                delete l_pb_cmd;
                return NULL;
        }
        else
        {

                conn_state_t l_state = l_nconn->get_state();
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
                        l_nconn->give_lock();
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
                if(l_state == CONN_STATE_DONE)
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

                                // Use (intptr_t) to convert back
                                // convert to void * with (void *) (intptr_t)
                                void *l_fd = (void *) (intptr_t)l_nconn->get_fd();
                                l_t_client->m_evr_loop->add_timer(HLP_DEFAULT_CONN_TIMEOUT_MS, evr_loop_file_timeout_cb, l_fd, &(l_nconn->m_timer_obj));

                        }
                        // You complete me...
                        else
                        {
                                //NDBG_PRINT("NOT FREE but can't reuse???\n");

                                ++(l_reqlet->m_stat_agg.m_num_errors);
                                ++(l_t_client->m_stat_err_state);

                                l_nconn->give_lock();
                                l_t_client->cleanup_connection(l_nconn);
                                delete l_client_pb_cmd;
                                delete l_pb_cmd;
                                return NULL;
                        }

                }
                // ---------------------------------------
                // Connection Free
                // ---------------------------------------
                else if(l_state == CONN_STATE_FREE)
                {

                        int l_status;
                        // Set scheme (mode HTTP/HTTPS)
                        l_nconn->set_scheme(l_reqlet->m_url.m_scheme);

                        // Bump stats
                        ++(l_reqlet->m_stat_agg.m_num_conn_started);

                        //NDBG_PRINT("%sCONNECT%s: %s\n", ANSI_COLOR_BG_MAGENTA, ANSI_COLOR_OFF, l_reqlet->m_url.m_host.c_str());
                        l_status = l_nconn->do_connect(l_reqlet->m_host_info, l_reqlet->m_url.m_host);
                        conn_state_t l_state = l_nconn->get_state();
                        if((STATUS_OK != l_status) &&
                                (l_state != CONN_STATE_CONNECTING))
                        {
                                NDBG_PRINT("Error: Performing do_connect\n");
                                //++(l_t_client->m_stat_err_do_connect);
                                ++(l_reqlet->m_stat_agg.m_num_errors);

                                l_t_client->cleanup_connection(l_nconn);
                                l_nconn->give_lock();
                                delete l_client_pb_cmd;
                                delete l_pb_cmd;
                                return NULL;

                        }

                        l_nconn->set_id(l_cmd_hash);
                        l_t_client->add_nconn(l_nconn);

                        // -------------------------------------------
                        // Add to event handler
                        // -------------------------------------------
                        int32_t l_fd = l_nconn->get_fd();
                        void *l_fd_ptr = (void *) (intptr_t)l_nconn->get_fd();
                        if (0 != l_t_client->m_evr_loop->add_file(l_fd, EVR_FILE_ATTR_MASK_WRITE, l_fd_ptr))
                        {
#if 0
                                NDBG_PRINT("Error: Couldn't add socket file descriptor\n");
                                ++(l_reqlet->m_stat_agg.m_num_errors);
                                goto connection_error;
#endif
                        }

                        if(l_state != CONN_STATE_CONNECTING)
                        {
                                ++(l_t_client->m_num_reqs_sent);

                                if (0 != l_t_client->m_evr_loop->add_file(l_fd, EVR_FILE_ATTR_MASK_READ, l_fd_ptr))
                                {
#if 0
                                        NDBG_PRINT("Error: Couldn't add socket file descriptor\n");
                                        ++(l_reqlet->m_stat_agg.m_num_errors);
                                        goto connection_error;
#endif
                                }

                                // TODO Add timer for callback
                                l_t_client->m_evr_loop->add_timer(HLP_DEFAULT_CONN_TIMEOUT_MS, evr_loop_file_timeout_cb, l_fd_ptr, &(l_nconn->m_timer_obj));

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
        l_nconn->give_lock();
        delete l_client_pb_cmd;
        delete l_pb_cmd;

        return NULL;

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

        l_reqlet->resolve();

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

        char *l_req_buf = ao_conn.m_req_buf;
        uint32_t l_req_buf_len = 0;
        uint32_t l_max_buf_len = sizeof(ao_conn.m_req_buf);

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
                //printf("Adding HEADER: %s: %s\n", i_header->first.c_str(), i_header->second.c_str());
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
        ao_conn.m_req_buf_len = l_req_buf_len;

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

        int32_t l_fd = a_nconn->get_fd();
        m_evr_loop->remove_file(l_fd, 0);
        a_nconn->done_cb();
        a_nconn->set_data1(NULL);
        remove_nconn(a_nconn);

        return STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t t_client::add_nconn(nconn *a_nconn)
{
        int32_t l_fd = a_nconn->get_fd();
        int32_t l_id = a_nconn->get_id();

        m_nconn_map[l_id] = a_nconn;
        m_nconn_vector[l_fd] = a_nconn;

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

        int32_t l_fd = a_nconn->get_fd();
        uint64_t l_id = a_nconn->get_id();

        pthread_mutex_lock(&m_nconn_map_mutex);
        if(m_nconn_map.find(l_id) != m_nconn_map.end())
        {
                m_nconn_map.erase(l_id);
        }
        pthread_mutex_unlock(&m_nconn_map_mutex);
        m_nconn_vector[l_fd] = NULL;
        m_nconn_free_list.push_back(a_nconn);

        return STATUS_OK;

}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
nconn *t_client::get_locked_conn(int32_t a_fd)
{
        nconn *l_conn;
        l_conn = m_nconn_vector[a_fd];
        if(l_conn)
        {
                l_conn->take_lock();
        }
        return l_conn;
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
        NDBG_OUTPUT("* %sSTATS%s: bytes_read: %lu tq = %lu eq = %lu nconn_vector = %lu free_size = %lu map_size = %lu\n",
                        ANSI_COLOR_FG_GREEN, ANSI_COLOR_OFF,
                        m_stat_bytes_read,
                        m_evr_cmd_loop->get_pq_size(),
                        m_evr_loop->get_pq_size(),
                        m_nconn_vector.size(),
                        m_nconn_free_list.size(),
                        m_nconn_map.size()
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

#if 0
        // Show connection info
        for(rconn_pb_map_t::iterator i_rconn = m_rconn_pb_map.begin();
                        i_rconn != m_rconn_pb_map.end();
                        ++i_rconn)
        {
                nconn *l_conn = i_rconn->second->m_conn;
                NDBG_OUTPUT("! %sCONN%s[%6d] Content-Length: %12" PRIu64 " TOTAL: %12u STATE: %d\n",
                                ANSI_COLOR_FG_MAGENTA, ANSI_COLOR_OFF,
                                l_conn->m_fd,
                                l_conn->m_http_parser.content_length,
                                l_conn->m_stat.m_total_bytes,
                                l_conn->m_state
                                );
        }
#endif
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

