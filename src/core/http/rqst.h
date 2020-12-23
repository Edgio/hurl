//! ----------------------------------------------------------------------------
//! Copyright Verizon.
//!
//! \file:    TODO
//! \details: TODO
//!
//! Licensed under the terms of the Apache 2.0 open source license.
//! Please refer to the LICENSE file in the project root for the terms.
//! ----------------------------------------------------------------------------
#ifndef _RQST_H
#define _RQST_H
//! ----------------------------------------------------------------------------
//! includes
//! ----------------------------------------------------------------------------
#include "http/hmsg.h"
#include <string>
#include <list>
namespace ns_hurl {
//! ----------------------------------------------------------------------------
//! Types
//! ----------------------------------------------------------------------------
typedef std::map <std::string, str_list_t> query_map_t;
//! ----------------------------------------------------------------------------
//! \details: TODO
//! ----------------------------------------------------------------------------
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
        void show(bool a_color = false);
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
