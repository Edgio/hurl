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
#include "ssl_util.h"
#include "time_util.h"
#include "../http/t_hlx.h"
#include "resolver.h"

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
void hlx::set_stats(bool a_val)
{
        m_stats = a_val;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void hlx::set_show_summary(bool a_val)
{
        m_show_summary = a_val;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void hlx::set_ssl_cipher_list(const std::string &a_cipher_list)
{
        m_ssl_cipher_list = a_cipher_list;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int hlx::set_ssl_options(const std::string &a_ssl_options_str)
{
        int32_t l_status;
        l_status = get_ssl_options_str_val(a_ssl_options_str, m_ssl_options);
        if(l_status != HLX_SERVER_STATUS_OK)
        {
                return HLX_SERVER_STATUS_ERROR;
        }
        return HLX_SERVER_STATUS_OK;

}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int hlx::set_ssl_options(long a_ssl_options)
{
        m_ssl_options = a_ssl_options;
        return HLX_SERVER_STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void hlx::set_ssl_ca_path(const std::string &a_ssl_ca_path)
{
        m_ssl_ca_path = a_ssl_ca_path;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void hlx::set_ssl_ca_file(const std::string &a_ssl_ca_file)
{
        m_ssl_ca_file = a_ssl_ca_file;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t hlx::add_listener(listener *a_listener)
{
        if(!a_listener)
        {
                return HLX_SERVER_STATUS_ERROR;
        }
        if(is_running())
        {
                for(t_hlx_list_t::iterator i_t = m_t_hlx_list.begin();
                    i_t != m_t_hlx_list.end();
                    ++i_t)
                {
                        if(*i_t)
                        {
                                int32_t l_status = (*i_t)->add_listener(*a_listener);
                                if(l_status != STATUS_OK)
                                {
                                        NDBG_PRINT("Error performing add_listener.\n");
                                        return HLX_SERVER_STATUS_ERROR;
                                }
                        }
                }
        }
        else
        {
                m_listener_list.push_back(a_listener);
        }
        return HLX_SERVER_STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
static int32_t child_subreq_completion_cb(nconn &a_nconn,
                                          subreq &a_subreq,
                                          http_resp &a_resp)
{
        if(a_subreq.m_child_completion_cb)
        {
                int32_t l_status;
                l_status = a_subreq.m_child_completion_cb(a_nconn, *a_subreq.m_parent, *a_subreq.get_resp());
                if(l_status != 0)
                {
                        // Do stuff???
                }
        }

        //NDBG_PRINT("child_subreq_completion_cb\n");
        if(a_subreq.m_parent)
        {
                pthread_mutex_lock(&(a_subreq.m_parent->m_parent_mutex));
                a_subreq.m_parent->bump_num_completed();
                pthread_mutex_unlock(&(a_subreq.m_parent->m_parent_mutex));
                //NDBG_PRINT("num_completed: %d\n", l_subreq->m_parent->m_num_completed);
                if(a_subreq.m_parent->is_done())
                {       int32_t l_status;
                        l_status = a_subreq.m_parent->m_completion_cb(a_nconn, *a_subreq.m_parent, *a_subreq.get_resp());
                        if(l_status != 0)
                        {
                                // Do stuff???
                        }
                }
        }
        return 0;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
static int32_t child_subreq_error_cb(nconn &a_nconn, subreq &a_subreq)
{
        //NDBG_PRINT("child_subreq_completion_cb\n");
        if(a_subreq.m_parent)
        {
                pthread_mutex_lock(&(a_subreq.m_parent->m_parent_mutex));
                a_subreq.m_parent->bump_num_completed();
                pthread_mutex_unlock(&(a_subreq.m_parent->m_parent_mutex));
                //NDBG_PRINT("num_completed: %d\n", l_subreq->m_parent->m_num_completed);
                if(a_subreq.m_parent->is_done())
                {
                        int32_t l_status;
                        l_status = a_subreq.m_parent->m_completion_cb(a_nconn, *a_subreq.m_parent, *a_subreq.get_resp());
                        if(l_status != 0)
                        {
                                // Do stuff???
                        }
                }
        }
        return 0;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void hlx::add_subreq_t(subreq &a_subreq)
{
        a_subreq.reset();

        // Dupe example
        if(a_subreq.m_type == subreq::SUBREQ_TYPE_DUPE)
        {
                uint32_t l_num_hlx = (uint32_t)m_t_hlx_list.size();
                uint32_t i_hlx_idx = 0;
                for(t_hlx_list_t::iterator i_t = m_t_hlx_list.begin();
                                i_t != m_t_hlx_list.end();
                                ++i_t, ++i_hlx_idx)
                {
                        subreq &l_subreq = create_subreq(a_subreq);

                        // Recalculate num fetches per thread
                        if(l_subreq.get_num_to_request() > 0)
                        {
                                uint32_t l_num_fetches_per_thread = l_subreq.get_num_to_request() / l_num_hlx;
                                uint32_t l_remainder_fetches = l_subreq.get_num_to_request() % l_num_hlx;
                                if (i_hlx_idx == (l_num_hlx - 1))
                                {
                                        l_num_fetches_per_thread += l_remainder_fetches;
                                }
                                //NDBG_PRINT("Num to fetch: %d\n", (int)l_num_fetches_per_thread);
                                l_subreq.set_num_to_request(l_num_fetches_per_thread);
                        }
                        if(l_subreq.get_num_to_request() != 0)
                        {
                                a_subreq.m_child_list.push_back(&l_subreq);
                                (*i_t)->add_subreq(l_subreq);
                        }
                }
        }
        else if(a_subreq.m_type == subreq::SUBREQ_TYPE_FANOUT && a_subreq.get_host_list().size())
        {
                // Create the child subreq's
                if(a_subreq.m_child_list.empty())
                {
                        // Create parent mutex
                        pthread_mutex_init(&(a_subreq.m_parent_mutex), NULL);

                        for(host_list_t::const_iterator i_h = a_subreq.get_host_list().begin();
                            i_h != a_subreq.get_host_list().end();
                            ++i_h)
                        {
                                subreq &l_subreq = create_subreq(a_subreq);
                                l_subreq.set_num_to_request(1);
                                l_subreq.m_host = i_h->m_host;
                                if(!i_h->m_hostname.empty())
                                {
                                        l_subreq.m_hostname = i_h->m_hostname;
                                }
                                if(!i_h->m_id.empty())
                                {
                                        l_subreq.m_id = i_h->m_id;
                                }
                                if(!i_h->m_where.empty())
                                {
                                        l_subreq.m_where = i_h->m_where;
                                }
                                l_subreq.m_parent = &a_subreq;
                                l_subreq.set_completion_cb(child_subreq_completion_cb);
                                l_subreq.set_error_cb(child_subreq_error_cb);
                                if(a_subreq.m_child_completion_cb)
                                {
                                        l_subreq.set_child_completion_cb(a_subreq.m_child_completion_cb);
                                }
                                a_subreq.m_child_list.push_back(&l_subreq);
                        }
                }
                //NDBG_PRINT("a_subreq.m_child_list.size(): %d\n", (int)a_subreq.m_child_list.size());
                // -------------------------------------------
                // Setup client requests
                // -------------------------------------------
                uint32_t i_h_idx = 0;
                uint32_t l_num_h = m_t_hlx_list.size();
                uint32_t l_num_s = a_subreq.m_child_list.size();
                subreq_list_t::const_iterator i_s = a_subreq.m_child_list.begin();
                for(t_hlx_list_t::iterator i_h = m_t_hlx_list.begin();
                                i_h != m_t_hlx_list.end();
                                ++i_h, ++i_h_idx)
                {
                        // TODO -would we ever not???
                        //if(m_split_requests_by_thread)
                        uint32_t l_len = l_num_s/l_num_h;
                        if(i_h_idx + 1 == l_num_h)
                        {
                                // Get remainder
                                l_len = l_num_s - (i_h_idx * l_len);
                        }
                        //NDBG_PRINT("l_len: %u\n", l_len);
                        for(uint32_t i_dx = 0; i_dx < l_len; ++i_dx)
                        {
                                //NDBG_PRINT("i_dx: %u\n", i_dx);
                                (*i_s)->reset();
                                (*i_h)->add_subreq(*(*i_s));
                                ++i_s;
                        }
                }
        }
        else if(a_subreq.m_type == subreq::SUBREQ_TYPE_NONE)
        {
                // TODO hash by host to thread...
                 (*(m_t_hlx_list.begin()))->add_subreq(a_subreq);
        }
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t hlx::add_subreq(subreq &a_subreq)
{
        if(is_running())
        {
                add_subreq_t(a_subreq);
        }
        else
        {
                m_subreq_queue.push(&a_subreq);
        }
        return HLX_SERVER_STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
subreq &hlx::create_subreq(const subreq &a_subreq)
{
        subreq *l_subreq = new subreq(a_subreq);
        return *l_subreq;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
subreq &hlx::create_subreq(const char *a_label)
{
        subreq *l_subreq = new subreq(a_label);
        return *l_subreq;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void hlx::set_collect_stats(bool a_val)
{
        m_collect_stats = a_val;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void hlx::set_use_persistent_pool(bool a_val)
{
        m_use_persistent_pool = a_val;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void hlx::set_sock_opt_no_delay(bool a_val)
{
        m_sock_opt_no_delay = a_val;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void hlx::set_sock_opt_send_buf_size(uint32_t a_send_buf_size)
{
        m_sock_opt_send_buf_size = a_send_buf_size;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void hlx::set_sock_opt_recv_buf_size(uint32_t a_recv_buf_size)
{
        m_sock_opt_recv_buf_size = a_recv_buf_size;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void hlx::set_split_requests_by_thread(bool a_val)
{
        m_split_requests_by_thread = a_val;
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
                if(HLX_SERVER_STATUS_OK != l_status)
                {
                        return HLX_SERVER_STATUS_ERROR;
                }
        }

        if(m_t_hlx_list.empty())
        {
                l_status = init_t_hlx_list();
                if(STATUS_OK != l_status)
                {
                        return HLX_SERVER_STATUS_ERROR;
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

        return HLX_SERVER_STATUS_OK;

}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int hlx::stop(void)
{
        int32_t l_retval = HLX_SERVER_STATUS_OK;
        for (t_hlx_list_t::iterator i_t = m_t_hlx_list.begin();
                        i_t != m_t_hlx_list.end();
                        ++i_t)
        {
                (*i_t)->stop();
        }
        for(listener_list_t::iterator i_t = m_listener_list.begin();
                        i_t != m_listener_list.end();
                        ++i_t)
        {
                if(*i_t)
                {
                        delete *i_t;
                }
        }
        m_listener_list.clear();

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
        return HLX_SERVER_STATUS_OK;
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
http_resp *hlx::create_response(nconn &a_nconn)
{
        http_resp *l_resp = new http_resp();
        nbq *l_out_q = a_nconn.get_out_q();
        l_resp->set_q(l_out_q);
        l_out_q->reset_write();
        return l_resp;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t hlx::queue_response(nconn &a_nconn, http_resp *a_resp)
{
        return 0;
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

#if 0
        bool l_first_stat = true;
        if(l_first_stat) l_first_stat = false;
        else
        {
                l_cur_offset += snprintf(l_json_buf + l_cur_offset, l_json_buf_max_len - l_cur_offset,",");
        }
#endif

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
                // Consider making getter to grab a copy
                ao_summary_info.m_success += (*i_t)->m_summary_info.m_success;
                ao_summary_info.m_error_addr += (*i_t)->m_summary_info.m_error_addr;
                ao_summary_info.m_error_conn += (*i_t)->m_summary_info.m_error_conn;
                ao_summary_info.m_error_unknown += (*i_t)->m_summary_info.m_error_unknown;
                ao_summary_info.m_ssl_error_self_signed += (*i_t)->m_summary_info.m_ssl_error_self_signed;
                ao_summary_info.m_ssl_error_expired += (*i_t)->m_summary_info.m_ssl_error_expired;
                ao_summary_info.m_ssl_error_other += (*i_t)->m_summary_info.m_ssl_error_other;
                append_to_map(ao_summary_info.m_ssl_protocols, (*i_t)->m_summary_info.m_ssl_protocols);
                append_to_map(ao_summary_info.m_ssl_ciphers, (*i_t)->m_summary_info.m_ssl_ciphers);
        }
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
                return HLX_SERVER_STATUS_OK;
        }

        m_resolver = new resolver();

        // -------------------------------------------
        // Init resolver with cache
        // -------------------------------------------
        int32_t l_ldb_init_status;
        l_ldb_init_status = m_resolver->init(m_ai_cache, m_use_ai_cache);
        if(STATUS_OK != l_ldb_init_status)
        {
                NDBG_PRINT("Error performing resolver init with ai_cache: %s\n", m_ai_cache.c_str());
                return HLX_SERVER_STATUS_ERROR;
        }

        // not initialized yet
        for(listener_list_t::iterator i_t = m_listener_list.begin();
                        i_t != m_listener_list.end();
                        ++i_t)
        {
                if(*i_t)
                {
                        int32_t l_status;
                        l_status = (*i_t)->init();
                        if(l_status != STATUS_OK)
                        {
                                NDBG_PRINT("Error performing listener setup.\n");
                                return HLX_SERVER_STATUS_ERROR;
                        }
                }
        }

        // -------------------------------------------
        // SSL init...
        // -------------------------------------------
        // Server
        m_ssl_server_ctx = ssl_init(m_ssl_cipher_list, // ctx cipher list str
                                    m_ssl_options,     // ctx options
                                    m_ssl_ca_file,     // ctx ca file
                                    m_ssl_ca_path,     // ctx ca path
                                    true,              // is server?
                                    m_tls_key,         // tls key
                                    m_tls_crt);        // tls crt
        if(NULL == m_ssl_server_ctx)
        {
                NDBG_PRINT("Error: performing ssl_init(server) with cipher_list: %s\n", m_ssl_cipher_list.c_str());
                return HLX_SERVER_STATUS_ERROR;
        }
        // Client
        m_ssl_client_ctx = ssl_init(m_ssl_cipher_list, // ctx cipher list str
                                    m_ssl_options,     // ctx options
                                    m_ssl_ca_file,     // ctx ca file
                                    m_ssl_ca_path,     // ctx ca path
                                    false,             // is server?
                                    m_tls_key,         // tls key
                                    m_tls_crt);        // tls crt
        if(NULL == m_ssl_client_ctx)
        {
                NDBG_PRINT("Error: performing ssl_init(client) with cipher_list: %s\n", m_ssl_cipher_list.c_str());
                return HLX_SERVER_STATUS_ERROR;
        }
        m_is_initd = true;
        return HLX_SERVER_STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int hlx::init_t_hlx_list(void)
{
        // -------------------------------------------
        // Bury the config into a settings struct
        // -------------------------------------------
        t_hlx_conf_t l_settings;
        l_settings.m_verbose = m_verbose;
        l_settings.m_color = m_color;
        l_settings.m_show_summary = m_show_summary;
        l_settings.m_evr_loop_type = (evr_loop_type_t)m_evr_loop_type;
        l_settings.m_num_parallel = m_num_parallel;
        // TODO ???
        //l_settings.m_timeout_s = m_timeout_s;
        l_settings.m_num_reqs_per_conn = m_num_reqs_per_conn;
        l_settings.m_collect_stats = m_collect_stats;
        l_settings.m_use_persistent_pool = m_use_persistent_pool;
        l_settings.m_sock_opt_recv_buf_size = m_sock_opt_recv_buf_size;
        l_settings.m_sock_opt_send_buf_size = m_sock_opt_send_buf_size;
        l_settings.m_sock_opt_no_delay = m_sock_opt_no_delay;
        l_settings.m_tls_key = m_tls_key;
        l_settings.m_tls_crt = m_tls_crt;
        l_settings.m_ssl_server_ctx = m_ssl_server_ctx;
        l_settings.m_ssl_cipher_list = m_ssl_cipher_list;
        l_settings.m_ssl_options_str = m_ssl_options_str;
        l_settings.m_ssl_options = m_ssl_options;
        l_settings.m_ssl_ca_file = m_ssl_ca_file;
        l_settings.m_ssl_ca_path = m_ssl_ca_path;
        l_settings.m_ssl_client_ctx = m_ssl_client_ctx;
        l_settings.m_resolver = m_resolver;
        l_settings.m_hlx = this;

        // -------------------------------------------
        // Create t_hlx list...
        // -------------------------------------------
        for(uint32_t i_server_idx = 0; i_server_idx < m_num_threads; ++i_server_idx)
        {
                int32_t l_status;
                t_hlx *l_t_hlx = new t_hlx(l_settings);
                for(listener_list_t::iterator i_t = m_listener_list.begin();
                                i_t != m_listener_list.end();
                                ++i_t)
                {
                        if(*i_t)
                        {
                                l_status = l_t_hlx->add_listener(*(*i_t));
                                if(l_status != STATUS_OK)
                                {
                                        delete l_t_hlx;
                                        l_t_hlx = NULL;
                                        NDBG_PRINT("Error performing add_listener.\n");
                                        return STATUS_ERROR;
                                }
                        }
                }
                m_t_hlx_list.push_back(l_t_hlx);
        }
        // 0 threads -make a single server
        if(m_num_threads == 0)
        {
                int32_t l_status;
                t_hlx *l_t_hlx = new t_hlx(l_settings);
                for(listener_list_t::iterator i_t = m_listener_list.begin();
                                i_t != m_listener_list.end();
                                ++i_t)
                {
                        if(*i_t)
                        {
                                l_status = l_t_hlx->add_listener(*(*i_t));
                                if(l_status != STATUS_OK)
                                {
                                        delete l_t_hlx;
                                        l_t_hlx = NULL;
                                        NDBG_PRINT("Error performing add_listener.\n");
                                        return STATUS_ERROR;
                                }
                        }
                }
                m_t_hlx_list.push_back(l_t_hlx);
        }

        // -------------------------------------------
        // Add any subreqs...
        // -------------------------------------------
        while(m_subreq_queue.size())
        {
                add_subreq_t(*(m_subreq_queue.front()));
                m_subreq_queue.pop();
        }

        return STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
hlx::hlx(void):
        m_verbose(false),
        m_color(false),
        m_stats(false),
        m_num_threads(1),

        // TODO Need to make epoll vector resizeable
        m_num_parallel(512),

        m_num_reqs_per_conn(-1),
        m_sock_opt_recv_buf_size(0),
        m_sock_opt_send_buf_size(0),
        m_sock_opt_no_delay(false),
        m_split_requests_by_thread(true),
        m_listener_list(),
        m_subreq_queue(),

        // TODO Migrate to listener
#if 1
        m_ssl_server_ctx(NULL),
        m_tls_key(),
        m_tls_crt(),
        m_ssl_cipher_list(),
        m_ssl_options_str(),
        m_ssl_options(0),
        m_ssl_ca_file(),
        m_ssl_ca_path(),
#endif
        m_ssl_client_ctx(NULL),
        m_resolver(NULL),
        m_use_ai_cache(true),
        m_ai_cache(),
        m_collect_stats(false),
        m_use_persistent_pool(false),
        m_show_summary(false),
        m_start_time_ms(0),
        m_t_hlx_list(),
        m_evr_loop_type(EVR_LOOP_EPOLL),
        m_is_initd(false)
{
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
        for(listener_list_t::iterator i_t = m_listener_list.begin();
                        i_t != m_listener_list.end();
                        ++i_t)
        {
                if(*i_t)
                {
                        delete *i_t;
                }
        }
        m_listener_list.clear();
        if(m_ssl_server_ctx)
        {
                SSL_CTX_free(m_ssl_server_ctx);
                m_ssl_server_ctx = NULL;
        }
        if(m_ssl_client_ctx)
        {
                SSL_CTX_free(m_ssl_client_ctx);
                m_ssl_client_ctx = NULL;
        }
        if(m_resolver)
        {
                delete m_resolver;
                m_resolver = NULL;
        }
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
summary_info_struct::summary_info_struct():
        m_success(),
        m_error_addr(),
        m_error_conn(),
        m_error_unknown(),
        m_ssl_error_self_signed(),
        m_ssl_error_expired(),
        m_ssl_error_other(),
        m_ssl_protocols(),
        m_ssl_ciphers()
{};

} //namespace ns_hlx {
