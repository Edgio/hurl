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

#include <unistd.h>

// inet_aton
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <errno.h>
#include <string.h>
#include <algorithm>
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
                const std::string & a_cipher_str,
                SSL_CTX *a_ssl_ctx,
                evr_type_t a_evr_type,
                uint32_t a_max_parallel_connections,
                int32_t a_run_time_s,
                int32_t a_timeout_s,
                int64_t a_max_reqs_per_conn,
                const std::string &a_url,
                const std::string &a_url_file,
                bool a_wildcarding):

	                m_t_run_thread(),
	                m_timeout_s(a_timeout_s),
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
	                m_nconn_vector(a_max_parallel_connections),
	                m_conn_free_list(),
	                m_conn_used_set(),
	                m_max_reqs_per_conn(a_max_reqs_per_conn),
	                m_run_time_s(a_run_time_s),
	                m_start_time_s(0),
	                m_url(a_url),
	                m_url_file(a_url_file),
	                m_wilcarding(a_wildcarding),
	                m_reqlet_map(),
	                m_num_fetches(-1),
	                m_num_fetched(0),
	                m_num_pending(0),
	                m_rate_limit(false),
	                m_rate_delta_us(0),
	                m_reqlet_mode(REQLET_MODE_ROUND_ROBIN),
	                m_reqlet_avail_list(),
	                m_last_reqlet_index(0),
	                m_last_get_req_us(0),
	                m_rand_ptr(NULL),
	                m_header_map(),
	                m_evr_loop(NULL)
{

        // Initialize rand...
        m_rand_ptr = malloc(sizeof(tinymt64_t));
        tinymt64_t *l_rand_ptr = (tinymt64_t*)m_rand_ptr;
        tinymt64_init(l_rand_ptr, get_time_us());

        for(uint32_t i_conn = 0; i_conn < a_max_parallel_connections; ++i_conn)
        {
                nconn *l_nconn = new nconn(m_verbose,
                                m_color,
                                m_sock_opt_recv_buf_size,
                                m_sock_opt_send_buf_size,
                                m_sock_opt_no_delay,
                                false,
                                true,
                                m_timeout_s,
                                m_max_reqs_per_conn,
                                m_rand_ptr);

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
        if(m_evr_loop)
        {
                l_status = m_evr_loop->stop();
                if(l_status != STATUS_OK)
                {
                        NDBG_PRINT("Error performing stop.\n");
                }
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
                return NULL;
        }

        nconn* l_nconn = static_cast<nconn*>(a_data);
        reqlet *l_reqlet = static_cast<reqlet *>(l_nconn->get_data1());
        t_client *l_t_client = g_t_client;

        if (false == l_t_client->has_available_fetches())
                return NULL;

        //NDBG_PRINT("%sWRITEABLE%s %p\n", ANSI_COLOR_FG_BLUE, ANSI_COLOR_OFF, l_nconn);

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

        return NULL;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void *t_client::evr_loop_file_readable_cb(void *a_data)
{
        //NDBG_PRINT("%sREADABLE%s\n", ANSI_COLOR_BG_GREEN, ANSI_COLOR_OFF);

        if(!a_data)
        {
                return NULL;
        }

        nconn* l_nconn = static_cast<nconn*>(a_data);
        reqlet *l_reqlet = static_cast<reqlet *>(l_nconn->get_data1());
        t_client *l_t_client = g_t_client;

        //NDBG_PRINT("%sREADABLE%s %p\n", ANSI_COLOR_FG_GREEN, ANSI_COLOR_OFF, l_nconn);

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
        if((l_nconn->get_state() == nconn::CONN_STATE_DONE) ||
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
                l_can_reuse = (l_nconn->can_reuse() && l_t_client->reqlet_give_and_can_reuse_conn(l_reqlet));

                //NDBG_PRINT("CONN[%d] %sREUSE%s: %d\n",
                //              l_nconn->m_fd,
                //              ANSI_COLOR_BG_RED, ANSI_COLOR_OFF,
                //              l_can_reuse
                //              );

                if(l_can_reuse)
                {
                        // Send request again...
                        l_t_client->create_request(*l_nconn, *l_reqlet);
                        l_nconn->send_request(true);
                        l_t_client->m_num_pending++;
                }
                // You complete me...
                else
                {
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
                return NULL;
        }

        nconn* l_nconn = static_cast<nconn*>(a_data);
        reqlet *l_reqlet = static_cast<reqlet *>(l_nconn->get_data1());
        t_client *l_t_client = g_t_client;
        uint64_t l_connection_id = l_nconn->get_id();

        //printf("%sT_O%s: %p\n",ANSI_COLOR_FG_BLUE, ANSI_COLOR_OFF, l_nconn);

        // Add stats
        add_stat_to_agg(l_reqlet->m_stat_agg, l_nconn->get_stats());
        l_nconn->reset_stats();

        if(l_t_client->m_verbose)
        {
                NDBG_PRINT("%sTIMING OUT CONN%s: i_conn: %lu HOST: %s LAST_STATE: %d\n\n",
                                ANSI_COLOR_BG_RED, ANSI_COLOR_OFF,
                                l_connection_id,
                                l_reqlet->m_url.m_host.c_str(),
                                l_nconn->get_state());
        }

        // Stats
        l_t_client->m_num_fetched++;
        l_reqlet->m_stat_agg.m_num_conn_completed++;
        ++(l_reqlet->m_stat_agg.m_num_idle_killed);

        // Connections
        l_t_client->cleanup_connection(l_nconn);

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
        int32_t l_status;

        // Add the urls
        if(!m_url.empty())
        {
                l_status = add_url(m_url);
                if(l_status != STATUS_OK)
                {
                        m_stopped = true;
                        return NULL;
                }
        }
        if(!m_url_file.empty())
        {
                l_status = add_url_file(m_url_file);
                if(l_status != STATUS_OK)
                {
                        m_stopped = true;
                        return NULL;
                }
        }

        // Set start time
        m_start_time_s = get_time_s();

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
                        m_max_parallel_connections);

        // -------------------------------------------
        // Main loop.
        // -------------------------------------------
        //NDBG_PRINT("starting main loop: m_run_time_s: %d start_time_s: %" PRIu64 "\n", m_run_time_s, get_time_s() - m_start_time_s);
        while(!m_stopped &&
                        //has_available_fetches() &&
                        !is_done() &&
                        ((m_run_time_s == -1) || (m_run_time_s > (int32_t)(get_time_s() - m_start_time_s))))
        {

                //NDBG_PRINT("TIME: %d\n", (int32_t)get_time_s() - m_start_time_s);
                // -------------------------------------------
                // Start Connections
                // -------------------------------------------
                //NDBG_PRINT("%sSTART_CONNECTIONS%s\n", ANSI_COLOR_BG_MAGENTA, ANSI_COLOR_OFF);
                l_status = start_connections();
                if(l_status != STATUS_OK)
                {
                        NDBG_PRINT("%sSTART_CONNECTIONS%s ERROR!\n", ANSI_COLOR_BG_RED, ANSI_COLOR_OFF);
                        return NULL;
                }

                // Run loop
                //NDBG_PRINT("%sRUN%s\n", ANSI_COLOR_BG_MAGENTA, ANSI_COLOR_OFF);
                m_evr_loop->run();
                //NDBG_PRINT("%sDONE%s\n", ANSI_COLOR_BG_MAGENTA, ANSI_COLOR_OFF);

        }

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
        // Find an empty connection slot.
        //NDBG_PRINT("m_conn_free_list.size(): %Zu\n", m_conn_free_list.size());
        for (conn_id_list_t::iterator i_conn = m_conn_free_list.begin(); i_conn != m_conn_free_list.end();)
        {
                if (false == has_available_fetches())
                        break;

                // Grab reqlet from repo
                reqlet *l_reqlet = NULL;
                // TODO Check for error???

                l_reqlet = reqlet_take();
                if(!l_reqlet)
                {
                        // No available reqlets
                        if(m_verbose)
                        {
                                NDBG_PRINT("Bailing out out.  Reason no reqlets available\n");
                        }
                        return STATUS_OK;

                }

                // Start connection for this reqlet
                //NDBG_PRINT("i_conn: %d\n", *i_conn);
                nconn *l_nconn = m_nconn_vector[*i_conn];
                // TODO Check for NULL

                int32_t l_status;

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

                //NDBG_PRINT("%sCONNECT%s: %s\n", ANSI_COLOR_BG_MAGENTA, ANSI_COLOR_OFF, l_reqlet->m_url.m_host.c_str());
                l_nconn->set_host(l_reqlet->m_url.m_host);
                l_status = l_nconn->run_state_machine(m_evr_loop, l_reqlet->m_host_info);
                if((l_status != STATUS_OK) &&
                                (l_nconn->get_state() != nconn::CONN_STATE_CONNECTING))
                {
                        NDBG_PRINT("Error: Performing do_connect: connection_state: %d status: %d\n", l_nconn->get_state(), l_status);
                        cleanup_connection(l_nconn);
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
        const std::string &l_path_ref = a_reqlet.get_path(m_rand_ptr);
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
        for(header_map_t::const_iterator i_header = m_header_map.begin();
                        i_header != m_header_map.end();
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

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void t_client::get_stats_copy(tag_stat_map_t &ao_tag_stat_map)
{
        // TODO Need to make this threadsafe -spinlock perhaps...
        for(reqlet_list_t::iterator i_reqlet = m_reqlet_avail_list.begin(); i_reqlet != m_reqlet_avail_list.end(); ++i_reqlet)
        {
                ao_tag_stat_map[(*i_reqlet)->get_label()] = (*i_reqlet)->m_stat_agg;
        }
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t t_client::add_avail(reqlet *a_reqlet)
{

        // Add to available list...
        //--m_unresolved_count;

        //if(a_reqlet)
        //	NDBG_PRINT("Add available: a_reqlet: %p %sHOST%s: %s\n", a_reqlet, ANSI_COLOR_FG_GREEN, ANSI_COLOR_OFF, a_reqlet->m_url.m_host.c_str());

        m_reqlet_avail_list.push_back(a_reqlet);

        return STATUS_OK;

}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t t_client::add_url(std::string &a_url)
{

        // TODO
        // Make threadsafe...

        reqlet *l_reqlet = new reqlet((uint64_t)(m_reqlet_map.size()));

        // Initialize
        int32_t l_status = STATUS_OK;
        l_status = l_reqlet->init_with_url(a_url, m_wilcarding);
        if(STATUS_OK != l_status)
        {
                NDBG_PRINT("Error performing init_with_url: %s\n", a_url.c_str());
                return STATUS_ERROR;
        }

        //NDBG_PRINT("PARSE: | %d | %s | %d | %s |\n",
        //		l_reqlet->m_url.m_scheme,
        //		l_reqlet->m_url.m_host.c_str(),
        //		l_reqlet->m_url.m_port,
        //		l_reqlet->m_url.m_path.c_str());

        m_reqlet_map[l_reqlet->get_id()] = l_reqlet;
        //++m_unresolved_count;

        // Slow resolve
        l_status = l_reqlet->resolve();
        if(l_status != STATUS_OK)
        {
                NDBG_PRINT("Error performing resolving host: %s\n", a_url.c_str());
                return STATUS_ERROR;
        }

        // Test connection

        //--m_unresolved_count;
        m_reqlet_avail_list.push_back(l_reqlet);

        //t_async_resolver::get()->add_lookup(l_reqlet, resolve_cb);

        return STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t t_client::add_url_file(std::string &a_url_file)
{

        FILE * l_file;
        int32_t l_status = STATUS_OK;

        l_file = fopen (a_url_file.c_str(),"r");
        if (NULL == l_file)
        {
                NDBG_PRINT("Error opening file.  Reason: %s\n", strerror(errno));
                return STATUS_ERROR;
        }

        //NDBG_PRINT("ADD_FILE: ADDING: %s\n", a_url_file.c_str());
        //uint32_t l_num_added = 0;

        ssize_t l_file_line_size = 0;
        char *l_file_line = NULL;
        size_t l_unused;
        while((l_file_line_size = getline(&l_file_line,&l_unused,l_file)) != -1)
        {

                //NDBG_PRINT("LINE: %s", l_file_line);

                std::string l_line(l_file_line);

                if(!l_line.empty())
                {
                        //NDBG_PRINT("Add url: %s\n", l_line.c_str());

                        l_line.erase( std::remove_if( l_line.begin(), l_line.end(), ::isspace ), l_line.end() );
                        if(!l_line.empty())
                        {
                                l_status = add_url(l_line);
                                if(STATUS_OK != l_status)
                                {
                                        NDBG_PRINT("Error performing addurl for url: %s\n", l_line.c_str());

                                        if(l_file_line)
                                        {
                                                free(l_file_line);
                                                l_file_line = NULL;
                                        }
                                        return STATUS_ERROR;
                                }
                        }
                }

                if(l_file_line)
                {
                        free(l_file_line);
                        l_file_line = NULL;
                }

        }

        //NDBG_PRINT("ADD_FILE: DONE: %s -- last line len: %d\n", a_url_file.c_str(), (int)l_file_line_size);
        //if(l_file_line_size == -1)
        //{
        //        NDBG_PRINT("Error: getline errno: %d Reason: %s\n", errno, strerror(errno));
        //}


        l_status = fclose(l_file);
        if (0 != l_status)
        {
                NDBG_PRINT("Error closing file.  Reason: %s\n", strerror(errno));
                return STATUS_ERROR;
        }

        return STATUS_OK;

}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
reqlet *t_client::reqlet_take(void)
{

        reqlet *l_reqlet = NULL;

        //NDBG_PRINT("m_rate_limit = %d m_rate_delta_us = %lu\n", m_rate_limit, m_rate_delta_us);
        //NDBG_PRINT("m_reqlet_avail_list.size(): %d\n", (int)m_reqlet_avail_list.size());

        if(0 == m_reqlet_avail_list.size())
        {
                return NULL;
        }

        limit_rate();

        // Based on mode
        switch(m_reqlet_mode)
        {
        case REQLET_MODE_ROUND_ROBIN:
        {
                uint32_t l_next_index = ((m_last_reqlet_index + 1) >= m_reqlet_avail_list.size()) ? 0 : m_last_reqlet_index + 1;
                //NDBG_PRINT("m_last_reqlet_index: %d\n", m_last_reqlet_index);
                m_last_reqlet_index = l_next_index;
                l_reqlet = m_reqlet_avail_list[m_last_reqlet_index];
                break;
        }
        case REQLET_MODE_SEQUENTIAL:
        {
                l_reqlet = m_reqlet_avail_list[m_last_reqlet_index];
                if(l_reqlet->is_done())
                {
                        uint32_t l_next_index = ((m_last_reqlet_index + 1) >= m_reqlet_avail_list.size()) ? 0 : m_last_reqlet_index + 1;
                        l_reqlet->reset();
                        m_last_reqlet_index = l_next_index;
                        l_reqlet = m_reqlet_avail_list[m_last_reqlet_index];
                }
                break;
        }
        case REQLET_MODE_RANDOM:
        {
                tinymt64_t *l_rand_ptr = (tinymt64_t*)m_rand_ptr;
                uint32_t l_next_index = (uint32_t)(tinymt64_generate_uint64(l_rand_ptr) % m_reqlet_avail_list.size());
                m_last_reqlet_index = l_next_index;
                l_reqlet = m_reqlet_avail_list[m_last_reqlet_index];
                break;
        }
        default:
        {
                // Default to round robin
                uint32_t l_next_index = ((m_last_reqlet_index + 1) >= m_reqlet_avail_list.size()) ? 0 : m_last_reqlet_index + 1;
                m_last_reqlet_index = l_next_index;
                l_reqlet = m_reqlet_avail_list[m_last_reqlet_index];
        }
        }


        if(l_reqlet)
        {
                l_reqlet->bump_num_requested();
        }

        return l_reqlet;

}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
bool t_client::reqlet_give_and_can_reuse_conn(reqlet *a_reqlet)
{
        limit_rate();
        return has_available_fetches();
}



