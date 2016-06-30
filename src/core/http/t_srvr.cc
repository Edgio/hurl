//: ----------------------------------------------------------------------------
//: Copyright (C) 2014 Verizon.  All Rights Reserved.
//: All Rights Reserved
//:
//: \file:    t_srvr.cc
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
#include "t_srvr.h"
#include "clnt_session.h"
#include "nbq.h"
#include "nconn_tcp.h"
#include "nconn_tls.h"
#include "ndebug.h"
#include "hlx/stat.h"
#include "hlx/status.h"
#include "hlx/api_resp.h"
#include "hlx/lsnr.h"
#include "hlx/time_util.h"
#include "hlx/trace.h"
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>

//: ----------------------------------------------------------------------------
//: Macros
//: ----------------------------------------------------------------------------
#define CHECK_FOR_NULL_ERROR_DEBUG(_data) \
        do {\
                if(!_data) {\
                        NDBG_PRINT("Error.\n");\
                        return HLX_STATUS_ERROR;\
                }\
        } while(0);

#define CHECK_FOR_NULL_ERROR(_data) \
        do {\
                if(!_data) {\
                        return HLX_STATUS_ERROR;\
                }\
        } while(0);

#define T_SRVR_SET_NCONN_OPT(_conn, _opt, _buf, _len) \
        do { \
                int _status = 0; \
                _status = _conn.set_opt((_opt), (_buf), (_len)); \
                if (_status != nconn::NC_STATUS_OK) { \
                        TRC_ERROR("set_opt %d.  Status: %d.\n", \
                                   _opt, _status); \
                        delete &_conn;\
                        return HLX_STATUS_ERROR;\
                } \
        } while(0)

#define T_CLNT_SET_NCONN_OPT(_conn, _opt, _buf, _len) \
        do { \
                int _status = 0; \
                _status = _conn.set_opt((_opt), (_buf), (_len)); \
                if (_status != nconn::NC_STATUS_OK) { \
                        TRC_ERROR("set_opt %d.  Status: %d.\n", \
                                   _opt, _status); \
                        return NULL;\
                } \
        } while(0)

#define T_HLX_SET_NCONN_OPT(_conn, _opt, _buf, _len) \
        do { \
                int _status = 0; \
                _status = _conn.set_opt((_opt), (_buf), (_len)); \
                if (_status != nconn::NC_STATUS_OK) { \
                        TRC_ERROR("set_opt %d.  Status: %d.\n", \
                                   _opt, _status); \
                        m_nconn_proxy_pool.release(&_conn); \
                        return HLX_STATUS_ERROR;\
                } \
        } while(0)

