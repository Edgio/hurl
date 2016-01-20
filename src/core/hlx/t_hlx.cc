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
#include "t_hlx.h"
#include "ndebug.h"
#include "nbq.h"
#include "nconn_tcp.h"
#include "nconn_tls.h"
#include "time_util.h"
#include "stat_util.h"

#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>

//: ----------------------------------------------------------------------------
//: Macros
//: ----------------------------------------------------------------------------
#define T_HLX_SET_NCONN_OPT(_conn, _opt, _buf, _len) \
        do { \
                int _status = 0; \
                _status = _conn.set_opt((_opt), (_buf), (_len)); \
                if (_status != nconn::NC_STATUS_OK) { \
                        NDBG_PRINT("STATUS_ERROR: set_opt %d.  Status: %d.\n", \
                                   _opt, _status); \
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


namespace ns_hlx {

//: ----------------------------------------------------------------------------
//: Types
//: ----------------------------------------------------------------------------
typedef obj_pool <nbq> nbq_pool_t;

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
template <typename T> bool get_from_pool_if_null(T* &ao_obj, obj_pool<T> &a_pool)
{
        bool l_new = false;
        if(!ao_obj)
        {
                // TODO Make function
                ao_obj = a_pool.get_free();
                if(!ao_obj)
                {
                        ao_obj = new T();
                        a_pool.add(ao_obj);
                        l_new = true;
                }
        }
        return l_new;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
template <>
bool get_from_pool_if_null<nbq>(nbq* &ao_obj, obj_pool<nbq> &a_pool)
{
        bool l_new = false;
        if(!ao_obj)
        {
                // TODO Make function
                ao_obj = a_pool.get_free();
                if(!ao_obj)
                {
                        ao_obj = new nbq(16384);
                        a_pool.add(ao_obj);
                        l_new = true;
                }
        }
        return l_new;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
t_hlx::t_hlx(const t_conf *a_t_conf):
        m_t_run_thread(),
        m_stat(),
        m_t_conf(a_t_conf),
        m_nconn_pool(-1),
        m_nconn_proxy_pool(a_t_conf->m_num_parallel),
        m_stopped(true),
        m_start_time_s(0),
        m_evr_loop(NULL),
        m_scheme(SCHEME_TCP),
        m_listening_nconn_list(),
        m_subr_queue(),
        m_hconn_pool(),
        m_resp_pool(),
        m_rqst_pool(),
        m_nbq_pool(),
#ifdef ASYNC_DNS_SUPPORT
        m_async_dns_is_initd(false),
        m_async_dns_ctx(NULL),
        m_async_dns_fd(-1),
        m_async_dns_nconn(NULL),
        m_async_dns_timer_obj(NULL),
        m_lookup_job_q(),
        m_lookup_job_pq(),
#endif
        m_is_initd(false)
{
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
        while(!m_subr_queue.empty())
        {
                subr *l_subr = m_subr_queue.front();
                if(l_subr)
                {
                        delete l_subr;
                }
                m_subr_queue.pop();
        }
#ifdef ASYNC_DNS_SUPPORT
        if(m_t_conf && m_t_conf->m_hlx && m_t_conf->m_hlx->get_nresolver())
        {
                m_t_conf->m_hlx->get_nresolver()->destroy_async(m_async_dns_ctx,
                                                                m_async_dns_fd,
                                                                m_lookup_job_q,
                                                                m_lookup_job_pq);
                if(m_async_dns_nconn)
                {
                        delete m_async_dns_nconn;
                }
        }
#endif
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
        m_evr_loop = new evr_loop(evr_file_readable_cb,
                                  evr_file_writeable_cb,
                                  evr_file_error_cb,
                                  m_t_conf->m_evr_loop_type,
                                  // TODO Need to make epoll vector resizeable...
                                  512,
                                  false);
        m_is_initd = true;
        return STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t t_hlx::add_lsnr(lsnr &a_lsnr)
{
        int32_t l_status;
        l_status = init();
        if(l_status != STATUS_OK)
        {
                NDBG_PRINT("Error performing init.\n");
                return STATUS_ERROR;
        }
        nconn *l_nconn = NULL;
        l_nconn = m_nconn_pool.create_conn(a_lsnr.get_scheme());
        l_status = config_conn(*l_nconn, a_lsnr.get_url_router(), HCONN_TYPE_CLIENT, false, false);
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
        hconn *l_hconn = get_hconn(a_lsnr.get_url_router(), HCONN_TYPE_CLIENT, false);
        if(!l_hconn)
        {
                NDBG_PRINT("Error: performing get_hconn\n");
                return STATUS_ERROR;
        }
        l_nconn->set_data(l_hconn);
        l_hconn->m_nconn = l_nconn;
        l_status = l_nconn->nc_set_listening(m_evr_loop, a_lsnr.get_fd());
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
int32_t t_hlx::subr_add(subr &a_subr)
{
        a_subr.set_t_hlx(this);
        //NDBG_PRINT("Adding subreq.\n");
        if(m_stopped)
        {
                m_subr_queue.push(&a_subr);
        }
        else
        {
                int32_t l_status;
                l_status = subr_try_start(a_subr);
                if(l_status == STATUS_AGAIN)
                {
                        m_subr_queue.push(&a_subr);
                        return STATUS_OK;
                }
                else if(l_status == STATUS_QUEUED_ASYNC_DNS)
                {
                        return STATUS_OK;
                }
                else
                {
                        return STATUS_ERROR;
                }
        }
        return STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t t_hlx::queue_api_resp(api_resp &a_api_resp, hconn &a_hconn)
{
        if(!get_from_pool_if_null(a_hconn.m_out_q, m_nbq_pool))
        {
                a_hconn.m_out_q->reset_write();
        }
        a_api_resp.serialize(*a_hconn.m_out_q);
        evr_file_writeable_cb(a_hconn.m_nconn);
        return STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t t_hlx::queue_output(hconn &a_hconn)
{
        evr_file_writeable_cb(a_hconn.m_nconn);
        return STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t t_hlx::subr_start(subr &a_subr, hconn &a_hconn, nconn &a_nconn)
{
        int32_t l_status;
        //NDBG_PRINT("TID[%lu]: %sSTART%s: Host: %s l_nconn: %p\n",
        //                pthread_self(),
        //                ANSI_COLOR_BG_BLUE, ANSI_COLOR_OFF,
        //                a_subr.get_label().c_str(),
        //                &a_nconn);
        resp *l_resp = static_cast<resp *>(a_hconn.m_hmsg);
        if(a_subr.get_detach_resp())
        {
                if(!l_resp)
                {
                        l_resp = new resp();
                }
                else
                {
                        l_resp->clear();
                }
        }
        else
        {
                if(!get_from_pool_if_null(l_resp, m_resp_pool))
                {
                        l_resp->clear();
                }
        }
        a_hconn.m_hmsg = l_resp;

        // Create request
        if(!a_subr.get_connect_only())
        {
                if(!get_from_pool_if_null(a_hconn.m_in_q, m_nbq_pool))
                {
                        a_hconn.m_in_q->reset_write();
                }

                a_hconn.m_hmsg->set_q(a_hconn.m_in_q);

                if(!a_hconn.m_out_q)
                {
                        if(!get_from_pool_if_null(a_hconn.m_out_q, m_nbq_pool))
                        {
                                a_hconn.m_out_q->reset_write();
                        }
                        subr::create_req_cb_t l_create_req_cb = a_subr.get_create_req_cb();
                        if(l_create_req_cb)
                        {
                                l_status = l_create_req_cb(a_subr, *a_hconn.m_out_q);
                                if(STATUS_OK != l_status)
                                {
                                        return STATUS_ERROR;
                                }
                        }
                }
                else
                {
                        if(a_subr.get_is_multipath())
                        {
                                // Reset in data
                                a_hconn.m_out_q->reset_write();
                                subr::create_req_cb_t l_create_req_cb = a_subr.get_create_req_cb();
                                if(l_create_req_cb)
                                {
                                        l_status = l_create_req_cb(a_subr, *a_hconn.m_out_q);
                                        if(STATUS_OK != l_status)
                                        {
                                                return STATUS_ERROR;
                                        }
                                }
                        }
                        else
                        {
                                a_hconn.m_out_q->reset_read();
                        }
                }

                // Display data from out q
                if(m_t_conf->m_verbose)
                {
                        if(m_t_conf->m_color) NDBG_OUTPUT("%s", ANSI_COLOR_FG_YELLOW);
                        a_hconn.m_out_q->print();
                        if(m_t_conf->m_color) NDBG_OUTPUT("%s", ANSI_COLOR_OFF);
                }
        }

        // Set subreq
        //l_hconn->m_rqst.m_subr = &a_subr;

        // Set start time
        if(a_subr.get_type() != SUBR_TYPE_DUPE)
        {
                a_subr.set_start_time_ms(get_time_ms());
        }
        l_status = m_evr_loop->add_timer(a_subr.get_timeout_ms(),
                                         evr_file_timeout_cb,
                                         &a_nconn,
                                         &(a_hconn.m_timer_obj));
        if(l_status != STATUS_OK)
        {
                //NDBG_PRINT("Error: Performing add_timer\n");
                return STATUS_ERROR;
        }

        //NDBG_PRINT("g_client_req_num: %d\n", ++g_client_req_num);
        //NDBG_PRINT("%sCONNECT%s: %s --data: %p\n",
        //           ANSI_COLOR_BG_MAGENTA, ANSI_COLOR_OFF,
        //           a_subr.m_host.c_str(), a_nconn.get_data());
        ++m_stat.m_num_ups_conn_started;
        l_status = a_nconn.nc_run_state_machine(m_evr_loop,
                                                nconn::NC_MODE_WRITE,
                                                a_hconn.m_in_q,
                                                a_hconn.m_out_q);
        a_nconn.bump_num_requested();
        if(l_status == nconn::NC_STATUS_ERROR)
        {
                //NDBG_PRINT("Error: Performing nc_run_state_machine. l_status: %d\n", l_status);
                cleanup_hconn(a_hconn);
                return STATUS_OK;
        }

        // Get request time
        if(a_nconn.get_collect_stats_flag())
        {
                a_nconn.set_request_start_time_us(get_time_us());
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
                //NDBG_PRINT("Error: performing m_nconn_pool.get\n");
                return NULL;
        }
        //NDBG_PRINT("%sGET_NEW%s: %u a_nconn: %p\n",
        //           ANSI_COLOR_BG_BLUE, ANSI_COLOR_OFF,
        //           (uint32_t)l_nconn->get_idx(), l_nconn);
        // Config
        int32_t l_status;
        l_status = config_conn(*l_nconn, a_url_router, HCONN_TYPE_CLIENT, true, false);
        if(l_status != STATUS_OK)
        {
                //NDBG_PRINT("Error: performing config_conn\n");
                return NULL;
        }
        hconn *l_hconn = static_cast<hconn *>(l_nconn->get_data());
        if(!l_hconn)
        {
                l_hconn = get_hconn(a_url_router, HCONN_TYPE_CLIENT, true);
                if(!l_hconn)
                {
                        //NDBG_PRINT("Error: performing config_conn\n");
                        return NULL;
                }
        }
        l_nconn->set_data(l_hconn);
        l_nconn->set_read_cb(http_parse);
        l_hconn->m_nconn = l_nconn;
        rqst *l_rqst = static_cast<rqst *>(l_hconn->m_hmsg);
        if(!get_from_pool_if_null(l_rqst, m_rqst_pool))
        {
                l_rqst->clear();
        }
        l_hconn->m_hmsg = l_rqst;
        if(!get_from_pool_if_null(l_hconn->m_in_q, m_nbq_pool))
        {
                l_hconn->m_in_q->reset_write();
        }
        if(!get_from_pool_if_null(l_hconn->m_out_q, m_nbq_pool))
        {
                l_hconn->m_out_q->reset_write();
        }
        l_hconn->m_hmsg->set_q(l_hconn->m_in_q);
        ++m_stat.m_cur_cln_conn_count;
        ++m_stat.m_num_cln_conn_started;
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
                //NDBG_PRINT("Error: creating thread.  Reason: %s\n.",
                //           strerror(l_pthread_error));
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
        //cleanup_hconn(m_listening_nconn, NULL, 200);
        m_stopped = true;
        int32_t l_status;
        l_status = m_evr_loop->signal_control();
        if(l_status != STATUS_OK)
        {
                //NDBG_PRINT("Error performing stop.\n");
        }
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t t_hlx::evr_file_writeable_cb(void *a_data)
{
        //NDBG_PRINT("%sWRITEABLE%s %p\n", ANSI_COLOR_BG_BLUE, ANSI_COLOR_OFF, a_data);
        if(!a_data)
        {
                //NDBG_PRINT("a_data == NULL\n");
                return STATUS_OK;
        }
        nconn* l_nconn = static_cast<nconn*>(a_data);
        CHECK_FOR_NULL_ERROR(l_nconn->get_data());
        hconn *l_hconn = static_cast<hconn *>(l_nconn->get_data());
        //NDBG_PRINT("%sWRITEABLE%s LABEL: %s HCONN: %p\n", ANSI_COLOR_FG_BLUE, ANSI_COLOR_OFF,
        //                l_nconn->get_label().c_str(), l_hconn);
        CHECK_FOR_NULL_ERROR(l_hconn->m_t_hlx);
        t_hlx *l_t_hlx = static_cast<t_hlx *>(l_hconn->m_t_hlx);

        if(l_hconn->m_type == HCONN_TYPE_CLIENT)
        {
                if(l_nconn->is_free())
                {
                        return STATUS_OK;
                }
        }

        // Cancel last timer
        if(l_hconn->m_timer_obj)
        {
                l_t_hlx->m_evr_loop->cancel_timer(l_hconn->m_timer_obj);
                l_hconn->m_timer_obj = NULL;
        }

        // Get timeout ms
        uint32_t l_timeout_ms = 0;
        if(l_hconn->m_subr)
        {
                l_timeout_ms = l_hconn->m_subr->get_timeout_ms();
        }
        else
        {
                l_timeout_ms = l_t_hlx->get_timeout_ms();
        }

        int32_t l_status = STATUS_OK;
        do {
                //NDBG_PRINT("%sWRITEABLE%s <--RUN_STATE_MACHINE--> out_q_sz  = %lu\n",
                //                ANSI_COLOR_FG_BLUE, ANSI_COLOR_OFF, l_hconn->m_out_q->read_avail());
                l_status = l_nconn->nc_run_state_machine(l_t_hlx->m_evr_loop,
                                                         nconn::NC_MODE_WRITE,
                                                         l_hconn->m_in_q,
                                                         l_hconn->m_out_q);
                //NDBG_PRINT("%sWRITEABLE%s l_status =  %d\n", ANSI_COLOR_FG_BLUE, ANSI_COLOR_OFF, l_status);
                if(l_status > 0)
                {
                        l_t_hlx->m_stat.m_num_bytes_written += l_status;
                }

                int32_t l_hstatus;
                l_hstatus = l_hconn->run_state_machine(nconn::NC_MODE_WRITE, l_status);
                //NDBG_PRINT("%sWRITEABLE%s l_hstatus = %d\n", ANSI_COLOR_FG_BLUE, ANSI_COLOR_OFF, l_hstatus);
                if(l_hstatus != STATUS_OK)
                {
                        // Modified connection status
                        l_status = l_hstatus;
                }
                if(l_nconn->is_free())
                {
                        return STATUS_OK;
                }
                switch(l_status)
                {
                case nconn::NC_STATUS_BREAK:
                {
                        goto done;
                }
                case nconn::NC_STATUS_EOF:
                {
                        l_t_hlx->cleanup_hconn(*l_hconn);
                        return STATUS_OK;
                }
                case nconn::NC_STATUS_ERROR:
                {
                        ++(l_t_hlx->m_stat.m_num_errors);
                        l_t_hlx->cleanup_hconn(*l_hconn);
                        return STATUS_ERROR;
                }
                }
        } while((l_status != nconn::NC_STATUS_AGAIN) &&
                (!l_t_hlx->m_stopped));
done:
        // Add idle timeout
        if(!l_hconn->m_timer_obj)
        {
                l_t_hlx->m_evr_loop->add_timer(l_timeout_ms,
                                               evr_file_timeout_cb,
                                               l_nconn,
                                               &(l_hconn->m_timer_obj));
        }
        return STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t t_hlx::evr_file_readable_cb(void *a_data)
{
        //NDBG_PRINT("%sREADABLE%s %p\n", ANSI_COLOR_BG_GREEN, ANSI_COLOR_OFF, a_data);
        if(!a_data)
        {
                return STATUS_OK;
        }
        nconn* l_nconn = static_cast<nconn*>(a_data);
        CHECK_FOR_NULL_ERROR(l_nconn->get_data());
        hconn *l_hconn = static_cast<hconn *>(l_nconn->get_data());
        //NDBG_PRINT("%sREADABLE%s LABEL: %s HCONN: %p\n", ANSI_COLOR_FG_GREEN, ANSI_COLOR_OFF,
        //                l_nconn->get_label().c_str(), l_hconn);
        CHECK_FOR_NULL_ERROR(l_hconn->m_t_hlx);
        t_hlx *l_t_hlx = static_cast<t_hlx *>(l_hconn->m_t_hlx);

#ifdef ASYNC_DNS_SUPPORT
        // Async Resolver
        if(l_nconn->get_id() == nresolver::S_RESOLVER_ID)
        {
                ++l_t_hlx->m_stat.m_num_ups_resolve_ev;
                return l_t_hlx->async_dns_handle_ev();
        }
#endif

        // Server -incoming client connections
        if(l_nconn->is_listening())
        {
                return l_t_hlx->handle_listen_ev(*l_hconn, *l_nconn);
        }

        //NDBG_PRINT("nconn host: %s --l_hconn->m_type: %d\n", l_nconn->get_label().c_str(), l_hconn->m_type);

        // Cancel last timer
        if(l_hconn->m_timer_obj)
        {
                l_t_hlx->m_evr_loop->cancel_timer(l_hconn->m_timer_obj);
                l_hconn->m_timer_obj = NULL;
        }

        // Get timeout ms
        uint32_t l_timeout_ms = 0;
        if(l_hconn->m_subr)
        {
                l_timeout_ms = l_hconn->m_subr->get_timeout_ms();
        }
        else
        {
                l_timeout_ms = l_t_hlx->get_timeout_ms();
        }

        int32_t l_status = STATUS_OK;
        do {
                // Flipping modes to support TLS connections
                nconn::mode_t l_mode = nconn::NC_MODE_READ;
                nbq *l_out_q = l_hconn->m_out_q;
                if(l_out_q && l_out_q->read_avail())
                {
                        l_mode = nconn::NC_MODE_WRITE;
                }
                l_status = l_nconn->nc_run_state_machine(l_t_hlx->m_evr_loop,
                                                         l_mode,
                                                         l_hconn->m_in_q,
                                                         l_hconn->m_out_q);
                //NDBG_PRINT("%sREADABLE%s l_status:  %d\n", ANSI_COLOR_FG_GREEN, ANSI_COLOR_OFF, l_status);
                if(l_status > 0)
                {
                        if(l_mode == nconn::NC_MODE_READ)
                        {
                                l_t_hlx->m_stat.m_num_bytes_read += l_status;
                        }
                        else if(l_mode == nconn::NC_MODE_WRITE)
                        {
                                l_t_hlx->m_stat.m_num_bytes_written += l_status;
                        }
                }
                int32_t l_hstatus;
                l_hstatus = l_hconn->run_state_machine(l_mode, l_status);
                //NDBG_PRINT("%sREADABLE%s l_hstatus: %d\n", ANSI_COLOR_FG_GREEN, ANSI_COLOR_OFF, l_hstatus);
                if(l_hstatus != STATUS_OK)
                {
                        // Modified connection status
                        l_status = l_hstatus;
                }
                if(l_nconn->is_free())
                {
                        return STATUS_OK;
                }
                switch(l_status)
                {
                case nconn::NC_STATUS_BREAK:
                {
                        goto done;
                }
                case nconn::NC_STATUS_IDLE:
                {
                        // -------------------------------------------
                        // Reset or add back to subr for reuse
                        // -------------------------------------------
                        if(!get_from_pool_if_null(l_hconn->m_in_q, l_t_hlx->m_nbq_pool))
                        {
                                l_hconn->m_in_q->reset_write();
                        }

                        int32_t l_status;
                        l_status = l_t_hlx->m_nconn_proxy_pool.add_idle(l_nconn);
                        if(l_status != STATUS_OK)
                        {
                                //NDBG_PRINT("Error: performing add_idle l_status: %d\n", l_status);
                                return STATUS_ERROR;
                        }
                        return STATUS_OK;
                }
                case nconn::NC_STATUS_EOF:
                {
                        l_t_hlx->cleanup_hconn(*l_hconn);
                        return STATUS_OK;
                }
                case nconn::NC_STATUS_ERROR:
                {
                        ++(l_t_hlx->m_stat.m_num_errors);
                        l_t_hlx->cleanup_hconn(*l_hconn);
                        return STATUS_ERROR;
                }
                }
        } while((l_status != nconn::NC_STATUS_AGAIN) &&
                (!l_t_hlx->m_stopped));
done:
        // Add idle timeout
        if(!l_hconn->m_timer_obj)
        {
                l_t_hlx->m_evr_loop->add_timer(l_timeout_ms,
                                               evr_file_timeout_cb,
                                               l_nconn,
                                               &(l_hconn->m_timer_obj));
        }
        return STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t t_hlx::evr_file_timeout_cb(void *a_data)
{
        //NDBG_PRINT("%sTIMEOUT%s %p\n", ANSI_COLOR_FG_RED, ANSI_COLOR_OFF, a_data);
        CHECK_FOR_NULL_ERROR(a_data);
        nconn* l_nconn = static_cast<nconn*>(a_data);
        CHECK_FOR_NULL_ERROR(l_nconn->get_data());
        hconn *l_hconn = static_cast<hconn *>(l_nconn->get_data());
        CHECK_FOR_NULL_ERROR(l_hconn->m_t_hlx);
        t_hlx *l_t_hlx = static_cast<t_hlx *>(l_hconn->m_t_hlx);

#ifdef ASYNC_DNS_SUPPORT
        // Async Resolver
        if(l_nconn->get_id() == nresolver::S_RESOLVER_ID)
        {
                std::string l_unused;
                l_t_hlx->m_async_dns_timer_obj = NULL;
                return l_t_hlx->async_dns_lookup(l_unused, 0, NULL);
        }
#endif

        //NDBG_PRINT("%sTIMEOUT%s HOST: %s\n",
        //           ANSI_COLOR_FG_RED, ANSI_COLOR_OFF,
        //           l_nconn->get_label().c_str());
        if(l_nconn->is_free())
        {
                return STATUS_OK;
        }
        //NDBG_PRINT("Error: evr_loop_file_timeout_cb\n");
        ++(l_t_hlx->m_stat.m_num_errors);
        int32_t l_hstatus;
        l_hstatus = l_hconn->run_state_machine(nconn::NC_MODE_TIMEOUT, 0);
        if(l_hstatus != STATUS_OK)
        {
                // TODO???
        }
        l_t_hlx->cleanup_hconn(*l_hconn);
        return STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t t_hlx::evr_timer_cb(void *a_data)
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
int32_t t_hlx::evr_file_error_cb(void *a_data)
{
        //NDBG_PRINT("%sERROR%s %p\n", ANSI_COLOR_FG_RED, ANSI_COLOR_OFF, a_data);
        CHECK_FOR_NULL_ERROR(a_data);
        nconn* l_nconn = static_cast<nconn*>(a_data);
        CHECK_FOR_NULL_ERROR(l_nconn->get_data());
        hconn *l_hconn = static_cast<hconn *>(l_nconn->get_data());
        CHECK_FOR_NULL_ERROR(l_hconn->m_t_hlx);
        t_hlx *l_t_hlx = static_cast<t_hlx *>(l_hconn->m_t_hlx);
        //if(l_nconn->is_free())
        //{
        //        return STATUS_OK;
        //}
        NDBG_PRINT("Error: file_error_cb\n");
        ++l_t_hlx->m_stat.m_num_errors;
        int32_t l_hstatus;
        l_hstatus = l_hconn->run_state_machine(nconn::NC_MODE_ERROR, 0);
        if(l_hstatus != STATUS_OK)
        {
                // TODO???
        }
        l_t_hlx->cleanup_hconn(*l_hconn);
        return STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t t_hlx::handle_listen_ev(hconn &a_hconn, nconn &a_nconn)
{
        int32_t l_status;

        // Returns new client fd on success
        l_status = a_nconn.nc_run_state_machine(m_evr_loop,
                                                 nconn::NC_MODE_NONE,
                                                 NULL,
                                                 NULL);
        if(l_status == nconn::NC_STATUS_ERROR)
        {
                return STATUS_ERROR;
        }

        int l_fd = l_status;
        // Get new connected client conn
        nconn *l_new_nconn = NULL;
        l_new_nconn = get_new_client_conn(l_fd, a_nconn.get_scheme(), a_hconn.m_url_router);
        if(!l_new_nconn)
        {
                //NDBG_PRINT("Error performing get_new_client_conn");
                return STATUS_ERROR;
        }

        if(!get_from_pool_if_null(a_hconn.m_in_q, m_nbq_pool))
        {
                a_hconn.m_in_q->reset();
        }

        // Set connected
        l_status = l_new_nconn->nc_set_accepting(m_evr_loop, l_fd);
        if(l_status != STATUS_OK)
        {
                NDBG_PRINT("Error: performing run_state_machine\n");
                cleanup_hconn(*(static_cast<hconn *>(l_new_nconn->get_data())));
                return STATUS_ERROR;
        }

        return STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
#ifdef ASYNC_DNS_SUPPORT
int32_t t_hlx::async_dns_init(void)
{
        if(m_async_dns_is_initd)
        {
                return STATUS_OK;
        }
        if(!m_t_conf || !(m_t_conf->m_hlx) || !m_t_conf->m_hlx->get_nresolver())
        {
                return STATUS_ERROR;
        }
        nresolver *l_nresolver = NULL;
        l_nresolver = m_t_conf->m_hlx->get_nresolver();
        //NDBG_PRINT("Initializing...\n");
        int32_t l_status;
        l_status = l_nresolver->init_async(&m_async_dns_ctx, m_async_dns_fd);
        if(l_status != STATUS_OK)
        {
                return STATUS_ERROR;
        }
        //NDBG_PRINT("m_async_dns_ctx: %p\n", m_async_dns_ctx);
        // close fd on exec
        //l_status = fcntl(m_fd, F_SETFD, FD_CLOEXEC);
        //if (l_status == -1)
        //{
        //        NDBG_PRINT("Error fcntl(FD_CLOEXEC) failed: %s\n", strerror(errno));
        //        return STATUS_ERROR;
        //}
        // Set non-blocking
        l_status = fcntl(m_async_dns_fd, F_SETFL, O_NONBLOCK | O_RDWR);
        if (l_status == -1)
        {
                NDBG_PRINT("Error fcntl(FD_CLOEXEC) failed: %s\n", strerror(errno));
                return STATUS_ERROR;
        }
        // Create fake connection object to work with hlx event handlers
        m_async_dns_nconn = new nconn_tcp();
        m_async_dns_nconn->set_id(nresolver::S_RESOLVER_ID);
        m_async_dns_nconn->nc_set_listening_nb(m_evr_loop, m_async_dns_fd);
        hconn *l_hconn = get_hconn(NULL, HCONN_TYPE_CLIENT, false);
        if(!l_hconn)
        {
                NDBG_PRINT("Error: performing config_conn\n");
                return STATUS_ERROR;
        }
        l_hconn->m_nconn = m_async_dns_nconn;
        m_async_dns_nconn->set_data(l_hconn);
        m_async_dns_is_initd = true;
        return STATUS_OK;
}
#endif

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
#ifdef ASYNC_DNS_SUPPORT
int32_t t_hlx::async_dns_handle_ev(void)
{
        if(!m_t_conf ||
           !(m_t_conf->m_hlx) ||
           !(m_t_conf->m_hlx->get_nresolver()) ||
           !(m_async_dns_ctx))
        {

                return STATUS_ERROR;
        }
        nresolver *l_nresolver = static_cast<nresolver *>(m_t_conf->m_hlx->get_nresolver());
        int32_t l_status;
        l_status = l_nresolver->handle_io(m_async_dns_ctx);
        if(l_status != STATUS_OK)
        {
                return STATUS_ERROR;
        }

        // Try dequeue any q'd dns req's
        std::string l_unused;
        l_status = async_dns_lookup(l_unused, 0, NULL);
        if(l_status != STATUS_OK)
        {
                return STATUS_ERROR;
        }
        return STATUS_OK;
}
#endif

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
#ifdef ASYNC_DNS_SUPPORT
int32_t t_hlx::async_dns_lookup(const std::string &a_host, uint16_t a_port, void *a_data)
{
        if(!m_t_conf || !m_t_conf->m_hlx || !m_t_conf->m_hlx->get_nresolver())
        {
                return -1;
        }
        nresolver *l_nresolver = static_cast<nresolver *>(m_t_conf->m_hlx->get_nresolver());
        int32_t l_status;
        l_status = l_nresolver->lookup_async(m_async_dns_ctx,
                                             a_host,
                                             a_port,
                                             async_dns_resolved_cb,
                                             a_data,
                                             m_stat.m_num_ups_resolve_active,
                                             m_lookup_job_q,
                                             m_lookup_job_pq);
        if(l_status != STATUS_OK)
        {
                return STATUS_ERROR;
        }

        // Add timer to handle timeouts
        if(m_stat.m_num_ups_resolve_active && !m_async_dns_timer_obj)
        {
                m_evr_loop->add_timer(1000,                     // Timeout ms
                                      evr_file_timeout_cb,      // timeout cb
                                      m_async_dns_nconn,        // data *
                                      &m_async_dns_timer_obj);  // timer obj
        }
        return STATUS_OK;
}
#endif

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
#ifdef ASYNC_DNS_SUPPORT
int32_t t_hlx::async_dns_resolved_cb(const host_info *a_host_info, void *a_data)
{
        //NDBG_PRINT("%sRESOLVE_CB%s: HERE\n", ANSI_COLOR_FG_WHITE, ANSI_COLOR_OFF);
        subr *l_subr = static_cast<subr *>(a_data);
        if(!l_subr)
        {
                //NDBG_PRINT("Error subr == NULL\n");
                return STATUS_ERROR;
        }
        t_hlx *l_t_hlx = l_subr->get_t_hlx();
        if(!l_t_hlx)
        {
                //NDBG_PRINT("Error l_t_hlx == NULL\n");
                // TODO Cleanup subr???
                return STATUS_ERROR;
        }
        // TODO DEBUG???
        //l_t_hlx->m_stat.m_subr_pending_resolv_map.erase(l_subr->get_label());
        if(!a_host_info)
        {
                //NDBG_PRINT("Error a_host_info == NULL --HOST: %s\n", l_subr->get_host().c_str());
                //NDBG_PRINT("Error l_host_info null\n");
                ++(l_t_hlx->m_stat.m_num_errors);
                l_subr->bump_num_requested();
                l_subr->bump_num_completed();
                subr::error_cb_t l_error_cb = l_subr->get_error_cb();
                if(l_error_cb)
                {
                        nconn_tcp l_nconn;
                        l_nconn.set_status(CONN_STATUS_ERROR_ADDR_LOOKUP_FAILURE);
                        l_error_cb(*l_subr, l_nconn);
                        // TODO Check status...
                        // release subr
                }
                // TODO if not detached delete subr...
                return STATUS_OK;
        }
        //NDBG_PRINT("l_subr: %p -HOST: %s\n", l_subr, l_subr->get_host().c_str());
        l_subr->set_host_info(*a_host_info);
        ++(l_t_hlx->m_stat.m_num_ups_resolved);

        // Special handling for DUPE'd subr's
        if(l_subr->get_type() == SUBR_TYPE_DUPE)
        {
                l_t_hlx->m_subr_queue.push(l_subr);
        }
        l_t_hlx->subr_add(*l_subr);
        return STATUS_OK;
}
#endif

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t t_hlx::subr_try_start(subr &a_subr)
{
        //NDBG_PRINT("l_subr label: %s --HOST: %s\n", l_subr->get_label().c_str(), l_subr->get_host().c_str());
        // Only run on resolved
        int32_t l_status;
        std::string l_error;

        // Try get idle from proxy pool
        nconn *l_nconn = NULL;
        l_nconn = m_nconn_proxy_pool.get_idle(a_subr.get_label());
        if(!l_nconn)
        {
                // Check for available proxy connections
                // If we maxed out -try again later...
                if(!m_nconn_proxy_pool.num_free())
                {
                        return STATUS_AGAIN;
                }

                nresolver *l_nresolver = m_t_conf->m_hlx->get_nresolver();
                if(!l_nresolver)
                {
                        //NDBG_PRINT("Error no resolver\n");
                        return STATUS_ERROR;
                }

                // Try fast
                host_info l_host_info;
                l_status = l_nresolver->lookup_tryfast(a_subr.get_host(),
                                                       a_subr.get_port(),
                                                       l_host_info);
                if(l_status != STATUS_OK)
                {
#ifdef ASYNC_DNS_SUPPORT
                        // If try fast fails lookup async
                        if(!m_async_dns_is_initd)
                        {
                                l_status = async_dns_init();
                                if(l_status != STATUS_OK)
                                {
                                        return STATUS_ERROR;
                                }
                        }

                        // TODO DEBUG???
                        //m_stat.m_subr_pending_resolv_map[a_subr.get_label()] = &a_subr;

                        ++(m_stat.m_num_ups_resolve_req);
                        //NDBG_PRINT("%sl_subr label%s: %s --HOST: %s\n", ANSI_COLOR_BG_RED, ANSI_COLOR_OFF, a_subr.get_label().c_str(), a_subr.get_host().c_str());
                        l_status = async_dns_lookup(a_subr.get_host(), a_subr.get_port(), &a_subr);
                        if(l_status != STATUS_OK)
                        {
                                //NDBG_PRINT("Error performing async_dns_lookup\n");
                                return STATUS_ERROR;
                        }
                        return STATUS_QUEUED_ASYNC_DNS;
#else
                        // sync dns
                        l_status = l_nresolver->lookup_sync(a_subr.get_host(), a_subr.get_port(), l_host_info);
                        if(l_status != STATUS_OK)
                        {
                                //NDBG_PRINT("Error: performing lookup_sync\n");
                                ++m_stat.m_num_errors;
                                a_subr.bump_num_requested();
                                a_subr.bump_num_completed();
                                subr::error_cb_t l_error_cb = a_subr.get_error_cb();
                                if(l_error_cb)
                                {
                                        nconn_tcp l_nconn_status;
                                        l_nconn_status.set_status(CONN_STATUS_ERROR_ADDR_LOOKUP_FAILURE);
                                        l_error_cb(a_subr, l_nconn_status);
                                }
                                m_subr_queue.pop();
                                return STATUS_ERROR;
                        }
                        else
                        {
                                ++(m_stat.m_num_ups_resolved);
                        }
#endif
                }

                // Get new connection
                l_nconn = m_nconn_proxy_pool.get(a_subr.get_scheme());
                if(!l_nconn)
                {
                        //NDBG_PRINT("Returning NULL\n");
                        return STATUS_AGAIN;
                }
                ++m_stat.m_cur_ups_conn_count;
                // Configure connection
                l_status = config_conn(*l_nconn,
                                       NULL,
                                       HCONN_TYPE_UPSTREAM,
                                       a_subr.get_save(),
                                       a_subr.get_connect_only());
                if(l_status != STATUS_OK)
                {
                        m_nconn_proxy_pool.release(l_nconn);
                        //NDBG_PRINT("Error performing config_conn\n");
                        return STATUS_ERROR;
                }
                l_nconn->set_host_info(l_host_info);
                a_subr.set_host_info(l_host_info);
                // Reset stats
                l_nconn->reset_stats();
        }
        // If we grabbed an idle connection spoof the connect time
        // for stats
        else
        {
                // Reset stats
                l_nconn->reset_stats();
                if(l_nconn->get_collect_stats_flag())
                {
                        l_nconn->set_connect_start_time_us(get_time_us());
                }
        }


        //NDBG_PRINT("%sSTARTING CONNECTION%s: HOST: %s\n",
        //                ANSI_COLOR_FG_RED, ANSI_COLOR_OFF, a_subr.get_host().c_str());
        // Configure connection for subr
        // Set ssl options
        if(l_nconn->get_scheme() == SCHEME_TLS)
        {
                bool l_val;
                l_val = a_subr.get_tls_verify();
                T_HLX_SET_NCONN_OPT((*l_nconn),
                                    nconn_tls::OPT_TLS_VERIFY,
                                    &(l_val),
                                    sizeof(bool));
                l_val = a_subr.get_tls_self_ok();
                T_HLX_SET_NCONN_OPT((*l_nconn),
                                    nconn_tls::OPT_TLS_VERIFY_ALLOW_SELF_SIGNED,
                                    &(l_val),
                                    sizeof(bool));
                l_val = a_subr.get_tls_no_host_check();
                T_HLX_SET_NCONN_OPT((*l_nconn),
                                    nconn_tls::OPT_TLS_VERIFY_NO_HOST_CHECK,
                                    &(l_val),
                                    sizeof(bool));
                l_val = a_subr.get_tls_sni();
                T_HLX_SET_NCONN_OPT((*l_nconn),
                                    nconn_tls::OPT_TLS_SNI,
                                    &(l_val),
                                    sizeof(bool));
                if(!a_subr.get_hostname().empty())
                {
                        T_HLX_SET_NCONN_OPT((*l_nconn),
                                            nconn_tls::OPT_TLS_HOSTNAME,
                                            a_subr.get_hostname().c_str(),
                                            a_subr.get_hostname().length());
                }
                else
                {
                        T_HLX_SET_NCONN_OPT((*l_nconn),
                                            nconn_tls::OPT_TLS_HOSTNAME,
                                            a_subr.get_host().c_str(),
                                            a_subr.get_host().length());
                }
        }

        hconn *l_hconn = static_cast<hconn *>(l_nconn->get_data());
        if(l_hconn)
        {
                //l_hconn->m_resp.clear();
                //l_hconn->m_rqst.clear();
        }
        else
        {
                l_hconn = get_hconn(NULL, HCONN_TYPE_UPSTREAM, a_subr.get_save());
                if(!l_hconn)
                {
                        NDBG_PRINT("Error performing get_hconn\n");
                        return STATUS_ERROR;
                }
        }

        l_nconn->set_data(l_hconn);
        l_nconn->set_read_cb(http_parse);

        l_hconn->m_nconn = l_nconn;
        l_hconn->m_subr = &a_subr;
        l_nconn->set_label(a_subr.get_label());
        l_status = subr_start(a_subr, *l_hconn, *l_nconn);
        if(l_status != STATUS_OK)
        {
                //NDBG_PRINT("Error performing request\n");
                return STATUS_ERROR;
        }
        else
        {
                a_subr.bump_num_requested();
                return STATUS_OK;
        }
        return STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t t_hlx::subr_try_deq(void)
{
        while(m_subr_queue.size() &&
              !m_stopped)
        {
                subr *l_subr = m_subr_queue.front();
                if(!l_subr)
                {
                        m_subr_queue.pop();
                        continue;
                }
                int32_t l_status;
                l_status = subr_try_start(*l_subr);
                if(l_status == STATUS_OK)
                {
                        if(l_subr->get_is_pending_done())
                        {
                                //NDBG_PRINT("POP'ing: host: %s\n",
                                //           l_subr->get_label().c_str());
                                m_subr_queue.pop();
                        }
                }
                else if(l_status == STATUS_AGAIN)
                {
                        // break since ran out of available connections
                        break;
                }
                else if(l_status == STATUS_QUEUED_ASYNC_DNS)
                {
                        m_subr_queue.pop();
                        continue;
                }
                else
                {
                        m_subr_queue.pop();
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

        // Pre-queue'd subrequests
        l_status = subr_try_deq();
        if(l_status != STATUS_OK)
        {
                //NDBG_PRINT("Error performing subr_try_deq.\n");
                //return NULL;
        }

        // -------------------------------------------------
        // Run server
        // -------------------------------------------------
        while(!m_stopped)
        {
                //NDBG_PRINT("Running.\n");
                ++m_stat.m_num_run;
                l_status = m_evr_loop->run();
                if(l_status != STATUS_OK)
                {
                        // TODO log run failure???

                }

                // Subrequests
                l_status = subr_try_deq();
                if(l_status != STATUS_OK)
                {
                        //NDBG_PRINT("Error performing subr_try_deq.\n");
                        //return NULL;
                }

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
int32_t t_hlx::cleanup_hconn(hconn &a_hconn)
{
        //NDBG_PRINT("%sCLEANUP%s: a_hconn: %p -label: %s\n",
        //           ANSI_COLOR_BG_RED, ANSI_COLOR_OFF,
        //           &a_hconn, a_hconn.m_nconn->get_label().c_str());
        //NDBG_PRINT_BT();
        // Cancel last timer
        if(a_hconn.m_timer_obj)
        {
                m_evr_loop->cancel_timer(a_hconn.m_timer_obj);
                a_hconn.m_timer_obj = NULL;
        }
        //NDBG_PRINT("%sADDING_BACK%s: %u a_nconn: %p type: %d\n",
        //           ANSI_COLOR_BG_GREEN, ANSI_COLOR_OFF,
        //           (uint32_t)a_nconn.get_idx(), &a_nconn, a_type);
        //NDBG_PRINT_BT();
        // Add back to free list
        if(a_hconn.m_type == HCONN_TYPE_CLIENT)
        {
                --m_stat.m_cur_cln_conn_count;
                ++m_stat.m_num_cln_conn_completed;
                if(STATUS_OK != m_nconn_pool.release(a_hconn.m_nconn))
                {
                        return STATUS_ERROR;
                }
                a_hconn.m_nconn = NULL;

                if(a_hconn.m_hmsg)
                {
                        m_rqst_pool.release(static_cast<rqst *>(a_hconn.m_hmsg));
                        a_hconn.m_hmsg = NULL;
                }
        }
        else if(a_hconn.m_type == HCONN_TYPE_UPSTREAM)
        {
                --m_stat.m_cur_ups_conn_count;
                ++m_stat.m_num_ups_conn_completed;
                if(STATUS_OK != m_nconn_proxy_pool.release(a_hconn.m_nconn))
                {
                        return STATUS_ERROR;
                }
                if(a_hconn.m_hmsg)
                {
                        m_resp_pool.release(static_cast<resp *>(a_hconn.m_hmsg));
                        a_hconn.m_hmsg = NULL;
                }
        }
        if(a_hconn.m_in_q)
        {
                m_nbq_pool.release(a_hconn.m_in_q);
                a_hconn.m_in_q = NULL;
        }
        if(a_hconn.m_out_q)
        {
                m_nbq_pool.release(a_hconn.m_out_q);
                a_hconn.m_out_q = NULL;
        }
        m_hconn_pool.release(&a_hconn);
        return STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t t_hlx::config_conn(nconn &a_nconn,
                           url_router *a_url_router,
                           hconn_type_t a_type,
                           bool a_save,
                           bool a_connect_only)
{
        a_nconn.set_num_reqs_per_conn(m_t_conf->m_num_reqs_per_conn);
        a_nconn.set_collect_stats(m_t_conf->m_collect_stats);
        a_nconn.set_connect_only(a_connect_only);

        // -------------------------------------------
        // Set options
        // -------------------------------------------
        // Set generic options
        T_HLX_SET_NCONN_OPT((a_nconn),
                            nconn_tcp::OPT_TCP_RECV_BUF_SIZE,
                            NULL,
                            m_t_conf->m_sock_opt_recv_buf_size);
        T_HLX_SET_NCONN_OPT((a_nconn),
                            nconn_tcp::OPT_TCP_SEND_BUF_SIZE,
                            NULL,
                            m_t_conf->m_sock_opt_send_buf_size);
        T_HLX_SET_NCONN_OPT((a_nconn),
                            nconn_tcp::OPT_TCP_NO_DELAY,
                            NULL,
                            m_t_conf->m_sock_opt_no_delay);

        // Set ssl options
        if(a_nconn.get_scheme() == SCHEME_TLS)
        {
                if(a_type == HCONN_TYPE_CLIENT)
                {
                        T_HLX_SET_NCONN_OPT(a_nconn,
                                            nconn_tls::OPT_TLS_CIPHER_STR,
                                            m_t_conf->m_tls_server_ctx_cipher_list.c_str(),
                                            m_t_conf->m_tls_server_ctx_cipher_list.length());
                        T_HLX_SET_NCONN_OPT(a_nconn,
                                            nconn_tls::OPT_TLS_CTX,
                                            m_t_conf->m_tls_server_ctx,
                                            sizeof(m_t_conf->m_tls_server_ctx));
                        if(!m_t_conf->m_tls_server_ctx_crt.empty())
                        {
                                T_HLX_SET_NCONN_OPT(a_nconn,
                                                    nconn_tls::OPT_TLS_TLS_CRT,
                                                    m_t_conf->m_tls_server_ctx_crt.c_str(),
                                                    m_t_conf->m_tls_server_ctx_crt.length());
                        }
                        if(!m_t_conf->m_tls_server_ctx_key.empty())
                        {
                                T_HLX_SET_NCONN_OPT(a_nconn,
                                                    nconn_tls::OPT_TLS_TLS_KEY,
                                                    m_t_conf->m_tls_server_ctx_key.c_str(),
                                                    m_t_conf->m_tls_server_ctx_key.length());
                        }
                        //T_HLX_SET_NCONN_OPT(a_nconn,
                        //                    nconn_tls::OPT_TLS_OPTIONS,
                        //                    &(m_t_conf->m_tls_server_ctx_options),
                        //                    sizeof(m_t_conf->m_tls_server_ctx_options));
                }
                else if(a_type == HCONN_TYPE_UPSTREAM)
                {
                        T_HLX_SET_NCONN_OPT(a_nconn,
                                            nconn_tls::OPT_TLS_CIPHER_STR,
                                            m_t_conf->m_tls_client_ctx_cipher_list.c_str(),
                                            m_t_conf->m_tls_client_ctx_cipher_list.length());
                        T_HLX_SET_NCONN_OPT(a_nconn,
                                            nconn_tls::OPT_TLS_CTX,
                                            m_t_conf->m_tls_client_ctx,
                                            sizeof(m_t_conf->m_tls_client_ctx));
                }
        }
        return STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
hconn * t_hlx::get_hconn(url_router *a_url_router,
                         hconn_type_t a_type,
                         bool a_save)
{
        hconn *l_hconn = NULL;
        if(!get_from_pool_if_null(l_hconn, m_hconn_pool))
        {
                //NDBG_PRINT("REUSED!!!\n");
                //l_hconn->m_resp.clear();
                //l_hconn->m_rqst.clear();
        }

        //NDBG_PRINT("Adding http_data: %p.\n", l_hconn);
        l_hconn->m_t_hlx = this;
        l_hconn->m_url_router = a_url_router;
        l_hconn->m_timer_obj = NULL;
        l_hconn->m_save = a_save;
        l_hconn->m_verbose = m_t_conf->m_verbose;
        l_hconn->m_color = m_t_conf->m_color;
        l_hconn->m_type = a_type;
        l_hconn->m_status_code = 0;
        l_hconn->m_http_parser_settings.on_status = hp_on_status;
        l_hconn->m_http_parser_settings.on_message_complete = hp_on_message_complete;
        if(l_hconn->m_save)
        {
                l_hconn->m_http_parser_settings.on_message_begin = hp_on_message_begin;
                l_hconn->m_http_parser_settings.on_url = hp_on_url;
                l_hconn->m_http_parser_settings.on_header_field = hp_on_header_field;
                l_hconn->m_http_parser_settings.on_header_value = hp_on_header_value;
                l_hconn->m_http_parser_settings.on_headers_complete = hp_on_headers_complete;
                l_hconn->m_http_parser_settings.on_body = hp_on_body;
        }
        l_hconn->m_http_parser.data = l_hconn;
        if(a_type == HCONN_TYPE_CLIENT)
        {
                http_parser_init(&(l_hconn->m_http_parser), HTTP_REQUEST);
        }
        else
        {
                http_parser_init(&(l_hconn->m_http_parser), HTTP_RESPONSE);
        }
        return l_hconn;
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
        //NDBG_PRINT("%sSTATUS_CODE%s: %d\n",
        //           ANSI_COLOR_BG_GREEN, ANSI_COLOR_OFF, a_req_stat.m_status_code);
        ++m_stat.m_status_code_count_map[a_status_code];
}

} //namespace ns_hlx {
