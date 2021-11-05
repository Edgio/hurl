//! ----------------------------------------------------------------------------
//! Copyright Edgecast Inc.
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
#include "nconn/host_info.h"
#include "dns/ai_cache.h"
#include "support/time_util.h"
#include "catch/catch.hpp"
#include <unistd.h>
//! ----------------------------------------------------------------------------
//! Tests
//! ----------------------------------------------------------------------------
TEST_CASE( "ai cache test", "[ai_cache]" )
{
        SECTION("Open bad cache file")
        {
                std::string l_bad_ai_cache_file = "/dude/iam/cool/cache.json";
                ns_hurl::ai_cache *l_ai_cache = new ns_hurl::ai_cache(l_bad_ai_cache_file);
                std::string l_handle = "MY_HANDLE";
                ns_hurl::host_info *l_host_info = new ns_hurl::host_info();
                l_host_info->m_expires_s = ns_hurl::get_time_s() + 100;
                l_ai_cache->add(l_handle, l_host_info);
                l_host_info = l_ai_cache->lookup(l_handle);
                REQUIRE(( l_host_info != NULL ));
                delete l_ai_cache;
                l_ai_cache = new ns_hurl::ai_cache(l_bad_ai_cache_file);
                l_host_info = l_ai_cache->lookup(l_handle);
                REQUIRE(( l_host_info == NULL ));
                delete l_ai_cache;
        }
        SECTION("Open valid cache file")
        {
                std::string l_bad_ai_cache_file = "cache1.json";
                ns_hurl::ai_cache *l_ai_cache = new ns_hurl::ai_cache(l_bad_ai_cache_file);
                std::string l_handle_1 = "MY_HANDLE_1";
                ns_hurl::host_info *l_host_info_1 = new ns_hurl::host_info();
                l_host_info_1->m_sa_len = 13;
                l_host_info_1->m_expires_s = ns_hurl::get_time_s()+100;
                l_ai_cache->add(l_handle_1, l_host_info_1);
                std::string l_handle_2 = "MY_HANDLE_2";
                ns_hurl::host_info *l_host_info_2 = new ns_hurl::host_info();
                l_host_info_2->m_sa_len = 8;
                l_host_info_2->m_expires_s = ns_hurl::get_time_s()+100;
                l_ai_cache->add(l_handle_2, l_host_info_2);
                ns_hurl::host_info *l_host_info;
                l_host_info = l_ai_cache->lookup(l_handle_1);
                REQUIRE(( l_host_info != NULL ));
                REQUIRE(( l_host_info->m_sa_len == 13 ));
                l_host_info = l_ai_cache->lookup(l_handle_2);
                REQUIRE(( l_host_info != NULL ));
                REQUIRE(( l_host_info->m_sa_len == 8 ));
                delete l_ai_cache;
                l_ai_cache = new ns_hurl::ai_cache(l_bad_ai_cache_file);
                l_host_info = l_ai_cache->lookup(l_handle_1);
                REQUIRE(( l_host_info != NULL ));
                REQUIRE(( l_host_info->m_sa_len == 13 ));
                l_host_info = l_ai_cache->lookup(l_handle_2);
                REQUIRE(( l_host_info != NULL ));
                REQUIRE(( l_host_info->m_sa_len == 8 ));
                delete l_ai_cache;
        }
        SECTION("Verify ttl")
        {
                std::string l_bad_ai_cache_file = "cache2.json";
                ns_hurl::ai_cache *l_ai_cache = new ns_hurl::ai_cache(l_bad_ai_cache_file);
                std::string l_handle_1 = "MY_HANDLE_1";
                ns_hurl::host_info *l_host_info_1 = new ns_hurl::host_info();
                l_host_info_1->m_sa_len = 13;
                l_host_info_1->m_expires_s = ns_hurl::get_time_s()+10;
                l_ai_cache->add(l_handle_1, l_host_info_1);
                std::string l_handle_2 = "MY_HANDLE_2";
                ns_hurl::host_info *l_host_info_2 = new ns_hurl::host_info();
                l_host_info_2->m_sa_len = 8;
                l_host_info_2->m_expires_s = ns_hurl::get_time_s()+1;
                l_ai_cache->add(l_handle_2, l_host_info_2);
                ns_hurl::host_info *l_host_info;
                l_host_info = l_ai_cache->lookup(l_handle_1);
                REQUIRE(( l_host_info != NULL ));
                REQUIRE(( l_host_info->m_sa_len == 13 ));
                l_host_info = l_ai_cache->lookup(l_handle_2);
                REQUIRE(( l_host_info != NULL ));
                REQUIRE(( l_host_info->m_sa_len == 8 ));
                sleep(2);
                delete l_ai_cache;
                l_ai_cache = new ns_hurl::ai_cache(l_bad_ai_cache_file);
                l_host_info = l_ai_cache->lookup(l_handle_1);
                REQUIRE(( l_host_info != NULL ));
                REQUIRE(( l_host_info->m_sa_len == 13 ));
                l_host_info = l_ai_cache->lookup(l_handle_2);
                REQUIRE(( l_host_info == NULL ));
                delete l_ai_cache;
        }
}
