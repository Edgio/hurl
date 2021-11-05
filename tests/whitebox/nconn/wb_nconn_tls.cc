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
#include "catch/catch.hpp"
#include "nconn/nconn_tls.h"
#include "support/tls_util.h"
#include "dns/nlookup.h"
#include "support/nbq.h"
#include <unistd.h>
//! ----------------------------------------------------------------------------
//! Tests
//! ----------------------------------------------------------------------------
TEST_CASE( "nconn tls test", "[nconn_tls]" )
{
        // TODO Test with no init or ctx set
        //      test for graceful failure...
        // ---------------------------------------------------------------------
        // TODO: Quarantining flaky test...
        // ---------------------------------------------------------------------
#if 0
        SECTION("Basic Connection Test")
        {
                // TLS Init...
                ns_hurl::tls_init();
                SSL_CTX *l_ctx = ns_hurl::tls_init_ctx("",0,"","",false,"","");
                REQUIRE((l_ctx != NULL));
                // TLS Setup...
                int32_t l_s;
                ns_hurl::nconn_tls l_c;
                l_s = l_c.set_opt(ns_hurl::nconn_tls::OPT_TLS_CTX, l_ctx, sizeof(l_ctx));
                REQUIRE((l_s == ns_hurl::nconn::NC_STATUS_OK));
                ns_hurl::host_info l_h;
                l_s = ns_hurl::nlookup("google.com", 443, l_h);
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
