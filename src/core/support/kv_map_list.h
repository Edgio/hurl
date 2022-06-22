//! ----------------------------------------------------------------------------
//! Copyright Edgio Inc.
//!
//! \file:    TODO
//! \details: TODO
//!
//! Licensed under the terms of the Apache 2.0 open source license.
//! Please refer to the LICENSE file in the project root for the terms.
//! ----------------------------------------------------------------------------
#ifndef _KV_MAP_LIST_H
#define _KV_MAP_LIST_H
//! ----------------------------------------------------------------------------
//! includes
//! ----------------------------------------------------------------------------
#include <string>
#include <list>
#include <map>
// for strcasecmp
#include <strings.h>
namespace ns_hurl {
//! ----------------------------------------------------------------------------
//! Types
//! ----------------------------------------------------------------------------
struct case_i_comp
{
        bool operator() (const std::string& lhs, const std::string& rhs) const
        {
                return strcasecmp(lhs.c_str(), rhs.c_str()) < 0;
        }
};
typedef std::list <std::string> str_list_t;
typedef std::map <std::string, str_list_t, case_i_comp> kv_map_list_t;
}
#endif
