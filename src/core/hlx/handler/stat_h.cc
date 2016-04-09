//: ----------------------------------------------------------------------------
//: Copyright (C) 2016 Verizon.  All Rights Reserved.
//: All Rights Reserved
//:
//: \file:    stat_h.cc
//: \details: TODO
//: \author:  Reed P. Morrison
//: \date:    01/19/2016
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
#include "hlx/stat_h.h"
#include "hlx/stat.h"
#include "hlx/rqst.h"
#include "hlx/api_resp.h"
#include "hlx/hlx.h"

#include "ndebug.h"

#include "rapidjson/document.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/prettywriter.h"

namespace ns_hlx {

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
stat_h::stat_h(void):
        default_rqst_h()
{
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
stat_h::~stat_h(void)
{
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
h_resp_t stat_h::do_get(hconn &a_hconn, rqst &a_rqst, const url_pmap_t &a_url_pmap)
{
        // Get verbose
        int32_t l_verbose = 0;
        const ns_hlx::query_map_t &l_q_map = a_rqst.get_url_query_map();
        ns_hlx::query_map_t::const_iterator i_q;
        if((i_q = l_q_map.find("verbose")) != l_q_map.end())
        {
                std::string l_str = i_q->second;
                l_verbose = atoi(i_q->second.c_str());
                if (l_verbose < 0)
                {
                        return H_RESP_CLIENT_ERROR;
                }
        }

        hlx *l_hlx = get_hlx(a_hconn);
        if(!l_hlx)
        {
                return H_RESP_SERVER_ERROR;
        }

        // -------------------------------------------------
        // Create body...
        // -------------------------------------------------
        t_stat_list_t l_threads_stat;
        t_stat_t l_stat;
        l_hlx->get_stat(l_stat, l_threads_stat);

        rapidjson::Document l_body;
        l_body.SetObject(); // Define doc as object -rather than array
        rapidjson::Document::AllocatorType& l_alloc = l_body.GetAllocator();

#define ADD_MEMBER(_m) \
        l_body.AddMember(#_m, l_stat.m_##_m, l_alloc)

        ADD_MEMBER(dns_resolve_req);
        ADD_MEMBER(dns_resolve_active);
        ADD_MEMBER(dns_resolved);
        ADD_MEMBER(dns_resolve_ev);
        ADD_MEMBER(ups_conn_started);
        ADD_MEMBER(ups_conn_completed);
        ADD_MEMBER(ups_reqs);
        ADD_MEMBER(ups_idle_killed);
        ADD_MEMBER(ups_subr_queued);
        ADD_MEMBER(cln_conn_started);
        ADD_MEMBER(cln_conn_completed);
        ADD_MEMBER(cln_reqs);
        ADD_MEMBER(cln_idle_killed);
        ADD_MEMBER(pool_conn_active);
        ADD_MEMBER(pool_conn_idle);
        ADD_MEMBER(pool_proxy_conn_active);
        ADD_MEMBER(pool_proxy_conn_idle);
        ADD_MEMBER(pool_hconn_free);
        ADD_MEMBER(pool_hconn_used);
        ADD_MEMBER(pool_resp_free);
        ADD_MEMBER(pool_resp_used);
        ADD_MEMBER(pool_rqst_free);
        ADD_MEMBER(pool_rqst_used);
        ADD_MEMBER(pool_nbq_free);
        ADD_MEMBER(pool_nbq_used);
        ADD_MEMBER(total_run);
        ADD_MEMBER(total_errors);
        ADD_MEMBER(total_bytes_read);
        ADD_MEMBER(total_bytes_written);
#ifdef __linux__
        ADD_MEMBER(rsc_cpu_usr_ms);
        ADD_MEMBER(rsc_cpu_sys_ms);
        ADD_MEMBER(rsc_mem_rss_kb);
#endif

        //xstat_t m_stat_us_connect;
        //xstat_t m_stat_us_first_response;
        //xstat_t m_stat_us_end_to_end;

        // TODO DEBUG???
        //subr_pending_resolv_map_t m_subr_pending_resolv_map;
        //status_code_count_map_t m_status_code_count_map;

        rapidjson::StringBuffer l_strbuf;
        rapidjson::Writer<rapidjson::StringBuffer> l_writer(l_strbuf);
        l_body.Accept(l_writer);

        api_resp &l_api_resp = create_api_resp(a_hconn);
        l_api_resp.add_std_headers(HTTP_STATUS_OK, "application/json",
                                   l_strbuf.GetSize(), a_rqst,
                                   *(l_hlx));
        l_api_resp.set_body_data(l_strbuf.GetString(), l_strbuf.GetSize());
        queue_api_resp(a_hconn, l_api_resp);
        return H_RESP_DONE;
}

} //namespace ns_hlx {
