//: ----------------------------------------------------------------------------
//: Copyright (C) 2015 Verizon.  All Rights Reserved.
//: All Rights Reserved
//:
//: \file:    wb_nbq.cc
//: \details: TODO
//: \author:  Reed P. Morrison
//: \date:    09/30/2015
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
#include "hlx/url_router.h"
#include "catch/catch.hpp"

//: ----------------------------------------------------------------------------
//: Macros
//: ----------------------------------------------------------------------------
# define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

//: ----------------------------------------------------------------------------
//: Types
//: ----------------------------------------------------------------------------
typedef struct router_ptr_pair_struct {
        const char *m_route;
        void *m_data;
} route_ptr_pair_t;

//: ----------------------------------------------------------------------------
//: Tests
//: ----------------------------------------------------------------------------
TEST_CASE( "url router test", "[url_router]" )
{
        SECTION("Build simple") {
                int32_t l_status;
                ns_hlx::url_router l_url_router;
                route_ptr_pair_t l_route_ptr_pairs[] =
                {
                        {"/monkeys/<monkey_name>/banana/<banana_number>", (void *)1},
                        {"/monkeez/<monkey_name>/banana/<banana_number>", (void *)33},
                        {"/circus/<circus_number>/hot_dog/<hot_dog_name>", (void *)45},
                        {"/cats_are/cool/dog/are/smelly", (void *)2},
                        {"/sweet/donuts/*", (void *)1337},
                        {"/bots*", (void *)1338},
                        {"/sweet/cakes/*/cupcakes", (void *)5337}
                };
                for(uint32_t i_route = 0; i_route < ARRAY_SIZE(l_route_ptr_pairs); ++i_route)
                {
                        //printf("ADDING ROUTE: i_route: %d route: %s\n", i_route, l_route_ptr_pairs[i_route].m_route);
                        //INFO("Adding route: " << l_route_ptr_pairs[i_route].m_route);
                        l_status = l_url_router.add_route(l_route_ptr_pairs[i_route].m_route, l_route_ptr_pairs[i_route].m_data);
                        REQUIRE((l_status == 0));
                }
        }
        SECTION("Fail on add duplicate") {
                int32_t l_status;
                ns_hlx::url_router l_url_router;
                l_status = l_url_router.add_route("/bonkers", (void *)8);
                REQUIRE((l_status == 0));
                l_status = l_url_router.add_route("/bonkers", (void *)10);
                REQUIRE((l_status != 0));
                l_status = l_url_router.add_route("/cats_are/cool/dog/are/smellfeet", (void *)100);
                REQUIRE((l_status == 0));
                l_status = l_url_router.add_route("/cats_are/cool/dog/are/smellfeet", (void *)101);
                REQUIRE((l_status != 0));
        }
        SECTION("Find existing routes") {
                int32_t l_status;
                ns_hlx::url_router l_url_router;
                route_ptr_pair_t l_route_ptr_pairs[] =
                {
                        {"/monkeys/<monkey_name>/banana/<banana_number>", (void *)1},
                        {"/monkeez/<monkey_name>/banana/<banana_number>", (void *)33},
                        {"/circus/<circus_number>/hot_dog/<hot_dog_name>", (void *)45},
                        {"/cats_are/cool/dog/are/smelly", (void *)2},
                        {"/sweet/donuts/*", (void *)1337},
                        {"/bots*", (void *)1338},
                        {"/sweet/cakes/*/cupcakes", (void *)5337},
                        {"/bloop_bleep", (void *)8337},
                        {"/bloop", (void *)9337}
                };
                for(uint32_t i_route = 0; i_route < ARRAY_SIZE(l_route_ptr_pairs); ++i_route)
                {
                        //printf("ADDING ROUTE: i_route: %d route: %s\n", i_route, l_route_ptr_pairs[i_route].m_route);
                        //INFO("Adding route: " << l_route_ptr_pairs[i_route].m_route);
                        l_status = l_url_router.add_route(l_route_ptr_pairs[i_route].m_route, l_route_ptr_pairs[i_route].m_data);
                        REQUIRE((l_status == 0));
                }
                route_ptr_pair_t l_find_ptr_pairs[] =
                {
                        {"/monkeys/bongo/banana/33", (void *)1},
                        {"/cats_are/cool/dog/are/smelly", (void *)2},
                        {"/sweet/donuts/pinky", (void *)1337},
                        {"/sweet/donuts/stinky", (void *)1337},
                        {"/bots", (void *)1338},
                        {"/botsy", (void *)1338},
                        {"/botsy/flopsy", (void *)1338},
                        {"/sweet/donuts/cavorting/anteaters", (void *)1337},
                        {"/sweet/donuts/trash/pandas/are/super/cool", (void *)1337},
                        {"/bloop_bleep", (void *)8337},
                        {"/bloop", (void *)9337}
                };
                for(uint32_t i_find = 0; i_find < ARRAY_SIZE(l_find_ptr_pairs); ++i_find)
                {
                        ns_hlx::url_pmap_t l_url_pmap;
                        const void *l_found = NULL;
                        std::string l_request_route;
                        l_request_route = l_find_ptr_pairs[i_find].m_route;
                        INFO("Finding route: " << l_request_route);
                        l_found = l_url_router.find_route(l_request_route, l_url_pmap);
                        REQUIRE((l_found == l_find_ptr_pairs[i_find].m_data));

                        // TODO Check for parameters
                        //if(l_found != NULL)
                        //{
                        //        for(ns_hlx::url_pmap_t::const_iterator i_param = l_url_pmap.begin();
                        //                        i_param != l_url_pmap.end();
                        //                        ++i_param)
                        //        {
                        //                printf(": Parameter: %s: %s\n", i_param->first.c_str(), i_param->second.c_str());
                        //        }
                        //}
                }
        }
        SECTION("Find non-existing routes") {
                int32_t l_status;
                ns_hlx::url_router l_url_router;
                route_ptr_pair_t l_route_ptr_pairs[] =
                {
                        {"/monkeys/<monkey_name>/banana/<banana_number>", (void *)1},
                        {"/monkeez/<monkey_name>/banana/<banana_number>", (void *)33},
                        {"/circus/<circus_number>/hot_dog/<hot_dog_name>", (void *)45},
                        {"/cats_are/cool/dog/are/smelly", (void *)2},
                        {"/sweet/donuts/*", (void *)1337},
                        {"/bots*", (void *)1338},
                        {"/sweet/cakes/*/cupcakes", (void *)5337}
                };
                for(uint32_t i_route = 0; i_route < ARRAY_SIZE(l_route_ptr_pairs); ++i_route)
                {
                        //printf("ADDING ROUTE: i_route: %d route: %s\n", i_route, l_route_ptr_pairs[i_route].m_route);
                        //INFO("Adding route: " << l_route_ptr_pairs[i_route].m_route);
                        l_status = l_url_router.add_route(l_route_ptr_pairs[i_route].m_route, l_route_ptr_pairs[i_route].m_data);
                        REQUIRE((l_status == 0));
                }
                route_ptr_pair_t l_find_ptr_pairs[] =
                {
                        {"/apes/are/neat", (void *)1},
                        {"/super/happy/dog/are/smelly", (void *)2},
                };
                for(uint32_t i_find = 0; i_find < ARRAY_SIZE(l_find_ptr_pairs); ++i_find)
                {
                        ns_hlx::url_pmap_t l_url_pmap;
                        const void *l_found = NULL;
                        std::string l_request_route;
                        l_request_route = l_find_ptr_pairs[i_find].m_route;
                        INFO("Finding route: " << l_request_route);
                        l_found = l_url_router.find_route(l_request_route, l_url_pmap);
                        REQUIRE((l_found == NULL));
                }
        }
}
