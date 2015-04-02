//: ----------------------------------------------------------------------------
//: Copyright (C) 2014 Verizon.  All Rights Reserved.
//: All Rights Reserved
//:
//: \file:    t_client.cc
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
#include "t_client.h"
#include "ndebug.h"
#include "reqlet.h"
#include "util.h"
#include "ssl_util.h"
#include "resolver.h"
#include "tinymt64.h"

#include <unistd.h>

//: ----------------------------------------------------------------------------
//: Constants
//: ----------------------------------------------------------------------------

//: ----------------------------------------------------------------------------
//: Macros
//: ----------------------------------------------------------------------------
#define T_CLIENT_CONN_CLEANUP(a_t_client, a_conn, a_reqlet, a_status, a_response) \
        do { \
                a_reqlet->set_response(a_status, a_response); \
                if(a_t_client->m_settings.m_show_summary) a_t_client->append_summary(a_reqlet);\
                ++(a_t_client->m_num_done);\
                if(a_status >= 500) ++(a_t_client->m_num_error); \
                a_t_client->cleanup_connection(a_conn); \
        }while(0)

#define T_CLIENT_SET_NCONN_OPT(_conn, _opt, _buf, _len) \
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
//: Thread local global
//: ----------------------------------------------------------------------------
__thread t_client *g_t_client = NULL;

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
#define COPY_SETTINGS(_field) m_settings._field = a_settings._field
t_client::t_client(const settings_struct_t &a_settings,
                   reqlet_vector_t a_reqlet_vector):
        m_t_run_thread(),
        m_settings(),
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
        m_stopped(false),
        m_nconn_vector(a_settings.m_num_parallel),
        m_conn_free_list(),
        m_conn_used_set(),
        m_num_fetches(-1),
        m_num_fetched(0),
        m_num_pending(0),
        m_run_time_s(-1),
        m_start_time_s(0),
        m_evr_loop(NULL),
        m_rate_delta_us(0),
        m_last_get_req_us(0),
        m_reqlet_vector(),
        m_reqlet_vector_idx(0),
        m_rand_ptr(NULL)
{
        for(int32_t i_conn = 0; i_conn < a_settings.m_num_parallel; ++i_conn)
        {
                m_nconn_vector[i_conn] = NULL;
                //NDBG_PRINT("ADDING i_conn: %u\n", i_conn);
                m_conn_free_list.push_back(i_conn);
        }

        // Friggin effc++
        COPY_SETTINGS(m_verbose);
        COPY_SETTINGS(m_color);
        COPY_SETTINGS(m_quiet);
        COPY_SETTINGS(m_show_summary);
        COPY_SETTINGS(m_url);
        COPY_SETTINGS(m_header_map);
        COPY_SETTINGS(m_verb);
        COPY_SETTINGS(m_req_body);
        COPY_SETTINGS(m_req_body_len);
        COPY_SETTINGS(m_t_client_list);
        COPY_SETTINGS(m_evr_loop_type);
        COPY_SETTINGS(m_num_parallel);
        COPY_SETTINGS(m_num_threads);
        COPY_SETTINGS(m_timeout_s);
        COPY_SETTINGS(m_rate);
        COPY_SETTINGS(m_request_mode);
        COPY_SETTINGS(m_num_end_fetches);
        COPY_SETTINGS(m_run_time_s);
        COPY_SETTINGS(m_connect_only);
        COPY_SETTINGS(m_save_response);
        COPY_SETTINGS(m_collect_stats);
        COPY_SETTINGS(m_num_reqs_per_conn);
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
        COPY_SETTINGS(m_resolver);

        // Set rate
        if(m_settings.m_rate != -1)
        {
                m_rate_delta_us = 1000000 / m_settings.m_rate;
        }

        // Initialize rand...
        m_rand_ptr = malloc(sizeof(tinymt64_t));
        tinymt64_t *l_rand_ptr = (tinymt64_t*)m_rand_ptr;
        tinymt64_init(l_rand_ptr, get_time_us());

        // Copy in requests
        uint64_t i_id = 0;
        for(reqlet_vector_t::const_iterator i_reqlet = a_reqlet_vector.begin();
            i_reqlet != a_reqlet_vector.end();
            ++i_reqlet)
        {
                // Will it blend!?!?
                reqlet *l_reqlet = new reqlet(**i_reqlet);
                //*l_reqlet = *i_reqlet;
                l_reqlet->set_id(i_id++);
                m_reqlet_vector.push_back(l_reqlet);
        }
        m_num_reqlets = m_reqlet_vector.size();
}


