//: ----------------------------------------------------------------------------
//: Copyright (C) 2016 Verizon.  All Rights Reserved.
//: All Rights Reserved
//:
//: \file:    wb_nresolver.cc
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
#include "hurl/nconn/host_info.h"
#include "hurl/dns/nresolver.h"
#include "hurl/dns/ai_cache.h"
#include "hurl/support/time_util.h"
#include "catch/catch.hpp"
#include <unistd.h>
#include <sys/poll.h>
//: ----------------------------------------------------------------------------
//: Constants
//: ----------------------------------------------------------------------------
#define GOOD_DATA_VALUE_1 0xDEADBEEFDEADBEEFUL
#define GOOD_DATA_VALUE_2 0xDEADBEEFDEADBEEEUL
#define BAD_DATA_VALUE_1  0xDEADBEEFDEADF154UL
#define BAD_DATA_VALUE_2  0xDEADBEEFDEADF155UL
//: ----------------------------------------------------------------------------
//: Globals
//: ----------------------------------------------------------------------------
static uint32_t g_dns_reqs_qd = 0;
static uint32_t g_lkp_sucess = 0;
static uint32_t g_lkp_fail = 0;
static uint32_t g_lkp_err = 0;
//: ----------------------------------------------------------------------------
//: Test helpers
//: ----------------------------------------------------------------------------
int32_t test_resolved_cb(const ns_hurl::host_info *a_host_info, void *a_data)
{
        --g_dns_reqs_qd;
        //printf("DEBUG: test_resolved_cb: a_host_info: %p a_data: %p g_dns_reqs_qd: %d\n", a_host_info, a_data, g_dns_reqs_qd);
        if(a_host_info &&
           ((a_data == (void *)(GOOD_DATA_VALUE_1)) ||
            (a_data == (void *)(GOOD_DATA_VALUE_2))))
        {
                ++g_lkp_sucess;
        }
        else if(!a_host_info &&
               ((a_data == (void *)(BAD_DATA_VALUE_1)) ||
               (a_data == (void *)(BAD_DATA_VALUE_2))))

        {
                ++g_lkp_fail;
        }
        else
        {
                ++g_lkp_err;
        }
        return 0;
}
//: ----------------------------------------------------------------------------
//: Get cache key
//: ----------------------------------------------------------------------------
TEST_CASE( "get cache key", "[get_cache_key]" )
{
        SECTION("Verify get cache key")
        {
                std::string l_host = "google.com";
                uint16_t l_port = 7868;
                std::string l_label = ns_hurl::get_cache_key(l_host, l_port);
                REQUIRE((l_label == "google.com:7868"));
        }
}
//: ----------------------------------------------------------------------------
//: nresolver
//: ----------------------------------------------------------------------------
TEST_CASE( "nresolver test", "[nresolver]" )
{
        SECTION("Validate No cache")
        {
                ns_hurl::nresolver *l_nresolver = new ns_hurl::nresolver();
                l_nresolver->init(false, "");
                ns_hurl::host_info l_host_info;
                int32_t l_status;
                l_status = l_nresolver->lookup_tryfast("google.com", 80, l_host_info);
                REQUIRE(( l_status == -1 ));
                l_status = l_nresolver->lookup_sync("google.com", 80, l_host_info);
                REQUIRE(( l_status == 0 ));
                l_status = l_nresolver->lookup_tryfast("google.com", 80, l_host_info);
                REQUIRE(( l_status == -1 ));
                bool l_use_cache = l_nresolver->get_use_cache();
                ns_hurl::ai_cache *l_ai_cache = l_nresolver->get_ai_cache();
                REQUIRE(( l_use_cache == false ));
                REQUIRE(( l_ai_cache == NULL ));
                delete l_nresolver;
        }
        SECTION("Validate cache")
        {
                ns_hurl::nresolver *l_nresolver = new ns_hurl::nresolver();
                l_nresolver->init(true);
                ns_hurl::host_info l_host_info;
                int32_t l_status;
                l_status = l_nresolver->lookup_sync("google.com", 80, l_host_info);
                REQUIRE(( l_status == 0 ));
                l_status = l_nresolver->lookup_sync("yahoo.com", 80, l_host_info);
                REQUIRE(( l_status == 0 ));
                l_status = l_nresolver->lookup_tryfast("yahoo.com", 80, l_host_info);
                REQUIRE(( l_status == 0 ));
                l_status = l_nresolver->lookup_tryfast("google.com", 80, l_host_info);
                REQUIRE(( l_status == 0 ));
                bool l_use_cache = l_nresolver->get_use_cache();
                ns_hurl::ai_cache *l_ai_cache = l_nresolver->get_ai_cache();
                REQUIRE(( l_use_cache == true ));
                REQUIRE(( l_ai_cache != NULL ));
                delete l_nresolver;
        }
#ifdef ASYNC_DNS_SUPPORT
        // ---------------------------------------------------------------------
        // TODO: Quarantining flaky test...
        // ---------------------------------------------------------------------
#if 0
        SECTION("Validate async")
        {
                ns_hurl::nresolver *l_nresolver = new ns_hurl::nresolver();
                l_nresolver->add_resolver_host("8.8.8.8");
                l_nresolver->set_retries(1);
                l_nresolver->set_timeout_s(1);

                // Set up poller
                // TODO

                ns_hurl::nresolver::adns_ctx *l_adns_ctx = NULL;
                l_adns_ctx = l_nresolver->get_new_adns_ctx(NULL, test_resolved_cb);
                REQUIRE((l_adns_ctx != NULL));

                uint64_t l_active;
                ns_hurl::nresolver::lookup_job_q_t l_lookup_job_q;
                ns_hurl::nresolver::lookup_job_pq_t l_lookup_job_pq;
                int32_t l_status = 0;
                void *l_job_handle = NULL;

                // Set globals
                g_lkp_sucess = 0;
                g_lkp_fail = 0;
                g_lkp_err = 0;

                // Good
                ++g_dns_reqs_qd;
                l_status = l_nresolver->lookup_async(l_adns_ctx,
                                                     "google.com", 80,
                                                     (void *)(GOOD_DATA_VALUE_1),
                                                     l_active, &l_job_handle);
                REQUIRE((l_status == 0));

                // Good
                ++g_dns_reqs_qd;
                l_status = l_nresolver->lookup_async(l_adns_ctx,
                                                     "yahoo.com", 80,
                                                     (void *)(GOOD_DATA_VALUE_2),
                                                     l_active, &l_job_handle);
                REQUIRE((l_status == 0));

                // Bad
                ++g_dns_reqs_qd;
                l_status = l_nresolver->lookup_async(l_adns_ctx,
                                                     "arfarhfarfbloop", 9874,
                                                     test_resolved_cb,
                                                     (void *)(BAD_DATA_VALUE_1),
                                                     l_active, l_lookup_job_q, l_lookup_job_pq, &l_job_handle);
                REQUIRE((l_status == 0));

                // Bad
                ++g_dns_reqs_qd;
                l_status = l_nresolver->lookup_async(l_adns_ctx,
                                                     "wonbaombaboiuiuiuoad.com", 80,
                                                     (void *)(BAD_DATA_VALUE_2),
                                                     l_active, &l_job_handle);
                REQUIRE((l_status == 0));


                uint32_t l_timeout_s = 5;
                uint64_t l_start_s = ns_hurl::get_time_s();
                while(g_dns_reqs_qd && ((ns_hurl::get_time_s()) < (l_start_s + l_timeout_s)))
                {
                        int l_count;
                        l_count = poll(&l_pfd, 1, 1*1000);
                        (void) l_count;
                        l_status = l_nresolver->handle_io(l_ctx);
                        std::string l_unused;
                        l_status = l_nresolver->lookup_async(l_ctx,
                                                             l_unused, 0,
                                                             test_resolved_cb,
                                                             NULL,
                                                             l_active, l_lookup_job_q, l_lookup_job_pq, &l_job_handle);
                        REQUIRE((l_status == 0));
                        if(g_dns_reqs_qd == 0)
                        {
                                break;
                        }
                }
                uint64_t l_end_s = ns_hurl::get_time_s();
                INFO("l_end_s:       " << l_end_s)
                INFO("l_start_s:     " << l_start_s)
                INFO("l_timeout_s:   " << l_timeout_s)
                INFO("g_dns_reqs_qd: " << g_dns_reqs_qd)

                //REQUIRE((l_end_s >= (l_start_s + l_timeout_s)));
                if((l_end_s >= (l_start_s + l_timeout_s)))
                {
                        // Handle timeouts
                }
                //printf("DEBUG: outtie\n");

                INFO("g_lkp_sucess:  " << g_lkp_sucess)
                REQUIRE((g_lkp_sucess == 2));
                INFO("g_lkp_fail:    " << g_lkp_fail)
                REQUIRE((g_lkp_fail == 2));
                INFO("g_lkp_err:     " << g_lkp_err)
                REQUIRE((g_lkp_err == 0));

                l_nresolver->destroy_async(l_adns_ctx);
                delete l_nresolver;
        }
#endif
        // ---------------------------------------------------------------------
        // TODO: Quarantining flaky test...
        // ---------------------------------------------------------------------
#if 0
        SECTION("Validate async -bad resolver")
        {
                ns_hurl::nresolver *l_nresolver = new ns_hurl::nresolver();

                // ---------------------------------------------------
                // Set resolver to something far and slow hopefully?
                // grabbed hong kong one from:
                // http://public-dns.tk/nameserver/hk.html
                // ---------------------------------------------------
                l_nresolver->add_resolver_host("210.0.128.242");
                l_nresolver->set_port(6969);
                l_nresolver->set_retries(1);
                l_nresolver->set_timeout_s(1);

                // Set up poller
                // TODO

                ns_hurl::nresolver::adns_ctx *l_adns_ctx = NULL;
                l_adns_ctx = l_nresolver->get_new_adns_ctx(NULL, test_resolved_cb);
                REQUIRE((l_adns_ctx != NULL));

                uint64_t l_active;
                int32_t l_status = 0;
                void *l_job_handle = NULL;

                // Set globals
                g_lkp_sucess = 0;
                g_lkp_fail = 0;
                g_lkp_err = 0;

                // Good
                ++g_dns_reqs_qd;
                l_status = l_nresolver->lookup_async(l_adns_ctx,
                                                     "google.com", 80,
                                                     (void *)(BAD_DATA_VALUE_1),
                                                     l_active, &l_job_handle);
                REQUIRE((l_status == 0));

                // Good
                ++g_dns_reqs_qd;
                l_status = l_nresolver->lookup_async(l_adns_ctx,
                                                     "yahoo.com", 80,
                                                     (void *)(BAD_DATA_VALUE_2),
                                                     l_active, &l_job_handle);
                REQUIRE((l_status == 0));

                // Bad
                ++g_dns_reqs_qd;
                l_status = l_nresolver->lookup_async(l_adns_ctx,
                                                     "arfarhfarfbloop", 9874,
                                                     (void *)(BAD_DATA_VALUE_1),
                                                     l_active, &l_job_handle);
                REQUIRE((l_status == 0));

                // Bad
                ++g_dns_reqs_qd;
                l_status = l_nresolver->lookup_async(l_adns_ctx,
                                                     "wonbaombaboiuiuiuoad.com", 80,
                                                     (void *)(BAD_DATA_VALUE_2),
                                                     l_active, &l_job_handle);
                REQUIRE((l_status == 0));


                uint32_t l_timeout_s = 5;
                uint64_t l_start_s = ns_hurl::get_time_s();
                while(g_dns_reqs_qd && ((ns_hurl::get_time_s()) < (l_start_s + l_timeout_s)))
                {
                        int l_count;
                        l_count = poll(&l_pfd, 1, 1*1000);
                        (void) l_count;
                        //printf("DEBUG: poll: %d\n", l_count);
                        l_status = l_nresolver->handle_io(l_ctx);
                        std::string l_unused;
                        l_status = l_nresolver->lookup_async(l_adns_ctx,
                                                             l_unused, 0,
                                                             NULL,
                                                             l_active, &l_job_handle);
                        REQUIRE((l_status == 0));
                        if(g_dns_reqs_qd == 0)
                        {
                                break;
                        }
                }


                uint64_t l_end_s = ns_hurl::get_time_s();
                INFO("l_end_s:       " << l_end_s)
                INFO("l_start_s:     " << l_start_s)
                INFO("l_timeout_s:   " << l_timeout_s)
                INFO("g_dns_reqs_qd: " << g_dns_reqs_qd)

                //REQUIRE((l_end_s >= (l_start_s + l_timeout_s)));
                if((l_end_s >= (l_start_s + l_timeout_s)))
                {
                        // Handle timeouts
                }
                //printf("DEBUG: outtie\n");

                INFO("g_lkp_sucess:  " << g_lkp_sucess)
                REQUIRE((g_lkp_sucess == 0));
                INFO("g_lkp_fail:    " << g_lkp_fail)
                REQUIRE((g_lkp_fail == 4));
                INFO("g_lkp_err:     " << g_lkp_err)
                REQUIRE((g_lkp_err == 0));

                l_nresolver->destroy_async(l_adns_ctx);
                delete l_nresolver;
        }
#endif

#endif
}
