//: ----------------------------------------------------------------------------
//: Copyright (C) 2014 Verizon.  All Rights Reserved.
//: All Rights Reserved
//:
//: \file:    t_hlx.cc
//: \details: TODO
//: \author:  Reed P. Morrison
//: \date:    10/05/2015
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
#include "ndebug.h"
#include "nbq.h"
#include "nconn_tcp.h"
#include "nconn_ssl.h"
#include "time_util.h"
#include "url_router.h"
#include "http_data.h"

#include <unistd.h>
#include <errno.h>
#include <string.h>
#include "t_hlx.h"

namespace ns_hlx {

//: ----------------------------------------------------------------------------
//: Macros
//: ----------------------------------------------------------------------------
#define T_HTTP_PROXY_SET_NCONN_OPT(_conn, _opt, _buf, _len) \
        do { \
                int _status = 0; \
                _status = _conn.set_opt((_opt), (_buf), (_len)); \
                if (_status != nconn::NC_STATUS_OK) { \
                        NDBG_PRINT("STATUS_ERROR: Failed to set_opt %d.  Status: %d.\n", _opt, _status); \
                        return STATUS_ERROR;\
                } \
        } while(0)

#define CHECK_FOR_NULL_ERROR_DEBUG(_data) \
        do {\
                if(!_data) {\
                        NDBG_PRINT("Error.\n");\
                        return STATUS_ERROR;\
                }\
        } while(0);

#define CHECK_FOR_NULL_ERROR(_data) \
        do {\
                if(!_data) {\
                        return STATUS_ERROR;\
                }\
        } while(0);

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
#define COPY_SETTINGS(_field) m_conf._field = a_t_hlx_conf._field
t_hlx::t_hlx(const t_hlx_conf_t &a_t_hlx_conf):
        m_t_run_thread(),
        m_conf(),
        m_summary_info(),
        m_nconn_pool(-1),
        m_nconn_proxy_pool(a_t_hlx_conf.m_num_parallel),
        m_stopped(false),
        m_start_time_s(0),
        m_evr_loop(NULL),
        m_scheme(SCHEME_TCP),
        m_listening_nconn_list(),
        m_subreq_queue(),
        m_default_handler(),
        m_http_data_pool(),
        m_stat(),
        m_subreq_q_fd(-1),
        m_subreq_q_nconn(NULL),
        m_is_initd(false)
{
        // Friggin effc++
        COPY_SETTINGS(m_verbose);
        COPY_SETTINGS(m_color);
        COPY_SETTINGS(m_evr_loop_type);
        COPY_SETTINGS(m_num_parallel);
        COPY_SETTINGS(m_show_summary);
        COPY_SETTINGS(m_timeout_s);
        COPY_SETTINGS(m_num_reqs_per_conn);
        COPY_SETTINGS(m_collect_stats);
        COPY_SETTINGS(m_use_persistent_pool);
        COPY_SETTINGS(m_sock_opt_recv_buf_size);
        COPY_SETTINGS(m_sock_opt_send_buf_size);
        COPY_SETTINGS(m_sock_opt_no_delay);
        COPY_SETTINGS(m_tls_key);
        COPY_SETTINGS(m_tls_crt);
        COPY_SETTINGS(m_ssl_server_ctx);
        COPY_SETTINGS(m_ssl_cipher_list);
        COPY_SETTINGS(m_ssl_options_str);
        COPY_SETTINGS(m_ssl_options);
        COPY_SETTINGS(m_ssl_ca_file);
        COPY_SETTINGS(m_ssl_ca_path);
        COPY_SETTINGS(m_ssl_client_ctx);
        COPY_SETTINGS(m_resolver);
        COPY_SETTINGS(m_hlx);
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
t_hlx::~t_hlx()
{
        if(m_evr_loop)
        {
                delete m_evr_loop;
        }

        for(listening_nconn_list_t::iterator i_conn = m_listening_nconn_list.begin();
                        i_conn != m_listening_nconn_list.end();
                        ++i_conn)
        {
                if(*i_conn)
                {
                        delete *i_conn;
                        *i_conn = NULL;
                }
        }
        m_listening_nconn_list.clear();
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t t_hlx::init(void)
{
        if(m_is_initd)
        {
                return STATUS_OK;
        }

        // Create loop
        m_evr_loop = new evr_loop(evr_loop_file_readable_cb,
                                  evr_loop_file_writeable_cb,
                                  evr_loop_file_error_cb,
                                  m_conf.m_evr_loop_type,
                                  // TODO Need to make epoll vector resizeable...
                                  512,
                                  false);

        // ---------------------------------------
        // Subrequest support
        // Fake nconn -for subreq notifications
        // ---------------------------------------
        m_subreq_q_nconn = m_nconn_pool.create_conn(SCHEME_TCP);
        m_subreq_q_nconn->set_idx(sc_subreq_q_conn_idx);
        m_subreq_q_fd = m_evr_loop->add_event(m_subreq_q_nconn);
        if(m_subreq_q_fd == STATUS_ERROR)
        {
                NDBG_PRINT("Error performing m_evr_loop->add_event\n");
                return STATUS_ERROR;
        }

        m_is_initd = true;
        return STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t t_hlx::add_listener(listener &a_listener)
{
        int32_t l_status;
        l_status = init();
        if(l_status != STATUS_OK)
        {
                NDBG_PRINT("Error performing init.\n");
                return STATUS_ERROR;
        }
        nconn *l_nconn = NULL;
        l_nconn = m_nconn_pool.create_conn(a_listener.get_scheme());
        l_status = config_conn(*l_nconn, a_listener.get_url_router(), HTTP_DATA_TYPE_SERVER, false, false);
        if(l_status != STATUS_OK)
        {
                if(l_nconn)
                {
                        delete l_nconn;
                        l_nconn = NULL;
                }
                NDBG_PRINT("Error: performing config_conn\n");
                return STATUS_ERROR;
        }
        l_status = config_data(*l_nconn, a_listener.get_url_router(), HTTP_DATA_TYPE_SERVER, false, false);
        if(l_status != STATUS_OK)
        {
                if(l_nconn)
                {
                        delete l_nconn;
                        l_nconn = NULL;
                }
                NDBG_PRINT("Error: performing config_conn\n");
                return STATUS_ERROR;
        }
        l_status = l_nconn->nc_set_listening(m_evr_loop, a_listener.get_fd());
        if(l_status != STATUS_OK)
        {
                if(l_nconn)
                {
                        delete l_nconn;
                        l_nconn = NULL;
                }
                NDBG_PRINT("Error performing nc_set_listening.\n");
                return STATUS_ERROR;
        }
        m_listening_nconn_list.push_back(l_nconn);
        return STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t t_hlx::add_subreq(subreq &a_subreq)
{
        //NDBG_PRINT("Adding subreq.\n");
        m_subreq_queue.push(&a_subreq);
        if(m_evr_loop)
        {
                m_evr_loop->signal_control();
        }
        return STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
nconn *t_hlx::get_proxy_conn(subreq &a_subreq)
{
        int32_t l_status;
        nconn *l_nconn = NULL;
        //NDBG_PRINT("a_subreq.m_keepalive: %d\n", a_subreq.m_keepalive);
        //NDBG_PRINT("m_idle size: %u\n", (int)m_nconn_proxy_pool.num_idle());
        l_nconn = m_nconn_proxy_pool.get_idle(a_subreq.m_host,
                                               a_subreq.m_scheme);
        if(!l_nconn)
        {
                // Try get a new connection
                l_nconn = m_nconn_proxy_pool.get(a_subreq.m_scheme);
                if(!l_nconn)
                {
                        return NULL;
                }
                // Configure connection
                l_status = config_conn(*l_nconn,
                                       NULL,
                                       HTTP_DATA_TYPE_CLIENT,
                                       a_subreq.m_save_response,
                                       a_subreq.m_connect_only);
                if(l_status != STATUS_OK)
                {
                        m_nconn_proxy_pool.release(l_nconn);
                        NDBG_PRINT("Error performing config_conn\n");
                        return NULL;
                }
                l_nconn->set_num_reqs_per_conn(a_subreq.get_num_reqs_per_conn());
                l_nconn->set_host_info(a_subreq.m_host_info);
        }

        // Configure data
        l_status = config_data(*l_nconn,
                               NULL,
                               HTTP_DATA_TYPE_CLIENT,
                               a_subreq.m_save_response,
                               a_subreq.m_connect_only);
        if(l_status != STATUS_OK)
        {
                m_nconn_proxy_pool.release(l_nconn);
                NDBG_PRINT("Error performing config_conn\n");
                return NULL;
        }
        return l_nconn;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t t_hlx::start_subrequest(subreq &a_subreq, nconn &a_nconn)
{
        int32_t l_status;
        //NDBG_PRINT("TID[%lu]: %sGET_CONNECTION%s: Host: %s l_nconn: %p\n",
        //                pthread_self(),
        //                ANSI_COLOR_BG_BLUE, ANSI_COLOR_OFF,
        //                a_subreq.m_host.c_str(),
        //                &a_nconn);
        http_data_t *l_data = static_cast<http_data_t *>(a_nconn.get_data());
        if(!l_data)
        {
                NDBG_PRINT("Error nconn data == NULL\n");
                return STATUS_ERROR;
        }

        l_data->m_http_resp.clear();

        // Set resp object
        a_subreq.set_resp(&(l_data->m_http_resp));

        // Assign the request for connection
        a_nconn.set_host(a_subreq.m_host);

        ++m_stat.m_num_conn_started;

        // Create request
        if(!a_subreq.m_connect_only)
        {
                // -----------------------------------------------------------
                // TODO create q pool... -this is gonna leak...
                // -----------------------------------------------------------
                if(!a_nconn.get_in_q())
                {
                        a_nconn.set_in_q(new nbq(16384));
                }
                else
                {
                        // Reset in data
                        a_nconn.get_in_q()->reset_write();
                }
                l_data->m_http_resp.set_q(a_nconn.get_in_q());

                if(!a_nconn.get_out_q())
                {
                        a_nconn.set_out_q(new nbq(16384));
                        l_data->m_http_req.set_q(a_nconn.get_out_q());
                        l_status = a_subreq.m_create_req_cb(a_subreq, l_data->m_http_req);
                        if(STATUS_OK != l_status)
                        {
                                return STATUS_ERROR;
                        }
                }
                else
                {
                        l_data->m_http_req.set_q(a_nconn.get_out_q());
                        if(a_subreq.m_multipath)
                        {
                                // Reset in data
                                a_nconn.get_out_q()->reset_write();
                                //NDBG_PRINT("%sCREATE_REQUEST%s: l_nconn[%p] l_out_q[%p] \n",
                                //                ANSI_COLOR_BG_WHITE, ANSI_COLOR_OFF, l_nconn, l_out_q);
                                l_status = a_subreq.m_create_req_cb(a_subreq, l_data->m_http_req);
                                if(STATUS_OK != l_status)
                                {
                                        return STATUS_ERROR;
                                }
                        }
                        else
                        {
                                a_nconn.get_out_q()->reset_read();
                        }
                }

                // Display data from out q
                if(m_conf.m_verbose)
                {
                        if(m_conf.m_color) NDBG_OUTPUT("%s", ANSI_COLOR_FG_YELLOW);
                        a_nconn.get_out_q()->print();
                        if(m_conf.m_color) NDBG_OUTPUT("%s", ANSI_COLOR_OFF);
                }
        }

        // Set subreq
        l_data->m_http_req.m_subreq = &a_subreq;

        // TODO Make configurable
        l_status = m_evr_loop->add_timer(a_subreq.m_timeout_s*1000, evr_loop_file_timeout_cb, &a_nconn, &(l_data->m_timer_obj));
        if(l_status != STATUS_OK)
        {
                NDBG_PRINT("Error: Performing add_timer\n");
                return STATUS_ERROR;
        }

        //NDBG_PRINT("g_client_req_num: %d\n", ++g_client_req_num);
        //NDBG_PRINT("%sCONNECT%s: %s --data: %p\n", ANSI_COLOR_BG_MAGENTA, ANSI_COLOR_OFF, a_subreq.m_host.c_str(), a_nconn.get_data());
        l_status = a_nconn.nc_run_state_machine(m_evr_loop, nconn::NC_MODE_WRITE);
        a_nconn.bump_num_requested();
        if(l_status == STATUS_ERROR)
        {
                //NDBG_PRINT("Error: Performing nc_run_state_machine. l_status: %d\n", l_status);
                //++(m_num_error);
                //++(m_num_done);
                a_subreq.set_response(500, "Performing nc_run_state_machine");
                //if(m_settings.m_show_summary) append_summary(a_subreq.get_http_resp());
                cleanup_connection(a_nconn, l_data->m_timer_obj, l_data->m_type);
                return STATUS_OK;
        }
        return STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
nconn *t_hlx::get_new_client_conn(int a_fd, scheme_t a_scheme, url_router *a_url_router)
{
        nconn *l_nconn;
        l_nconn = m_nconn_pool.get(a_scheme);
        if(!l_nconn)
        {
                NDBG_PRINT("Error: performing m_nconn_pool.get\n");
                return NULL;
        }

        //NDBG_PRINT("%sGET_NEW%s: %u a_nconn: %p\n", ANSI_COLOR_BG_BLUE, ANSI_COLOR_OFF, (uint32_t)l_nconn->get_idx(), l_nconn);

        // Config
        int32_t l_status;
        l_status = config_conn(*l_nconn, a_url_router, HTTP_DATA_TYPE_SERVER, true, false);
        if(l_status != STATUS_OK)
        {
                NDBG_PRINT("Error: performing config_conn\n");
                return NULL;
        }
        l_status = config_data(*l_nconn, a_url_router, HTTP_DATA_TYPE_SERVER, true, false);
        if(l_status != STATUS_OK)
        {
                NDBG_PRINT("Error: performing config_conn\n");
                return NULL;
        }

        ++m_stat.m_cur_conn_count;
        ++m_stat.m_num_conn_started;

        // Add output queue for response
        // TODO create q pool... -this is gonna leak...
        nbq *l_out_q = new nbq(16384);
        l_nconn->set_out_q(l_out_q);
        http_data_t *l_nconn_data = static_cast<http_data_t *>(l_nconn->get_data());
        l_nconn_data->m_http_resp.set_q(l_out_q);

        // Setup input q
        if(l_nconn->get_in_q())
        {
                l_nconn->get_in_q()->reset();
        }
        else
        {
                l_nconn->set_in_q(new nbq(16384));
        }

        return l_nconn;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int t_hlx::run(void)
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
void t_hlx::stop(void)
{
        // Cleanup server connection
        //cleanup_connection(m_listening_nconn, NULL, 200);
        m_stopped = true;
        int32_t l_status;
        l_status = m_evr_loop->signal_control();
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
void t_hlx::get_stats_copy(t_stat_t &ao_stat)
{
        ao_stat = m_stat;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
bool t_hlx::subreq_complete(subreq &a_subreq, nconn &a_nconn)
{
        bool l_complete = false;
        a_subreq.bump_num_completed();
        // Call completion handler
        if(a_subreq.m_completion_cb)
        {
                a_subreq.m_completion_cb(a_nconn,
                                         a_subreq,
                                         *(a_subreq.get_resp()));
                // Check for req resp data
                nconn *l_req_conn = a_subreq.get_req_conn();
                if(l_req_conn && l_req_conn->get_out_q() && l_req_conn->get_out_q()->read_avail())
                {
                        int32_t l_status = evr_loop_file_writeable_cb(l_req_conn);
                        if(l_status != STATUS_OK)
                        {
                                NDBG_PRINT("Error: performing evr_loop_file_writeable_cb\n");
                                l_complete = true;
                        }
                }
        }
        // Connect only
        if(a_subreq.m_connect_only)
        {
                a_subreq.set_response(200, "Connected Successfully");
                if(m_conf.m_show_summary)
                {
                        append_summary(&a_nconn, a_subreq.get_resp());
                }
                l_complete = true;
        }
        return l_complete;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t t_hlx::evr_loop_file_writeable_cb(void *a_data)
{
        //NDBG_PRINT("%sWRITEABLE%s %p\n", ANSI_COLOR_FG_BLUE, ANSI_COLOR_OFF, a_data);
        if(!a_data)
        {
                //NDBG_PRINT("a_data == NULL\n");
                return STATUS_OK;
        }
        nconn* l_nconn = static_cast<nconn*>(a_data);
        CHECK_FOR_NULL_ERROR(l_nconn->get_data());
        http_data_t *l_data = static_cast<http_data_t *>(l_nconn->get_data());
        CHECK_FOR_NULL_ERROR(l_data->m_ctx);
        t_hlx *l_t_hlx = static_cast<t_hlx *>(l_data->m_ctx);

        if(l_data->m_type == HTTP_DATA_TYPE_SERVER)
        {
                if(l_nconn->is_free())
                {
                        return STATUS_OK;
                }
        }

        // Cancel last timer
        l_t_hlx->m_evr_loop->cancel_timer(l_data->m_timer_obj);

        // Get timeout ms
        uint32_t l_timeout_ms = l_t_hlx->get_timeout_s()*1000;

        int32_t l_status = STATUS_OK;
        l_status = l_nconn->nc_run_state_machine(l_t_hlx->m_evr_loop, nconn::NC_MODE_WRITE);
        if(l_status == STATUS_ERROR)
        {
                if(l_data->m_type == HTTP_DATA_TYPE_CLIENT)
                {
                        if(l_data->m_http_req.m_subreq)
                        {
                                if(l_data->m_http_req.m_subreq->m_error_cb)
                                {
                                        l_data->m_http_req.m_subreq->m_error_cb(*l_nconn, *(l_data->m_http_req.m_subreq));
                                }
                        }
                }
                l_t_hlx->cleanup_connection(*l_nconn, l_data->m_timer_obj, l_data->m_type);
                return STATUS_ERROR;
        }

        if(l_data->m_type == HTTP_DATA_TYPE_CLIENT)
        {
                if(l_data->m_http_req.m_subreq)
                {
                        l_timeout_ms = l_data->m_http_req.m_subreq->m_timeout_s*1000;
                }
                // Get request time
                if(!l_nconn->m_request_start_time_us && l_nconn->m_collect_stats_flag)
                {
                        l_nconn->m_request_start_time_us = get_time_us();
                }
        }
        if(l_nconn->is_done())
        {
                if(l_data->m_type == HTTP_DATA_TYPE_CLIENT)
                {
                        if(l_data->m_http_req.m_subreq)
                        {
                                bool l_complete = l_t_hlx->subreq_complete(*l_data->m_http_req.m_subreq, *l_nconn);
                                if(l_complete)
                                {
                                        l_t_hlx->cleanup_connection(*l_nconn, l_data->m_timer_obj, l_data->m_type);
                                        return STATUS_OK;
                                }
                        }
                }
                else if(l_data->m_type == HTTP_DATA_TYPE_SERVER)
                {
                        l_t_hlx->cleanup_connection(*l_nconn, l_data->m_timer_obj, l_data->m_type);
                        return STATUS_OK;
                }
                return STATUS_OK;
        }

        // Add idle timeout
        l_t_hlx->m_evr_loop->add_timer(l_timeout_ms, evr_loop_file_timeout_cb, l_nconn, &(l_data->m_timer_obj));
        return STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t t_hlx::evr_loop_file_readable_cb(void *a_data)
{
        //NDBG_PRINT("%sREADABLE%s %p\n", ANSI_COLOR_FG_GREEN, ANSI_COLOR_OFF, a_data);
        if(!a_data)
        {
                return STATUS_OK;
        }
        nconn* l_nconn = static_cast<nconn*>(a_data);
        CHECK_FOR_NULL_ERROR(l_nconn->get_data());
        http_data_t *l_data = static_cast<http_data_t *>(l_nconn->get_data());
        CHECK_FOR_NULL_ERROR(l_data->m_ctx);
        t_hlx *l_t_hlx = static_cast<t_hlx *>(l_data->m_ctx);

        // Cancel last timer
        l_t_hlx->m_evr_loop->cancel_timer(l_data->m_timer_obj);

        int32_t l_status = STATUS_OK;

        //NDBG_PRINT("%sREADABLE%s l_nconn->is_listening(): %d\n", ANSI_COLOR_FG_GREEN, ANSI_COLOR_OFF, l_nconn->is_listening());
        if(l_nconn->is_listening())
        {
                // Returns new client fd on success
                l_status = l_nconn->nc_run_state_machine(l_t_hlx->m_evr_loop, nconn::NC_MODE_NONE);
                if(l_status < 0)
                {
                        //NDBG_PRINT("Error: performing run_state_machine\n");
                        return STATUS_ERROR;
                }
                int l_fd = l_status;
                // Get new connected client conn
                nconn *l_new_nconn = NULL;
                l_new_nconn = l_t_hlx->get_new_client_conn(l_fd, l_nconn->m_scheme, l_data->m_url_router);
                if(!l_new_nconn)
                {
                        //NDBG_PRINT("Error performing get_new_client_conn");
                        return STATUS_ERROR;
                }

                // Set connected
                l_status = l_new_nconn->nc_set_accepting(l_t_hlx->m_evr_loop, l_fd);
                if(l_status != STATUS_OK)
                {
                        //NDBG_PRINT("Error: performing run_state_machine\n");
                        l_t_hlx->cleanup_connection(*l_new_nconn, NULL, l_data->m_type);
                        return STATUS_ERROR;
                }

                return STATUS_OK;
        }

        //NDBG_PRINT("nconn host: %s --l_data->m_type: %d\n", l_nconn->m_host.c_str(), l_data->m_type);

        // Get timeout ms
        uint32_t l_timeout_ms = l_t_hlx->get_timeout_s()*1000;
        if(l_data->m_type == HTTP_DATA_TYPE_CLIENT)
        {
                int32_t l_status = STATUS_OK;
                do {
                        nconn::mode_t l_mode = nconn::NC_MODE_READ;
                        nbq *l_out_q = l_nconn->get_out_q();
                        if(l_out_q && l_out_q->read_avail())
                        {
                                l_mode = nconn::NC_MODE_WRITE;
                        }
                        l_status = l_nconn->nc_run_state_machine(l_t_hlx->m_evr_loop, l_mode);
                        if(l_status >= 0)
                        {
                                l_t_hlx->m_stat.m_num_bytes_read += l_status;
                        }
                        if(l_status == nconn::NC_STATUS_EOF)
                        {
                                l_t_hlx->cleanup_connection(*l_nconn, l_data->m_timer_obj, l_data->m_type);
                                return STATUS_OK;
                        }
                        if(l_data->m_http_req.m_subreq)
                        {
                                l_timeout_ms = l_data->m_http_req.m_subreq->m_timeout_s*1000;
                        }

                        //NDBG_PRINT("l_status:                %d\n", l_status);
                        //NDBG_PRINT("l_nconn->is_done():      %d\n", l_nconn->is_done());
                        //NDBG_PRINT("l_http_resp->m_complete: %d\n", l_data->m_http_resp.m_complete);
                        if((l_nconn->is_done()) ||
                           (l_data->m_http_resp.m_complete) ||
                           (l_status == STATUS_ERROR))
                        {
                                //NDBG_PRINT("l_done: %d -- l_status: %d --proxy size: %d\n", l_done, l_status, (int)l_t_hlx->m_nconn_proxy_pool.get_nconn_obj_pool().used_size());
                                // Get request time
                                if(l_nconn->m_collect_stats_flag)
                                {
                                        l_nconn->m_stat.m_tt_completion_us = get_delta_time_us(l_nconn->m_request_start_time_us);
                                }
                                l_t_hlx->add_stat_to_agg(l_nconn->get_stats(), l_data->m_status_code);
                                l_nconn->reset_stats();

                                // Subrequest...
                                if(l_data->m_http_req.m_subreq)
                                {
                                        bool l_complete = l_t_hlx->subreq_complete(*l_data->m_http_req.m_subreq, *l_nconn);
                                        if(l_complete)
                                        {
                                                l_t_hlx->cleanup_connection(*l_nconn, l_data->m_timer_obj, l_data->m_type);
                                                return STATUS_OK;
                                        }
                                }
                                if(l_status == STATUS_ERROR)
                                {
                                        //NDBG_PRINT("Error: performing run_state_machine l_status: %d\n", l_status);
                                        //l_subreq->set_response(901, l_nconn->m_last_error.c_str());
                                        ++(l_t_hlx->m_stat.m_num_errors);
                                        if(l_t_hlx->m_conf.m_show_summary) l_t_hlx->append_summary(l_nconn, &l_data->m_http_resp);
                                        l_t_hlx->cleanup_connection(*l_nconn, l_data->m_timer_obj, l_data->m_type);
                                        return STATUS_ERROR;
                                }
                                else
                                {
                                        // Display...
                                        if(l_t_hlx->m_conf.m_verbose)
                                        {
                                                if(l_t_hlx->m_conf.m_color) NDBG_OUTPUT("%s", ANSI_COLOR_FG_CYAN);
                                                l_data->m_http_resp.show();
                                                if(l_t_hlx->m_conf.m_color) NDBG_OUTPUT("%s", ANSI_COLOR_OFF);
                                        }

                                        // TODO REMOVE
                                        //NDBG_PRINT("CONN %sREUSE%s: l_nconn->can_reuse():          %d\n", ANSI_COLOR_BG_RED, ANSI_COLOR_OFF, l_nconn->can_reuse());
                                        //NDBG_PRINT("CONN %sREUSE%s: l_data->m_supports_keep_alive: %d\n", ANSI_COLOR_BG_RED, ANSI_COLOR_OFF, l_data->m_supports_keep_alives);
                                        if(!l_nconn->can_reuse() ||
                                           !l_data->m_supports_keep_alives ||
                                           (l_data->m_http_req.m_subreq && l_data->m_http_req.m_subreq->is_done() && !l_t_hlx->m_conf.m_use_persistent_pool))
                                        {
                                                if(l_data->m_http_resp.get_status() >= 500)
                                                {
                                                        //++(l_t_client->m_num_error);
                                                }
                                                //++(l_t_client->m_num_done);
                                                //l_subreq->set_response(l_http_resp->get_status(), "");
                                                if(l_t_hlx->m_conf.m_show_summary) l_t_hlx->append_summary(l_nconn, &l_data->m_http_resp);
                                                l_t_hlx->cleanup_connection(*l_nconn, l_data->m_timer_obj, l_data->m_type);
                                                return STATUS_OK;
                                        }

                                        // Cancel last timer
                                        l_t_hlx->m_evr_loop->cancel_timer(l_data->m_timer_obj);
                                        uint64_t l_last_connect_us = l_nconn->m_stat.m_tt_connect_us;
                                        l_nconn->reset_stats();
                                        l_nconn->m_stat.m_tt_connect_us = l_last_connect_us;

                                        // -------------------------------------------
                                        // If not using pool try resend on same
                                        // connection.
                                        // NOTE:
                                        // This is an optimization meant for load
                                        // testing -whereas pool is used if hlx_client
                                        // used as client proxy.
                                        // -------------------------------------------
                                        //NDBG_PRINT("l_t_hlx->m_conf.m_use_persistent_pool: %d\n", l_t_hlx->m_conf.m_use_persistent_pool);
                                        if(!l_t_hlx->m_conf.m_use_persistent_pool)
                                        {
                                                if((l_data->m_http_req.m_subreq && !l_data->m_http_req.m_subreq->is_pending_done()) &&
                                                   !l_t_hlx->m_stopped)
                                                {
                                                        // Get request time
                                                        if(l_nconn->m_collect_stats_flag)
                                                        {
                                                                l_nconn->m_request_start_time_us = get_time_us();
                                                        }
                                                        // Send request again...
                                                        l_status = l_t_hlx->start_subrequest(*(l_data->m_http_req.m_subreq), *l_nconn);
                                                        if(l_status != STATUS_OK)
                                                        {
                                                                NDBG_PRINT("Error: performing request\n");
                                                                ++(l_t_hlx->m_stat.m_num_errors);
                                                                if(l_t_hlx->m_conf.m_show_summary) l_t_hlx->append_summary(l_nconn, &l_data->m_http_resp);
                                                                l_t_hlx->cleanup_connection(*l_nconn, l_data->m_timer_obj, l_data->m_type);
                                                                return STATUS_ERROR;
                                                        }
                                                        l_data->m_http_req.m_subreq->bump_num_requested();
                                                        if(l_data->m_http_req.m_subreq->is_pending_done())
                                                        {
                                                                l_t_hlx->m_subreq_queue.pop();
                                                        }
                                                }
                                                else
                                                {
                                                        if(l_t_hlx->m_conf.m_show_summary) l_t_hlx->append_summary(l_nconn, &l_data->m_http_resp);
                                                        l_t_hlx->cleanup_connection(*l_nconn, l_data->m_timer_obj, l_data->m_type);
                                                }
                                        }
                                        else
                                        {
                                                l_status = l_t_hlx->m_nconn_proxy_pool.add_idle(l_nconn);
                                                if(l_status != STATUS_OK)
                                                {
                                                        NDBG_PRINT("Error: performing l_t_client->m_nconn_pool.add_idle l_status: %d\n", l_status);
                                                        ++(l_t_hlx->m_stat.m_num_errors);
                                                        if(l_t_hlx->m_conf.m_show_summary) l_t_hlx->append_summary(l_nconn, &l_data->m_http_resp);
                                                        l_t_hlx->cleanup_connection(*l_nconn, l_data->m_timer_obj, l_data->m_type);
                                                        return STATUS_ERROR;
                                                }
                                        }
                                        return STATUS_OK;
                                }
                                return STATUS_OK;
                        }
                } while((l_status != nconn::NC_STATUS_AGAIN) && (!l_t_hlx->m_stopped));
        }
        else if(l_data->m_type == HTTP_DATA_TYPE_SERVER)
        {
                do {
                        l_status = l_nconn->nc_run_state_machine(l_t_hlx->m_evr_loop, nconn::NC_MODE_READ);
                        if(l_status > 0)
                        {
                                l_t_hlx->m_stat.m_num_bytes_read += l_status;
                        }
                        //NDBG_PRINT("l_status: %d\n", l_status);
                        if(l_status == nconn::NC_STATUS_EOF)
                        {
                                //NDBG_PRINT("cleanup\n");
                                l_t_hlx->cleanup_connection(*l_nconn, l_data->m_timer_obj, l_data->m_type);
                                return STATUS_OK;
                        }
                        else if(l_status == STATUS_ERROR)
                        {
                                l_t_hlx->cleanup_connection(*l_nconn, l_data->m_timer_obj, l_data->m_type);
                                return STATUS_ERROR;
                        }

                        //NDBG_PRINT("l_req->m_complete: %d\n", l_req->m_complete);
                        if(l_data->m_http_req.m_complete)
                        {
                                //NDBG_PRINT("g_req_num: %d\n", ++g_req_num);
                                ++l_t_hlx->m_stat.m_num_reqs;

                                // Reset out q
                                l_nconn->get_out_q()->reset_write();

                                // -----------------------------------------------------
                                // main loop request handling...
                                // -----------------------------------------------------
                                int32_t l_hdlr_status;
                                l_hdlr_status = l_t_hlx->handle_req(*l_nconn, l_data->m_url_router, l_data->m_http_req, l_data->m_http_resp);
                                if(l_hdlr_status != 0)
                                {
                                        //NDBG_PRINT("Error performing handle_req\n");
                                        l_t_hlx->cleanup_connection(*l_nconn, l_data->m_timer_obj, l_data->m_type);
                                        return STATUS_ERROR;
                                }
                                l_data->m_http_req.clear();
                                if(l_nconn->get_out_q() && l_nconn->get_out_q()->read_avail())
                                {
                                        // Try write if out q
                                        int32_t l_write_status;
                                        l_write_status = l_nconn->nc_run_state_machine(l_t_hlx->m_evr_loop, nconn::NC_MODE_WRITE);
                                        if(l_write_status > 0)
                                        {
                                                l_t_hlx->m_stat.m_num_bytes_written += l_write_status;
                                        }
                                        // TODO Check status???
                                }
                                if(l_status != nconn::NC_STATUS_EOF)
                                {
                                        // Reset input q
                                        l_nconn->get_in_q()->reset();
                                }
                        }
                } while(l_status != nconn::NC_STATUS_AGAIN && (!l_t_hlx->m_stopped));
        }

        // Add idle timeout
        l_t_hlx->m_evr_loop->add_timer(l_timeout_ms, evr_loop_file_timeout_cb, l_nconn, &(l_data->m_timer_obj));

        return STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t t_hlx::evr_loop_file_timeout_cb(void *a_data)
{
        //NDBG_PRINT("%sTIMEOUT%s %p\n", ANSI_COLOR_FG_RED, ANSI_COLOR_OFF, a_data);
        CHECK_FOR_NULL_ERROR(a_data);
        nconn* l_nconn = static_cast<nconn*>(a_data);
        CHECK_FOR_NULL_ERROR(l_nconn->get_data());
        http_data_t *l_data = static_cast<http_data_t *>(l_nconn->get_data());
        CHECK_FOR_NULL_ERROR(l_data->m_ctx);
        t_hlx *l_t_hlx = static_cast<t_hlx *>(l_data->m_ctx);
        //NDBG_PRINT("Timer: %p\n",l_data->m_timer_obj);
        if(l_nconn->is_free())
        {
                return STATUS_OK;
        }
        ++(l_t_hlx->m_stat.m_num_errors);
        ++l_t_hlx->m_stat.m_num_idle_killed;
        if(l_data->m_type == HTTP_DATA_TYPE_CLIENT)
        {
                if(l_data->m_http_req.m_subreq)
                {
                        l_data->m_http_req.m_subreq->bump_num_completed();
                        if(l_data->m_http_req.m_subreq->m_error_cb)
                        {
                                //NDBG_PRINT_BT();
                                l_data->m_http_req.m_subreq->m_error_cb(*l_nconn, *(l_data->m_http_req.m_subreq));
                        }
                }
        }
        l_t_hlx->append_summary(l_nconn, NULL);
        l_t_hlx->cleanup_connection(*l_nconn, l_data->m_timer_obj, l_data->m_type);
        return STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t t_hlx::evr_loop_timer_cb(void *a_data)
{
        //NDBG_PRINT("%sTIMER%s %p\n", ANSI_COLOR_FG_RED, ANSI_COLOR_OFF, a_data);
        if(!a_data)
        {
                //NDBG_PRINT("a_data == NULL\n");
                return STATUS_OK;
        }
        return STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t t_hlx::evr_loop_file_error_cb(void *a_data)
{
        //NDBG_PRINT("%sERROR%s %p\n", ANSI_COLOR_FG_RED, ANSI_COLOR_OFF, a_data);
        CHECK_FOR_NULL_ERROR(a_data);
        nconn* l_nconn = static_cast<nconn*>(a_data);
        CHECK_FOR_NULL_ERROR(l_nconn->get_data());
        http_data_t *l_data = static_cast<http_data_t *>(l_nconn->get_data());
        CHECK_FOR_NULL_ERROR(l_data->m_ctx);
        t_hlx *l_t_hlx = static_cast<t_hlx *>(l_data->m_ctx);
        //if(l_nconn->is_free())
        //{
        //        return STATUS_OK;
        //}
        ++l_t_hlx->m_stat.m_num_errors;
        if(l_data->m_type == HTTP_DATA_TYPE_CLIENT)
        {
                if(l_data->m_http_req.m_subreq)
                {
                        l_data->m_http_req.m_subreq->bump_num_completed();
                        //NDBG_PRINT("l_data->m_http_req.m_subreq->get_num_completed(): %d\n", l_data->m_http_req.m_subreq->get_num_completed());
                        if(l_data->m_http_req.m_subreq->m_error_cb)
                        {
                                //NDBG_PRINT_BT();
                                l_data->m_http_req.m_subreq->m_error_cb(*l_nconn, *(l_data->m_http_req.m_subreq));
                        }
                }
        }
        l_t_hlx->append_summary(l_nconn, NULL);
        l_t_hlx->cleanup_connection(*l_nconn, l_data->m_timer_obj, l_data->m_type);
        return STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t t_hlx::try_deq_subreq(void)
{
        //NDBG_PRINT("Dequeue: m_subreq_queue.size(): %d\n", (int)m_subreq_queue.size());
        uint32_t l_queue_size = m_subreq_queue.size();
        while(l_queue_size && !m_stopped)
        {
                --l_queue_size;
                //NDBG_PRINT("Dequeue: m_subreq_queue.size(): %d --proxy size: %d\n", (int)m_subreq_queue.size(), (int)m_nconn_proxy_pool.get_nconn_obj_pool().used_size());
                // Start subreq
                subreq *l_subreq = m_subreq_queue.front();
                // Only run on resolved
                if(!l_subreq->is_resolved())
                {
                        int32_t l_status;
                        if((m_conf.m_resolver == NULL) ||
                           (l_status = l_subreq->resolve(m_conf.m_resolver))!= STATUS_OK)
                        {
                                ++m_stat.m_num_errors;
                                ++m_summary_info.m_error_addr;
                                l_subreq->bump_num_requested();
                                l_subreq->bump_num_completed();
                                append_summary(NULL, NULL);
                                if(l_subreq->m_error_cb)
                                {
                                        nconn_tcp l_nconn;
                                        l_subreq->m_error_cb(l_nconn, *l_subreq);
                                }
                                m_subreq_queue.pop();
                                continue;
                        }
                        ++(m_stat.m_num_resolved);
                }
                int32_t l_status;
                nconn *l_nconn = NULL;
                l_nconn = get_proxy_conn(*l_subreq);
                if(!l_nconn)
                {
                        // Push to back
                        m_subreq_queue.pop();
                        m_subreq_queue.push(l_subreq);
                        continue;
                }
                l_status = start_subrequest(*l_subreq, *l_nconn);
                if(l_status != STATUS_OK)
                {
                        //NDBG_PRINT("Error performing request\n");
                        m_subreq_queue.pop();
                }
                else
                {
                        l_subreq->bump_num_requested();
                        //NDBG_PRINT("l_subreq->is_pending_done(): %d\n", l_subreq->is_pending_done());
                        if(l_subreq->is_pending_done())
                        {
                                //NDBG_PRINT("POP'ing: host: %s\n", l_subreq->m_host.c_str());
                                m_subreq_queue.pop();
                        }
                }
        }
        return STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void *t_hlx::t_run(void *a_nothing)
{
        int32_t l_status;
        l_status = init();
        if(l_status != STATUS_OK)
        {
                NDBG_PRINT("Error performing init.\n");
                return NULL;
        }
        m_stopped = false;

        // Set start time
        m_start_time_s = get_time_s();

        // TODO Test -remove
        //uint64_t l_last_time_ms = get_time_ms();
        //uint64_t l_num_run = 0;

        // -------------------------------------------------
        // Run server
        // -------------------------------------------------
        while(!m_stopped)
        {
                //NDBG_PRINT("Running.\n");
                l_status = try_deq_subreq();
                if(l_status != STATUS_OK)
                {
                        NDBG_PRINT("Error performing try_deq_subreq.\n");
                        //return NULL;
                }
                l_status = m_evr_loop->run();
        }
        //NDBG_PRINT("Stopped...\n");
        m_stopped = true;
        return NULL;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t t_hlx::append_summary(nconn *a_nconn, http_resp *a_resp)
{
        uint16_t l_status;
        if(!a_resp)
        {
                l_status = 900;
        }
        else
        {
                l_status = a_resp->get_status();
        }
        //NDBG_PRINT("%sTID[%lu]%s: status: %u\n", ANSI_COLOR_BG_BLUE, pthread_self(), ANSI_COLOR_OFF, l_status);
        if(l_status == 900)
        {
                ++m_summary_info.m_error_addr;
        }
        else if((l_status == 0) ||
                (l_status == 901) ||
                (l_status == 902))
        {
                const std::string &l_body = a_resp->get_body();
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
        }
        else
        {
                ++m_summary_info.m_error_unknown;
        }

        if((l_status < 900) && a_nconn && a_nconn->m_scheme == SCHEME_SSL)
        {
                void *l_cipher;
                a_nconn->get_opt(nconn_ssl::OPT_SSL_INFO_CIPHER_STR, &l_cipher, NULL);
                a_resp->m_ssl_info_cipher_str = (const char *)l_cipher;

                void *l_protocol;
                a_nconn->get_opt(nconn_ssl::OPT_SSL_INFO_PROTOCOL_STR, &l_protocol, NULL);
                a_resp->m_ssl_info_protocol_str = (const char *)l_protocol;

                // TODO Flag for summary???
                // Add to summary...
                if(l_cipher)
                {
                        std::string l_cipher_str = (char *)l_cipher;
                        ++m_summary_info.m_ssl_ciphers[l_cipher_str];
                }
                if(l_protocol)
                {
                        std::string l_protocol_str = (char *)l_protocol;
                        ++m_summary_info.m_ssl_protocols[l_protocol_str];
                }
        }

        return STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t t_hlx::cleanup_connection(nconn &a_nconn, evr_timer_event_t *a_timer_obj, http_data_type_t a_type)
{
        //NDBG_PRINT("%sCLEANUP%s: a_nconn: %p -host: %s\n", ANSI_COLOR_FG_GREEN, ANSI_COLOR_OFF, &a_nconn, a_nconn.m_host.c_str());
        // Cancel last timer
        if(a_timer_obj)
        {
                m_evr_loop->cancel_timer(a_timer_obj);
        }

        nbq *l_out_q = a_nconn.get_out_q();
        if(l_out_q)
        {
                delete l_out_q;
                l_out_q = NULL;
                a_nconn.set_out_q(NULL);
        }
        nbq *l_in_q = a_nconn.get_in_q();
        if(l_in_q)
        {
                delete l_in_q;
                l_in_q = NULL;
                a_nconn.set_in_q(NULL);
        }

        //NDBG_PRINT("%sADDING_BACK%s: %u a_nconn: %p type: %d\n",
        //           ANSI_COLOR_BG_GREEN, ANSI_COLOR_OFF,
        //           (uint32_t)a_nconn.get_idx(), &a_nconn, a_type);
        //NDBG_PRINT_BT();
        // Add back to free list
        if(a_type == HTTP_DATA_TYPE_SERVER)
        {
                //NDBG_PRINT("m_nconn_pool size: %d\n", (int)m_nconn_pool.get_nconn_obj_pool().used_size());
                //NDBG_PRINT("m_idle size:       %u\n", (int)m_nconn_pool.num_idle());
                if(STATUS_OK != m_nconn_pool.release(&a_nconn))
                {
                        return STATUS_ERROR;
                }
                //NDBG_PRINT("m_nconn_pool size: %d\n", (int)m_nconn_pool.get_nconn_obj_pool().used_size());
                //NDBG_PRINT("m_idle size:       %u\n", (int)m_nconn_pool.num_idle());
        }
        else if(a_type == HTTP_DATA_TYPE_CLIENT)
        {
                //NDBG_PRINT("m_nconn_proxy_pool size: %d\n", (int)m_nconn_proxy_pool.get_nconn_obj_pool().used_size());
                //NDBG_PRINT("m_idle size:       %u\n", (int)m_nconn_proxy_pool.num_idle());
                if(STATUS_OK != m_nconn_proxy_pool.release(&a_nconn))
                {
                        return STATUS_ERROR;
                }
                //NDBG_PRINT("m_nconn_proxy_pool size: %d\n", (int)m_nconn_proxy_pool.get_nconn_obj_pool().used_size());
                //NDBG_PRINT("m_idle size:       %u\n", (int)m_nconn_proxy_pool.num_idle());
        }

        --m_stat.m_cur_conn_count;
        ++m_stat.m_num_conn_completed;

        return STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t t_hlx::handle_req(nconn &a_nconn, url_router *a_url_router, http_req &a_req, http_resp &ao_resp)
{
        std::string l_path;
        l_path.assign(a_req.m_p_url.m_ptr, a_req.m_p_url.m_len);
        //NDBG_PRINT("PATH: %s\n", l_path.c_str());
        //NDBG_PRINT("l_t_hlx->m_url_router: %p\n", a_url_router);
        http_request_handler *l_request_handler = NULL;
        url_param_map_t l_param_map;
        l_request_handler = (http_request_handler *)a_url_router->find_route(l_path,l_param_map);
        //NDBG_PRINT("l_request_handler: %p\n", l_request_handler);

        int32_t l_hdlr_status = 0;
        if(l_request_handler)
        {
                // Method switch
                switch(a_req.m_method)
                {
                case HTTP_GET:
                {
                        l_hdlr_status = l_request_handler->do_get(*(m_conf.m_hlx), a_nconn, a_req, l_param_map, ao_resp);
                        break;
                }
                case HTTP_POST:
                {
                        l_hdlr_status = l_request_handler->do_post(*(m_conf.m_hlx), a_nconn, a_req, l_param_map, ao_resp);
                        break;
                }
                case HTTP_PUT:
                {
                        l_hdlr_status = l_request_handler->do_put(*(m_conf.m_hlx), a_nconn, a_req, l_param_map, ao_resp);
                        break;
                }
                case HTTP_DELETE:
                {
                        l_hdlr_status = l_request_handler->do_delete(*(m_conf.m_hlx), a_nconn, a_req, l_param_map, ao_resp);
                        break;
                }
                default:
                {
                        l_hdlr_status = l_request_handler->do_default(*(m_conf.m_hlx), a_nconn, a_req, l_param_map, ao_resp);
                        break;
                }
                }
        }
        else
        {
                // Send default response
                //NDBG_PRINT("Default response\n");
                l_hdlr_status = m_default_handler.do_get(*(m_conf.m_hlx), a_nconn, a_req, l_param_map, ao_resp);
        }

        // TODO Handler errors?

        return l_hdlr_status;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t t_hlx::config_conn(nconn &a_nconn,
                           url_router *a_url_router,
                           http_data_type_t a_type,
                           bool a_save,
                           bool a_connect_only)
{
        a_nconn.set_num_reqs_per_conn(m_conf.m_num_reqs_per_conn);
        a_nconn.set_collect_stats(m_conf.m_collect_stats);
        a_nconn.set_connect_only(a_connect_only);

        // -------------------------------------------
        // Set options
        // -------------------------------------------
        // Set generic options
        T_HTTP_PROXY_SET_NCONN_OPT((a_nconn), nconn_tcp::OPT_TCP_RECV_BUF_SIZE, NULL, m_conf.m_sock_opt_recv_buf_size);
        T_HTTP_PROXY_SET_NCONN_OPT((a_nconn), nconn_tcp::OPT_TCP_SEND_BUF_SIZE, NULL, m_conf.m_sock_opt_send_buf_size);
        T_HTTP_PROXY_SET_NCONN_OPT((a_nconn), nconn_tcp::OPT_TCP_NO_DELAY, NULL, m_conf.m_sock_opt_no_delay);

        // Set ssl options
        if(a_nconn.m_scheme == SCHEME_SSL)
        {
                T_HTTP_PROXY_SET_NCONN_OPT(a_nconn,
                                       nconn_ssl::OPT_SSL_CIPHER_STR,
                                       m_conf.m_ssl_cipher_list.c_str(),
                                       m_conf.m_ssl_cipher_list.length());

                if(a_type == HTTP_DATA_TYPE_SERVER)
                {
                        T_HTTP_PROXY_SET_NCONN_OPT(a_nconn,
                                               nconn_ssl::OPT_SSL_CTX,
                                               m_conf.m_ssl_server_ctx,
                                               sizeof(m_conf.m_ssl_server_ctx));
                }
                else
                {
                        T_HTTP_PROXY_SET_NCONN_OPT(a_nconn,
                                               nconn_ssl::OPT_SSL_CTX,
                                               m_conf.m_ssl_client_ctx,
                                               sizeof(m_conf.m_ssl_client_ctx));
                }

                T_HTTP_PROXY_SET_NCONN_OPT(a_nconn,
                                       nconn_ssl::OPT_SSL_VERIFY,
                                       &(m_conf.m_ssl_verify),
                                       sizeof(m_conf.m_ssl_verify));

                //T_CLIENT_SET_NCONN_OPT((*l_nconn), nconn_ssl::OPT_SSL_OPTIONS,
                //                              &(a_settings.m_ssl_options),
                //                              sizeof(a_settings.m_ssl_options));

                if(!m_conf.m_tls_crt.empty())
                {
                        T_HTTP_PROXY_SET_NCONN_OPT(a_nconn,
                                               nconn_ssl::OPT_SSL_TLS_CRT,
                                               m_conf.m_tls_crt.c_str(),
                                               m_conf.m_tls_crt.length());
                }
                if(!m_conf.m_tls_key.empty())
                {
                        T_HTTP_PROXY_SET_NCONN_OPT(a_nconn,
                                               nconn_ssl::OPT_SSL_TLS_KEY,
                                               m_conf.m_tls_key.c_str(),
                                               m_conf.m_tls_key.length());
                }
        }

        return STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t t_hlx::config_data(nconn &a_nconn,
                           url_router *a_url_router,
                           http_data_type_t a_type,
                           bool a_save,
                           bool a_connect_only)
{
        http_data_t *l_data = static_cast<http_data_t *>(a_nconn.get_data());
        if(!l_data)
        {
                http_data_t *l_data = m_http_data_pool.get_free();
                if(l_data)
                {
                        NDBG_PRINT("REUSED!!!\n");
                        l_data->m_http_resp.clear();
                        l_data->m_http_req.clear();
                }
                else
                {
                        l_data = new http_data_t();
                        m_http_data_pool.add(l_data);
                }
                //NDBG_PRINT("Adding http_data: %p.\n", l_data);
                a_nconn.set_data(l_data);
                l_data->m_nconn = &a_nconn;
                l_data->m_ctx = this;
                l_data->m_url_router = a_url_router;
                l_data->m_timer_obj = NULL;
                l_data->m_save = a_save;
                l_data->m_type = a_type;
                l_data->m_supports_keep_alives = false;
                l_data->m_status_code = 0;
                l_data->m_http_parser_settings.on_status = hp_on_status;
                l_data->m_http_parser_settings.on_message_complete = hp_on_message_complete;
                if(l_data->m_save)
                {
                        l_data->m_http_parser_settings.on_message_begin = hp_on_message_begin;
                        l_data->m_http_parser_settings.on_url = hp_on_url;
                        l_data->m_http_parser_settings.on_header_field = hp_on_header_field;
                        l_data->m_http_parser_settings.on_header_value = hp_on_header_value;
                        l_data->m_http_parser_settings.on_headers_complete = hp_on_headers_complete;
                        l_data->m_http_parser_settings.on_body = hp_on_body;
                }
                l_data->m_http_parser.data = l_data;
                if(a_type == HTTP_DATA_TYPE_SERVER)
                {
                        http_parser_init(&(l_data->m_http_parser), HTTP_REQUEST);
                }
                else
                {
                        http_parser_init(&(l_data->m_http_parser), HTTP_RESPONSE);
                }
                a_nconn.set_read_cb(http_parse);
        }
        else
        {
                l_data->m_http_resp.clear();
                l_data->m_http_req.clear();
        }
        return STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void t_hlx::add_stat_to_agg(const req_stat_t &a_req_stat, uint16_t a_status_code)
{
        update_stat(m_stat.m_stat_us_connect, a_req_stat.m_tt_connect_us);
        update_stat(m_stat.m_stat_us_first_response, a_req_stat.m_tt_first_read_us);
        update_stat(m_stat.m_stat_us_end_to_end, a_req_stat.m_tt_completion_us);

        // Totals
        ++m_stat.m_total_reqs;
        m_stat.m_total_bytes += a_req_stat.m_total_bytes;

        // Status code
        //NDBG_PRINT("%sSTATUS_CODE%s: %d\n", ANSI_COLOR_BG_GREEN, ANSI_COLOR_OFF, a_req_stat.m_status_code);
        ++m_stat.m_status_code_count_map[a_status_code];
}

} //namespace ns_hlx {
