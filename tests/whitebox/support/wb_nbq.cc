//: ----------------------------------------------------------------------------
//: Copyright (C) 2016 Verizon.  All Rights Reserved.
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
#include "hlx/nbq.h"
#include "catch/catch.hpp"

//: ----------------------------------------------------------------------------
//: Constants
//: ----------------------------------------------------------------------------
#define BLOCK_SIZE 256

//: ----------------------------------------------------------------------------
//: Test helpers
//: ----------------------------------------------------------------------------
char *create_buf(uint32_t a_size)
{
        char *l_buf = (char *)malloc(a_size);
        for(uint32_t i_c = 0; i_c < a_size; ++i_c)
        {
                l_buf[i_c] = (char)(0xFF&i_c);
        }
        return l_buf;
}

//: ----------------------------------------------------------------------------
//: Test helpers
//: ----------------------------------------------------------------------------
void nbq_write(ns_hlx::nbq &a_nbq, char *a_buf, uint32_t a_write_size, uint32_t a_write_per)
{
        uint64_t l_write_size = a_write_size;
        uint64_t l_left = l_write_size;
        uint64_t l_written = 0;
        while(l_left)
        {
                int32_t l_status = 0;
                uint32_t l_write_size = ((a_write_per) > l_left)?l_left:(a_write_per);
                l_status = a_nbq.write(a_buf, l_write_size);
                if(l_status > 0)
                {
                        l_written += l_status;
                        l_left -= l_status;
                }
        }
}

//: ----------------------------------------------------------------------------
//: Test helpers
//: ----------------------------------------------------------------------------
void nbq_read(ns_hlx::nbq &a_nbq, char *a_buf, uint32_t a_read_per)
{
        char *l_rd_buf = (char *)malloc(a_read_per);
        uint64_t l_read = 0;
        uint32_t l_per_read_size = (a_read_per);
        while(a_nbq.read_avail())
        {
                int32_t l_status = 0;
                //NDBG_PRINT(": Try read: %u\n", l_per_read_size);
                l_status = a_nbq.read(l_rd_buf, l_per_read_size);
                if(l_status > 0)
                {
                        l_read += l_status;
                        //ns_hlx::mem_display((const uint8_t *)l_rd_buf, l_status);
                }
        }
        free(l_rd_buf);
}

//: ----------------------------------------------------------------------------
//: Tests
//: ----------------------------------------------------------------------------
TEST_CASE( "nbq test", "[nbq]" ) {

        // Create router
        ns_hlx::nbq l_nbq(BLOCK_SIZE);
        char *l_buf = create_buf(BLOCK_SIZE);

        SECTION("Writing then Reading to new") {
                nbq_write(l_nbq, l_buf, 331, BLOCK_SIZE);
                REQUIRE(( l_nbq.read_avail() == 331 ));
                //l_nbq.b_display_all());
                nbq_read(l_nbq, l_buf, BLOCK_SIZE);
                REQUIRE(( l_nbq.read_avail() == 0 ));
        }
        SECTION("Reset Writing then Reading to new") {
                l_nbq.reset_read();
                REQUIRE(( l_nbq.read_avail() == 0 ));
                nbq_read(l_nbq, l_buf, BLOCK_SIZE);
                REQUIRE(( l_nbq.read_avail() == 0 ));
                l_nbq.reset_write();
                l_nbq.reset_read();
                REQUIRE(( l_nbq.read_avail() == 0 ));
                nbq_read(l_nbq, l_buf, BLOCK_SIZE);
                REQUIRE(( l_nbq.read_avail() == 0 ));
                nbq_write(l_nbq, l_buf, 331, BLOCK_SIZE);
                REQUIRE(( l_nbq.read_avail() == 331 ));
                nbq_read(l_nbq, l_buf, BLOCK_SIZE);
                REQUIRE(( l_nbq.read_avail() == 0 ));
        }
        SECTION("Reset Writing then Reading") {
                l_nbq.reset();
                REQUIRE(( l_nbq.read_avail() == 0 ));
                nbq_write(l_nbq, l_buf, 331, BLOCK_SIZE);
                REQUIRE(( l_nbq.read_avail() == 331 ));
                nbq_read(l_nbq, l_buf, BLOCK_SIZE);
                REQUIRE(( l_nbq.read_avail() == 0 ));
        }
        SECTION("Reset Writing/Writing then Reading") {
                l_nbq.reset();
                REQUIRE(( l_nbq.read_avail() == 0 ));
                nbq_write(l_nbq, l_buf, 331, BLOCK_SIZE);
                nbq_write(l_nbq, l_buf, 331, BLOCK_SIZE);
                REQUIRE(( l_nbq.read_avail() == 662 ));
                //l_nbq.b_display_all());
                nbq_read(l_nbq, l_buf, BLOCK_SIZE);
                REQUIRE(( l_nbq.read_avail() == 0 ));
        }
}
