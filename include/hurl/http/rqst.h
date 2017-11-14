//: ----------------------------------------------------------------------------
//: Copyright (C) 2016 Verizon.  All Rights Reserved.
//: All Rights Reserved
//:
//: \file:    rqst.h
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
#ifndef _RQST_H
#define _RQST_H

//: ----------------------------------------------------------------------------
//: Includes
//: ----------------------------------------------------------------------------
#include "hurl/http/hmsg.h"
#include <string>
#include <list>

namespace ns_hurl {

//: ----------------------------------------------------------------------------
//: Types
//: ----------------------------------------------------------------------------
typedef std::map <std::string, str_list_t> query_map_t;

//: ----------------------------------------------------------------------------
//: \details: TODO
//: ----------------------------------------------------------------------------
class rqst : public hmsg
{
public:
        // -------------------------------------------------
        // Public methods
        // -------------------------------------------------
        rqst();
        ~rqst();

        void clear(void);
        void init(bool a_save);

        const std::string &get_url();
        const std::string &get_url_path();
        const std::string &get_url_query();
        const query_map_t &get_url_query_map();
        const std::string &get_url_fragment();
        const char *get_method_str();

        // Debug
        void show();

        // -------------------------------------------------
        // Public members
        // -------------------------------------------------

        // ---------------------------------------
        // raw http request offsets
        // ---------------------------------------
        cr_t m_p_url;
        int m_method;
        bool m_expect;

private:
        // -------------------------------------------------
        // Private methods
        // -------------------------------------------------
        int32_t parse_uri(void);
        int32_t parse_query(const std::string &a_query, query_map_t &ao_query_map);

        // -------------------------------------------------
        // Private members
        // -------------------------------------------------
        bool m_url_parsed;
        std::string m_url;
        std::string m_url_path;
        std::string m_url_query;
        query_map_t m_url_query_map;
        std::string m_url_fragment;
};

}

#endif
