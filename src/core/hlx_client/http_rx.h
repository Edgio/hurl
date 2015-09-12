//: ----------------------------------------------------------------------------
//: Copyright (C) 2014 Verizon.  All Rights Reserved.
//: All Rights Reserved
//:
//: \file:    http_rx.h
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
#ifndef _HTTP_RX_H
#define _HTTP_RX_H

//: ----------------------------------------------------------------------------
//: Includes
//: ----------------------------------------------------------------------------
#include "nconn_pool.h"
#include "settings.h"
#include "ndebug.h"
#include "http_rx.h"

// signal
#include <signal.h>

//: ----------------------------------------------------------------------------
//: Constants
//: ----------------------------------------------------------------------------

//: ----------------------------------------------------------------------------
//: Macros
//: ----------------------------------------------------------------------------


//: ----------------------------------------------------------------------------
//: Fwd decl's
//: ----------------------------------------------------------------------------


namespace ns_hlx {

//: ----------------------------------------------------------------------------
//: Types
//: ----------------------------------------------------------------------------
typedef std::vector <std::string> path_substr_vector_t;
typedef std::vector <std::string> path_vector_t;
typedef std::map<std::string, std::string> header_map_t;

typedef struct range_struct {
        uint32_t m_start;
        uint32_t m_end;
} range_t;

typedef std::vector <range_t> range_vector_t;
typedef std::map<uint16_t, uint32_t > status_code_count_map_t;


//: ----------------------------------------------------------------------------
//: http_rx
//: ----------------------------------------------------------------------------
class http_rx {

public:

        // -------------------------------------------------
        // Wildcard support
        // -------------------------------------------------
        typedef enum path_order_enum {
                EXPLODED_PATH_ORDER_SEQUENTIAL = 0,
                EXPLODED_PATH_ORDER_RANDOM
        } path_order_t;

        // -------------------------------------------------
        // Public methods
        // -------------------------------------------------
        http_rx(std::string a_id, int64_t a_num_to_req = 1):
                m_id(a_id),
                m_label("UNDEFINED"),
                m_host(),
                m_path(),
                m_query(),
                m_fragment(),
                m_userinfo(),
                m_hostname(),
                m_port(),
                m_where(),
                m_scheme(),
                m_host_info(),
                m_idx(0),
                m_stat_agg(),
                m_multipath(false),
                m_is_resolved_flag(),
                m_num_to_req(a_num_to_req),
                m_num_reqd(),
                m_repeat_path_flag(false),
                m_path_vector(),
                m_path_order(EXPLODED_PATH_ORDER_SEQUENTIAL),
                m_path_vector_last_idx(0),
                m_label_hash(0),
                m_resp(NULL)
                {};
        http_rx(const http_rx &a_http_rx);
        http_rx& operator=(const http_rx &a_http_rx);
        ~http_rx()
        {};

        void reset(void) {m_num_reqd = 0;};
        int32_t init_with_url(const std::string &a_url, bool a_wildcarding=false);
        void bump_num_requested(void) {++m_num_reqd;}
        int32_t resolve(resolver &a_resolver);
        bool is_resolved(void) {return m_is_resolved_flag;}
        const std::string &get_id(void) {return m_id;}
        const std::string &get_path(void *a_rand);
        const std::string &get_label(void);
        uint64_t get_label_hash(void);
        void set_host(const std::string &a_host);
        void set_port(uint16_t &a_port) { m_port = a_port;}
        void set_response(uint16_t a_response_status, const char *a_response);
        void set_id(uint64_t a_id) {m_id = a_id;}

        void set_http_resp(http_resp *a_resp) {m_resp = a_resp;}
        http_resp *get_http_resp(void) {return m_resp;}

        bool is_done(void) {
                if((m_num_to_req == -1) || m_num_reqd < (uint64_t)m_num_to_req) return false;
                else return true;
        }

        // -------------------------------------------------
        // Public members
        // -------------------------------------------------
        std::string m_id;
        std::string m_label;
        std::string m_host;
        std::string m_path;
        std::string m_query;
        std::string m_fragment;
        std::string m_userinfo;
        std::string m_hostname;
        uint16_t m_port;
        std::string m_where;
        nconn::scheme_t m_scheme;
        host_info_t m_host_info;
        uint32_t m_idx;

        t_stat_t m_stat_agg;
        bool m_multipath;

#if 0
        // TODO Cookies!!!
        void set_uri(const std::string &a_uri);
        const kv_list_map_t &get_uri_decoded_query(void);
#endif

private:

        // -------------------------------------------
        // Private methods
        // -------------------------------------------
        //DISALLOW_COPY_AND_ASSIGN(http_rx)

        int32_t special_effects_parse(void);
        int32_t add_option(const char *a_key, const char *a_val);
        int32_t parse_path(const char *a_path, path_substr_vector_t &ao_substr_vector, range_vector_t &ao_range_vector);
        int32_t path_exploder(std::string a_path_part,
                                                  const path_substr_vector_t &a_substr_vector, uint32_t a_substr_idx,
                                                  const range_vector_t &a_range_vector, uint32_t a_range_idx);

        // -------------------------------------------------
        // Private members
        // -------------------------------------------------
        bool m_is_resolved_flag;
        int64_t m_num_to_req;
        uint64_t m_num_reqd;

        // -------------------------------------------
        // Special effects...
        // Supports urls like:
        // http://127.0.0.1:80/[0X000001-0X00000A]/100K/[1-1000]/[1-100].gif;order=random
        // -------------------------------------------
        // Wildcards
        bool m_repeat_path_flag;
        path_vector_t m_path_vector;

        // Meta
        // order=random
        path_order_t m_path_order;
        uint32_t m_path_vector_last_idx;

        uint64_t m_label_hash;

        // Resp object
        http_resp *m_resp;

};

} //namespace ns_hlx {

#endif // #ifndef _HTTP_RX_H