//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
t_client::~t_client()
{
        for(reqlet_vector_t::const_iterator i_reqlet = m_reqlet_vector.begin();
            i_reqlet != m_reqlet_vector.end();
            ++i_reqlet)
        {
                if(*i_reqlet)
                {
                        delete *i_reqlet;
                }
        }
        m_reqlet_vector.clear();

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
int32_t t_client::append_summary(reqlet *a_reqlet)
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

        // TODO
        return STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void t_client::limit_rate()
{
        if(m_settings.m_rate != -1)
        {
                uint64_t l_cur_time_us = get_time_us();
                if((l_cur_time_us - m_last_get_req_us) < m_rate_delta_us)
                {
                        usleep(m_rate_delta_us - (l_cur_time_us - m_last_get_req_us));
                }
                m_last_get_req_us = get_time_us();
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
//: \details: Get the next reqlet to process depending on request mode
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
reqlet *t_client::get_reqlet(void)
{
        reqlet *l_reqlet = NULL;

        //NDBG_PRINT("%sREQLST%s[%lu]: .\n", ANSI_COLOR_FG_CYAN, ANSI_COLOR_OFF, m_reqlet_list.size());
        //NDBG_PRINT("m_rate_limit = %d m_rate_delta_us = %lu\n", m_rate_limit, m_rate_delta_us);
        //NDBG_PRINT("m_reqlet_avail_list.size(): %d\n", (int)m_reqlet_avail_list.size());

        if(0 == m_reqlet_vector.size())
        {
                return NULL;
        }

        limit_rate();

        // Based on mode
        switch(m_settings.m_request_mode)
        {
        case REQUEST_MODE_ROUND_ROBIN:
        {
                uint32_t l_next_index = ((m_reqlet_vector_idx + 1) >= m_reqlet_vector.size()) ? 0 : m_reqlet_vector_idx + 1;
                //NDBG_PRINT("m_last_reqlet_index: %d\n", m_last_reqlet_index);
                m_reqlet_vector_idx = l_next_index;
                l_reqlet = m_reqlet_vector[m_reqlet_vector_idx];
                break;
        }
        case REQUEST_MODE_SEQUENTIAL:
        {
                l_reqlet = m_reqlet_vector[m_reqlet_vector_idx];
                if(l_reqlet->is_done())
                {
                        uint32_t l_next_index = ((m_reqlet_vector_idx + 1) >= m_reqlet_vector.size()) ? 0 : m_reqlet_vector_idx + 1;
                        l_reqlet->reset();
                        m_reqlet_vector_idx = l_next_index;
                        l_reqlet = m_reqlet_vector[m_reqlet_vector_idx];
                }
                break;
        }
        case REQUEST_MODE_RANDOM:
        {
                tinymt64_t *l_rand_ptr = (tinymt64_t*)m_rand_ptr;
                uint32_t l_next_index = (uint32_t)(tinymt64_generate_uint64(l_rand_ptr) % m_reqlet_vector.size());
                m_reqlet_vector_idx = l_next_index;
                l_reqlet = m_reqlet_vector[m_reqlet_vector_idx];
                break;
        }
        default:
        {
                // Default to round robin
                uint32_t l_next_index = ((m_reqlet_vector_idx + 1) >= m_reqlet_vector.size()) ? 0 : m_reqlet_vector_idx + 1;
                m_reqlet_vector_idx = l_next_index;
                l_reqlet = m_reqlet_vector[m_reqlet_vector_idx];
        }
        }


        if(l_reqlet)
        {
                l_reqlet->bump_num_requested();
        }

        // TODO UPGET LOCALLY
        ++m_num_get;
        ++m_reqlet_vector_idx;

        return l_reqlet;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
reqlet *t_client::try_get_resolved(void)
{
        reqlet *l_reqlet = NULL;
        int32_t l_status;

        l_reqlet = get_reqlet();
        if(NULL == l_reqlet)
        {
                return NULL;
        }

        // Try resolve
        if(NULL == m_settings.m_resolver ||
           (l_status =
            l_reqlet->resolve(*m_settings.m_resolver))!= STATUS_OK)
        {
                // TODO Set response and error
                ++m_num_resolved;
                ++m_num_error;
                append_summary(l_reqlet);
                return NULL;
        }

        ++m_num_resolved;
        return l_reqlet;

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
                return NULL;
        }

        nconn* l_nconn = static_cast<nconn*>(a_data);
        reqlet *l_reqlet = static_cast<reqlet *>(l_nconn->get_data1());
        t_client *l_t_client = g_t_client;

        // Cancel last timer
        l_t_client->m_evr_loop->cancel_timer(&(l_nconn->m_timer_obj));

#if 0
        if (false == l_t_client->has_available_fetches())
                return NULL;
#endif

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
        //NDBG_PRINT("%sREADABLE%s %p\n", ANSI_COLOR_FG_GREEN, ANSI_COLOR_OFF, a_data);
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
        if((STATUS_ERROR == l_status) &&
           l_nconn->m_verbose)
        {
                NDBG_PRINT("Error: performing run_state_machine\n");
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

                // Bump stats
                if(l_status == STATUS_ERROR)
                {
                        ++(l_reqlet->m_stat_agg.m_num_errors);
                        T_CLIENT_CONN_CLEANUP(l_t_client, l_nconn, l_reqlet, 901, l_nconn->m_last_error.c_str());
                }
                else
                {
                        // Give back reqlet
                        bool l_can_reuse = l_nconn->can_reuse();
                        bool l_can_reuse_conn = (l_t_client->m_num_fetches == -1) ||
                                                (l_t_client->m_num_fetches > l_t_client->m_num_pending);

                        //NDBG_PRINT("CONN %sREUSE%s: l_can_reuse: %d  l_can_reuse_conn: %d\n",
                        //              ANSI_COLOR_BG_RED, ANSI_COLOR_OFF,
                        //              l_can_reuse,
                        //              l_can_reuse_conn);

                        if(l_can_reuse && l_can_reuse_conn)
                        {
                                ++l_reqlet->m_stat_agg.m_num_conn_completed;

                                // Send request again...
                                l_t_client->limit_rate();
                                l_t_client->create_request(*l_nconn, *l_reqlet);
                                l_nconn->send_request(true);
                                ++l_t_client->m_num_fetched;
                        }
                        // You complete me...
                        else
                        {
                                //NDBG_PRINT("DONE: l_reqlet: %sHOST%s: %d / %d\n",
                                //              ANSI_COLOR_BG_RED, ANSI_COLOR_OFF,
                                //              l_can_reuse,
                                //              l_can_reuse_conn);

                                if(l_t_client->m_settings.m_connect_only)
                                {
                                        T_CLIENT_CONN_CLEANUP(l_t_client, l_nconn, l_reqlet, 200, "Connected Successfully");
                                }
                                else
                                {
                                        T_CLIENT_CONN_CLEANUP(l_t_client, l_nconn, l_reqlet, 0, "");
                                }
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
        //NDBG_PRINT("%sTIMEOUT%s %p\n", ANSI_COLOR_FG_CYAN, ANSI_COLOR_OFF, a_data);
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
        add_stat_to_agg(l_reqlet->m_stat_agg, l_nconn->get_stats());
        l_nconn->reset_stats();

        if(l_t_client->m_settings.m_verbose)
        {
                NDBG_PRINT("%sTIMING OUT CONN%s: i_conn: %lu HOST: %s THIS: %p\n",
                                ANSI_COLOR_BG_RED, ANSI_COLOR_OFF,
                                l_nconn->get_id(),
                                l_reqlet->m_url.m_host.c_str(),
                                l_t_client);
        }

        // Stats
        ++l_t_client->m_num_fetched;
        ++l_reqlet->m_stat_agg.m_num_idle_killed;

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
        m_evr_loop = new evr_loop(evr_loop_file_readable_cb,
                                  evr_loop_file_writeable_cb,
                                  evr_loop_file_error_cb,
                                  m_settings.m_evr_loop_type,
                                  m_settings.m_num_parallel);

        // Set start time
        m_start_time_s = get_time_s();

        // -------------------------------------------
        // Main loop.
        // -------------------------------------------
        //NDBG_PRINT("starting main loop\n");
        while(!m_stopped &&
              !is_pending_done() &&
              ((m_run_time_s == -1) || (m_run_time_s > (int32_t)(get_time_s() - m_start_time_s))))
        {
                // -------------------------------------------
                // Start Connections
                // -------------------------------------------
                //NDBG_PRINT("%sSTART_CONNECTIONS%s\n", ANSI_COLOR_BG_MAGENTA, ANSI_COLOR_OFF);
                int32_t l_status;
                l_status = start_connections();
                if(l_status != STATUS_OK)
                {
                        //NDBG_PRINT("%sSTART_CONNECTIONS%s ERROR!\n", ANSI_COLOR_BG_RED, ANSI_COLOR_OFF);
                        return NULL;
                }

                // Run loop
                m_evr_loop->run();

        }
        //NDBG_PRINT("%sFINISHING_CONNECTIONS%s -done: %d -- m_stopped: %d m_num_fetched: %d + pending: %d/ m_num_fetches: %d\n",
        //                ANSI_COLOR_BG_MAGENTA, ANSI_COLOR_OFF,
        //                is_pending_done(), m_stopped,
        //                (int)m_num_fetched, (int) m_num_pending, (int)m_num_fetches);

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
        //NDBG_PRINT("waiting: m_num_pending: %d --time-left: %d\n", (int)m_num_pending, int(l_end_time - l_cur_time));
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

        //NDBG_PRINT("CREATING NEW CONNECTION: id: %u\n", a_id);

        if(a_reqlet.m_url.m_scheme == nconn::SCHEME_TCP)
        {
                // TODO SET OPTIONS!!!
                l_nconn = new nconn_tcp(m_settings.m_verbose,
                                        m_settings.m_color,
                                        m_settings.m_num_reqs_per_conn,
                                        m_settings.m_save_response,
                                        m_settings.m_collect_stats,
                                        m_settings.m_connect_only);
        }
        else if(a_reqlet.m_url.m_scheme == nconn::SCHEME_SSL)
        {
                // TODO SET OPTIONS!!!
                l_nconn = new nconn_ssl(m_settings.m_verbose,
                                        m_settings.m_color,
                                        m_settings.m_num_reqs_per_conn,
                                        m_settings.m_save_response,
                                        m_settings.m_collect_stats,
                                        m_settings.m_connect_only);
        }

        l_nconn->set_id(a_id);

        // -------------------------------------------
        // Set options
        // -------------------------------------------
        // Set generic options
        T_CLIENT_SET_NCONN_OPT((*l_nconn), nconn_tcp::OPT_TCP_RECV_BUF_SIZE, NULL, m_settings.m_sock_opt_recv_buf_size);
        T_CLIENT_SET_NCONN_OPT((*l_nconn), nconn_tcp::OPT_TCP_SEND_BUF_SIZE, NULL, m_settings.m_sock_opt_send_buf_size);
        T_CLIENT_SET_NCONN_OPT((*l_nconn), nconn_tcp::OPT_TCP_NO_DELAY, NULL, m_settings.m_sock_opt_no_delay);

        // Set ssl options
        if(a_reqlet.m_url.m_scheme == nconn::SCHEME_SSL)
        {
                T_CLIENT_SET_NCONN_OPT((*l_nconn), nconn_ssl::OPT_SSL_CIPHER_STR,
                                              m_settings.m_ssl_cipher_list.c_str(),
                                              m_settings.m_ssl_cipher_list.length());

                T_CLIENT_SET_NCONN_OPT((*l_nconn), nconn_ssl::OPT_SSL_CTX,
                                             m_settings.m_ssl_ctx,
                                             sizeof(m_settings.m_ssl_ctx));

                T_CLIENT_SET_NCONN_OPT((*l_nconn), nconn_ssl::OPT_SSL_VERIFY,
                                              &(m_settings.m_ssl_verify),
                                              sizeof(m_settings.m_ssl_verify));

                //T_CLIENT_SET_NCONN_OPT((*l_nconn), nconn_ssl::OPT_SSL_OPTIONS,
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

        // Find an empty client slot.
        //NDBG_PRINT("m_conn_free_list.size(): %Zu\n", m_conn_free_list.size());
        for (conn_id_list_t::iterator i_conn = m_conn_free_list.begin();
               (i_conn != m_conn_free_list.end()) &&
               (!is_pending_done()) &&
               !m_stopped;
             )
        {

                //NDBG_PRINT("STARTING i_conn: %u\n", *i_conn);

                // Loop trying to get reqlet
                l_reqlet = NULL;
                while(((l_reqlet = try_get_resolved()) == NULL) && (!is_pending_done()));

                if((l_reqlet == NULL) &&
                   is_pending_done())
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
                        //NDBG_PRINT("Destroy nconn and recreate: %u -- l_nconn->m_scheme: %d -- l_reqlet->m_url.m_scheme: %d\n",
                        //                *i_conn,
                        //                l_nconn->m_scheme,
                        //                l_reqlet->m_url.m_scheme);
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
                        m_nconn_vector[*i_conn] = l_nconn;
                }

                // Assign the reqlet for this client
                l_nconn->set_data1(l_reqlet);

                // Bump stats
                ++l_reqlet->m_stat_agg.m_num_conn_started;

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
                                "%s %.500s HTTP/1.1\r\n", m_settings.m_verb.c_str(), l_path_ref.c_str());
        } else {
                l_req_buf_len = snprintf(l_req_buf, l_max_buf_len,
                                "%s / HTTP/1.1\r\n", m_settings.m_verb.c_str());
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

        // -------------------------------------------
        // body
        // -------------------------------------------
        if(m_settings.m_req_body)
        {
                memcpy(l_req_buf + l_req_buf_len, m_settings.m_req_body, m_settings.m_req_body_len);
                l_req_buf_len += m_settings.m_req_body_len;
        }

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

        //NDBG_PRINT("%sCLEANUP%s:\n", ANSI_COLOR_BG_BLUE, ANSI_COLOR_OFF);

        uint64_t l_conn_id = a_nconn->get_id();

        // Cancel last timer
        if(a_cancel_timer)
        {
                m_evr_loop->cancel_timer(&(a_nconn->m_timer_obj));
        }
        a_nconn->reset_stats();
        a_nconn->cleanup(m_evr_loop);

        //NDBG_PRINT("%sADDING_BACK%s: %u\n", ANSI_COLOR_BG_GREEN, ANSI_COLOR_OFF, (uint32_t)l_conn_id);

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
void t_client::get_stats_copy(tag_stat_map_t &ao_tag_stat_map)
{
        // TODO FIX
        // Do we need this function if have a reqlet-repo
#if 0
        // TODO Need to make this threadsafe -spinlock perhaps...
        for(reqlet_list_t::iterator i_reqlet = m_reqlet_avail_list.begin(); i_reqlet != m_reqlet_avail_list.end(); ++i_reqlet)
        {
                ao_tag_stat_map[(*i_reqlet)->get_label()] = (*i_reqlet)->m_stat_agg;
        }
#endif
}

} //namespace ns_hlx {
