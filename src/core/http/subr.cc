//: ----------------------------------------------------------------------------
//: Copyright (C) 2016 Verizon.  All Rights Reserved.
//: All Rights Reserved
//:
//: \file:    subr.cc
//: \details: TODO
//: \author:  Reed P. Morrison
//: \date:    07/20/2015
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
#include "nconn_tcp.h"
#include "ndebug.h"
#include "nresolver.h"
#include "t_srvr.h"
#include "hlx/ups_srvr_session.h"
#include "hlx/clnt_session.h"
#include "hlx/string_util.h"
#include "hlx/subr.h"
#include "hlx/api_resp.h"
#include "hlx/resp.h"
#include "hlx/status.h"
#include "http_parser/http_parser.h"
#include <stdlib.h>
#include <string.h>

namespace ns_hlx {

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
subr::subr(void):
        m_state(SUBR_STATE_NONE),
        m_scheme(SCHEME_NONE),
        m_host(),
        m_port(0),
        m_server_label(),
        m_save(true),
        m_connect_only(false),
        m_timeout_ms(10000),
        m_path(),
        m_query(),
        m_fragment(),
        m_userinfo(),
        m_hostname(),
        m_verb("GET"),
        m_keepalive(false),
        m_id(),
        m_where(),
        m_headers(),
        m_body_data(NULL),
        m_body_data_len(0),
        m_error_cb(NULL),
        m_completion_cb(NULL),
        m_data(NULL),
        m_detach_resp(false),
        m_uid(0),
        m_ups_srvr_session(NULL),
        m_clnt_session(NULL),
        m_host_info(),
        m_t_srvr(NULL),
        m_start_time_ms(0),
        m_end_time_ms(0),
        m_lookup_job(NULL),
        m_i_q(),
        m_tls_verify(false),
        m_tls_sni(false),
        m_tls_self_ok(false),
        m_tls_no_host_check(false),
        m_ups(NULL)
{
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
subr::subr(const subr &a_subr):
        m_state(a_subr.m_state),
        m_scheme(a_subr.m_scheme),
        m_host(a_subr.m_host),
        m_port(a_subr.m_port),
        m_server_label(a_subr.m_server_label),
        m_save(a_subr.m_save),
        m_connect_only(a_subr.m_connect_only),
        m_timeout_ms(a_subr.m_timeout_ms),
        m_path(a_subr.m_path),
        m_query(a_subr.m_query),
        m_fragment(a_subr.m_fragment),
        m_userinfo(a_subr.m_userinfo),
        m_hostname(a_subr.m_hostname),
        m_verb(a_subr.m_verb),
        m_keepalive(a_subr.m_keepalive),
        m_id(a_subr.m_id),
        m_where(a_subr.m_where),
        m_headers(a_subr.m_headers),
        m_body_data(a_subr.m_body_data),
        m_body_data_len(a_subr.m_body_data_len),
        m_error_cb(a_subr.m_error_cb),
        m_completion_cb(a_subr.m_completion_cb),
        m_data(a_subr.m_data),
        m_detach_resp(a_subr.m_detach_resp),
        m_uid(a_subr.m_uid),
        m_ups_srvr_session(a_subr.m_ups_srvr_session),
        m_clnt_session(a_subr.m_clnt_session),
        m_host_info(a_subr.m_host_info),
        m_t_srvr(a_subr.m_t_srvr),
        m_start_time_ms(a_subr.m_start_time_ms),
        m_end_time_ms(a_subr.m_end_time_ms),
        m_lookup_job(a_subr.m_lookup_job),
        m_i_q(a_subr.m_i_q),
        m_tls_verify(a_subr.m_tls_verify),
        m_tls_sni(a_subr.m_tls_sni),
        m_tls_self_ok(a_subr.m_tls_self_ok),
        m_tls_no_host_check(a_subr.m_tls_no_host_check),
        m_ups(a_subr.m_ups)
{
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
subr::~subr(void)
{
}

//: ----------------------------------------------------------------------------
//:                                  Getters
//: ----------------------------------------------------------------------------
//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
subr::subr_state_t subr::get_state(void) const
{
        return m_state;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
scheme_t subr::get_scheme(void) const
{
        return m_scheme;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
bool subr::get_save(void) const
{
        return m_save;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
bool subr::get_connect_only(void) const
{
        return m_connect_only;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
const std::string &subr::get_host(void) const
{
        return m_host;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
const std::string &subr::get_hostname(void) const
{
        return m_hostname;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
const std::string &subr::get_id(void) const
{
        return m_id;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
const std::string &subr::get_where(void) const
{
        return m_where;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
uint16_t subr::get_port(void) const
{
        return m_port;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
bool subr::get_keepalive(void) const
{
        return m_keepalive;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
subr::error_cb_t subr::get_error_cb(void) const
{
        return m_error_cb;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
subr::completion_cb_t subr::get_completion_cb(void) const
{
        return m_completion_cb;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t subr::get_timeout_ms(void) const
{
        return m_timeout_ms;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void *subr::get_data(void) const
{
        return m_data;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
bool subr::get_detach_resp(void) const
{
        return m_detach_resp;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
uint64_t subr::get_uid(void) const
{
        return m_uid;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
ups_srvr_session *subr::get_ups_srvr_session(void)
{
        return m_ups_srvr_session;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
clnt_session *subr::get_clnt_session(void) const
{
        return m_clnt_session;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
const host_info &subr::get_host_info(void) const
{
        return m_host_info;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
t_srvr *subr::get_t_srvr(void) const
{
        return m_t_srvr;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
uint64_t subr::get_start_time_ms(void) const
{
        return m_start_time_ms;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
uint64_t subr::get_end_time_ms(void) const
{
        return m_end_time_ms;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void *subr::get_lookup_job(void)
{
        return m_lookup_job;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
bool subr::get_tls_verify(void) const
{
        return m_tls_verify;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
bool subr::get_tls_sni(void) const
{
        return m_tls_sni;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
bool subr::get_tls_self_ok(void) const
{
        return m_tls_self_ok;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
bool subr::get_tls_no_host_check(void) const
{
        return m_tls_no_host_check;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void subr::reset_label(void)
{
        switch(m_scheme)
        {
        case SCHEME_NONE:
        {
                m_server_label += "none://";
                break;
        }
        case SCHEME_TCP:
        {
                m_server_label += "http://";
                break;
        }
        case SCHEME_TLS:
        {
                m_server_label += "https://";
                break;
        }
        default:
        {
                m_server_label += "default://";
                break;
        }
        }
        m_server_label += m_host;
        char l_port_str[16];
        snprintf(l_port_str, 16, ":%u", m_port);
        m_server_label += l_port_str;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
const std::string &subr::get_label(void)
{
        if(m_server_label.empty())
        {
                reset_label();
        }
        return m_server_label;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
base_u *subr::get_ups(void)
{
        return m_ups;
}

//: ----------------------------------------------------------------------------
//:                                  Setters
//: ----------------------------------------------------------------------------
//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void subr::set_state(subr_state_t a_state)
{
        m_state = a_state;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void subr::set_scheme(scheme_t a_scheme)
{
        m_scheme = a_scheme;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void subr::set_save(bool a_val)
{
        m_save = a_val;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void subr::set_connect_only(bool a_val)
{
        m_connect_only = a_val;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void subr::set_keepalive(bool a_val)
{
        m_keepalive = a_val;
        del_header("Connection");
        if(m_keepalive)
        {
                set_header("Connection", "keep-alive");
                //if(m_num_reqs_per_conn == 1)
                //{
                //        set_num_reqs_per_conn(-1);
                //}
        }
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void subr::set_error_cb(error_cb_t a_cb)
{
        m_error_cb = a_cb;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void subr::set_completion_cb(completion_cb_t a_cb)
{
        m_completion_cb = a_cb;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void subr::set_timeout_ms(int32_t a_val)
{
        m_timeout_ms = a_val;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void subr::set_host(const std::string &a_val)
{
        m_host = a_val;
        del_header("Host");
        set_header("Host", a_val);
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void subr::set_hostname(const std::string &a_val)
{
        m_hostname = a_val;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void subr::set_id(const std::string &a_val)
{
        m_id = a_val;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void subr::set_where(const std::string &a_val)
{
        m_where = a_val;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void subr::set_port(uint16_t a_val)
{
        m_port = a_val;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void subr::set_data(void *a_data)
{
        m_data = a_data;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void subr::set_detach_resp(bool a_val)
{
        m_detach_resp = a_val;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void subr::set_uid(uint64_t a_uid)
{
        m_uid = a_uid;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void subr::set_ups_srvr_session(ups_srvr_session *a_ups_srvr_session)
{
        m_ups_srvr_session = a_ups_srvr_session;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void subr::set_clnt_session(clnt_session *a_clnt_session)
{
        m_clnt_session = a_clnt_session;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void subr::set_host_info(const host_info &a_host_info)
{
        m_host_info = a_host_info;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void subr::set_t_srvr(t_srvr *a_t_srvr)
{
        m_t_srvr = a_t_srvr;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void subr::set_start_time_ms(uint64_t a_val)
{
       m_start_time_ms = a_val;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void subr::set_end_time_ms(uint64_t a_val)
{
        m_end_time_ms = a_val;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void subr::set_lookup_job(void *a_data)
{
        m_lookup_job = a_data;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void subr::set_i_q(subr_list_t::iterator a_i_q)
{
        m_i_q = a_i_q;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void subr::set_tls_verify(bool a_val)
{
        m_tls_verify = a_val;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void subr::set_tls_sni(bool a_val)
{
        m_tls_sni = a_val;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void subr::set_tls_self_ok(bool a_val)
{
        m_tls_self_ok = a_val;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void subr::set_tls_no_host_check(bool a_val)
{
        m_tls_no_host_check = a_val;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void subr::set_ups(base_u *a_ups)
{
        m_ups = a_ups;
}

//: ----------------------------------------------------------------------------
//:                             Request Getters
//: ----------------------------------------------------------------------------

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
const std::string &subr::get_verb(void)
{
        return m_verb;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
bool subr::get_expect_resp_body_flag(void)
{
        if(m_verb == "HEAD")
        {
                return false;
        }
        else
        {
                return true;
        }
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
const std::string &subr::get_path(void)
{
        return m_path;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
const std::string &subr::get_query(void)
{
        return m_query;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
const std::string &subr::get_fragment(void)
{
        return m_fragment;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
const kv_map_list_t &subr::get_headers(void)
{
        return m_headers;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
const char *subr::get_body_data(void)
{
        return m_body_data;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
uint32_t subr::get_body_len(void)
{
        return m_body_data_len;
}

//: ----------------------------------------------------------------------------
//:                             Request Setters
//: ----------------------------------------------------------------------------

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void subr::set_verb(const std::string &a_verb)
{
        m_verb = a_verb;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void subr::set_path(const std::string &a_path)
{
        m_path = a_path;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void subr::set_query(const std::string &a_query)
{
        m_query = a_query;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void subr::set_fragment(const std::string &a_fragment)
{
        m_fragment = a_fragment;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void subr::set_headers(const kv_map_list_t &a_headers_list)
{
        m_headers = a_headers_list;
        kv_map_list_t::const_iterator i_vl = m_headers.find("Connection");
        if(i_vl != m_headers.end())
        {
                if(i_vl->second.size() &&
                   (strncasecmp(i_vl->second.begin()->c_str(), "keep-alive", sizeof("keep-alive")) == 0))
                {
                        m_keepalive = true;
                }
        }
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int subr::set_header(const std::string &a_key, const std::string &a_val)
{
        kv_map_list_t::iterator i_obj = m_headers.find(a_key);
        if(i_obj != m_headers.end())
        {
                // Special handling for Host/User-agent/referer
                bool l_replace = false;
                bool l_remove = false;
                if(!strcasecmp(a_key.c_str(), "User-Agent") ||
                   !strcasecmp(a_key.c_str(), "Referer") ||
                   !strcasecmp(a_key.c_str(), "Accept") ||
                   !strcasecmp(a_key.c_str(), "Host"))
                {
                        l_replace = true;
                        if(a_val.empty())
                        {
                                l_remove = true;
                        }
                }
                if(l_replace)
                {
                        i_obj->second.pop_front();
                        if(!l_remove)
                        {
                                i_obj->second.push_back(a_val);
                        }
                }
                else
                {
                        i_obj->second.push_back(a_val);
                }
        }
        else
        {
                str_list_t l_list;
                l_list.push_back(a_val);
                m_headers[a_key] = l_list;
        }
        return HLX_STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int subr::del_header(const std::string &a_key)
{
        kv_map_list_t::iterator i_obj = m_headers.find(a_key);
        if(i_obj != m_headers.end())
        {
                m_headers.erase(i_obj);
        }
        return HLX_STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void subr::set_body_data(const char *a_ptr, uint32_t a_len)
{
        m_body_data = a_ptr;
        m_body_data_len = a_len;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void subr::clear_headers(void)
{
        m_headers.clear();
}

//: ----------------------------------------------------------------------------
//:                              Initialize
//: ----------------------------------------------------------------------------
//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t subr::init_with_url(const std::string &a_url)
{
        std::string l_url_fixed = a_url;
        // Find scheme prefix "://"
        if(a_url.find("://", 0) == std::string::npos)
        {
                l_url_fixed = "http://" + a_url;
        }

        //NDBG_PRINT("Parse url:           %s\n", a_url.c_str());
        //NDBG_PRINT("Parse a_wildcarding: %d\n", a_wildcarding);
        http_parser_url l_url;
        http_parser_url_init(&l_url);
        // silence bleating memory sanitizers...
        //memset(&l_url, 0, sizeof(l_url));
        int l_status;
        l_status = http_parser_parse_url(l_url_fixed.c_str(), l_url_fixed.length(), 0, &l_url);
        if(l_status != 0)
        {
                NDBG_PRINT("Error parsing url: %s\n", l_url_fixed.c_str());
                // TODO get error msg from http_parser
                return HLX_STATUS_ERROR;
        }

        // Set no port
        m_port = 0;

        for(uint32_t i_part = 0; i_part < UF_MAX; ++i_part)
        {
                //NDBG_PRINT("i_part: %d offset: %d len: %d\n", i_part, l_url.field_data[i_part].off, l_url.field_data[i_part].len);
                //NDBG_PRINT("len+off: %d\n",       l_url.field_data[i_part].len + l_url.field_data[i_part].off);
                //NDBG_PRINT("a_url.length(): %d\n", (int)a_url.length());
                if(l_url.field_data[i_part].len &&
                  // TODO Some bug with parser -parsing urls like "http://127.0.0.1" sans paths
                  ((l_url.field_data[i_part].len + l_url.field_data[i_part].off) <= l_url_fixed.length()))
                {
                        switch(i_part)
                        {
                        case UF_SCHEMA:
                        {
                                std::string l_part = l_url_fixed.substr(l_url.field_data[i_part].off, l_url.field_data[i_part].len);
                                //NDBG_PRINT("l_part: %s\n", l_part.c_str());
                                if(l_part == "http")
                                {
                                        m_scheme = SCHEME_TCP;
                                }
                                else if(l_part == "https")
                                {
                                        m_scheme = SCHEME_TLS;
                                }
                                else
                                {
                                        NDBG_PRINT("Error schema[%s] is unsupported\n", l_part.c_str());
                                        return HLX_STATUS_ERROR;
                                }
                                break;
                        }
                        case UF_HOST:
                        {
                                std::string l_part = l_url_fixed.substr(l_url.field_data[i_part].off, l_url.field_data[i_part].len);
                                //NDBG_PRINT("l_part[UF_HOST]: %s\n", l_part.c_str());
                                m_host = l_part;
                                break;
                        }
                        case UF_PORT:
                        {
                                std::string l_part = l_url_fixed.substr(l_url.field_data[i_part].off, l_url.field_data[i_part].len);
                                //NDBG_PRINT("l_part[UF_PORT]: %s\n", l_part.c_str());
                                m_port = (uint16_t)strtoul(l_part.c_str(), NULL, 10);
                                break;
                        }
                        case UF_PATH:
                        {
                                std::string l_part = l_url_fixed.substr(l_url.field_data[i_part].off, l_url.field_data[i_part].len);
                                //NDBG_PRINT("l_part[UF_PATH]: %s\n", l_part.c_str());
                                m_path = l_part;
                                break;
                        }
                        case UF_QUERY:
                        {
                                std::string l_part = l_url_fixed.substr(l_url.field_data[i_part].off, l_url.field_data[i_part].len);
                                //NDBG_PRINT("l_part[UF_QUERY]: %s\n", l_part.c_str());
                                m_query = l_part;
                                break;
                        }
                        case UF_FRAGMENT:
                        {
                                std::string l_part = l_url_fixed.substr(l_url.field_data[i_part].off, l_url.field_data[i_part].len);
                                //NDBG_PRINT("l_part[UF_FRAGMENT]: %s\n", l_part.c_str());
                                m_fragment = l_part;
                                break;
                        }
                        case UF_USERINFO:
                        {
                                std::string l_part = l_url_fixed.substr(l_url.field_data[i_part].off, l_url.field_data[i_part].len);
                                //sNDBG_PRINT("l_part[UF_USERINFO]: %s\n", l_part.c_str());
                                m_userinfo = l_part;
                                break;
                        }
                        default:
                        {
                                break;
                        }
                        }
                }
        }

        // Default ports
        if(!m_port)
        {
                switch(m_scheme)
                {
                case SCHEME_TCP:
                {
                        m_port = 80;
                        break;
                }
                case SCHEME_TLS:
                {
                        m_port = 443;
                        break;
                }
                default:
                {
                        m_port = 80;
                        break;
                }
                }
        }
        //m_num_to_req = m_path_vector.size();
        //NDBG_PRINT("Showing parsed url.\n");
        //m_url.show();
        if (HLX_STATUS_OK != l_status)
        {
                // Failure
                NDBG_PRINT("Error parsing url: %s.\n", l_url_fixed.c_str());
                return HLX_STATUS_ERROR;
        }
        //NDBG_PRINT("Parsed url: %s\n", l_url_fixed.c_str());
        return HLX_STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: Cancel active or remove pending subrequest
//: \return:  status OK on success ERROR on fail
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t subr::cancel(void)
{
        //NDBG_PRINT("CANCEL host: %s --m_state: %d\n", m_host.c_str(), m_state);
        // -------------------------------------------------
        // Cancel subr based on state
        // -------------------------------------------------
        switch(m_state)
        {
        case SUBR_STATE_QUEUED:
        {
                // Delete from queue
                *m_i_q = NULL;
                if(m_error_cb)
                {
                        m_error_cb(*(this), NULL, HTTP_STATUS_GATEWAY_TIMEOUT, get_resp_status_str(HTTP_STATUS_GATEWAY_TIMEOUT));
                        // TODO Check status...
                }
                return HLX_STATUS_OK;
        }
        case SUBR_STATE_DNS_LOOKUP:
        {
                // Get lookup job
                nresolver::job *l_job = static_cast<nresolver::job *>(m_lookup_job);
                if(l_job)
                {
                        l_job->m_cb = NULL;
                }
                if(m_error_cb)
                {
                        m_error_cb(*(this), NULL, HTTP_STATUS_GATEWAY_TIMEOUT, get_resp_status_str(HTTP_STATUS_GATEWAY_TIMEOUT));
                        // TODO Check status...
                }
                return HLX_STATUS_OK;
        }
        case SUBR_STATE_ACTIVE:
        {
                if(m_ups_srvr_session &&
                   m_ups_srvr_session->m_t_srvr &&
                   m_ups_srvr_session->m_nconn)
                {
                        //NDBG_PRINT("%sCANCEL%s: nconn: %p\n", ANSI_COLOR_FG_RED, ANSI_COLOR_OFF, m_clnt->m_nconn);
                        if(m_ups_srvr_session->m_resp)
                        {
                                m_ups_srvr_session->m_resp->set_status(HTTP_STATUS_GATEWAY_TIMEOUT);
                        }
                        m_ups_srvr_session->m_nconn->set_status(CONN_STATUS_CANCELLED);
                        int32_t l_status;
                        l_status = m_ups_srvr_session->subr_error(HTTP_STATUS_GATEWAY_TIMEOUT);
                        if(l_status != HLX_STATUS_OK)
                        {
                                // TODO Check error;
                        }
                        return m_ups_srvr_session->m_t_srvr->cleanup_srvr_session(m_ups_srvr_session, m_ups_srvr_session->m_nconn);
                }
                else
                {
                        //NDBG_PRINT("ERROR\n");
                        return HLX_STATUS_ERROR;
                }
                break;
        }
        default:
        {
                break;
        }
        }
        return HLX_STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t subr::create_request(nbq &ao_q)
{
        std::string l_path_ref = get_path();

        char l_buf[2048];
        int32_t l_len = 0;
        if(l_path_ref.empty())
        {
                l_path_ref = "/";
        }
        if(!(get_query().empty()))
        {
                l_path_ref += "?";
                l_path_ref += get_query();
        }
        //NDBG_PRINT("HOST: %s PATH: %s\n", a_reqlet.m_url.m_host.c_str(), l_path_ref.c_str());
        l_len = snprintf(l_buf, sizeof(l_buf),
                        "%s %.500s HTTP/1.1", get_verb().c_str(), l_path_ref.c_str());

        nbq_write_request_line(ao_q, l_buf, l_len);

        // -------------------------------------------
        // Add repo headers
        // -------------------------------------------
        bool l_specd_host = false;
        bool l_specd_ua = false;

        // Loop over header map
        for(kv_map_list_t::const_iterator i_hl = get_headers().begin();
            i_hl != get_headers().end();
            ++i_hl)
        {
                if(i_hl->first.empty() || i_hl->second.empty())
                {
                        continue;
                }
                for(str_list_t::const_iterator i_v = i_hl->second.begin();
                    i_v != i_hl->second.end();
                    ++i_v)
                {
                        nbq_write_header(ao_q, i_hl->first.c_str(), i_hl->first.length(), i_v->c_str(), i_v->length());
                        if (strcasecmp(i_hl->first.c_str(), "host") == 0)
                        {
                                l_specd_host = true;
                        }
                        if (strcasecmp(i_hl->first.c_str(), "user-agent") == 0)
                        {
                                l_specd_ua = true;
                        }

                }
        }

        // -------------------------------------------
        // Default Host if unspecified
        // -------------------------------------------
        if (!l_specd_host)
        {
                nbq_write_header(ao_q, "Host", strlen("Host"),
                                 get_host().c_str(), get_host().length());
        }

        // -------------------------------------------
        // Default server if unspecified
        // -------------------------------------------
        if (!l_specd_ua)
        {
                if(!m_t_srvr->get_srvr())
                {
                        return HLX_STATUS_ERROR;
                }
                const std::string &l_ua = m_t_srvr->get_srvr()->get_server_name();
                nbq_write_header(ao_q, "User-Agent", strlen("User-Agent"),
                                l_ua.c_str(), l_ua.length());
        }

        // -------------------------------------------
        // body
        // -------------------------------------------
        if(get_body_data() && get_body_len())
        {
                //NDBG_PRINT("Write: buf: %p len: %d\n", l_buf, l_len);
                nbq_write_body(ao_q, get_body_data(), get_body_len());
        }
        else
        {
                nbq_write_body(ao_q, NULL, 0);
        }
        return HLX_STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
#if 0
void subr::set_uri(const std::string &a_uri)
{
        m_uri = a_uri;

        // -------------------------------------------------
        // TODO Zero copy with something like substring...
        // This is pretty awful performance wise
        // -------------------------------------------------
        // Read uri up to first '?'
        size_t l_query_pos = 0;
        if((l_query_pos = m_uri.find('?', 0)) == std::string::npos)
        {
                // No query string -path == uri
                m_path = m_uri;
                return;
        }

        m_path = m_uri.substr(0, l_query_pos);

        // TODO Url decode???

        std::string l_query = m_uri.substr(l_query_pos + 1, m_uri.length() - l_query_pos + 1);

        // Split the query by '&'
        if(!l_query.empty())
        {

                //NDBG_PRINT("%s__QUERY__%s: l_query: %s\n", ANSI_COLOR_BG_WHITE, ANSI_COLOR_OFF, l_query.c_str());

                size_t l_qi_begin = 0;
                size_t l_qi_end = 0;
                bool l_last = false;
                while (!l_last)
                {
                        l_qi_end = l_query.find('&', l_qi_begin);
                        if(l_qi_end == std::string::npos)
                        {
                                l_last = true;
                                l_qi_end = l_query.length();
                        }

                        std::string l_query_item = l_query.substr(l_qi_begin, l_qi_end - l_qi_begin);

                        // Search for '='
                        size_t l_qi_val_pos = 0;
                        l_qi_val_pos = l_query_item.find('=', 0);
                        std::string l_q_k;
                        std::string l_q_v;
                        if(l_qi_val_pos != std::string::npos)
                        {
                                l_q_k = l_query_item.substr(0, l_qi_val_pos);
                                l_q_v = l_query_item.substr(l_qi_val_pos + 1, l_query_item.length());
                        }
                        else
                        {
                                l_q_k = l_query_item;
                        }

                        //NDBG_PRINT("%s__QUERY__%s: k[%s]: %s\n",
                        //                ANSI_COLOR_BG_WHITE, ANSI_COLOR_OFF, l_q_k.c_str(), l_q_v.c_str());

                        // Add to list
                        kv_list_map_t::iterator i_key = m_query.find(l_q_k);
                        if(i_key == m_query.end())
                        {
                                value_list_t l_list;
                                l_list.push_back(l_q_v);
                                m_query[l_q_k] = l_list;
                        }
                        else
                        {
                                i_key->second.push_back(l_q_v);
                        }

                        // Move fwd
                        l_qi_begin = l_qi_end + 1;

                }
        }

}
#endif

#if 0
//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
const kv_list_map_t &subr::get_uri_decoded_query(void)
{

        if(m_query_uri_decoded.empty() && !m_query.empty())
        {

                // Decode the arguments for now
                for(kv_list_map_t::const_iterator i_kv = m_query.begin();
                    i_kv != m_query.end();
                    ++i_kv)
                {
                        value_list_t l_value_list;
                        for(value_list_t::const_iterator i_v = i_kv->second.begin();
                            i_v != i_kv->second.end();
                            ++i_v)
                        {
                                std::string l_v = uri_decode(*i_v);
                                l_value_list.push_back(l_v);
                        }

                        std::string l_k = uri_decode(i_kv->first);
                        m_query_uri_decoded[l_k] = l_value_list;
                }
        }

        return m_query_uri_decoded;

}
#endif

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
#if 0
void subr::show(bool a_color)
{
        std::string l_host_color = "";
        std::string l_query_color = "";
        std::string l_header_color = "";
        std::string l_body_color = "";
        std::string l_off_color = "";
        if(a_color)
        {
                l_host_color = ANSI_COLOR_FG_BLUE;
                l_query_color = ANSI_COLOR_FG_MAGENTA;
                l_header_color = ANSI_COLOR_FG_GREEN;
                l_body_color = ANSI_COLOR_FG_YELLOW;
                l_off_color = ANSI_COLOR_OFF;
        }

        // Host
        NDBG_OUTPUT("%sUri%s:  %s\n", l_host_color.c_str(), l_off_color.c_str(), m_uri.c_str());
        NDBG_OUTPUT("%sPath%s: %s\n", l_host_color.c_str(), l_off_color.c_str(), m_path.c_str());

        // Query
        for(kv_list_map_t::iterator i_key = m_query.begin();
                        i_key != m_query.end();
            ++i_key)
        {
                NDBG_OUTPUT("%s%s%s: %s\n",
                                l_query_color.c_str(), i_key->first.c_str(), l_off_color.c_str(),
                                i_key->second.begin()->c_str());
        }


        // Headers
        for(kv_list_map_t::iterator i_key = m_headers.begin();
            i_key != m_headers.end();
            ++i_key)
        {
                NDBG_OUTPUT("%s%s%s: %s\n",
                                l_header_color.c_str(), i_key->first.c_str(), l_off_color.c_str(),
                                i_key->second.begin()->c_str());
        }

        // Body
        NDBG_OUTPUT("%sBody%s: %s\n", l_body_color.c_str(), l_off_color.c_str(), m_body.c_str());

}
#endif

} //namespace ns_hlx {
