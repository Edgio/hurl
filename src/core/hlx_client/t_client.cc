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
#include "ssl_util.h"
#include "time_util.h"
#include "resolver.h"
#include "tinymt64.h"
#include "nconn_tcp.h"
#include "nconn_ssl.h"

#include <unistd.h>

//: ----------------------------------------------------------------------------
//: Macros
//: ----------------------------------------------------------------------------
#define T_CLIENT_CONN_CLEANUP(a_t_client, a_conn, a_http_rx, a_status, a_response, a_error) \
        do { \
                if(a_http_rx)\
                        a_http_rx->set_response(a_status, a_response); \
                if(a_t_client->m_settings.m_show_summary)\
                        a_t_client->append_summary(a_http_rx);\
                ++(a_t_client->m_num_done);\
                if(a_status >= 500) {++(a_t_client->m_num_error);}\
                a_t_client->cleanup_connection(a_conn, true, a_error); \
        }while(0)

#define T_CLIENT_SET_NCONN_OPT(_conn, _opt, _buf, _len) \
        do { \
                int _status = 0; \
                _status = _conn.set_opt((_opt), (_buf), (_len)); \
                if (_status != nconn::NC_STATUS_OK) { \
                        NDBG_PRINT("STATUS_ERROR: Failed to set_opt %d.  Status: %d.\n", _opt, _status); \
                        return STATUS_ERROR;\
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
t_client::t_client(const client_settings_struct_t &a_settings):
        m_t_run_thread(),
        m_settings(),
        m_num_resolved(0),
        m_num_done(0),
        m_num_error(0),
        m_summary_info(),
        m_nconn_pool(a_settings.m_num_parallel),
        m_stopped(false),
        m_num_fetches(-1),
        m_num_fetched(0),
        m_start_time_s(0),
        m_evr_loop(NULL),
        m_rate_delta_us(0),
        m_last_get_req_us(0),
        m_http_rx_vector(),
        m_http_rx_vector_idx(0),
        m_rand_ptr(NULL)
{
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
        COPY_SETTINGS(m_evr_loop_type);
        COPY_SETTINGS(m_num_parallel);
        COPY_SETTINGS(m_timeout_s);
        COPY_SETTINGS(m_rate);
        COPY_SETTINGS(m_request_mode);
        COPY_SETTINGS(m_num_end_fetches);
        COPY_SETTINGS(m_connect_only);
        COPY_SETTINGS(m_save_response);
        COPY_SETTINGS(m_collect_stats);
        COPY_SETTINGS(m_use_persistent_pool);
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

        // Create loop
        m_evr_loop = new evr_loop(evr_loop_file_readable_cb,
                                  evr_loop_file_writeable_cb,
                                  evr_loop_file_error_cb,
                                  m_settings.m_evr_loop_type,
                                  m_settings.m_num_parallel);

}



//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void t_client::reset(void)
{
        m_num_resolved = 0;
        m_num_done = 0;
        m_num_error = 0;
        m_num_fetched = 0;
        m_start_time_s = 0;
        m_last_get_req_us = 0;
        m_http_rx_vector_idx = 0;

        m_summary_info.m_success = 0;
        m_summary_info.m_error_addr = 0;
        m_summary_info.m_error_conn = 0;
        m_summary_info.m_error_unknown = 0;
        m_summary_info.m_ssl_error_self_signed = 0;
        m_summary_info.m_ssl_error_expired = 0;
        m_summary_info.m_ssl_error_other = 0;
        m_summary_info.m_ssl_protocols.clear();
        m_summary_info.m_ssl_ciphers.clear();

        for(http_rx_vector_t::const_iterator i_rx = m_http_rx_vector.begin();
            i_rx != m_http_rx_vector.end();
            ++i_rx)
        {
                (*i_rx)->m_stat_agg.clear();
        }

}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
t_client::~t_client()
{
        for(http_rx_vector_t::const_iterator i_req = m_http_rx_vector.begin();
            i_req != m_http_rx_vector.end();
            ++i_req)
        {
                if(*i_req)
                {
                        delete *i_req;
                }
        }
        m_http_rx_vector.clear();

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
int32_t t_client::append_summary(http_rx *a_http_rx)
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
        if(!a_http_rx)
        {
                return STATUS_ERROR;
        }

        // Get resp
        http_resp *l_resp = a_http_rx->get_http_resp();
        if(!l_resp)
        {
                return STATUS_ERROR;
        }

        uint16_t l_status = l_resp->get_status();
        const std::string &l_body = l_resp->get_body();

        //NDBG_PRINT("TID[%lu]: status: %u\n", pthread_self(), l_status);

        if(l_status == 900)
        {
                ++m_summary_info.m_error_addr;
        }
        else if((l_status == 0) ||
                (l_status == 901) ||
                (l_status == 902))
        {
                // Missing ca
                if(l_body.find("unable to get local issuer certificate") != std::string::npos)
                {
                        ++m_summary_info.m_ssl_error_other;
                }
                // expired
                if(l_body.find("certificate has expired") != std::string::npos)
                {
                        ++m_summary_info.m_ssl_error_expired;
                }
                // expired
                if(l_body.find("self signed certificate") != std::string::npos)
                {
                        ++m_summary_info.m_ssl_error_self_signed;
                }

                ++m_summary_info.m_error_conn;
        }
        else if(l_status == 200)
        {
                ++m_summary_info.m_success;

                header_map_t::iterator i_h;
                if((i_h = l_resp->m_conn_info.find("Protocol")) != l_resp->m_conn_info.end())
                {
                        ++m_summary_info.m_ssl_protocols[i_h->second];
                }
                if((i_h = l_resp->m_conn_info.find("Cipher")) != l_resp->m_conn_info.end())
                {
                        ++m_summary_info.m_ssl_ciphers[i_h->second];
                }
        }
        else
        {
                ++m_summary_info.m_error_unknown;
        }
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
        //NDBG_PRINT("%sSTOP%s:\n", ANSI_COLOR_BG_RED, ANSI_COLOR_OFF);
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
http_rx *t_client::get_rx(void)
{

        // TODO rename m_http_rx_vector_idx to next index and maybe create cur idx val

        http_rx *l_rx = NULL;

        //NDBG_PRINT("%sREQLST%s[%lu]: MODE: %d\n", ANSI_COLOR_FG_CYAN, ANSI_COLOR_OFF, m_http_rx_vector.size(), m_settings.m_request_mode);
        //NDBG_PRINT("m_rate:                     %d\n",  m_settings.m_rate);
        //NDBG_PRINT("m_rate_delta_us:            %lu\n", m_rate_delta_us);
        //NDBG_PRINT("m_reqlet_vector_idx:        %u\n",  m_http_rx_vector_idx);
        //NDBG_PRINT("m_reqlet_avail_list.size(): %d\n",  (int)m_http_rx_vector.size());

        if(0 == m_http_rx_vector.size())
        {
                return NULL;
        }

        // Based on mode
        switch(m_settings.m_request_mode)
        {
        case hlx_client::REQUEST_MODE_ROUND_ROBIN:
        {
                l_rx = m_http_rx_vector[m_http_rx_vector_idx];
                uint32_t l_next_index = m_http_rx_vector_idx + 1;
                if(l_next_index >= m_http_rx_vector.size())
                {
                        l_next_index = 0;
                }
                //NDBG_PRINT("m_next:                     %u\n", l_next_index);
                m_http_rx_vector_idx = l_next_index;
                break;
        }
        case hlx_client::REQUEST_MODE_SEQUENTIAL:
        {
                l_rx = m_http_rx_vector[m_http_rx_vector_idx];
                if(l_rx->is_done())
                {
                        uint32_t l_next_index = ((m_http_rx_vector_idx + 1) >= m_http_rx_vector.size()) ? 0 : m_http_rx_vector_idx + 1;
                        l_rx->reset();
                        m_http_rx_vector_idx = l_next_index;
                }
                break;
        }
        case hlx_client::REQUEST_MODE_RANDOM:
        {
                tinymt64_t *l_rand_ptr = (tinymt64_t*)m_rand_ptr;
                uint32_t l_next_index = (uint32_t)(tinymt64_generate_uint64(l_rand_ptr) % m_http_rx_vector.size());
                m_http_rx_vector_idx = l_next_index;
                l_rx = m_http_rx_vector[m_http_rx_vector_idx];
                break;
        }
        default:
        {
                // Default to round robin
                uint32_t l_next_index = ((m_http_rx_vector_idx + 1) >= m_http_rx_vector.size()) ? 0 : m_http_rx_vector_idx + 1;
                m_http_rx_vector_idx = l_next_index;
                l_rx = m_http_rx_vector[m_http_rx_vector_idx];
                break;
        }
        }


        if(l_rx)
        {
                l_rx->bump_num_requested();
        }

        //NDBG_PRINT("m_reqlet_vector_idx:        %u\n",  m_reqlet_vector_idx);
        //NDBG_PRINT("host:                       %s\n", l_reqlet->m_url.m_host.c_str());

        return l_rx;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t t_client::evr_loop_file_writeable_cb(void *a_data)
{
        //NDBG_PRINT("%sWRITEABLE%s %p\n", ANSI_COLOR_FG_BLUE, ANSI_COLOR_OFF, a_data);
        if(!a_data)
        {
                //NDBG_PRINT("Error a_data == null\n");
                return STATUS_ERROR;
        }

        nconn* l_nconn = static_cast<nconn*>(a_data);
        t_client *l_t_client = g_t_client;
        http_rx *l_http_rx = static_cast<http_rx*>(l_nconn->get_data2());
        if(!l_http_rx)
        {
                NDBG_PRINT("Error no http_rx associated with connection\n");
                return STATUS_ERROR;
        }

        int32_t l_status = STATUS_OK;
        l_status = l_nconn->nc_run_state_machine(l_t_client->m_evr_loop, nconn::NC_MODE_WRITE);
        if(STATUS_ERROR == l_status)
        {
                //if(l_nconn->m_verbose)
                //{
                //        NDBG_PRINT("Error: performing run_state_machine\n");
                //}
                T_CLIENT_CONN_CLEANUP(l_t_client, l_nconn, l_http_rx, 901, l_nconn->m_last_error.c_str(), STATUS_ERROR);
                return STATUS_ERROR;
        }

        // Get request time
        if(!l_nconn->m_request_start_time_us && l_nconn->m_collect_stats_flag)
        {
                l_nconn->m_request_start_time_us = get_time_us();
        }

        if(l_nconn->is_done())
        {
                if(l_t_client->m_settings.m_connect_only)
                {
                        T_CLIENT_CONN_CLEANUP(l_t_client, l_nconn, l_http_rx, 200, "Connected Successfully", STATUS_OK);
                }
                else
                {
                        T_CLIENT_CONN_CLEANUP(l_t_client, l_nconn, l_http_rx, 0, "", STATUS_ERROR);
                }
                return STATUS_OK;
        }


        // Add idle timeout
        l_t_client->m_evr_loop->add_timer( l_t_client->get_timeout_s()*1000, evr_loop_file_timeout_cb, l_nconn, &(l_nconn->m_timer_obj));

        return STATUS_OK;

}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t t_client::evr_loop_file_readable_cb(void *a_data)
{
        //NDBG_PRINT("%sREADABLE%s %p\n", ANSI_COLOR_BG_RED, ANSI_COLOR_OFF, a_data);
        if(!a_data)
        {
                //NDBG_PRINT("Error a_data == null\n");
                return STATUS_ERROR;
        }

        nconn* l_nconn = static_cast<nconn*>(a_data);
        t_client *l_t_client = g_t_client;
        http_rx *l_http_rx = static_cast<http_rx*>(l_nconn->get_data2());
        if(!l_http_rx)
        {
                NDBG_PRINT("Error no http_rx associated with connection\n");
                return STATUS_ERROR;
        }

        // TODO REMOVE
        //NDBG_PRINT("%sREADABLE%s HOST[%s]\n", ANSI_COLOR_BG_RED, ANSI_COLOR_OFF, l_http_rx->m_host.c_str());

        http_resp *l_http_resp = static_cast<http_resp*>(l_nconn->get_data1());
        if(!l_http_rx)
        {
                NDBG_PRINT("Error no http_resp associated with connection\n");
                return STATUS_ERROR;
        }

        bool l_done = l_http_resp->m_complete;
        int32_t l_status = STATUS_OK;
        nconn::mode_t l_mode = nconn::NC_MODE_READ;
        nbq *l_out_q = l_nconn->get_out_q();
        if(l_out_q && l_out_q->read_avail())
        {
                l_mode = nconn::NC_MODE_WRITE;
        }
        l_status = l_nconn->nc_run_state_machine(l_t_client->m_evr_loop, l_mode);
        // TODO Check status???
        //if((STATUS_ERROR == l_status) &&
        //   l_nconn->m_verbose)
        //{
        //        NDBG_PRINT("Error: performing run_state_machine\n");
        //        return STATUS_ERROR;
        //}

        if(l_status >= 0)
        {
                l_http_rx->m_stat_agg.m_num_bytes_read += l_status;
        }

        //NDBG_PRINT("%sREADABLE%s HOST[%s] l_http_resp->m_complete: %d\n", ANSI_COLOR_BG_RED, ANSI_COLOR_OFF, l_http_rx->m_host.c_str(), l_http_resp->m_complete);
        //NDBG_PRINT("%sREADABLE%s HOST[%s] l_nconn->is_done():      %d\n", ANSI_COLOR_BG_RED, ANSI_COLOR_OFF, l_http_rx->m_host.c_str(), l_nconn->is_done());
        //NDBG_PRINT("%sREADABLE%s HOST[%s] l_status:                %d\n", ANSI_COLOR_BG_RED, ANSI_COLOR_OFF, l_http_rx->m_host.c_str(), l_status);

        // Mark as complete
        if(l_nconn->is_done() || (!l_done && l_http_resp->m_complete))
        {
                l_done = true;
        }
        else
        {
                l_done = false;
        }

        //NDBG_PRINT("%sREADABLE%s HOST[%s] l_done:                  %d\n", ANSI_COLOR_BG_RED, ANSI_COLOR_OFF, l_http_rx->m_host.c_str(), l_done);

        // Check for done...
        if((l_done) ||
           (l_status == STATUS_ERROR))
        {
                // Get request time
                if(l_nconn->m_collect_stats_flag)
                {
                        l_nconn->m_stat.m_tt_completion_us = get_delta_time_us(l_nconn->m_request_start_time_us);
                }

                // Add stats
                l_t_client->add_stat_to_agg(l_http_rx->m_stat_agg, l_nconn->get_stats());
                l_nconn->reset_stats();

                // Bump stats
                if(l_status == STATUS_ERROR)
                {
                        ++(l_http_rx->m_stat_agg.m_num_errors);
                        T_CLIENT_CONN_CLEANUP(l_t_client, l_nconn, l_http_rx, 901, l_nconn->m_last_error.c_str(), STATUS_ERROR);
                }
                else
                {
                        l_nconn->bump_num_requested();
                        if(l_t_client->m_settings.m_connect_only)
                        {
                                T_CLIENT_CONN_CLEANUP(l_t_client, l_nconn, l_http_rx, 200, "Connected Successfully", STATUS_OK);
                                return STATUS_OK;
                        }
                        // TODO REMOVE
                        //NDBG_PRINT("CONN %sREUSE%s: l_nconn->can_reuse(): %d\n", ANSI_COLOR_BG_RED, ANSI_COLOR_OFF, l_nconn->can_reuse());
                        //NDBG_PRINT("CONN %sREUSE%s: l_http_rx->is_done(): %d\n", ANSI_COLOR_BG_RED, ANSI_COLOR_OFF, l_http_rx->is_done());
                        if(!l_nconn->can_reuse() ||
                           l_http_rx->is_done())
                        {
                                T_CLIENT_CONN_CLEANUP(l_t_client, l_nconn, l_http_rx, l_http_resp->get_status(), "", STATUS_OK);
                                return STATUS_OK;
                        }

                        if(l_t_client->m_settings.m_show_summary)
                        {
                                l_t_client->append_summary(l_http_rx);
                        }
                        ++(l_t_client->m_num_done);

                        // Cancel last timer
                        l_t_client->m_evr_loop->cancel_timer(&(l_nconn->m_timer_obj));
                        uint64_t l_last_connect_us = l_nconn->m_stat.m_tt_connect_us;
                        l_nconn->reset_stats();
                        l_nconn->m_stat.m_tt_connect_us = l_last_connect_us;

                        // Reduce num pending
                        ++(l_t_client->m_num_fetched);

                        // -------------------------------------------
                        // If not using pool try resend on same
                        // connection.
                        //
                        // NOTE:
                        // This is an optimization meant for load
                        // testing -whereas pool is used if hlx_client
                        // used as client proxy.
                        // -------------------------------------------
                        if(!l_t_client->m_settings.m_use_persistent_pool)
                        {
                                if(!l_t_client->is_pending_done() &&
                                   !l_t_client->m_stopped)
                                {
                                        ++l_http_rx->m_stat_agg.m_num_conn_completed;

                                        l_t_client->limit_rate();

                                        // Get request time
                                        if(l_nconn->m_collect_stats_flag)
                                        {
                                                l_nconn->m_request_start_time_us = get_time_us();
                                        }

                                        // Send request again...
                                        l_status = l_t_client->request(l_http_rx, l_nconn);
                                        if(l_status != STATUS_OK)
                                        {
                                                NDBG_PRINT("Error: performing request\n");
                                                T_CLIENT_CONN_CLEANUP(l_t_client, l_nconn, l_http_rx, 901, "Error performing request", STATUS_ERROR);
                                                return STATUS_ERROR;
                                        }
                                }
                                else
                                {
                                        T_CLIENT_CONN_CLEANUP(l_t_client, l_nconn, l_http_rx, 0, "", STATUS_OK);
                                }

                        }
                        else
                        {
                                l_status = l_t_client->m_nconn_pool.add_idle(l_nconn);
                                if(STATUS_OK != l_status)
                                {
                                        T_CLIENT_CONN_CLEANUP(l_t_client, l_nconn, l_http_rx, 901, "Error setting idle", STATUS_ERROR);
                                        return STATUS_ERROR;
                                }
                        }
                }
                return STATUS_OK;
        }

        // Add idle timeout
        l_t_client->m_evr_loop->add_timer( l_t_client->get_timeout_s()*1000, evr_loop_file_timeout_cb, l_nconn, &(l_nconn->m_timer_obj));

        return STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t t_client::evr_loop_timer_completion_cb(void *a_data)
{
        return STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t t_client::evr_loop_file_error_cb(void *a_data)
{
        //NDBG_PRINT("%sSTATUS_ERRORS%s\n", ANSI_COLOR_FG_RED, ANSI_COLOR_OFF);
        if(!a_data)
        {
                //NDBG_PRINT("Error a_data == null\n");
                return STATUS_ERROR;
        }

         // TODO cleanup???

        return STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t t_client::evr_loop_file_timeout_cb(void *a_data)
{
        //NDBG_PRINT("%sTIMEOUT%s %p\n", ANSI_COLOR_BG_RED, ANSI_COLOR_OFF, a_data);
        if(!a_data)
        {
                //NDBG_PRINT("Error a_data == null\n");
                return STATUS_ERROR;
        }

        nconn* l_nconn = static_cast<nconn*>(a_data);
        t_client *l_t_client = g_t_client;
        http_rx *l_http_rx = static_cast<http_rx*>(l_nconn->get_data2());
        if(!l_http_rx)
        {
                NDBG_PRINT("Error no http_rx associated with connection\n");
                return STATUS_ERROR;
        }

        //printf("%sT_O%s: %p\n",ANSI_COLOR_FG_BLUE, ANSI_COLOR_OFF,
        //                l_rconn->m_timer_obj);

        l_t_client->add_stat_to_agg(l_http_rx->m_stat_agg, l_nconn->get_stats());

        if(l_t_client->m_settings.m_verbose)
        {
                // TODO FIX!!!
                //NDBG_PRINT("%sTIMING OUT CONN%s: i_conn: %lu HOST: %s THIS: %p\n",
                //                ANSI_COLOR_BG_RED, ANSI_COLOR_OFF,
                //                l_nconn->get_id(),
                //                l_reqlet->m_url.m_host.c_str(),
                //                l_t_client);
        }

        // Stats
        ++l_t_client->m_num_fetched;
        ++l_http_rx->m_stat_agg.m_num_idle_killed;

        // Cleanup
        T_CLIENT_CONN_CLEANUP(l_t_client, l_nconn, l_http_rx, 902, "Connection timed out", STATUS_ERROR);
        return STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t t_client::evr_loop_timer_cb(void *a_data)
{
        //NDBG_PRINT("%sTIMER%s %p\n", ANSI_COLOR_FG_RED, ANSI_COLOR_OFF, a_data);
        return STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void *t_client::t_run(void *a_nothing)
{

        reset();

        m_stopped = false;

        // Set thread local
        g_t_client = this;

        // Set start time
        m_start_time_s = get_time_s();

        // -------------------------------------------
        // Resolve hostnames...
        // -------------------------------------------
        if(m_settings.m_verbose)
        {
                NDBG_OUTPUT("thread[%lu] resolving hostnames...\n", pthread_self());
        }
        for(http_rx_vector_t::iterator i_rx = m_http_rx_vector.begin();
            i_rx != m_http_rx_vector.end() && !m_stopped;
            ++i_rx)
        {
                int32_t l_status;
                if(m_settings.m_verbose)
                {
                        NDBG_OUTPUT("Resolving host: %s\n", (*i_rx)->m_host.c_str());
                }
                if((m_settings.m_resolver == NULL) ||
                   (l_status = (*i_rx)->resolve(*m_settings.m_resolver))!= STATUS_OK)
                {
                        if(m_settings.m_verbose)
                        {
                                NDBG_PRINT("Error resolving http_rx host: %s\n", (*i_rx)->m_host.c_str());
                        }
                        ++m_num_error;
                        ++m_summary_info.m_error_addr;
                        append_summary((*i_rx));
                }

                ++m_num_resolved;
                ++((*i_rx)->m_stat_agg.m_num_resolved);

        }
        if(m_settings.m_verbose)
        {
                NDBG_OUTPUT("thread[%lu] Done resolving hostnames...\n", pthread_self());
        }

        // TODO Make resolve and quit option???
        //m_stopped = true;
        //return NULL;

        // -------------------------------------------
        // Main loop.
        // -------------------------------------------
        while(!m_stopped &&
              !is_pending_done())
        {
                // -------------------------------------------
                // Start Connections
                // -------------------------------------------
                //NDBG_PRINT("%sSTART_CONNECTIONS%s\n", ANSI_COLOR_BG_MAGENTA, ANSI_COLOR_OFF);
                int32_t l_status;
                l_status = start_connections();
                if(l_status != STATUS_OK)
                {
                        NDBG_PRINT("%sSTART_CONNECTIONS%s ERROR!\n", ANSI_COLOR_BG_RED, ANSI_COLOR_OFF);
                        return NULL;
                }

                // Run loop
                l_status = m_evr_loop->run();
                if(l_status != STATUS_OK)
                {
                        //NDBG_PRINT("%sm_evr_loop->run%s ERROR!\n", ANSI_COLOR_BG_RED, ANSI_COLOR_OFF);
                        // TODO Log error and continue???
                        //return NULL;
                }

        }
        //NDBG_PRINT("%sFINISHING_CONNECTIONS%s -done: %d -- m_stopped: %d m_num_fetched: %d + pending: %d/ m_num_fetches: %d\n",
        //                ANSI_COLOR_BG_MAGENTA, ANSI_COLOR_OFF,
        //                is_pending_done(), m_stopped,
        //                (int)m_num_fetched, (int) m_nconn_pool.num_in_use(), (int)m_num_fetches);

        // Still awaiting responses -wait...
        uint64_t l_cur_time = get_time_s();
        uint64_t l_end_time = l_cur_time + m_settings.m_timeout_s;
        while(!m_stopped &&
              (m_nconn_pool.num_in_use() > 0) &&
              (l_cur_time < l_end_time))
        {
                // Run loop
                //NDBG_PRINT("waiting: m_num_pending: %d --time-left: %d\n", (int)m_nconn_pool.num_in_use(), int(l_end_time - l_cur_time));
                m_evr_loop->run();

                // TODO -this is pretty hard polling -make option???
                usleep(10000);
                l_cur_time = get_time_s();

        }
        //NDBG_PRINT("%sDONE_CONNECTIONS%s\n", ANSI_COLOR_BG_YELLOW, ANSI_COLOR_OFF);
        //NDBG_PRINT("waiting: m_num_pending: %d --time-left: %d\n", (int)m_nconn_pool.num_in_use(), int(l_end_time - l_cur_time));
        m_stopped = true;
        return NULL;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t t_client::request(http_rx *a_http_rx, nconn *a_nconn)
{
        nconn *l_nconn = a_nconn;
        int32_t l_status;
        //NDBG_PRINT("TID[%lu]: Making request: Host: %s a_nconn: %p\n", pthread_self(), a_http_rx->m_host.c_str(), a_nconn);
        if(!l_nconn)
        {
                if(m_settings.m_use_persistent_pool)
                {
                        l_status = m_nconn_pool.get_try_idle(a_http_rx->m_host,
                                                             a_http_rx->m_scheme,
                                                             nconn::TYPE_CLIENT,
                                                             &l_nconn);
                }
                else
                {
                        l_status = m_nconn_pool.get(a_http_rx->m_scheme,
                                                    nconn::TYPE_CLIENT,
                                                    &l_nconn);
                }
                if(l_status == nconn::NC_STATUS_AGAIN)
                {
                        // Out of connections -try again later...
                        return STATUS_OK;
                }
                else if(!l_nconn ||
                        (l_status != nconn::NC_STATUS_OK))
                {
                        NDBG_PRINT("Error l_nconn == NULL\n");
                        return STATUS_ERROR;
                }

                // Configure connection
                l_status = config_conn(l_nconn);
                if(l_status != STATUS_OK)
                {
                        NDBG_PRINT("Error performing config_conn\n");
                        return STATUS_ERROR;
                }

                l_nconn->set_host_info(a_http_rx->m_host_info);
        }

        //NDBG_PRINT("TID[%lu]: %sGET_CONNECTION%s: Host: %s l_nconn: %p\n",
        //                pthread_self(),
        //                ANSI_COLOR_BG_BLUE, ANSI_COLOR_OFF,
        //                a_http_rx->m_host.c_str(),
        //                l_nconn);

        http_resp *l_resp = static_cast<http_resp *>(l_nconn->get_data1());
        if(!l_nconn->get_data1())
        {
                l_resp = new http_resp();
                l_nconn->set_data1(l_resp);
        }
        else
        {
                l_resp->clear();
        }

        // Set resp object
        a_http_rx->set_http_resp(l_resp);

        l_resp->m_complete = false;

        l_nconn->set_data2(a_http_rx);

        // Assign the request for connection
        l_nconn->set_host(a_http_rx->m_host);

        ++a_http_rx->m_stat_agg.m_num_conn_started;

        // Create request
        if(!m_settings.m_connect_only)
        {

                // -----------------------------------------------------------
                // TODO create q pool... -this is gonna leak...
                // -----------------------------------------------------------
                nbq *l_out_q = l_nconn->get_out_q();

                bool l_changed = false;
                if(a_http_rx->m_multipath || !l_out_q)
                {
                        l_changed = true;
                }

                nbq *l_in_q = l_nconn->get_in_q();
                // In queue???
                if(!l_in_q)
                {
                        l_in_q = new nbq(16384);
                        l_nconn->set_in_q(l_in_q);
                }
                else
                {
                        // Reset in data
                        l_in_q->reset_write();
                }

                if(!l_changed && l_out_q)
                {
                        //NDBG_PRINT("Bailing already set to: %u\n", l_req_buf_len);
                        l_nconn->get_out_q()->reset_read();
                }

                if(!l_out_q)
                {
                        l_out_q = new nbq(16384);
                        l_nconn->set_out_q(l_out_q);
                }
                if(l_changed)
                {

                        // Reset in data
                        l_out_q->reset_write();
                        //NDBG_PRINT("%sCREATE_REQUEST%s: l_nconn[%p] l_out_q[%p] \n",
                        //                ANSI_COLOR_BG_WHITE, ANSI_COLOR_OFF, l_nconn, l_out_q);
                        l_status = create_request(*l_out_q, *a_http_rx);
                        if(STATUS_OK != l_status)
                        {
                                return STATUS_ERROR;
                        }
                }
        }

        // TODO Make configurable
        l_status = m_evr_loop->add_timer(m_settings.m_timeout_s*1000, evr_loop_file_timeout_cb, l_nconn, &(l_nconn->m_timer_obj));
        if(l_status != STATUS_OK)
        {
                NDBG_PRINT("Error: Performing add_timer\n");
                return STATUS_ERROR;
        }

        //NDBG_PRINT("%sCONNECT%s: %s --data: %p\n", ANSI_COLOR_BG_MAGENTA, ANSI_COLOR_OFF, a_http_rx->m_host.c_str(), l_nconn->get_data1());
        l_status = l_nconn->nc_run_state_machine(m_evr_loop, nconn::NC_MODE_WRITE);
        if(l_status == STATUS_ERROR)
        {
                NDBG_PRINT("Error: Performing nc_run_state_machine\n");
                T_CLIENT_CONN_CLEANUP(this, l_nconn, a_http_rx, 500, "Performing nc_run_state_machine", STATUS_ERROR);
                return STATUS_OK;
        }

        return STATUS_OK;

}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t t_client::start_connections(void)
{

        //NDBG_PRINT("start_connections\n");
        //NDBG_PRINT("m_num_done:                %d\n", (int)m_num_done);
        //NDBG_PRINT("m_num_pending:             %d\n", (int)m_nconn_pool.num_in_use());
        //NDBG_PRINT("m_settings.m_num_parallel: %d\n", (int)m_settings.m_num_parallel);
        //NDBG_PRINT("m_stopped:                 %d\n", (int)m_stopped);
        //NDBG_PRINT("is_pending_done():         %d\n", (int)is_pending_done());
        while((m_nconn_pool.num_in_use() < (uint32_t)m_settings.m_num_parallel) &&
              !m_stopped &&
              (!is_pending_done()))
        {
                //NDBG_PRINT("try_get_resolved\n");
                http_rx *l_rx = get_rx();
                if(!l_rx)
                {
                        //NDBG_PRINT("%scontinue%s\n", ANSI_COLOR_BG_WHITE, ANSI_COLOR_OFF);
                        continue;
                }

                // Only run on resolved
                if(!l_rx->is_resolved())
                {
                        //NDBG_PRINT("%scontinue%s\n", ANSI_COLOR_BG_WHITE, ANSI_COLOR_OFF);
                        continue;
                }

                // TODO REMOVE
                //NDBG_PRINT("TID[%lu]: m_num_pending: %d m_settings.m_num_parallel: %d m_stopped: %d is_pending_done: %d\n",
                //                pthread_self(),
                //                m_nconn_pool.num_in_use(),
                //                m_settings.m_num_parallel,
                //                m_stopped,
                //                is_pending_done());
                int32_t l_status = request(l_rx);
                if(l_status != STATUS_OK)
                {
                        //NDBG_PRINT("%serror%s\n", ANSI_COLOR_BG_RED, ANSI_COLOR_OFF);
                        return STATUS_ERROR;
                }

                limit_rate();

        }
        return STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t t_client::create_request(nbq &a_q, http_rx &a_http_rx)
{
        // -------------------------------------------
        // Request.
        // -------------------------------------------
        std::string l_path_ref = a_http_rx.get_path(m_rand_ptr);
        char l_buf[2048];
        int32_t l_len = 0;
        if(l_path_ref.empty())
        {
                l_path_ref = "/";
        }
        if(!(a_http_rx.m_query.empty()))
        {
                l_path_ref += "?";
                l_path_ref += a_http_rx.m_query;
        }
        //NDBG_PRINT("HOST: %s PATH: %s\n", a_reqlet.m_url.m_host.c_str(), l_path_ref.c_str());
        l_len = snprintf(l_buf, sizeof(l_buf),
                        "%s %.500s HTTP/1.1\r\n", m_settings.m_verb.c_str(), l_path_ref.c_str());

        //NDBG_PRINT("Write: buf: %p len: %d\n", l_buf, l_len);
        a_q.write(l_buf, l_len);

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
                        l_len = snprintf(l_buf, sizeof(l_buf),
                                         "%s: %s\r\n",
                                         i_header->first.c_str(), i_header->second.c_str());
                        //NDBG_PRINT("Write: buf: %p len: %d\n", l_buf, l_len);
                        a_q.write(l_buf, l_len);
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
                l_len = snprintf(l_buf, sizeof(l_buf),
                                 "Host: %s\r\n", a_http_rx.m_host.c_str());
                //NDBG_PRINT("Write: buf: %p len: %d\n", l_buf, l_len);
                a_q.write(l_buf, l_len);
        }

        // -------------------------------------------
        // End of request terminator...
        // -------------------------------------------
        l_len = snprintf(l_buf, sizeof(l_buf), "\r\n");
        //NDBG_PRINT("Write: buf: %p len: %d\n", l_buf, l_len);
        a_q.write(l_buf, l_len);

        // -------------------------------------------
        // body
        // -------------------------------------------
        if(m_settings.m_req_body)
        {
                //NDBG_PRINT("Write: buf: %p len: %d\n", l_buf, l_len);
                a_q.write(m_settings.m_req_body, m_settings.m_req_body_len);
        }

        return STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t t_client::cleanup_connection(nconn *a_nconn, bool a_cancel_timer, int32_t a_status)
{

        //NDBG_PRINT("%sCLEANUP%s: PTR: %p\n", ANSI_COLOR_BG_BLUE, ANSI_COLOR_OFF, a_nconn);
        //NDBG_PRINT_BT();

        // Cancel last timer
        if(a_cancel_timer)
        {
                m_evr_loop->cancel_timer(&(a_nconn->m_timer_obj));
        }

        // Reduce num pending
        ++m_num_fetched;

        //NDBG_PRINT("%sCLEANUP%s: m_num_fetched: %d m_num_pending: %d\n", ANSI_COLOR_BG_BLUE, ANSI_COLOR_OFF, (int)m_num_fetched, (int)m_nconn_pool.num_in_use());
        //NDBG_PRINT("%sADDING_BACK%s: PTR: %p %u\n", ANSI_COLOR_BG_GREEN, ANSI_COLOR_OFF, a_nconn, (uint32_t)a_nconn->get_id());

        // Add back to free list
        if(STATUS_OK != m_nconn_pool.release(a_nconn))
        {
                NDBG_PRINT("%sCLEANUP_ERROR%s: PTR: %p\n", ANSI_COLOR_BG_RED, ANSI_COLOR_OFF, a_nconn);
                return STATUS_ERROR;
        }

        return STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t t_client::set_http_requests(http_rx_vector_t a_http_rx_vector)
{

        // Remove any existing...
        for(http_rx_vector_t::iterator i_req = m_http_rx_vector.begin();
            i_req != m_http_rx_vector.end();
            ++i_req)
        {
                if(*i_req)
                {
                        delete *i_req;
                        *i_req = NULL;
                }
        }
        m_http_rx_vector.clear();

        uint32_t i_id = 0;
        for(http_rx_vector_t::iterator i_req = a_http_rx_vector.begin();
            i_req != a_http_rx_vector.end();
            ++i_req)
        {
                http_rx *l_rx = new http_rx(**i_req);
                l_rx->m_idx = i_id++;
                m_http_rx_vector.push_back(l_rx);
        }

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
void t_client::get_stats_copy(hlx_client::tag_stat_map_t &ao_tag_stat_map)
{
        // TODO Make threadsafe...
        for(http_rx_vector_t::iterator i_rx = m_http_rx_vector.begin(); i_rx != m_http_rx_vector.end(); ++i_rx)
        {
                ao_tag_stat_map[(*i_rx)->get_label()] = (*i_rx)->m_stat_agg;
        }
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void t_client::add_stat_to_agg(hlx_client::t_stat_t &ao_stat_agg, const req_stat_t &a_req_stat)
{
        update_stat(ao_stat_agg.m_stat_us_connect, a_req_stat.m_tt_connect_us);
        update_stat(ao_stat_agg.m_stat_us_first_response, a_req_stat.m_tt_first_read_us);
        update_stat(ao_stat_agg.m_stat_us_end_to_end, a_req_stat.m_tt_completion_us);

        // Totals
        ++ao_stat_agg.m_total_reqs;
        ao_stat_agg.m_total_bytes += a_req_stat.m_total_bytes;

        // Status code
        //NDBG_PRINT("%sSTATUS_CODE%s: %d\n", ANSI_COLOR_BG_GREEN, ANSI_COLOR_OFF, a_req_stat.m_status_code);
        ++ao_stat_agg.m_status_code_count_map[a_req_stat.m_status_code];

}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t t_client::config_conn(nconn *a_nconn)
{
        a_nconn->set_num_reqs_per_conn(m_settings.m_num_reqs_per_conn);
        a_nconn->set_save_response(m_settings.m_save_response);
        a_nconn->set_collect_stats(m_settings.m_collect_stats);
        a_nconn->set_connect_only(m_settings.m_connect_only);

        // -------------------------------------------
        // Set options
        // -------------------------------------------
        // Set generic options
        T_CLIENT_SET_NCONN_OPT((*a_nconn), nconn_tcp::OPT_TCP_RECV_BUF_SIZE, NULL, m_settings.m_sock_opt_recv_buf_size);
        T_CLIENT_SET_NCONN_OPT((*a_nconn), nconn_tcp::OPT_TCP_SEND_BUF_SIZE, NULL, m_settings.m_sock_opt_send_buf_size);
        T_CLIENT_SET_NCONN_OPT((*a_nconn), nconn_tcp::OPT_TCP_NO_DELAY, NULL, m_settings.m_sock_opt_no_delay);

        // Set ssl options
        if(a_nconn->m_scheme == nconn::SCHEME_SSL)
        {
                T_CLIENT_SET_NCONN_OPT((*a_nconn),
                                       nconn_ssl::OPT_SSL_CIPHER_STR,
                                       m_settings.m_ssl_cipher_list.c_str(),
                                       m_settings.m_ssl_cipher_list.length());

                T_CLIENT_SET_NCONN_OPT((*a_nconn),
                                       nconn_ssl::OPT_SSL_CTX,
                                       m_settings.m_ssl_ctx,
                                       sizeof(m_settings.m_ssl_ctx));

                T_CLIENT_SET_NCONN_OPT((*a_nconn),
                                       nconn_ssl::OPT_SSL_VERIFY,
                                       &(m_settings.m_ssl_verify),
                                       sizeof(m_settings.m_ssl_verify));

                //T_CLIENT_SET_NCONN_OPT((*l_nconn), nconn_ssl::OPT_SSL_OPTIONS,
                //                              &(a_settings.m_ssl_options),
                //                              sizeof(a_settings.m_ssl_options));

                if(!m_settings.m_tls_crt.empty())
                {
                        T_CLIENT_SET_NCONN_OPT((*a_nconn),
                                               nconn_ssl::OPT_SSL_TLS_CRT,
                                               m_settings.m_tls_crt.c_str(),
                                               m_settings.m_tls_crt.length());
                }
                if(!m_settings.m_tls_key.empty())
                {
                        T_CLIENT_SET_NCONN_OPT((*a_nconn),
                                               nconn_ssl::OPT_SSL_TLS_KEY,
                                               m_settings.m_tls_key.c_str(),
                                               m_settings.m_tls_key.length());
                }
        }
        return STATUS_OK;
}

} //namespace ns_hlx {