//: ----------------------------------------------------------------------------
//: Constants
//: ----------------------------------------------------------------------------
#ifdef ASYNC_DNS_SUPPORT
#ifndef STATUS_QUEUED_ASYNC_DNS
#define STATUS_QUEUED_ASYNC_DNS -3
#endif
#endif

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
                ao_obj = a_pool.get_free();
                if(!ao_obj)
                {
                        ao_obj = new nbq(4096);
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
t_srvr::t_srvr(const t_conf *a_t_conf):
        m_t_run_thread(),
        m_orphan_in_q(NULL),
        m_orphan_out_q(NULL),
        m_stat(),
        m_upsv_status_code_count_map(),
        m_nconn_pool(S_POOL_ID_CLIENT, 512,1000000),
        m_nconn_proxy_pool(S_POOL_ID_UPS_PROXY, a_t_conf->m_num_parallel,4096),
        m_clnt_session_pool(),
        m_ups_srvr_session_pool(),
        m_t_conf(a_t_conf),
        m_stopped(true),
        m_start_time_s(0),
        m_evr_loop(NULL),
        m_scheme(SCHEME_TCP),
        m_listening_nconn_list(),
        m_subr_list(),
        m_subr_list_size(0),
        m_nbq_pool(),
        m_resp_pool(),
        m_rqst_pool(),
#ifdef ASYNC_DNS_SUPPORT
        m_adns_ctx(NULL),
#endif
        m_is_initd(false),
        m_stat_copy(),
        m_upsv_status_code_count_map_copy(),
        m_stat_copy_mutex(),
        m_clnt_session_writeable_data(NULL)
{
        pthread_mutex_init(&m_stat_copy_mutex, NULL);

        m_orphan_in_q = get_nbq();
        m_orphan_out_q = get_nbq();

}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
t_srvr::~t_srvr()
{
        m_nconn_pool.evict_all_idle();
        m_nconn_proxy_pool.evict_all_idle();
        if(m_evr_loop)
        {
                delete m_evr_loop;
                m_evr_loop = NULL;
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
        while(!m_subr_list.empty())
        {
                subr *l_subr = m_subr_list.front();
                if(l_subr)
                {
                        delete l_subr;
                }
                //m_subr_list.pop_front();
                subr_dequeue();
        }
#ifdef ASYNC_DNS_SUPPORT
        if(m_t_conf && m_t_conf->m_srvr && m_t_conf->m_srvr->get_nresolver())
        {
                m_t_conf->m_srvr->get_nresolver()->destroy_async(m_adns_ctx);
        }
#endif
        pthread_mutex_destroy(&m_stat_copy_mutex);
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t t_srvr::init(void)
{
        if(m_is_initd)
        {
                return HLX_STATUS_OK;
        }

        // Create loop
        m_evr_loop = new evr_loop(m_t_conf->m_evr_loop_type,
                                  512); // TODO Need to make epoll vector resizeable...
        if(!m_evr_loop)
        {
                TRC_ERROR("m_evr_loop == NULL");
                return HLX_STATUS_ERROR;
        }

        // TODO check !NULL
#ifdef ASYNC_DNS_SUPPORT
        if(!m_adns_ctx)
        {
                nresolver *l_nresolver = m_t_conf->m_srvr->get_nresolver();
                if(!l_nresolver)
                {
                        TRC_ERROR("l_nresolver == NULL\n");
                        return HLX_STATUS_ERROR;
                }
                m_adns_ctx = l_nresolver->get_new_adns_ctx(m_evr_loop, adns_resolved_cb);
                if(!m_adns_ctx)
                {
                        TRC_ERROR("performing get_new_adns_ctx\n");
                        return HLX_STATUS_ERROR;
                }
        }
#endif

        m_is_initd = true;
        return HLX_STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t t_srvr::add_lsnr(lsnr &a_lsnr)
{
        int32_t l_status;
        l_status = init();
        if(l_status != HLX_STATUS_OK)
        {
                //NDBG_PRINT("Error performing init.\n");
                return HLX_STATUS_ERROR;
        }
        nconn *l_nconn = NULL;
        l_nconn = nconn_pool::s_create_new_conn(a_lsnr.get_scheme());
        l_nconn->set_ctx(this);
        l_nconn->set_num_reqs_per_conn(m_t_conf->m_num_reqs_per_conn);
        l_nconn->set_collect_stats(m_t_conf->m_collect_stats);
        l_nconn->set_connect_only(false);
        l_nconn->setup_evr_fd(evr_fd_readable_lsnr_cb,
                              NULL,
                              NULL);

        T_SRVR_SET_NCONN_OPT((*l_nconn),nconn_tcp::OPT_TCP_RECV_BUF_SIZE,NULL,m_t_conf->m_sock_opt_recv_buf_size);
        T_SRVR_SET_NCONN_OPT((*l_nconn),nconn_tcp::OPT_TCP_SEND_BUF_SIZE,NULL,m_t_conf->m_sock_opt_send_buf_size);
        T_SRVR_SET_NCONN_OPT((*l_nconn),nconn_tcp::OPT_TCP_NO_DELAY,NULL,m_t_conf->m_sock_opt_no_delay);
        if(l_nconn->get_scheme() == SCHEME_TLS)
        {
                T_SRVR_SET_NCONN_OPT((*l_nconn),nconn_tls::OPT_TLS_CIPHER_STR,m_t_conf->m_tls_server_ctx_cipher_list.c_str(),m_t_conf->m_tls_server_ctx_cipher_list.length());
                T_SRVR_SET_NCONN_OPT((*l_nconn),nconn_tls::OPT_TLS_CTX,m_t_conf->m_tls_server_ctx,sizeof(m_t_conf->m_tls_server_ctx));
                if(!m_t_conf->m_tls_server_ctx_crt.empty())
                {
                        T_SRVR_SET_NCONN_OPT((*l_nconn),nconn_tls::OPT_TLS_TLS_CRT,m_t_conf->m_tls_server_ctx_crt.c_str(),m_t_conf->m_tls_server_ctx_crt.length());
                }
                if(!m_t_conf->m_tls_server_ctx_key.empty())
                {
                        T_SRVR_SET_NCONN_OPT((*l_nconn),nconn_tls::OPT_TLS_TLS_KEY,m_t_conf->m_tls_server_ctx_key.c_str(),m_t_conf->m_tls_server_ctx_key.length());
                }
                T_SRVR_SET_NCONN_OPT((*l_nconn),nconn_tls::OPT_TLS_OPTIONS,&(m_t_conf->m_tls_server_ctx_options),sizeof(m_t_conf->m_tls_server_ctx_options));
        }

        l_nconn->set_data(&a_lsnr);
        l_nconn->set_evr_loop(m_evr_loop);
        l_status = l_nconn->nc_set_listening(a_lsnr.get_fd());
        if(l_status != HLX_STATUS_OK)
        {
                if(l_nconn)
                {
                        delete l_nconn;
                        l_nconn = NULL;
                }
                //NDBG_PRINT("Error performing nc_set_listening.\n");
                return HLX_STATUS_ERROR;
        }
        m_listening_nconn_list.push_back(l_nconn);
        return HLX_STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t t_srvr::subr_add(subr &a_subr)
{
        a_subr.set_t_srvr(this);
        //NDBG_PRINT("Adding subreq.\n");
        if(m_stopped)
        {
                a_subr.set_state(subr::SUBR_STATE_QUEUED);
                //m_subr_list.push_back(&a_subr);
                subr_enqueue(a_subr);
                a_subr.set_i_q(--(m_subr_list.end()));
        }
        else
        {
                int32_t l_status;
                l_status = subr_try_start(a_subr);
                if(l_status == HLX_STATUS_AGAIN)
                {
                        a_subr.set_state(subr::SUBR_STATE_QUEUED);
                        //m_subr_list.push_back(&a_subr);
                        subr_enqueue(a_subr);
                        a_subr.set_i_q(--(m_subr_list.end()));
                        return HLX_STATUS_OK;
                }
                else if(l_status == STATUS_QUEUED_ASYNC_DNS)
                {
                        a_subr.set_state(subr::SUBR_STATE_DNS_LOOKUP);
                        a_subr.set_i_q(m_subr_list.end());
                        return HLX_STATUS_OK;
                }
                else if(l_status ==  HLX_STATUS_OK)
                {
                        return HLX_STATUS_OK;
                }
                else
                {
                        return HLX_STATUS_ERROR;
                }
        }
        return HLX_STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t t_srvr::queue_api_resp(api_resp &a_api_resp, clnt_session &a_clnt_session)
{
        if(!a_clnt_session.m_out_q)
        {
                bool l_new;
                l_new = get_from_pool_if_null(a_clnt_session.m_out_q, m_nbq_pool);
                if(!l_new)
                {
                        a_clnt_session.m_out_q->reset_write();
                }
        }

        // access info
        a_clnt_session.m_access_info.m_resp_status = a_api_resp.get_status();

        int32_t l_s;
        l_s = a_api_resp.serialize(*a_clnt_session.m_out_q);
        if(l_s != HLX_STATUS_OK)
        {
                return HLX_STATUS_ERROR;
        }
        m_clnt_session_writeable_data = a_clnt_session.m_nconn;
        return HLX_STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t t_srvr::queue_output(clnt_session &a_clnt_session)
{
        m_clnt_session_writeable_data = a_clnt_session.m_nconn;
        return HLX_STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t t_srvr::subr_start(subr &a_subr, ups_srvr_session &a_ups_srvr_session, nconn &a_nconn)
{
        int32_t l_status;
        //NDBG_PRINT("TID[%lu]: %sSTART%s: Host: %s l_nconn: %p\n",
        //                pthread_self(),
        //                ANSI_COLOR_BG_BLUE, ANSI_COLOR_OFF,
        //                a_subr.get_label().c_str(),
        //                &a_nconn);
        resp *l_resp = a_ups_srvr_session.m_resp;
        get_from_pool_if_null(l_resp, m_resp_pool);
        l_resp->init(a_subr.get_save());
        l_resp->m_http_parser->data = l_resp;
        a_nconn.set_read_cb(http_parse);
        a_nconn.set_read_cb_data(l_resp);
        a_ups_srvr_session.m_resp = l_resp;
        a_ups_srvr_session.m_resp->m_expect_resp_body_flag = a_subr.get_expect_resp_body_flag();
        a_ups_srvr_session.m_rqst_resp_logging = m_t_conf->m_rqst_resp_logging;
        a_ups_srvr_session.m_rqst_resp_logging_color = m_t_conf->m_rqst_resp_logging_color;

        // Create request
        if(!a_subr.get_connect_only())
        {
                if(!get_from_pool_if_null(a_ups_srvr_session.m_in_q, m_nbq_pool))
                {
                        a_ups_srvr_session.m_in_q->reset_write();
                }

                a_ups_srvr_session.m_resp->set_q(a_ups_srvr_session.m_in_q);

                if(!a_ups_srvr_session.m_out_q)
                {
                        if(!get_from_pool_if_null(a_ups_srvr_session.m_out_q, m_nbq_pool))
                        {
                                a_ups_srvr_session.m_out_q->reset_write();
                        }
                        subr::create_req_cb_t l_create_req_cb = a_subr.get_create_req_cb();
                        if(l_create_req_cb)
                        {
                                l_status = l_create_req_cb(a_subr, *a_ups_srvr_session.m_out_q);
                                if(HLX_STATUS_OK != l_status)
                                {
                                        return HLX_STATUS_ERROR;
                                }
                        }
                }
                else
                {
                        if(a_subr.get_is_multipath())
                        {
                                // Reset in data
                                a_ups_srvr_session.m_out_q->reset_write();
                                subr::create_req_cb_t l_create_req_cb = a_subr.get_create_req_cb();
                                if(l_create_req_cb)
                                {
                                        l_status = l_create_req_cb(a_subr, *a_ups_srvr_session.m_out_q);
                                        if(HLX_STATUS_OK != l_status)
                                        {
                                                return HLX_STATUS_ERROR;
                                        }
                                }
                        }
                        else
                        {
                                a_ups_srvr_session.m_out_q->reset_read();
                        }
                }

                // Display data from out q
                if(a_ups_srvr_session.m_rqst_resp_logging)
                {
                        if(a_ups_srvr_session.m_rqst_resp_logging_color) TRC_OUTPUT("%s", ANSI_COLOR_FG_YELLOW);
                        a_ups_srvr_session.m_out_q->print();
                        if(a_ups_srvr_session.m_rqst_resp_logging_color) TRC_OUTPUT("%s", ANSI_COLOR_OFF);
                }
        }
        ++m_stat.m_upsv_reqs;
        // Set start time
        if(a_subr.get_kind() != subr::SUBR_KIND_DUPE)
        {
                a_subr.set_start_time_ms(get_time_ms());
        }
        l_status = add_timer(a_subr.get_timeout_ms(),
                             ups_srvr_session::evr_fd_timeout_cb,
                             &a_nconn,
                             (void **)&(a_ups_srvr_session.m_timer_obj));
        if(l_status != HLX_STATUS_OK)
        {
                //NDBG_PRINT("Error: Performing add_timer\n");
                return HLX_STATUS_ERROR;
        }
        //NDBG_PRINT("g_client_req_num: %d\n", ++g_client_req_num);
        //NDBG_PRINT("%sCONNECT%s: %s --data: %p\n",
        //           ANSI_COLOR_BG_MAGENTA, ANSI_COLOR_OFF,
        //           a_subr.m_host.c_str(), a_nconn.get_data());
        l_status = a_nconn.nc_run_state_machine(nconn::NC_MODE_WRITE,
                                                a_ups_srvr_session.m_in_q,
                                                a_ups_srvr_session.m_out_q);
        a_nconn.bump_num_requested();
        if(l_status == nconn::NC_STATUS_ERROR)
        {
                ++(m_stat.m_upsv_errors);
                return ups_srvr_session::teardown(this, &a_ups_srvr_session, &a_nconn, HTTP_STATUS_INTERNAL_SERVER_ERROR);
        }
        else if(l_status > 0)
        {
                m_stat.m_upsv_bytes_written += l_status;
        }

        // Get request time
        if(a_nconn.get_collect_stats_flag())
        {
                a_nconn.set_request_start_time_us(get_time_us());
        }

        return HLX_STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
nconn *t_srvr::get_new_client_conn(scheme_t a_scheme, lsnr *a_lsnr)
{
        nconn *l_nconn;
        l_nconn = m_nconn_pool.get_new_active("CLIENT", a_scheme);
        if(!l_nconn)
        {
                TRC_ERROR("performing m_nconn_pool.get\n");
                return NULL;
        }
        // ---------------------------------------
        // connection setup
        // ---------------------------------------
        l_nconn->set_ctx(this);
        l_nconn->set_num_reqs_per_conn(m_t_conf->m_num_reqs_per_conn);
        l_nconn->set_collect_stats(m_t_conf->m_collect_stats);
        l_nconn->set_connect_only(false);
        l_nconn->set_evr_loop(m_evr_loop);
        l_nconn->setup_evr_fd(clnt_session::evr_fd_readable_cb,
                              clnt_session::evr_fd_writeable_cb,
                              clnt_session::evr_fd_error_cb);
        T_CLNT_SET_NCONN_OPT((*l_nconn),nconn_tcp::OPT_TCP_RECV_BUF_SIZE,NULL,m_t_conf->m_sock_opt_recv_buf_size);
        T_CLNT_SET_NCONN_OPT((*l_nconn),nconn_tcp::OPT_TCP_SEND_BUF_SIZE,NULL,m_t_conf->m_sock_opt_send_buf_size);
        T_CLNT_SET_NCONN_OPT((*l_nconn),nconn_tcp::OPT_TCP_NO_DELAY,NULL,m_t_conf->m_sock_opt_no_delay);
        if(l_nconn->get_scheme() == SCHEME_TLS)
        {
                T_CLNT_SET_NCONN_OPT((*l_nconn),nconn_tls::OPT_TLS_CIPHER_STR,m_t_conf->m_tls_server_ctx_cipher_list.c_str(),m_t_conf->m_tls_server_ctx_cipher_list.length());
                T_CLNT_SET_NCONN_OPT((*l_nconn),nconn_tls::OPT_TLS_CTX,m_t_conf->m_tls_server_ctx,sizeof(m_t_conf->m_tls_server_ctx));
                if(!m_t_conf->m_tls_server_ctx_crt.empty())
                {
                        T_CLNT_SET_NCONN_OPT((*l_nconn),nconn_tls::OPT_TLS_TLS_CRT,m_t_conf->m_tls_server_ctx_crt.c_str(),m_t_conf->m_tls_server_ctx_crt.length());
                }
                if(!m_t_conf->m_tls_server_ctx_key.empty())
                {
                        T_CLNT_SET_NCONN_OPT((*l_nconn),nconn_tls::OPT_TLS_TLS_KEY,m_t_conf->m_tls_server_ctx_key.c_str(),m_t_conf->m_tls_server_ctx_key.length());
                }
                T_CLNT_SET_NCONN_OPT((*l_nconn),nconn_tls::OPT_TLS_OPTIONS,&(m_t_conf->m_tls_server_ctx_options),sizeof(m_t_conf->m_tls_server_ctx_options));
        }

        // ---------------------------------------
        // clnt session setup
        // ---------------------------------------
        clnt_session *l_clnt_session = static_cast<clnt_session *>(l_nconn->get_data());
        if(!l_clnt_session)
        {
                if(!get_from_pool_if_null(l_clnt_session, m_clnt_session_pool))
                {
                        //NDBG_PRINT("REUSED!!!\n");
                }
        }
        l_clnt_session->m_t_srvr = this;
        l_clnt_session->m_lsnr = a_lsnr;
        l_clnt_session->m_timer_obj = NULL;
        l_clnt_session->m_rqst_resp_logging = m_t_conf->m_rqst_resp_logging;
        l_clnt_session->m_rqst_resp_logging_color = m_t_conf->m_rqst_resp_logging_color;
        l_clnt_session->m_resp_done_cb = m_t_conf->m_resp_done_cb;
        if(!get_from_pool_if_null(l_clnt_session->m_in_q, m_nbq_pool))
        {
                l_clnt_session->m_in_q->reset_write();
        }
        l_clnt_session->m_out_q = NULL;
        l_clnt_session->m_nconn = l_nconn;
        l_nconn->set_data(l_clnt_session);

        // ---------------------------------------
        // rqst setup
        // ---------------------------------------
        if(!get_from_pool_if_null(l_clnt_session->m_rqst, m_rqst_pool))
        {
                l_clnt_session->m_rqst->init(true);
        }
        l_nconn->set_read_cb(http_parse);
        l_nconn->set_read_cb_data(l_clnt_session->m_rqst);
        l_clnt_session->m_rqst->set_q(l_clnt_session->m_in_q);

        // stats
        ++m_stat.m_clnt_conn_started;
        return l_nconn;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int t_srvr::run(void)
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
                return HLX_STATUS_ERROR;
        }
        return HLX_STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void t_srvr::signal(void)
{
        int32_t l_status;
        l_status = m_evr_loop->signal();
        if(l_status != HLX_STATUS_OK)
        {
                // TODO ???
        }
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void t_srvr::stop(void)
{
        m_stopped = true;
        signal();
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
#ifdef ASYNC_DNS_SUPPORT
int32_t t_srvr::adns_resolved_cb(const host_info *a_host_info, void *a_data)
{
        //NDBG_PRINT("%sRESOLVE_CB%s: a_host_info: %p a_data: %p\n",
        //                ANSI_COLOR_FG_WHITE, ANSI_COLOR_OFF,
        //                a_host_info, a_data);
        subr *l_subr = static_cast<subr *>(a_data);
        if(!l_subr)
        {
                TRC_ERROR("subr == NULL\n");
                return HLX_STATUS_ERROR;
        }
        l_subr->set_lookup_job(NULL);
        t_srvr *l_t_srvr = l_subr->get_t_srvr();
        if(!l_t_srvr)
        {
                // TODO Cleanup subr???
                TRC_ERROR("l_t_srvr == NULL\n");
                return HLX_STATUS_ERROR;
        }
        // TODO DEBUG???
        //l_t_srvr->m_stat.m_subr_pending_resolv_map.erase(l_subr->get_label());
        if(!a_host_info)
        {
                //NDBG_PRINT("a_host_info == NULL --HOST: %s\n", l_subr->get_host().c_str());
                //NDBG_PRINT("l_host_info null\n");
                ++(l_t_srvr->m_stat.m_upsv_errors);
                l_subr->bump_num_requested();
                l_subr->bump_num_completed();
                subr::error_cb_t l_error_cb = l_subr->get_error_cb();
                if(l_error_cb)
                {
                        l_error_cb(*l_subr, NULL, HTTP_STATUS_BAD_GATEWAY, "address lookup failure");
                        // TODO check status
                        return HLX_STATUS_OK;
                }
                return HLX_STATUS_OK;
        }
        //NDBG_PRINT("l_subr: %p -HOST: %s\n", l_subr, l_subr->get_host().c_str());
        l_subr->set_host_info(*a_host_info);
        ++(l_t_srvr->m_stat.m_dns_resolved);
        // Special handling for DUPE'd subr's
        if(l_subr->get_kind() == subr::SUBR_KIND_DUPE)
        {
                //l_t_srvr->m_subr_list.push_back(l_subr);
                l_t_srvr->subr_enqueue(*l_subr);
                l_subr->set_i_q(--(l_t_srvr->m_subr_list.end()));
        }
        l_t_srvr->subr_add(*l_subr);
        return HLX_STATUS_OK;
}
#endif

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t t_srvr::evr_fd_readable_lsnr_cb(void *a_data)
{
        //NDBG_PRINT("%sREADABLE%s %p\n", ANSI_COLOR_BG_GREEN, ANSI_COLOR_OFF, a_data);
        if(!a_data)
        {
                return HLX_STATUS_OK;
        }
        nconn* l_nconn = static_cast<nconn*>(a_data);
        CHECK_FOR_NULL_ERROR(l_nconn->get_ctx());
        t_srvr *l_t_srvr = static_cast<t_srvr *>(l_nconn->get_ctx());
        lsnr *l_lsnr = static_cast<lsnr *>(l_nconn->get_data());
        //NDBG_PRINT("%sREADABLE%s LABEL: %s LSNR: %p\n", ANSI_COLOR_FG_GREEN, ANSI_COLOR_OFF,
        //                l_nconn->get_label().c_str(), l_lsnr);
        // Server -incoming client connections
        if(!l_nconn->is_listening())
        {
                return HLX_STATUS_ERROR;
        }
        int32_t l_status;

        if(!l_lsnr || !l_nconn)
        {
                return HLX_STATUS_ERROR;
        }

        // Returns new client fd on success
        l_status = l_nconn->nc_run_state_machine(nconn::NC_MODE_NONE,
                                                 NULL,
                                                 NULL);
        if(l_status == nconn::NC_STATUS_ERROR)
        {
                return HLX_STATUS_ERROR;
        }

        // Get new connected client conn
        nconn *l_new_nconn = NULL;
        l_new_nconn = l_t_srvr->get_new_client_conn(l_nconn->get_scheme(), l_lsnr);
        if(!l_new_nconn)
        {
                //NDBG_PRINT("Error performing get_new_client_conn");
                return HLX_STATUS_ERROR;
        }
        clnt_session *l_clnt_session = static_cast<clnt_session *>(l_new_nconn->get_data());

        // ---------------------------------------
        // Set access info
        // TODO move to clnt_session???
        // ---------------------------------------
        l_nconn->get_remote_sa(l_clnt_session->m_access_info.m_conn_clnt_sa,
                               l_clnt_session->m_access_info.m_conn_clnt_sa_len);
        l_lsnr->get_sa(l_clnt_session->m_access_info.m_conn_upsv_sa,
                       l_clnt_session->m_access_info.m_conn_upsv_sa_len);
        l_clnt_session->m_access_info.m_start_time_ms = get_time_ms();
        l_clnt_session->m_access_info.m_total_time_ms = 0;
        l_clnt_session->m_access_info.m_bytes_in = 0;
        l_clnt_session->m_access_info.m_bytes_out = 0;

        // Set connected
        int l_fd = l_status;
        l_status = l_new_nconn->nc_set_accepting(l_fd);
        if(l_status != HLX_STATUS_OK)
        {
                //NDBG_PRINT("Error: performing run_state_machine\n");
                l_t_srvr->cleanup_clnt_session(l_clnt_session, l_new_nconn);
                // TODO Check return
                return HLX_STATUS_ERROR;
        }
        return HLX_STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t t_srvr::subr_try_start(subr &a_subr)
{
        //NDBG_PRINT("l_subr label: %s --HOST: %s\n", a_subr.get_label().c_str(), a_subr.get_host().c_str());
        // Only run on resolved
        int32_t l_status;
        std::string l_error;

        // Try get idle from proxy pool
        nconn *l_nconn = NULL;
        l_nconn = m_nconn_proxy_pool.get_idle(a_subr.get_label());
        //NDBG_PRINT("l_nconn: %p\n", l_nconn);
        if(!l_nconn)
        {
                // Check for available active connections
                // If we maxed out -try again later...
                if(!m_nconn_proxy_pool.get_active_available())
                {
                        return HLX_STATUS_AGAIN;
                }
                // TODO -reenable limits
#if 0
                // Check if we've exceeded max parallel
                // If we maxed out active connections for this label
                // -try again later...
                if((a_subr.get_max_parallel() > 0) &&
                   (m_nconn_proxy_pool.get_active_label(a_subr.get_label()) >= (uint64_t)a_subr.get_max_parallel()))
                {
                        // Send 503 (HTTP_STATUS_SERVICE_UNAVAILABLE) back to requester
                        subr::error_cb_t l_error_cb = a_subr.get_error_cb();
                        if(l_error_cb)
                        {
                                l_error_cb(a_subr,
                                           NULL,
                                           HTTP_STATUS_SERVICE_NOT_AVAILABLE,
                                           get_resp_status_str(HTTP_STATUS_SERVICE_NOT_AVAILABLE));
                                return HLX_STATUS_OK;
                        }
                        return HLX_STATUS_OK;
                }
#endif
                nresolver *l_nresolver = m_t_conf->m_srvr->get_nresolver();
                if(!l_nresolver)
                {
                        //NDBG_PRINT("Error no resolver\n");
                        return HLX_STATUS_ERROR;
                }

                // Try fast
                host_info l_host_info;
                l_status = l_nresolver->lookup_tryfast(a_subr.get_host(),
                                                       a_subr.get_port(),
                                                       l_host_info);
                if(l_status != HLX_STATUS_OK)
                {
#ifdef ASYNC_DNS_SUPPORT
                        if(!m_t_conf->m_srvr->get_dns_use_sync())
                        {
                                // TODO check !NULL
                                // If try fast fails lookup async
                                if(!m_adns_ctx)
                                {
                                        m_adns_ctx = l_nresolver->get_new_adns_ctx(m_evr_loop, adns_resolved_cb);
                                        if(!m_adns_ctx)
                                        {
                                                TRC_ERROR("performing get_new_adns_ctx\n");
                                                return HLX_STATUS_ERROR;
                                        }
                                }
                                //m_stat.m_subr_pending_resolv_map[a_subr.get_label()] = &a_subr;
                                ++(m_stat.m_dns_resolve_req);
                                //NDBG_PRINT("%sl_subr label%s: %s --HOST: %s\n", ANSI_COLOR_BG_RED, ANSI_COLOR_OFF, a_subr.get_label().c_str(), a_subr.get_host().c_str());
                                void *l_job_handle = NULL;
                                l_status = l_nresolver->lookup_async(m_adns_ctx,
                                                                     a_subr.get_host(),
                                                                     a_subr.get_port(),
                                                                     &a_subr,
                                                                     &l_job_handle);
                                if(l_status != HLX_STATUS_OK)
                                {
                                        return HLX_STATUS_ERROR;
                                }
                                if(l_job_handle)
                                {
                                        a_subr.set_lookup_job(l_job_handle);
                                }
                                return STATUS_QUEUED_ASYNC_DNS;
                        }
                        else
                        {
#endif
                        // sync dns
                        l_status = l_nresolver->lookup_sync(a_subr.get_host(), a_subr.get_port(), l_host_info);
                        if(l_status != HLX_STATUS_OK)
                        {
                                //NDBG_PRINT("Error: performing lookup_sync\n");
                                ++m_stat.m_upsv_errors;
                                a_subr.bump_num_requested();
                                a_subr.bump_num_completed();
                                subr::error_cb_t l_error_cb = a_subr.get_error_cb();
                                if(l_error_cb)
                                {
                                        l_error_cb(a_subr, NULL, HTTP_STATUS_BAD_GATEWAY, "address lookup failure");
                                        // TODO check status
                                }
                                return HLX_STATUS_ERROR;
                        }
                        else
                        {
                                ++(m_stat.m_dns_resolved);
                        }
#ifdef ASYNC_DNS_SUPPORT
                        }
#endif
                }
                // -----------------------------------------
                // connection setup
                // -----------------------------------------
                l_nconn = m_nconn_proxy_pool.get_new_active(a_subr.get_label(), a_subr.get_scheme());
                if(!l_nconn)
                {
                        //NDBG_PRINT("Returning NULL\n");
                        return HLX_STATUS_AGAIN;
                }
                l_nconn->set_ctx(this);
                l_nconn->set_num_reqs_per_conn(m_t_conf->m_num_reqs_per_conn);
                l_nconn->set_collect_stats(m_t_conf->m_collect_stats);
                l_nconn->set_connect_only(a_subr.get_connect_only());
                l_nconn->setup_evr_fd(ups_srvr_session::evr_fd_readable_cb,
                                      ups_srvr_session::evr_fd_writeable_cb,
                                      ups_srvr_session::evr_fd_error_cb);
                T_HLX_SET_NCONN_OPT((*l_nconn),nconn_tcp::OPT_TCP_RECV_BUF_SIZE,NULL,m_t_conf->m_sock_opt_recv_buf_size);
                T_HLX_SET_NCONN_OPT((*l_nconn),nconn_tcp::OPT_TCP_SEND_BUF_SIZE,NULL,m_t_conf->m_sock_opt_send_buf_size);
                T_HLX_SET_NCONN_OPT((*l_nconn),nconn_tcp::OPT_TCP_NO_DELAY,NULL,m_t_conf->m_sock_opt_no_delay);
                if(l_nconn->get_scheme() == SCHEME_TLS)
                {
                        T_HLX_SET_NCONN_OPT((*l_nconn),nconn_tls::OPT_TLS_CIPHER_STR,m_t_conf->m_tls_client_ctx_cipher_list.c_str(),m_t_conf->m_tls_client_ctx_cipher_list.length());
                        T_HLX_SET_NCONN_OPT((*l_nconn),nconn_tls::OPT_TLS_CTX,m_t_conf->m_tls_client_ctx,sizeof(m_t_conf->m_tls_client_ctx));
                }
                l_nconn->set_host_info(l_host_info);
                a_subr.set_host_info(l_host_info);
                // Reset stats
                l_nconn->reset_stats();

                ++m_stat.m_upsv_conn_started;
                m_stat.m_pool_proxy_conn_active = m_nconn_proxy_pool.get_active_size();

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
        if(l_nconn->get_scheme() == SCHEME_TLS)
        {
                bool l_val;
                l_val = a_subr.get_tls_verify();
                T_HLX_SET_NCONN_OPT((*l_nconn), nconn_tls::OPT_TLS_VERIFY, &(l_val), sizeof(bool));
                l_val = a_subr.get_tls_self_ok();
                T_HLX_SET_NCONN_OPT((*l_nconn), nconn_tls::OPT_TLS_VERIFY_ALLOW_SELF_SIGNED, &(l_val), sizeof(bool));
                l_val = a_subr.get_tls_no_host_check();
                T_HLX_SET_NCONN_OPT((*l_nconn), nconn_tls::OPT_TLS_VERIFY_NO_HOST_CHECK, &(l_val), sizeof(bool));
                l_val = a_subr.get_tls_sni();
                T_HLX_SET_NCONN_OPT((*l_nconn), nconn_tls::OPT_TLS_SNI, &(l_val), sizeof(bool));
                if(!a_subr.get_hostname().empty())
                {
                        T_HLX_SET_NCONN_OPT((*l_nconn), nconn_tls::OPT_TLS_HOSTNAME, a_subr.get_hostname().c_str(), a_subr.get_hostname().length());
                }
                else
                {
                        T_HLX_SET_NCONN_OPT((*l_nconn), nconn_tls::OPT_TLS_HOSTNAME, a_subr.get_host().c_str(), a_subr.get_host().length());
                }
        }

        ups_srvr_session *l_ups_srvr_session = NULL;
        if(!get_from_pool_if_null(l_ups_srvr_session, m_ups_srvr_session_pool))
        {
                //NDBG_PRINT("REUSED!!!\n");
        }

        //NDBG_PRINT("Adding http_data: %p.\n", l_clnt_session);
        l_ups_srvr_session->m_t_srvr = this;
        l_ups_srvr_session->m_timer_obj = NULL;

        // Setup nconn
        l_nconn->set_data(l_ups_srvr_session);
        l_nconn->set_evr_loop(m_evr_loop);
        l_nconn->set_pre_connect_cb(a_subr.get_pre_connect_cb());

        // Setup clnt_session
        l_ups_srvr_session->m_nconn = l_nconn;
        l_ups_srvr_session->m_subr = &a_subr;

        // Assign clnt_session
        a_subr.set_ups_srvr_session(l_ups_srvr_session);
        l_status = subr_start(a_subr, *l_ups_srvr_session, *l_nconn);
        if(l_status != HLX_STATUS_OK)
        {
                //NDBG_PRINT("Error performing request\n");
                return HLX_STATUS_ERROR;
        }
        else
        {
                a_subr.set_state(subr::SUBR_STATE_ACTIVE);
                a_subr.bump_num_requested();
                return HLX_STATUS_OK;
        }
        return HLX_STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t t_srvr::subr_try_deq(void)
{
        while(m_subr_list_size &&
              !m_stopped)
        {
                subr *l_subr = m_subr_list.front();
                if(!l_subr)
                {
                        subr_dequeue();
                }
                else
                {
                        int32_t l_status;
                        l_status = subr_try_start(*l_subr);
                        if(l_status == HLX_STATUS_OK)
                        {
                                if(l_subr->get_is_pending_done())
                                {
                                        //NDBG_PRINT("POP'ing: host: %s\n",
                                        //           l_subr->get_label().c_str());
                                        subr_dequeue();
                                        l_subr->set_i_q(m_subr_list.end());
                                }
                        }
                        else if(l_status == HLX_STATUS_AGAIN)
                        {
                                // break since ran out of available connections
                                break;
                        }
#ifdef ASYNC_DNS_SUPPORT
                        else if(l_status == STATUS_QUEUED_ASYNC_DNS)
                        {
                                l_subr->set_state(subr::SUBR_STATE_DNS_LOOKUP);
                                subr_dequeue();
                                l_subr->set_i_q(m_subr_list.end());
                        }
#endif
                        else
                        {
                                subr_dequeue();
                                l_subr->set_i_q(m_subr_list.end());
                        }
                }
        }
        return HLX_STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void *t_srvr::t_run(void *a_nothing)
{
        int32_t l_s;
        l_s = init();
        if(l_s != HLX_STATUS_OK)
        {
                NDBG_PRINT("Error performing init.\n");
                return NULL;
        }
        m_stopped = false;

        // Reset stats...
        m_stat.clear();

        if(m_t_conf->m_update_stats_ms)
        {
                // Add timers...
                void *l_timer = NULL;
                ns_hlx::add_timer(this, m_t_conf->m_update_stats_ms,
                                  s_stat_update, NULL,
                                  &l_timer);
        }

        // Set start time
        m_start_time_s = get_time_s();
        // TODO Test -remove
        //uint64_t l_last_time_ms = get_time_ms();
        //uint64_t l_num_run = 0;

        // Pre-queue'd subrequests
        subr_try_deq();
        // check return status???

        // -------------------------------------------------
        // Run server
        // -------------------------------------------------
        while(!m_stopped)
        {
                //NDBG_PRINT("Running.\n");
                ++m_stat.m_total_run;
                l_s = m_evr_loop->run();
                if(l_s != HLX_STATUS_OK)
                {
                        // TODO log run failure???

                }

                void *l_data = dequeue_clnt_session_writeable();
                if(l_data)
                {
                        l_s = clnt_session::evr_fd_writeable_cb(l_data);
                        if(l_s != HLX_STATUS_OK)
                        {
                                // TODO log failure
                        }
                }

                // Subrequests
                l_s = subr_try_deq();
                if(l_s != HLX_STATUS_OK)
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
int32_t t_srvr::cleanup_clnt_session(clnt_session *a_clnt_session, nconn *a_nconn)
{
        if(a_clnt_session)
        {
                if(a_clnt_session->m_timer_obj)
                {
                        m_evr_loop->cancel_timer(a_clnt_session->m_timer_obj);
                        a_clnt_session->m_timer_obj = NULL;
                }
                a_clnt_session->m_nconn = NULL;
                if(a_clnt_session->m_rqst)
                {
                        m_rqst_pool.release(a_clnt_session->m_rqst);
                        a_clnt_session->m_rqst = NULL;
                }
                if(a_clnt_session->m_in_q)
                {
                        m_nbq_pool.release(a_clnt_session->m_in_q);
                        a_clnt_session->m_in_q = NULL;
                }
                if(a_clnt_session->m_out_q)
                {
                        m_nbq_pool.release(a_clnt_session->m_out_q);
                        a_clnt_session->m_out_q = NULL;
                }
                a_clnt_session->clear();
                m_clnt_session_pool.release(a_clnt_session);
        }
        if(a_nconn)
        {
                uint32_t l_id = a_nconn->get_pool_id();
                if(l_id == S_POOL_ID_CLIENT)
                {
                        ++m_stat.m_clnt_conn_completed;
                        if(HLX_STATUS_OK != m_nconn_pool.release(a_nconn))
                        {
                                return HLX_STATUS_ERROR;
                        }
                        m_stat.m_pool_conn_active = m_nconn_pool.get_active_size();
                        m_stat.m_pool_conn_idle = m_nconn_pool.get_idle_size();
                }
                // TODO Log error???
        }
        return HLX_STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t t_srvr::cleanup_srvr_session(ups_srvr_session *a_ups_srvr_session, nconn *a_nconn)
{
        if(a_ups_srvr_session)
        {
                // Cancel last timer
                if(a_ups_srvr_session->m_timer_obj)
                {
                        m_evr_loop->cancel_timer(a_ups_srvr_session->m_timer_obj);
                        a_ups_srvr_session->m_timer_obj = NULL;
                }
                a_ups_srvr_session->m_nconn = NULL;
                if(a_ups_srvr_session->m_resp)
                {
                        m_resp_pool.release(a_ups_srvr_session->m_resp);
                        a_ups_srvr_session->m_resp = NULL;
                }
                if(a_ups_srvr_session->m_in_q)
                {
                        if(!a_ups_srvr_session->m_in_q_detached)
                        {
                                m_nbq_pool.release(a_ups_srvr_session->m_in_q);
                        }
                        a_ups_srvr_session->m_in_q = NULL;
                }
                if(a_ups_srvr_session->m_out_q)
                {
                        m_nbq_pool.release(a_ups_srvr_session->m_out_q);
                        a_ups_srvr_session->m_out_q = NULL;
                }
                a_ups_srvr_session->clear();
                m_ups_srvr_session_pool.release(a_ups_srvr_session);
        }
        if(a_nconn)
        {
                uint32_t l_id = a_nconn->get_pool_id();
                if(l_id == S_POOL_ID_UPS_PROXY)
                {
                        ++m_stat.m_upsv_conn_completed;
                        if(HLX_STATUS_OK != m_nconn_proxy_pool.release(a_nconn))
                        {
                                return HLX_STATUS_ERROR;
                        }
                        m_stat.m_pool_proxy_conn_active = m_nconn_proxy_pool.get_active_size();
                        m_stat.m_pool_proxy_conn_idle = m_nconn_proxy_pool.get_idle_size();
                }
                // TODO Log error???
        }
        return HLX_STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void t_srvr::release_resp(resp *a_resp)
{
        if(!a_resp)
        {
                return;
        }
        m_resp_pool.release(a_resp);
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void t_srvr::release_nbq(nbq *a_nbq)
{
        if(!a_nbq)
        {
                return;
        }
        m_nbq_pool.release(a_nbq);
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t t_srvr::s_stat_update(void *a_ctx, void *a_data)
{
        t_srvr *l_hlx = static_cast<t_srvr *>(a_ctx);
        if(!l_hlx)
        {
                return HLX_STATUS_ERROR;
        }
        l_hlx->stat_update();
        // Add timers...
        void *l_timer = NULL;
        ns_hlx::add_timer(a_ctx, l_hlx->m_t_conf->m_update_stats_ms,
                          s_stat_update, a_data,
                          &l_timer);
        return HLX_STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void t_srvr::stat_update(void)
{
        pthread_mutex_lock(&m_stat_copy_mutex);
        m_stat_copy = m_stat;
#ifdef ASYNC_DNS_SUPPORT
        if(m_t_conf && m_t_conf->m_srvr && m_t_conf->m_srvr->get_nresolver())
        {
                m_stat_copy.m_dns_resolve_active = m_t_conf->m_srvr->get_nresolver()->get_active(m_adns_ctx);
        }
#endif
        m_stat_copy.m_upsv_subr_queued = m_subr_list_size;
        m_stat_copy.m_pool_conn_active = m_nconn_pool.get_active_size();
        m_stat_copy.m_pool_conn_idle = m_nconn_pool.get_idle_size();
        m_stat_copy.m_pool_proxy_conn_active = m_nconn_proxy_pool.get_active_size();
        m_stat_copy.m_pool_proxy_conn_idle = m_nconn_proxy_pool.get_idle_size();
        m_stat_copy.m_pool_clnt_free = m_clnt_session_pool.free_size();
        m_stat_copy.m_pool_clnt_used = m_clnt_session_pool.used_size();
        m_stat_copy.m_pool_resp_free = m_resp_pool.free_size();
        m_stat_copy.m_pool_resp_used = m_resp_pool.used_size();
        m_stat_copy.m_pool_rqst_free = m_rqst_pool.free_size();
        m_stat_copy.m_pool_rqst_used = m_rqst_pool.used_size();
        m_stat_copy.m_pool_nbq_free = m_nbq_pool.free_size();
        m_stat_copy.m_pool_nbq_used = m_nbq_pool.used_size();
        if(m_t_conf->m_count_response_status)
        {
                m_upsv_status_code_count_map_copy = m_upsv_status_code_count_map;
        }
        pthread_mutex_unlock(&m_stat_copy_mutex);

}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t t_srvr::get_stat(t_stat_cntr_t &ao_stat)
{
        pthread_mutex_lock(&m_stat_copy_mutex);
        ao_stat = m_stat_copy;
        pthread_mutex_unlock(&m_stat_copy_mutex);
        return HLX_STATUS_OK;
}


//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t t_srvr::get_ups_status_code_count_map(status_code_count_map_t &ao_ups_status_code_count_map)
{
        pthread_mutex_lock(&m_stat_copy_mutex);
        ao_ups_status_code_count_map = m_upsv_status_code_count_map_copy;
        pthread_mutex_unlock(&m_stat_copy_mutex);
        return HLX_STATUS_OK;

}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
ups_srvr_session * t_srvr::get_ups_srvr(lsnr *a_lsnr)
{
        ups_srvr_session *l_ups_srvr_session = NULL;
        get_from_pool_if_null(l_ups_srvr_session, m_ups_srvr_session_pool);
        l_ups_srvr_session->m_t_srvr = this;
        l_ups_srvr_session->m_timer_obj = NULL;
        return l_ups_srvr_session;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
srvr *t_srvr::get_srvr(void)
{
        if(!m_t_conf)
        {
                TRC_ERROR("m_t_conf = NULL");
                return NULL;
        }
        return m_t_conf->m_srvr;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
nbq *t_srvr::get_nbq(void)
{
        nbq *l_nbq = NULL;
        bool l_new = false;
        l_nbq = m_nbq_pool.get_free();
        if(!l_nbq)
        {
                l_nbq = new nbq(4096);
                m_nbq_pool.add(l_nbq);
                l_new = true;
        }
        if(!l_new)
        {
                l_nbq->reset_write();
        }
        return l_nbq;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void t_srvr::add_stat_to_agg(const conn_stat_t &a_conn_stat, uint16_t a_status_code)
{
        update_stat(m_stat.m_upsv_stat_us_connect, a_conn_stat.m_tt_connect_us);
        update_stat(m_stat.m_upsv_stat_us_first_response, a_conn_stat.m_tt_first_read_us);
        update_stat(m_stat.m_upsv_stat_us_end_to_end, a_conn_stat.m_tt_completion_us);
        if(m_t_conf->m_count_response_status)
        {
                ++m_upsv_status_code_count_map[a_status_code];
        }
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t t_srvr::add_timer(uint32_t a_time_ms,
                         timer_cb_t a_timer_cb,
                         void *a_data,
                         void **ao_timer)
{
        if(!m_evr_loop)
        {
                return HLX_STATUS_ERROR;
        }
        evr_timer_t *l_t = NULL;
        int32_t l_status;
        l_status = m_evr_loop->add_timer(a_time_ms,
                                         a_timer_cb,
                                         this,
                                         a_data,
                                         &l_t);
        if(l_status != HLX_STATUS_OK)
        {
                return HLX_STATUS_ERROR;
        }
        *ao_timer = l_t;
        return HLX_STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t t_srvr::cancel_timer(void *a_timer)
{
        if(!m_evr_loop)
        {
                return HLX_STATUS_ERROR;
        }
        evr_timer_t *l_t = static_cast<evr_timer_t *>(a_timer);
        return m_evr_loop->cancel_timer(l_t);
}


} //namespace ns_hlx {
