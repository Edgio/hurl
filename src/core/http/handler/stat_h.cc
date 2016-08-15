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
#include "hlx/srvr.h"
#include "hlx/clnt_session.h"

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
h_resp_t stat_h::do_get(clnt_session &a_clnt_session,
                        rqst &a_rqst,
                        const url_pmap_t &a_url_pmap)
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

        srvr *l_srvr = a_clnt_session.get_srvr();
        if(!l_srvr)
        {
                return H_RESP_SERVER_ERROR;
        }

        // -------------------------------------------------
        // Create body...
        // -------------------------------------------------
        t_stat_cntr_list_t l_threads_stat;
        t_stat_cntr_t l_stat;
        t_stat_calc_t l_stat_calc;
        l_srvr->get_stat(l_stat, l_stat_calc, l_threads_stat);

        rapidjson::StringBuffer l_strbuf;
        rapidjson::Writer<rapidjson::StringBuffer> l_writer(l_strbuf);
        l_writer.StartObject();

#define ADD_MEMBER(_m) do {\
                l_writer.Key(#_m);\
                l_writer.Uint64(l_stat.m_##_m);\
        } while(0)

        ADD_MEMBER(dns_resolve_req);
        ADD_MEMBER(dns_resolve_active);
        ADD_MEMBER(dns_resolved);
        ADD_MEMBER(dns_resolve_ev);

        ADD_MEMBER(upsv_conn_started);
        ADD_MEMBER(upsv_conn_completed);
        ADD_MEMBER(upsv_reqs);
        ADD_MEMBER(upsv_idle_killed);
        ADD_MEMBER(upsv_subr_queued);
        ADD_MEMBER(upsv_resp);
        ADD_MEMBER(upsv_resp_status_2xx);
        ADD_MEMBER(upsv_resp_status_3xx);
        ADD_MEMBER(upsv_resp_status_4xx);
        ADD_MEMBER(upsv_resp_status_5xx);
        ADD_MEMBER(upsv_errors);
        ADD_MEMBER(upsv_bytes_read);
        ADD_MEMBER(upsv_bytes_written);

        ADD_MEMBER(clnt_conn_started);
        ADD_MEMBER(clnt_conn_completed);
        ADD_MEMBER(clnt_reqs);
        ADD_MEMBER(clnt_idle_killed);
        ADD_MEMBER(clnt_resp);
        ADD_MEMBER(clnt_resp_status_2xx);
        ADD_MEMBER(clnt_resp_status_3xx);
        ADD_MEMBER(clnt_resp_status_4xx);
        ADD_MEMBER(clnt_resp_status_5xx);
        ADD_MEMBER(clnt_errors);
        ADD_MEMBER(clnt_bytes_read);
        ADD_MEMBER(clnt_bytes_written);

        ADD_MEMBER(pool_conn_active);
        ADD_MEMBER(pool_conn_idle);
        ADD_MEMBER(pool_proxy_conn_active);
        ADD_MEMBER(pool_proxy_conn_idle);
        ADD_MEMBER(pool_clnt_free);
        ADD_MEMBER(pool_clnt_used);
        ADD_MEMBER(pool_resp_free);
        ADD_MEMBER(pool_resp_used);
        ADD_MEMBER(pool_rqst_free);
        ADD_MEMBER(pool_rqst_used);
        ADD_MEMBER(pool_nbq_free);
        ADD_MEMBER(pool_nbq_used);

        ADD_MEMBER(total_run);
        ADD_MEMBER(total_add_timer);

#define ADD_MEMBER_CALC(_m) do {\
                l_writer.Key(#_m);\
                l_writer.Double(l_stat_calc.m_##_m);\
        } while(0)

        ADD_MEMBER_CALC(clnt_req_s);
        ADD_MEMBER_CALC(clnt_bytes_read_s);
        ADD_MEMBER_CALC(clnt_bytes_write_s);
        ADD_MEMBER_CALC(clnt_resp_status_2xx_pcnt);
        ADD_MEMBER_CALC(clnt_resp_status_3xx_pcnt);
        ADD_MEMBER_CALC(clnt_resp_status_4xx_pcnt);
        ADD_MEMBER_CALC(clnt_resp_status_5xx_pcnt);
        ADD_MEMBER_CALC(upsv_req_s);
        ADD_MEMBER_CALC(upsv_bytes_read_s);
        ADD_MEMBER_CALC(upsv_bytes_write_s);
        ADD_MEMBER_CALC(upsv_resp_status_2xx_pcnt);
        ADD_MEMBER_CALC(upsv_resp_status_3xx_pcnt);
        ADD_MEMBER_CALC(upsv_resp_status_4xx_pcnt);
        ADD_MEMBER_CALC(upsv_resp_status_5xx_pcnt);

        //xstat_t m_stat_us_connect;
        //xstat_t m_stat_us_first_response;
        //xstat_t m_stat_us_end_to_end;

        // TODO Verbose mode with proxy connection info -hostnames...

        l_writer.EndObject();

        api_resp &l_api_resp = create_api_resp(a_clnt_session);
        l_api_resp.add_std_headers(HTTP_STATUS_OK,
                                   "application/json",
                                   l_strbuf.GetSize(),
                                   a_rqst.m_supports_keep_alives,
                                   *(l_srvr));
        l_api_resp.set_body_data(l_strbuf.GetString(), l_strbuf.GetSize());
        queue_api_resp(a_clnt_session, l_api_resp);
        return H_RESP_DONE;
}

} //namespace ns_hlx {
