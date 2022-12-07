//! ----------------------------------------------------------------------------
//! Copyright Edgio Inc.
//!
//! \file:    TODO
//! \details: TODO
//!
//! Licensed under the terms of the Apache 2.0 open source license.
//! Please refer to the LICENSE file in the project root for the terms.
//! ----------------------------------------------------------------------------
//! ----------------------------------------------------------------------------
//! includes
//! ----------------------------------------------------------------------------
#include "status.h"
#include "support/ndebug.h"
#include "support/kv_map_list.h"
#include "catch/catch.hpp"
//! ----------------------------------------------------------------------------
//! append
//! ----------------------------------------------------------------------------
static inline void _append(ns_hurl::kv_map_list_t& a_map,
                           const std::string& a_key,
                           const std::string& a_val)
{
        ns_hurl::kv_map_list_t::iterator i_obj = a_map.find(a_key);
        if (i_obj != a_map.end())
        {
                i_obj->second.push_back(a_val);
                return;
        }
        ns_hurl::str_list_t l_list;
        l_list.push_back(a_val);
        a_map[a_key] = l_list;
        return;
}
//! ----------------------------------------------------------------------------
//! append
//! ----------------------------------------------------------------------------
static inline void _show(ns_hurl::kv_map_list_t& a_map)
{
        for(auto && i_k : a_map)
        {
                NDBG_OUTPUT("%s: ", i_k.first.c_str());
                for(auto && i_v : i_k.second)
                {
                        NDBG_OUTPUT("%s ", i_v.c_str());
                }
                NDBG_OUTPUT("\n");
        }
}
//! ----------------------------------------------------------------------------
//! Tests
//! ----------------------------------------------------------------------------
TEST_CASE( "kv_map test", "[kv_map]" ) {
        SECTION("basic test") {
                ns_hurl::kv_map_list_t l_kvml;
                NDBG_PRINT("APPEND\n");
                _append(l_kvml, "TEST", "TEST");
                NDBG_PRINT("APPEND\n");
                _append(l_kvml, "TEST-TEST", "TEST");
                REQUIRE((l_kvml.size() == 2));
        }
}
