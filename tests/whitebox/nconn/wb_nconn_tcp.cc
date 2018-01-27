//: ----------------------------------------------------------------------------
//: Copyright (C) 2016 Verizon.  All Rights Reserved.
//: All Rights Reserved
//:
//: \file:    wb_nconn_tcp.cc
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
#include "hurl/nconn/nconn_tcp.h"
#include "hurl/dns/nlookup.h"
#include "hurl/support/nbq.h"
#include <unistd.h>
//: ----------------------------------------------------------------------------
//: Tests
//: ----------------------------------------------------------------------------
TEST_CASE( "nconn tcp test", "[nconn_tcp]" )
{
        // ---------------------------------------------------------------------
        // TODO: Quarantining flaky test...
        // ---------------------------------------------------------------------
#if 0
        SECTION("Basic Connection Test")
        {
                ns_hurl::nconn_tcp l_c;
                ns_hurl::host_info l_h;
                int32_t l_s;
                l_s = ns_hurl::nlookup("google.com", 80, l_h);
                REQUIRE((l_s == 0));
                l_c.set_host_info(l_h);
                ns_hurl::nbq l_iq(16384);
                ns_hurl::nbq l_oq(16384);
                l_oq.write("GET\r\n\r\n", strlen("GET\r\n\r\n"));
                do {
                        l_s = l_c.nc_run_state_machine(NULL,ns_hurl::nconn::NC_MODE_WRITE, &l_iq, &l_oq);
                        REQUIRE((l_s != ns_hurl::nconn::NC_STATUS_ERROR));
                        usleep(100000);
                } while(l_s == ns_hurl::nconn::NC_STATUS_AGAIN);
                do {
                        l_s = l_c.nc_run_state_machine(NULL,ns_hurl::nconn::NC_MODE_READ, &l_iq, &l_oq);
                        REQUIRE((l_s != ns_hurl::nconn::NC_STATUS_ERROR));
                        usleep(100000);
                } while(l_s == ns_hurl::nconn::NC_STATUS_AGAIN);
                char l_buf[256];
                l_iq.read(l_buf, 256);
                REQUIRE((strncmp(l_buf, "HTTP", 4) == 0));
        }
#endif
}
