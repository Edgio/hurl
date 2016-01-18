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

#include "rapidjson/document.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/prettywriter.h"

#ifndef __STDC_FORMAT_MACROS
#define __STDC_FORMAT_MACROS
#endif
#include <inttypes.h>

//: ----------------------------------------------------------------------------
//: Macros
//: ----------------------------------------------------------------------------
#define JS_ADD_MEMBER(_obj, _key, _val, _l_js_alloc)\
_obj.AddMember(_key,\
                rapidjson::Value(_val, _l_js_alloc).Move(),\
                _l_js_alloc)

namespace ns_hlx {

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
static void add_to_total_stat_agg(t_stat_t &ao_stat_agg, const t_stat_t &a_add_total_stat)
{

        // Stats
        add_stat(ao_stat_agg.m_stat_us_connect , a_add_total_stat.m_stat_us_connect);
        add_stat(ao_stat_agg.m_stat_us_first_response , a_add_total_stat.m_stat_us_first_response);
        add_stat(ao_stat_agg.m_stat_us_end_to_end , a_add_total_stat.m_stat_us_end_to_end);

        ao_stat_agg.m_num_ups_resolve_req += a_add_total_stat.m_num_ups_resolve_req;
        ao_stat_agg.m_num_ups_resolve_active += a_add_total_stat.m_num_ups_resolve_active;
        ao_stat_agg.m_num_ups_resolved += a_add_total_stat.m_num_ups_resolved;
        ao_stat_agg.m_num_ups_conn_started += a_add_total_stat.m_num_ups_conn_started;
        ao_stat_agg.m_cur_ups_conn_count += a_add_total_stat.m_cur_ups_conn_count;
        ao_stat_agg.m_num_ups_conn_completed += a_add_total_stat.m_num_ups_conn_completed;
        ao_stat_agg.m_num_ups_reqs += a_add_total_stat.m_num_ups_reqs;
        ao_stat_agg.m_num_ups_idle_killed += a_add_total_stat.m_num_ups_idle_killed;

        ao_stat_agg.m_num_cln_conn_started += a_add_total_stat.m_num_cln_conn_started;
        ao_stat_agg.m_cur_cln_conn_count += a_add_total_stat.m_cur_cln_conn_count;
        ao_stat_agg.m_num_cln_conn_completed += a_add_total_stat.m_num_cln_conn_completed;
        ao_stat_agg.m_num_cln_reqs += a_add_total_stat.m_num_cln_reqs;
        ao_stat_agg.m_num_cln_idle_killed += a_add_total_stat.m_num_cln_idle_killed;

        ao_stat_agg.m_total_bytes += a_add_total_stat.m_total_bytes;
        ao_stat_agg.m_total_reqs += a_add_total_stat.m_total_reqs;
        ao_stat_agg.m_num_errors += a_add_total_stat.m_num_errors;
        ao_stat_agg.m_num_bytes_read += a_add_total_stat.m_num_bytes_read;
        ao_stat_agg.m_num_bytes_written += a_add_total_stat.m_num_bytes_written;

        for(status_code_count_map_t::const_iterator i_code =
                        a_add_total_stat.m_status_code_count_map.begin();
                        i_code != a_add_total_stat.m_status_code_count_map.end();
                        ++i_code)
        {
                status_code_count_map_t::iterator i_code2;
                i_code2 = ao_stat_agg.m_status_code_count_map.find(i_code->first);
                if(i_code2 == ao_stat_agg.m_status_code_count_map.end())
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
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
hlx::hlx(void):
        m_t_conf(NULL),
        m_stats(false),
        m_num_threads(1),
        m_lsnr_list(),
        m_subr_pre_queue(),
        m_nresolver(NULL),
        m_use_ai_cache(true),
        m_ai_cache(NRESOLVER_DEFAULT_AI_CACHE_FILE),
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
//:                               Getters
//: ----------------------------------------------------------------------------
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
int32_t hlx::get_stats_json(char **ao_json_buf, uint32_t &ao_json_buf_len)
{
        t_stat_t l_total;

        uint64_t l_time_ms = get_time_ms();
        // Get stats
        get_stats(l_total);

        rapidjson::Document l_body;
        l_body.SetObject(); // Define doc as object -rather than array
        rapidjson::Document::AllocatorType& l_alloc = l_body.GetAllocator();
        rapidjson::Value l_data_array(rapidjson::kArrayType);

        // Single member array...
        rapidjson::Value l_s;
        l_s.SetObject();
        JS_ADD_MEMBER(l_s, "key", "NA", l_alloc);
        l_s.AddMember("count", l_total.m_total_reqs, l_alloc);
        l_s.AddMember("time", l_time_ms, l_alloc);

        l_body.AddMember("data", l_data_array, l_alloc);

        rapidjson::StringBuffer l_strbuf;
        rapidjson::Writer<rapidjson::StringBuffer> l_writer(l_strbuf);
        l_body.Accept(l_writer);

        ao_json_buf_len = l_strbuf.GetSize();
        *ao_json_buf = (char *)malloc(sizeof(char)*(ao_json_buf_len+1));
        strncpy(*ao_json_buf, l_strbuf.GetString(), ao_json_buf_len);
        return HLX_STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
#define DISPLAY_DNS_STAT(_stat) NDBG_OUTPUT("| %s%24s%s: %12lu\n", ANSI_COLOR_FG_MAGENTA, #_stat, ANSI_COLOR_OFF,(uint64_t)l_stat._stat)
#define DISPLAY_CLN_STAT(_stat) NDBG_OUTPUT("| %s%24s%s: %12lu\n", ANSI_COLOR_FG_GREEN,   #_stat, ANSI_COLOR_OFF,(uint64_t)l_stat._stat)
#define DISPLAY_SRV_STAT(_stat) NDBG_OUTPUT("| %s%24s%s: %12lu\n", ANSI_COLOR_FG_BLUE,    #_stat, ANSI_COLOR_OFF,(uint64_t)l_stat._stat)
#define DISPLAY_GEN_STAT(_stat) NDBG_OUTPUT("| %s%24s%s: %12lu\n", ANSI_COLOR_FG_CYAN,    #_stat, ANSI_COLOR_OFF,(uint64_t)l_stat._stat)
void hlx::display_stats(void)
{
        uint32_t i_t = 0;
        for(t_hlx_list_t::const_iterator i_client = m_t_hlx_list.begin();
           i_client != m_t_hlx_list.end();
           ++i_client, ++i_t)
        {
                // Get stuff from client...
                // TODO
                t_stat_t l_stat;
                (*i_client)->get_stats_copy(l_stat);
                NDBG_OUTPUT("+-----------------------------------------------------------\n");
                NDBG_OUTPUT("| THREAD [%6d]\n", i_t);
                NDBG_OUTPUT("+-----------------------------------------------------------\n");
                //xstat_t m_stat_us_connect;
                //xstat_t m_stat_us_first_response;
                //xstat_t m_stat_us_end_to_end;
                DISPLAY_DNS_STAT(m_num_ups_resolve_req);
                DISPLAY_DNS_STAT(m_num_ups_resolve_active);
                DISPLAY_DNS_STAT(m_num_ups_resolved);
                DISPLAY_DNS_STAT(m_num_ups_resolve_ev);
                DISPLAY_CLN_STAT(m_num_ups_conn_started);
                DISPLAY_CLN_STAT(m_cur_ups_conn_count);
                DISPLAY_CLN_STAT(m_num_ups_conn_completed);
                DISPLAY_CLN_STAT(m_num_ups_reqs);
                DISPLAY_CLN_STAT(m_num_ups_idle_killed);
                DISPLAY_SRV_STAT(m_num_cln_conn_started);
                DISPLAY_SRV_STAT(m_cur_cln_conn_count);
                DISPLAY_SRV_STAT(m_num_cln_conn_completed);
                DISPLAY_SRV_STAT(m_num_cln_reqs);
                DISPLAY_SRV_STAT(m_num_cln_idle_killed);
                DISPLAY_GEN_STAT(m_num_run);
                DISPLAY_GEN_STAT(m_total_bytes);
                DISPLAY_GEN_STAT(m_total_reqs);
                DISPLAY_GEN_STAT(m_num_errors);
                DISPLAY_GEN_STAT(m_num_bytes_read);
                DISPLAY_GEN_STAT(m_num_bytes_written);
                //NDBG_OUTPUT("| %sPending resolve%s: \n", ANSI_COLOR_FG_RED, ANSI_COLOR_OFF);
                //for(subr_pending_resolv_map_t::const_iterator i_r = l_stat.m_subr_pending_resolv_map.begin();
                //    i_r != l_stat.m_subr_pending_resolv_map.end();
                //    ++i_r)
                //{
                //        NDBG_OUTPUT("|     %s\n", i_r->first.c_str());
                //}
                //status_code_count_map_t m_status_code_count_map;
        }
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
//:                               Setters
//: ----------------------------------------------------------------------------
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
//: \details: Global timeout for connect/read/write
//: \return:  NA
//: \param:   a_val: timeout in seconds
//: ----------------------------------------------------------------------------
void hlx::set_timeout_ms(uint32_t a_val)
{
        m_t_conf->m_timeout_ms = a_val;
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
        l_status = get_tls_options_str_val(a_tls_options_str,
                                           m_t_conf->m_tls_server_ctx_options);
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
        l_status = get_tls_options_str_val(a_tls_options_str,
                                           m_t_conf->m_tls_client_ctx_options);
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
int hlx::set_tls_client_ctx_options(long a_tls_options)
{
        m_t_conf->m_tls_client_ctx_options = a_tls_options;
        return STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: Running
//: ----------------------------------------------------------------------------
//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t hlx::register_lsnr(lsnr *a_lsnr)
{
        if(!a_lsnr)
        {
                return HLX_STATUS_ERROR;
        }
        int32_t l_status;
        l_status = a_lsnr->init();
        if(l_status != STATUS_OK)
        {
                NDBG_PRINT("Error performing lsnr init.\n");
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
                                l_status = (*i_t)->add_lsnr(*a_lsnr);
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

#if 0
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
                                uint32_t l_num_fetches_per_thread =
                                                l_subr.get_num_to_request() / l_num_hlx;
                                uint32_t l_remainder_fetches =
                                                l_subr.get_num_to_request() % l_num_hlx;
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
                                (*i_t)->subr_add(l_subr);
                        }
                }
                delete &a_subr;
        }
        else if(a_subr.get_type() == SUBR_TYPE_NONE)
        {
                (*m_t_hlx_subr_iter)->subr_add(a_subr);
                ++m_t_hlx_subr_iter;
                if(m_t_hlx_subr_iter == m_t_hlx_list.end())
                {
                        m_t_hlx_subr_iter = m_t_hlx_list.begin();
                }
        }
}
#endif

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void hlx::pre_queue_subr(subr &a_subr)
{
        m_subr_pre_queue.push(&a_subr);
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
uint64_t hlx::get_next_subr_uuid(void)
{
        return ++m_cur_subr_uid;
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
        l_ldb_init_status = m_nresolver->init(m_use_ai_cache, m_ai_cache);
        if(STATUS_OK != l_ldb_init_status)
        {
                NDBG_PRINT("Error performing resolver init with ai_cache: %s\n",
                           m_ai_cache.c_str());
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
        signal(SIGPIPE, SIG_IGN);

        // -------------------------------------------
        // SSL init...
        // -------------------------------------------
        tls_init();

        std::string l_unused;
        // Server
        m_t_conf->m_tls_server_ctx = tls_init_ctx(
                  m_t_conf->m_tls_server_ctx_cipher_list, // ctx cipher list str
                  m_t_conf->m_tls_server_ctx_options,     // ctx options
                  l_unused,                               // ctx ca file
                  l_unused,                               // ctx ca path
                  true,                                   // is server?
                  m_t_conf->m_tls_server_ctx_key,         // tls key
                  m_t_conf->m_tls_server_ctx_crt);        // tls crt
        if(NULL == m_t_conf->m_tls_server_ctx)
        {
                NDBG_PRINT("Error: performing ssl_init(server) with cipher_list: %s\n",
                           m_t_conf->m_tls_server_ctx_cipher_list.c_str());
                return HLX_STATUS_ERROR;
        }
        // Client
        m_t_conf->m_tls_client_ctx = tls_init_ctx(
                m_t_conf->m_tls_client_ctx_cipher_list, // ctx cipher list str
                m_t_conf->m_tls_client_ctx_options,     // ctx options
                m_t_conf->m_tls_client_ctx_ca_file,     // ctx ca file
                m_t_conf->m_tls_client_ctx_ca_path,     // ctx ca path
                false,                                  // is server?
                l_unused,                               // tls key
                l_unused);                              // tls crt
        if(NULL == m_t_conf->m_tls_client_ctx)
        {
                NDBG_PRINT("Error: performing ssl_init(client) with cipher_list: %s\n",
                           m_t_conf->m_tls_client_ctx_cipher_list.c_str());
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
        while(m_subr_pre_queue.size())
        {
                subr *l_subr = m_subr_pre_queue.front();

                if(l_subr->get_type() == SUBR_TYPE_DUPE)
                {
                        uint32_t l_num_hlx = (uint32_t)m_t_hlx_list.size();
                        uint32_t i_hlx_idx = 0;
                        for(t_hlx_list_t::iterator i_t = m_t_hlx_list.begin();
                            i_t != m_t_hlx_list.end();
                            ++i_t, ++i_hlx_idx)
                        {
                                subr *l_duped_subr = new subr(*l_subr);
                                l_duped_subr->set_uid(get_next_subr_uuid());

                                // Recalculate num fetches per thread
                                if(l_duped_subr->get_num_to_request() > 0)
                                {
                                        uint32_t l_num_fetches_per_thread =
                                                        l_duped_subr->get_num_to_request() / l_num_hlx;
                                        uint32_t l_remainder_fetches =
                                                        l_duped_subr->get_num_to_request() % l_num_hlx;
                                        if (i_hlx_idx == (l_num_hlx - 1))
                                        {
                                                l_num_fetches_per_thread += l_remainder_fetches;
                                        }
                                        //NDBG_PRINT("Num to fetch: %d\n", (int)l_num_fetches_per_thread);
                                        l_duped_subr->set_num_to_request(l_num_fetches_per_thread);
                                }
                                if(l_duped_subr->get_num_to_request() != 0)
                                {
                                        //a_http_req.m_sr_child_list.push_back(&l_rqst);
                                        (*i_t)->subr_add(*l_duped_subr);
                                }
                        }
                        delete l_subr;
                }
                else if(l_subr->get_type() == SUBR_TYPE_NONE)
                {
                        (*m_t_hlx_subr_iter)->subr_add(*l_subr);
                        ++m_t_hlx_subr_iter;
                        if(m_t_hlx_subr_iter == m_t_hlx_list.end())
                        {
                                m_t_hlx_subr_iter = m_t_hlx_list.begin();
                        }
                }
                m_subr_pre_queue.pop();
        }
        return STATUS_OK;
}

} //namespace ns_hlx {
