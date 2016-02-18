//: ----------------------------------------------------------------------------
//: Copyright (C) 2015 Verizon.  All Rights Reserved.
//: All Rights Reserved
//:
//: \file:    wb_nconn_pool.cc
//: \details: TODO
//: \author:  Reed P. Morrison
//: \date:    02/16/2016
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
#include "catch/catch.hpp"
#include "nconn_pool.h"

//: ----------------------------------------------------------------------------
//: Tests
//: ----------------------------------------------------------------------------
TEST_CASE( "nconn pool test", "[nconn_pool]" )
{
        SECTION("Basic Connection Pool Test")
        {
                INFO("Init");
                ns_hlx::nconn_pool l_p(16);
                REQUIRE((l_p.num_free() == 16));

                INFO("Get 3 connections");
                ns_hlx::nconn *l_c1 = NULL;
                l_c1 = l_p.get("MONKEY", ns_hlx::scheme_t::SCHEME_TCP);
                ns_hlx::nconn *l_c2 = NULL;
                l_c2 = l_p.get("BANANA", ns_hlx::scheme_t::SCHEME_TCP);
                ns_hlx::nconn *l_c3 = NULL;
                l_c3 = l_p.get("GORILLA", ns_hlx::scheme_t::SCHEME_TCP);
                ns_hlx::nconn *l_c4 = NULL;
                l_c4 = l_p.get("KOALA", ns_hlx::scheme_t::SCHEME_TCP);
                REQUIRE((l_c1 != NULL));
                REQUIRE((l_c2 != NULL));
                REQUIRE((l_c3 != NULL));
                REQUIRE((l_c4 != NULL));
                REQUIRE((l_p.num_in_use() == 4));
                REQUIRE((l_p.num_idle() == 0));
                REQUIRE((l_p.num_free() == 12));

                INFO("Release 2");
                int32_t l_s;
                l_s = l_p.release(l_c1);
                REQUIRE((l_s == 0));
                l_s = l_p.release(l_c2);
                REQUIRE((l_s == 0));
                REQUIRE((l_p.num_in_use() == 2));
                REQUIRE((l_p.num_idle() == 0));
                REQUIRE((l_p.num_free() == 14));

                INFO("Add idle");
                l_s = l_p.add_idle(l_c3);
                REQUIRE((l_s == 0));
                REQUIRE((l_p.num_in_use() == 2));
                REQUIRE((l_p.num_idle() == 1));
                REQUIRE((l_p.num_free() == 14));

                INFO("Get idle");
                ns_hlx::nconn *l_ci = NULL;
                l_ci = l_p.get_idle("KOALA");
                REQUIRE((l_ci == NULL));
                l_ci = l_p.get_idle("GORILLA");
                REQUIRE((l_ci != NULL));
                REQUIRE((l_ci->get_label() == "GORILLA"));
                REQUIRE((l_p.num_in_use() == 2));
                REQUIRE((l_p.num_idle() == 0));
                REQUIRE((l_p.num_free() == 14));

                INFO("Get all free")
                for(uint32_t i = 0; i < 14; ++i)
                {
                        ns_hlx::nconn *l_ct = NULL;
                        l_ct = l_p.get("BLOOP", ns_hlx::scheme_t::SCHEME_TCP);
                        REQUIRE((l_ct != NULL));
                }
                REQUIRE((l_p.num_in_use() == 16));
                REQUIRE((l_p.num_idle() == 0));
                REQUIRE((l_p.num_free() == 0));
                ns_hlx::nconn *l_ct = NULL;
                l_ct = l_p.get("BLOOP", ns_hlx::scheme_t::SCHEME_TCP);
                REQUIRE((l_ct == NULL));
                l_ct = l_p.get("BLOOP", ns_hlx::scheme_t::SCHEME_TCP);
                REQUIRE((l_ct == NULL));
        }
}
