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
#include "hle.h"
#include "reqlet_repo.h"

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
#define T_CLIENT_CONN_CLEANUP(a_t_client, a_conn, a_reqlet, a_status, a_response) \
        do { \
                a_reqlet->set_response(a_status, a_response); \
                if(a_status >= 500) reqlet_repo::get()->up_done(true); \
                else reqlet_repo::get()->up_done(false); \
                a_t_client->cleanup_connection(a_conn); \
        }while(0)

//: ----------------------------------------------------------------------------
//: Fwd Decl's
//: ----------------------------------------------------------------------------

//: ----------------------------------------------------------------------------
//: Thread local global
//: ----------------------------------------------------------------------------
__thread t_client *g_t_client = NULL;

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
t_client::t_client(bool a_verbose,
                bool a_color,
                uint32_t a_sock_opt_recv_buf_size,
                uint32_t a_sock_opt_send_buf_size,
                bool a_sock_opt_no_delay,
                uint32_t a_timeout_s,
                const std::string & a_cipher_str,
                SSL_CTX *a_ssl_ctx,
                evr_loop_type_t a_evr_loop_type,
                uint32_t a_max_parallel_connections):

        m_t_run_thread(),
        m_verbose(a_verbose),
        m_color(a_color),
        m_sock_opt_recv_buf_size(a_sock_opt_recv_buf_size),
        m_sock_opt_send_buf_size(a_sock_opt_send_buf_size),
        m_sock_opt_no_delay(a_sock_opt_no_delay),
        m_timeout_s(a_timeout_s),
        m_cipher_str(a_cipher_str),
        m_ssl_ctx(a_ssl_ctx),
        m_evr_loop_type(a_evr_loop_type),
        m_stopped(false),
        m_max_parallel_connections(a_max_parallel_connections),
        m_nconn_vector(a_max_parallel_connections),
        m_conn_free_list(),
        m_conn_used_set(),
        m_num_fetches(-1),
        m_num_fetched(0),
        m_num_pending(0),
        m_header_map(),
        m_evr_loop(NULL)
{

        for(uint32_t i_conn = 0; i_conn < a_max_parallel_connections; ++i_conn)
        {
                nconn *l_nconn = new nconn(m_verbose,
                                m_color,
                                m_sock_opt_recv_buf_size,
                                m_sock_opt_send_buf_size,
                                m_sock_opt_no_delay,
                                true,
                                false,
                                m_timeout_s,
                                1,
                                NULL);

                l_nconn->set_id(i_conn);
                m_nconn_vector[i_conn] = l_nconn;
                l_nconn->set_ssl_ctx(a_ssl_ctx);
                m_conn_free_list.push_back(i_conn);
        }
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
                delete m_nconn_vector[i_conn];
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
                NDBG_PRINT("Error: performing run_state_machine\n");
                T_CLIENT_CONN_CLEANUP(l_t_client, l_nconn, l_reqlet, 500, "Error performing connect_cb");
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

        //NDBG_PRINT("%sREADABLE%s[%d] %p\n", ANSI_COLOR_FG_GREEN, ANSI_COLOR_OFF, l_nconn->m_fd, l_nconn);

        // Cancel last timer
        l_t_client->m_evr_loop->cancel_timer(&(l_nconn->m_timer_obj));

        int32_t l_status = STATUS_OK;
        l_status = l_nconn->run_state_machine(l_t_client->m_evr_loop, l_reqlet->m_host_info);
        if(STATUS_ERROR == l_status)
        {
                NDBG_PRINT("Error: performing run_state_machine\n");
                T_CLIENT_CONN_CLEANUP(l_t_client, l_nconn, l_reqlet, 500, "Error performing connect_cb");
                return NULL;
        }

        if(l_status >= 0)
        {
                l_reqlet->m_stat_agg.m_num_bytes_read += l_status;
        }

        // Check for done...
        if((l_nconn->get_state() == nconn::CONN_STATE_DONE) ||
                        (l_status == STATUS_ERROR))
        {
                if(l_status == STATUS_ERROR)
                {
                        T_CLIENT_CONN_CLEANUP(l_t_client, l_nconn, l_reqlet, 500, "Unknown error");
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
        //		l_rconn->m_timer_obj);

        // Add stats
        //add_stat_to_agg(l_reqlet->m_stat_agg, l_nconn->get_stats());
        if(l_t_client->m_verbose)
        {
                NDBG_PRINT("%sTIMING OUT CONN%s: i_conn: %lu HOST: %s LAST_STATE: %d THIS: %p\n",
                                ANSI_COLOR_BG_RED, ANSI_COLOR_OFF,
                                l_nconn->get_id(),
                                l_reqlet->m_url.m_host.c_str(),
                                l_nconn->get_state(),
                                l_t_client);
        }

        // Cleanup
        T_CLIENT_CONN_CLEANUP(l_t_client, l_nconn, l_reqlet, 502, "Connection timed out");

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

        // Set thread local
        g_t_client = this;

        // Create loop
        m_evr_loop = new evr_loop(
                        evr_loop_file_readable_cb,
                        evr_loop_file_writeable_cb,
                        evr_loop_file_error_cb,
                        m_evr_loop_type,
                        m_max_parallel_connections);

        reqlet_repo *l_reqlet_repo = reqlet_repo::get();

        // -------------------------------------------
        // Main loop.
        // -------------------------------------------
        //NDBG_PRINT("starting main loop\n");
        while(!m_stopped &&
                        !l_reqlet_repo->done())
        {

                // -------------------------------------------
                // Start Connections
                // -------------------------------------------
                //NDBG_PRINT("%sSTART_CONNECTIONS%s\n", ANSI_COLOR_BG_MAGENTA, ANSI_COLOR_OFF);
                if(!l_reqlet_repo->done())
                {
                        int32_t l_status;
                        l_status = start_connections();
                        if(l_status != STATUS_OK)
                        {
                                NDBG_PRINT("%sSTART_CONNECTIONS%s ERROR!\n", ANSI_COLOR_BG_RED, ANSI_COLOR_OFF);
                                return NULL;
                        }
                }

                // Run loop
                m_evr_loop->run();

        }

        //NDBG_PRINT("%sFINISHING_CONNECTIONS%s\n", ANSI_COLOR_BG_MAGENTA, ANSI_COLOR_OFF);

        // Still awaiting responses -wait...
        uint64_t l_cur_time = get_time_s();
        uint64_t l_end_time = l_cur_time + m_timeout_s;
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
int32_t t_client::start_connections(void)
{
        int32_t l_status;
        reqlet_repo *l_reqlet_repo = reqlet_repo::get();
        reqlet *l_reqlet = NULL;

        // Find an empty connection slot.
        //NDBG_PRINT("m_conn_free_list.size(): %Zu\n", m_conn_free_list.size());
        for (conn_id_list_t::iterator i_conn = m_conn_free_list.begin();
               (i_conn != m_conn_free_list.end()) &&
               (!l_reqlet_repo->done()) &&
               !m_stopped;
             )
        {

                // Loop trying to get reqlet
                l_reqlet = NULL;
                while(((l_reqlet = l_reqlet_repo->try_get_resolved()) == NULL) && (!l_reqlet_repo->done()));
                if((l_reqlet == NULL) && l_reqlet_repo->done())
                {
                        // Bail out
                        return STATUS_OK;
                }

                // Start connection for this reqlet
                //NDBG_PRINT("i_conn: %d\n", *i_conn);
                nconn *l_nconn = m_nconn_vector[*i_conn];
                // TODO Check for NULL


                // Assign the reqlet for this connection
                l_nconn->set_data1(l_reqlet);

                // Set scheme (mode HTTP/HTTPS)
                l_nconn->set_scheme(l_reqlet->m_url.m_scheme);

                // Bump stats
                ++(l_reqlet->m_stat_agg.m_num_conn_started);

                // Create request
                create_request(*l_nconn, *l_reqlet);

                m_conn_used_set.insert(*i_conn);
                m_conn_free_list.erase(i_conn++);

                // TODO Make configurable
                m_evr_loop->add_timer(m_timeout_s*1000, evr_loop_file_timeout_cb, l_nconn, &(l_nconn->m_timer_obj));

                // Add to num pending
                ++m_num_pending;

                //NDBG_PRINT("%sCONNECT%s: %s\n", ANSI_COLOR_BG_MAGENTA, ANSI_COLOR_OFF, l_reqlet->m_url.m_host.c_str());
                l_nconn->set_host(l_reqlet->m_url.m_host);
                l_status = l_nconn->run_state_machine(m_evr_loop, l_reqlet->m_host_info);
                if((STATUS_OK != l_status) &&
                                (l_nconn->get_state() != nconn::CONN_STATE_CONNECTING))
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

        // Get connection
        char *l_req_buf = ao_conn.m_req_buf;
        uint32_t l_req_buf_len = 0;
        uint32_t l_max_buf_len = sizeof(ao_conn.m_req_buf);

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
        for(header_map_t::const_iterator i_header = m_header_map.begin();
                        i_header != m_header_map.end();
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

        uint64_t l_conn_id = a_nconn->get_id();

        // Cancel last timer
        if(a_cancel_timer)
        {
                m_evr_loop->cancel_timer(&(a_nconn->m_timer_obj));
        }
        m_evr_loop->del_fd(a_nconn->get_fd());
        a_nconn->reset_stats();
        a_nconn->done_cb();

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
        m_header_map[a_header_key] = a_header_val;
        return l_retval;
}


