//: ----------------------------------------------------------------------------
//: Copyright (C) 2014 Verizon.  All Rights Reserved.
//: All Rights Reserved
//:
//: \file:    hlx.cc
//: \details: TODO
//: \author:  Reed P. Morrison
//: \date:    05/28/2015
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
#include "hlx/hlx.h"
#include "ndebug.h"
#include "tls_util.h"
#include "time_util.h"
#include "stat_util.h"
#include "t_hlx.h"
#include "nresolver.h"
#include "nconn_tcp.h"

#ifndef __STDC_FORMAT_MACROS
#define __STDC_FORMAT_MACROS
#endif
#include <inttypes.h>

namespace ns_hlx {

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
hlx::hlx(void):
        m_t_conf(NULL),
        m_stats(false),
        m_num_threads(1),
        m_lsnr_list(),
        m_subr_queue(),
        m_nresolver(NULL),
        m_use_ai_cache(true),
        m_ai_cache(),
        m_start_time_ms(0),
        m_t_hlx_list(),
        m_t_hlx_subr_iter(),
        m_is_initd(false),
        m_cur_subr_uid(0)
{
        m_t_conf = new t_conf();
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
hlx::~hlx()
{
        for(t_hlx_list_t::iterator i_t = m_t_hlx_list.begin();
                        i_t != m_t_hlx_list.end();
                        ++i_t)
        {
                if(*i_t)
                {
                        delete *i_t;
                }
        }
        m_t_hlx_list.clear();
        for(lsnr_list_t::iterator i_t = m_lsnr_list.begin();
                        i_t != m_lsnr_list.end();
                        ++i_t)
        {
                if(*i_t)
                {
                        delete *i_t;
                }
        }
        m_lsnr_list.clear();
        if(m_t_conf->m_tls_server_ctx)
        {
                SSL_CTX_free(m_t_conf->m_tls_server_ctx);
                m_t_conf->m_tls_server_ctx = NULL;
        }
        if(m_t_conf->m_tls_client_ctx)
        {
                SSL_CTX_free(m_t_conf->m_tls_client_ctx);
                m_t_conf->m_tls_client_ctx = NULL;
        }
        if(m_nresolver)
        {
                delete m_nresolver;
                m_nresolver = NULL;
        }

        if(m_t_conf)
        {
            delete m_t_conf;
            m_t_conf = NULL;
        }
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void hlx::set_verbose(bool a_val)
{
        m_t_conf->m_verbose = a_val;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void hlx::set_color(bool a_val)
{
        m_t_conf->m_color = a_val;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void hlx::set_stats(bool a_val)
{
        m_t_conf->m_collect_stats = a_val;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void hlx::set_num_threads(uint32_t a_num_threads)
{
        m_num_threads = a_num_threads;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void hlx::set_num_parallel(uint32_t a_num_parallel)
{
        m_t_conf->m_num_parallel = a_num_parallel;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void hlx::set_num_reqs_per_conn(int32_t a_num_reqs_per_conn)
{
        m_t_conf->m_num_reqs_per_conn = a_num_reqs_per_conn;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void hlx::set_start_time_ms(uint64_t a_start_time_ms)
{
        m_start_time_ms = a_start_time_ms;
}

#if 0
//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void hlx::set_show_summary(bool a_val)
{
        m_t_conf->m_show_summary = a_val;
}
#endif

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t hlx::add_lsnr(lsnr *a_lsnr)
{
        if(!a_lsnr)
        {
                return HLX_STATUS_ERROR;
        }
        if(is_running())
        {
                for(t_hlx_list_t::iterator i_t = m_t_hlx_list.begin();
                    i_t != m_t_hlx_list.end();
                    ++i_t)
                {
                        if(*i_t)
                        {
                                int32_t l_status = (*i_t)->add_lsnr(*a_lsnr);
                                if(l_status != STATUS_OK)
                                {
                                        NDBG_PRINT("Error performing add_lsnr.\n");
                                        return HLX_STATUS_ERROR;
                                }
                        }
                }
        }
        else
        {
                m_lsnr_list.push_back(a_lsnr);
        }
        return HLX_STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void hlx::add_subr_t(subr &a_subr)
{
        if(a_subr.get_type() == SUBR_TYPE_DUPE)
        {
                uint32_t l_num_hlx = (uint32_t)m_t_hlx_list.size();
                uint32_t i_hlx_idx = 0;
                for(t_hlx_list_t::iterator i_t = m_t_hlx_list.begin();
                                i_t != m_t_hlx_list.end();
                                ++i_t, ++i_hlx_idx)
                {
                        subr &l_subr = create_subr(a_subr);

                        // Recalculate num fetches per thread
                        if(l_subr.get_num_to_request() > 0)
                        {
                                uint32_t l_num_fetches_per_thread = l_subr.get_num_to_request() / l_num_hlx;
                                uint32_t l_remainder_fetches = l_subr.get_num_to_request() % l_num_hlx;
                                if (i_hlx_idx == (l_num_hlx - 1))
                                {
                                        l_num_fetches_per_thread += l_remainder_fetches;
                                }
                                //NDBG_PRINT("Num to fetch: %d\n", (int)l_num_fetches_per_thread);
                                l_subr.set_num_to_request(l_num_fetches_per_thread);
                        }
                        if(l_subr.get_num_to_request() != 0)
                        {
                                //a_http_req.m_sr_child_list.push_back(&l_rqst);
                                (*i_t)->add_subr(l_subr);
                        }
                }
                delete &a_subr;
        }
        else if(a_subr.get_type() == SUBR_TYPE_NONE)
        {
                (*m_t_hlx_subr_iter)->add_subr(a_subr);
                ++m_t_hlx_subr_iter;
                if(m_t_hlx_subr_iter == m_t_hlx_list.end())
                {
                        m_t_hlx_subr_iter = m_t_hlx_list.begin();
                }
        }
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void hlx::queue_subr(subr *a_subr)
{
        m_subr_queue.push(a_subr);
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t hlx::queue_subr(hconn *a_hconn, subr &a_subr)
{
        int32_t l_status;
        if(!m_is_initd)
        {
                l_status = init();
                if(HLX_STATUS_OK != l_status)
                {
                        return HLX_STATUS_ERROR;
                }
        }
        if(a_hconn)
        {
                a_subr.set_requester_hconn(a_hconn);
        }

        if(!m_nresolver)
        {
                return HLX_STATUS_ERROR;
        }
        if(is_running())
        {
                add_subr_t(a_subr);
        }
        else
        {
                queue_subr(&a_subr);
        }
        return HLX_STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
subr &hlx::create_subr(const subr &a_subr)
{
        subr *l_subr = new subr(a_subr);
        l_subr->set_uid(++m_cur_subr_uid);
        return *l_subr;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
subr &hlx::create_subr(void)
{
        subr *l_subr = new subr();
        l_subr->set_uid(++m_cur_subr_uid);
        return *l_subr;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
api_resp &hlx::create_api_resp(void)
{
        api_resp *l_api_resp = new api_resp();
        return *l_api_resp;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t hlx::queue_api_resp(hconn &a_hconn, api_resp &a_api_resp)
{
        if(!a_hconn.m_t_hlx)
        {
            return HLX_STATUS_ERROR;
        }
        a_hconn.m_t_hlx->queue_api_resp(a_api_resp, a_hconn);
        delete &a_api_resp;
        return HLX_STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t hlx::queue_resp(hconn &a_hconn)
{
        if(!a_hconn.m_t_hlx)
        {
            return HLX_STATUS_ERROR;
        }
        a_hconn.m_t_hlx->queue_output(a_hconn);
        return HLX_STATUS_OK;
}
//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void hlx::set_collect_stats(bool a_val)
{
        m_t_conf->m_collect_stats = a_val;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void hlx::set_use_persistent_pool(bool a_val)
{
        m_t_conf->m_use_persistent_pool = a_val;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void hlx::set_sock_opt_no_delay(bool a_val)
{
        m_t_conf->m_sock_opt_no_delay = a_val;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void hlx::set_sock_opt_send_buf_size(uint32_t a_send_buf_size)
{
        m_t_conf->m_sock_opt_send_buf_size = a_send_buf_size;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void hlx::set_sock_opt_recv_buf_size(uint32_t a_recv_buf_size)
{
        m_t_conf->m_sock_opt_recv_buf_size = a_recv_buf_size;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void hlx::set_use_ai_cache(bool a_val)
{
        m_use_ai_cache = a_val;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void hlx::set_ai_cache(const std::string &a_ai_cache)
{
        m_ai_cache = a_ai_cache;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t hlx::run(void)
{
        //NDBG_PRINT("Running...\n");

        int l_status = 0;
        if(!m_is_initd)
        {
                l_status = init();
                if(HLX_STATUS_OK != l_status)
                {
                        return HLX_STATUS_ERROR;
                }
        }

        if(m_t_hlx_list.empty())
        {
                l_status = init_t_hlx_list();
                if(STATUS_OK != l_status)
                {
                        return HLX_STATUS_ERROR;
                }
        }

        set_start_time_ms(get_time_ms());

        // -------------------------------------------
        // Run...
        // -------------------------------------------
        if(m_num_threads == 0)
        {
                (*(m_t_hlx_list.begin()))->t_run(NULL);
        }
        else
        {
                for(t_hlx_list_t::iterator i_t = m_t_hlx_list.begin();
                                i_t != m_t_hlx_list.end();
                                ++i_t)
                {
                        (*i_t)->run();
                }
        }

        return HLX_STATUS_OK;

}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int hlx::stop(void)
{
        int32_t l_retval = HLX_STATUS_OK;
        for (t_hlx_list_t::iterator i_t = m_t_hlx_list.begin();
                        i_t != m_t_hlx_list.end();
                        ++i_t)
        {
                (*i_t)->stop();
        }
        for(lsnr_list_t::iterator i_t = m_lsnr_list.begin();
                        i_t != m_lsnr_list.end();
                        ++i_t)
        {
                if(*i_t)
                {
                        delete *i_t;
                }
        }
        m_lsnr_list.clear();

        return l_retval;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t hlx::wait_till_stopped(void)
{
        // Join all threads before exit
        for(t_hlx_list_t::iterator i_server = m_t_hlx_list.begin();
           i_server != m_t_hlx_list.end();
            ++i_server)
        {
                pthread_join(((*i_server)->m_t_run_thread), NULL);
        }
        return HLX_STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
bool hlx::is_running(void)
{
        for (t_hlx_list_t::iterator i_t = m_t_hlx_list.begin();
                        i_t != m_t_hlx_list.end();
                        ++i_t)
        {
                if((*i_t)->is_running())
                {
                        return true;
                }
        }
        return false;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void hlx::get_stats(t_stat_t &ao_all_stats)
{
        // -------------------------------------------
        // Aggregate
        // -------------------------------------------
        for(t_hlx_list_t::const_iterator i_client = m_t_hlx_list.begin();
           i_client != m_t_hlx_list.end();
           ++i_client)
        {
                // Get stuff from client...
                // TODO
                t_stat_t l_stat;
                (*i_client)->get_stats_copy(l_stat);
                add_to_total_stat_agg(ao_all_stats, l_stat);
        }
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t hlx::get_stats_json(char *l_json_buf, uint32_t l_json_buf_max_len)
{
        t_stat_t l_total;

        uint64_t l_time_ms = get_time_ms();
        // Get stats
        get_stats(l_total);

        int l_cur_offset = 0;
        l_cur_offset += snprintf(l_json_buf + l_cur_offset, l_json_buf_max_len - l_cur_offset,"{\"data\": [");

        l_cur_offset += snprintf(l_json_buf + l_cur_offset, l_json_buf_max_len - l_cur_offset,
                        "{\"key\": \"%s\", \"value\": ",
                        "NA");
        l_cur_offset += snprintf(l_json_buf + l_cur_offset, l_json_buf_max_len - l_cur_offset,
                        "{\"%s\": %" PRIu64 ", \"%s\": %" PRIu64 "}",
                        "count", (uint64_t)(l_total.m_total_reqs),
                        "time", (uint64_t)(l_time_ms));
        l_cur_offset += snprintf(l_json_buf + l_cur_offset, l_json_buf_max_len - l_cur_offset,"}");
        l_cur_offset += snprintf(l_json_buf + l_cur_offset, l_json_buf_max_len - l_cur_offset,"]}");
        return l_cur_offset;

}

#if 0
//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
static void append_to_map(summary_map_t &ao_sum, const summary_map_t &a_append)
{
        for(summary_map_t::const_iterator i_sum = a_append.begin();
            i_sum != a_append.end();
           ++i_sum)
        {
                summary_map_t::iterator i_obj = ao_sum.find(i_sum->first);
                if(i_obj != ao_sum.end())
                {
                        i_obj->second += i_sum->second;
                } else {
                        ao_sum[i_sum->first] = i_sum->second;
                }
        }
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void hlx::get_summary_info(summary_info_t &ao_summary_info)
{
        for(t_hlx_list_t::const_iterator i_t = m_t_hlx_list.begin();
            i_t != m_t_hlx_list.end();
            ++i_t)
        {
                const summary_info_t &l_summary_info = (*i_t)->get_summary_info();
                // Consider making getter to grab a copy
                ao_summary_info.m_success += l_summary_info.m_success;
                ao_summary_info.m_error_addr += l_summary_info.m_error_addr;
                ao_summary_info.m_error_conn += l_summary_info.m_error_conn;
                ao_summary_info.m_error_unknown += l_summary_info.m_error_unknown;
                ao_summary_info.m_tls_error_self_signed += l_summary_info.m_tls_error_self_signed;
                ao_summary_info.m_tls_error_expired += l_summary_info.m_tls_error_expired;
                ao_summary_info.m_tls_error_other += l_summary_info.m_tls_error_other;
                append_to_map(ao_summary_info.m_tls_protocols, l_summary_info.m_tls_protocols);
                append_to_map(ao_summary_info.m_tls_ciphers, l_summary_info.m_tls_ciphers);
        }
}
#endif

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void hlx::set_tls_server_ctx_cipher_list(const std::string &a_cipher_list)
{
        m_t_conf->m_tls_server_ctx_cipher_list = a_cipher_list;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int hlx::set_tls_server_ctx_options(const std::string &a_tls_options_str)
{
        int32_t l_status;
        l_status = get_tls_options_str_val(a_tls_options_str, m_t_conf->m_tls_server_ctx_options);
        if(l_status != HLX_STATUS_OK)
        {
                return HLX_STATUS_ERROR;
        }
        return HLX_STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int hlx::set_tls_server_ctx_options(long a_tls_options)
{
        m_t_conf->m_tls_server_ctx_options = a_tls_options;
        return STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void hlx::set_tls_server_ctx_key(const std::string &a_tls_key)
{
        m_t_conf->m_tls_server_ctx_key = a_tls_key;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void hlx::set_tls_server_ctx_crt(const std::string &a_tls_crt)
{
        m_t_conf->m_tls_server_ctx_crt = a_tls_crt;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void hlx::set_tls_client_ctx_cipher_list(const std::string &a_cipher_list)
{
        m_t_conf->m_tls_client_ctx_cipher_list = a_cipher_list;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void hlx::set_tls_client_ctx_ca_path(const std::string &a_tls_ca_path)
{
        m_t_conf->m_tls_client_ctx_ca_path = a_tls_ca_path;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void hlx::set_tls_client_ctx_ca_file(const std::string &a_tls_ca_file)
{
        m_t_conf->m_tls_client_ctx_ca_file = a_tls_ca_file;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int hlx::set_tls_client_ctx_options(const std::string &a_tls_options_str)
{
        int32_t l_status;
        l_status = get_tls_options_str_val(a_tls_options_str, m_t_conf->m_tls_client_ctx_options);
        if(l_status != HLX_STATUS_OK)
        {
                return HLX_STATUS_ERROR;
        }
        return HLX_STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void hlx::set_tls_verify(bool a_val)
{
        m_t_conf->m_tls_client_verify = a_val;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void hlx::set_tls_sni(bool a_val)
{
        m_t_conf->m_tls_client_sni = a_val;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void hlx::set_tls_self_ok(bool a_val)
{
        m_t_conf->m_tls_client_self_ok = a_val;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int hlx::set_tls_client_ctx_options(long a_tls_options)
{
        m_t_conf->m_tls_client_ctx_options = a_tls_options;
        return STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
nresolver *hlx::get_nresolver(void)
{
        return m_nresolver;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void hlx::add_to_total_stat_agg(t_stat_t &ao_stat_agg, const t_stat_t &a_add_total_stat)
{

        // Stats
        add_stat(ao_stat_agg.m_stat_us_connect , a_add_total_stat.m_stat_us_connect);
        add_stat(ao_stat_agg.m_stat_us_first_response , a_add_total_stat.m_stat_us_first_response);
        add_stat(ao_stat_agg.m_stat_us_end_to_end , a_add_total_stat.m_stat_us_end_to_end);

        ao_stat_agg.m_total_bytes += a_add_total_stat.m_total_bytes;
        ao_stat_agg.m_total_reqs += a_add_total_stat.m_total_reqs;

        ao_stat_agg.m_num_resolve_req += a_add_total_stat.m_num_resolve_req;
        ao_stat_agg.m_num_resolve_active += a_add_total_stat.m_num_resolve_active;
        ao_stat_agg.m_num_resolved += a_add_total_stat.m_num_resolved;
        ao_stat_agg.m_num_conn_started += a_add_total_stat.m_num_conn_started;
        ao_stat_agg.m_num_conn_completed += a_add_total_stat.m_num_conn_completed;
        ao_stat_agg.m_num_idle_killed += a_add_total_stat.m_num_idle_killed;
        ao_stat_agg.m_num_errors += a_add_total_stat.m_num_errors;
        ao_stat_agg.m_num_bytes_read += a_add_total_stat.m_num_bytes_read;

        for(status_code_count_map_t::const_iterator i_code = a_add_total_stat.m_status_code_count_map.begin();
                        i_code != a_add_total_stat.m_status_code_count_map.end();
                        ++i_code)
        {
                status_code_count_map_t::iterator i_code2;
                if((i_code2 = ao_stat_agg.m_status_code_count_map.find(i_code->first)) == ao_stat_agg.m_status_code_count_map.end())
                {
                        ao_stat_agg.m_status_code_count_map[i_code->first] = i_code->second;
                }
                else
                {
                        i_code2->second += i_code->second;
                }
        }
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  client status indicating success or failure
//: \param:   TODO
//: ----------------------------------------------------------------------------
int hlx::init(void)
{
        if(true == m_is_initd)
        {
                return HLX_STATUS_OK;
        }

        m_nresolver = new nresolver();

        // -------------------------------------------
        // Init resolver with cache
        // -------------------------------------------
        int32_t l_ldb_init_status;
        l_ldb_init_status = m_nresolver->init(m_ai_cache, m_use_ai_cache);
        if(STATUS_OK != l_ldb_init_status)
        {
                NDBG_PRINT("Error performing resolver init with ai_cache: %s\n", m_ai_cache.c_str());
                return HLX_STATUS_ERROR;
        }

        // not initialized yet
        for(lsnr_list_t::iterator i_t = m_lsnr_list.begin();
                        i_t != m_lsnr_list.end();
                        ++i_t)
        {
                if(*i_t)
                {
                        int32_t l_status;
                        l_status = (*i_t)->init();
                        if(l_status != STATUS_OK)
                        {
                                NDBG_PRINT("Error performing lsnr setup.\n");
                                return HLX_STATUS_ERROR;
                        }
                }
        }

        // -------------------------------------------
        // SSL init...
        // -------------------------------------------
        tls_init();

        std::string l_unused;
        // Server
        m_t_conf->m_tls_server_ctx = tls_init_ctx(m_t_conf->m_tls_server_ctx_cipher_list, // ctx cipher list str
                                                  m_t_conf->m_tls_server_ctx_options,     // ctx options
                                                  l_unused,                               // ctx ca file
                                                  l_unused,                               // ctx ca path
                                                  true,                                   // is server?
                                                  m_t_conf->m_tls_server_ctx_key,         // tls key
                                                  m_t_conf->m_tls_server_ctx_crt);        // tls crt
        if(NULL == m_t_conf->m_tls_server_ctx)
        {
                NDBG_PRINT("Error: performing ssl_init(server) with cipher_list: %s\n", m_t_conf->m_tls_server_ctx_cipher_list.c_str());
                return HLX_STATUS_ERROR;
        }
        // Client
        m_t_conf->m_tls_client_ctx = tls_init_ctx(m_t_conf->m_tls_client_ctx_cipher_list, // ctx cipher list str
                                                  m_t_conf->m_tls_client_ctx_options,     // ctx options
                                                  m_t_conf->m_tls_client_ctx_ca_file,     // ctx ca file
                                                  m_t_conf->m_tls_client_ctx_ca_path,     // ctx ca path
                                                  false,                                  // is server?
                                                  l_unused,                               // tls key
                                                  l_unused);                              // tls crt
        if(NULL == m_t_conf->m_tls_client_ctx)
        {
                NDBG_PRINT("Error: performing ssl_init(client) with cipher_list: %s\n", m_t_conf->m_tls_client_ctx_cipher_list.c_str());
                return HLX_STATUS_ERROR;
        }
        m_is_initd = true;
        return HLX_STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int hlx::init_t_hlx_list(void)
{
        // -------------------------------------------
        // Create t_hlx list...
        // -------------------------------------------
        // 0 threads -make a single hlx
        m_t_conf->m_hlx = this;
        if(m_num_threads == 0)
        {
                int32_t l_status;
                t_hlx *l_t_hlx = new t_hlx(m_t_conf);
                for(lsnr_list_t::iterator i_t = m_lsnr_list.begin();
                                i_t != m_lsnr_list.end();
                                ++i_t)
                {
                        if(*i_t)
                        {
                                l_status = l_t_hlx->add_lsnr(*(*i_t));
                                if(l_status != STATUS_OK)
                                {
                                        delete l_t_hlx;
                                        l_t_hlx = NULL;
                                        NDBG_PRINT("Error performing add_lsnr.\n");
                                        return STATUS_ERROR;
                                }
                        }
                }
                m_t_hlx_list.push_back(l_t_hlx);
        }
        else
        {
                for(uint32_t i_server_idx = 0; i_server_idx < m_num_threads; ++i_server_idx)
                {
                        int32_t l_status;
                        t_hlx *l_t_hlx = new t_hlx(m_t_conf);
                        for(lsnr_list_t::iterator i_t = m_lsnr_list.begin();
                                        i_t != m_lsnr_list.end();
                                        ++i_t)
                        {
                                if(*i_t)
                                {
                                        l_status = l_t_hlx->add_lsnr(*(*i_t));
                                        if(l_status != STATUS_OK)
                                        {
                                                delete l_t_hlx;
                                                l_t_hlx = NULL;
                                                NDBG_PRINT("Error performing add_lsnr.\n");
                                                return STATUS_ERROR;
                                        }
                                }
                        }
                        m_t_hlx_list.push_back(l_t_hlx);
                }
        }
        m_t_hlx_subr_iter = m_t_hlx_list.begin();

        // -------------------------------------------
        // Add any subreqs...
        // -------------------------------------------
        while(m_subr_queue.size())
        {
                add_subr_t(*(m_subr_queue.front()));
                m_subr_queue.pop();
        }

        return STATUS_OK;
}

} //namespace ns_hlx {
