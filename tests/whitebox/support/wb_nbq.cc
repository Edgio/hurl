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
#include "hurl/status.h"
#include "hurl/support/nbq.h"
#include "hurl/support/trace.h"
#include "ndebug.h"
#include "catch/catch.hpp"
//: ----------------------------------------------------------------------------
//: Constants
//: ----------------------------------------------------------------------------
#define BLOCK_SIZE 256
//: ----------------------------------------------------------------------------
//: Test helpers
//: ----------------------------------------------------------------------------
#define TO_HEX(i) (i <= 9 ? '0' + i : 'A' - 10 + i)
char *create_uniform_buf(uint32_t a_size)
{
        char *l_buf = (char *)malloc(a_size);
        for(uint32_t i_c = 0; i_c < a_size; ++i_c)
        {
                l_buf[i_c] = TO_HEX(i_c % 16);
        }
        return l_buf;
}
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
void nbq_write(ns_hurl::nbq &a_nbq, char *a_buf, uint32_t a_write_size, uint32_t a_write_per)
{
        uint64_t l_write_size = a_write_size;
        uint64_t l_left = l_write_size;
        uint64_t l_written = 0;
        while(l_left)
        {
                int32_t l_s = 0;
                uint32_t l_write_size = ((a_write_per) > l_left)?l_left:(a_write_per);
                l_s = a_nbq.write(a_buf+l_written, l_write_size);
                if(l_s > 0)
                {
                        l_written += l_s;
                        l_left -= l_s;
                }
        }
}
//: ----------------------------------------------------------------------------
//: Test helpers
//: ----------------------------------------------------------------------------
void nbq_read(ns_hurl::nbq &a_nbq, char *a_buf, uint32_t a_read_per)
{
        uint64_t l_read = 0;
        uint32_t l_per_read_size = (a_read_per);
        while(a_nbq.read_avail())
        {
                int32_t l_s = 0;
                //NDBG_PRINT(": Try read: %u\n", l_per_read_size);
                l_s = a_nbq.read((a_buf+l_read), l_per_read_size);
                if(l_s > 0)
                {
                        l_read += l_s;
                        //ns_hurl::mem_display((const uint8_t *)(a_buf+l_read), l_s, true);
                }
        }
}
//: ----------------------------------------------------------------------------
//: Verify contents of nbq
//: ----------------------------------------------------------------------------
int32_t verify_contents(ns_hurl::nbq &a_nbq, uint64_t a_len, uint16_t a_offset)
{
        uint64_t l_read = 0;
        //NDBG_PRINT("a_nbq.read_avail(): %lu\n", a_nbq.read_avail());
        while(a_nbq.read_avail() &&
              (l_read < a_len))
        {
                char l_cmp = TO_HEX((l_read + a_offset) % 16);
                int32_t l_s = 0;
                char l_char;
                l_s = a_nbq.read(&l_char, 1);
                //NDBG_PRINT("l_s: %d\n", l_s);
                if(l_s != 1)
                {
                        //NDBG_PRINT("error\n");
                        return HURL_STATUS_ERROR;
                }
                //NDBG_PRINT("l_cmp: %c l_char: %c -l_read: %lu\n", l_cmp, l_char, l_read);
                if(l_cmp != l_char)
                {
                        //NDBG_PRINT("error l_cmp: %c l_char: %c\n", l_cmp, l_char);
                        return HURL_STATUS_ERROR;
                }
                ++l_read;
        }
        if(l_read != a_len)
        {
                //NDBG_PRINT("error l_read = %lu a_len = %lu\n", l_read, a_len);
                return HURL_STATUS_ERROR;
        }
        return HURL_STATUS_OK;
}
//: ----------------------------------------------------------------------------
//: Verify contents of buf
//: ----------------------------------------------------------------------------
int32_t verify_contents(char *l_buf, uint64_t a_len, uint16_t a_offset)
{
        uint64_t l_read = 0;
        //NDBG_PRINT("a_nbq.read_avail(): %lu\n", a_nbq.read_avail());
        while(l_read < a_len)
        {
                char l_cmp = TO_HEX((l_read + a_offset) % 16);
                int32_t l_s = 0;
                char l_char;
                l_char = l_buf[l_read];
                //NDBG_PRINT("l_cmp: %c l_char: %c -l_read: %lu\n", l_cmp, l_char, l_read);
                if(l_cmp != l_char)
                {
                        //NDBG_PRINT("error l_cmp: %c l_char: %c\n", l_cmp, l_char);
                        return HURL_STATUS_ERROR;
                }
                ++l_read;
        }
        if(l_read != a_len)
        {
                //NDBG_PRINT("error l_read = %lu a_len = %lu\n", l_read, a_len);
                return HURL_STATUS_ERROR;
        }
        return HURL_STATUS_OK;
}
//: ----------------------------------------------------------------------------
//: Tests
//: ----------------------------------------------------------------------------
TEST_CASE( "nbq test", "[nbq]" ) {
        ns_hurl::trc_log_level_set(ns_hurl::TRC_LOG_LEVEL_NONE);
        SECTION("writing then reading to new") {
                ns_hurl::nbq l_nbq(BLOCK_SIZE);
                char *l_buf = create_buf(888);
                nbq_write(l_nbq, l_buf, 888, BLOCK_SIZE);
                REQUIRE(( l_nbq.read_avail() == 888 ));
                //l_nbq.b_display_all());
                char *l_rbuf = (char *)malloc(888);
                nbq_read(l_nbq, l_rbuf, BLOCK_SIZE);
                REQUIRE(( l_nbq.read_avail() == 0 ));
                if(l_buf)
                {
                        free(l_buf);
                        l_buf = NULL;
                }
                if(l_rbuf)
                {
                        free(l_rbuf);
                        l_rbuf = NULL;
                }
        }
        SECTION("reset writing then reading to new") {
                ns_hurl::nbq l_nbq(BLOCK_SIZE);
                char *l_buf = create_buf(888);
                l_nbq.reset_read();
                char *l_rbuf = (char *)malloc(888);
                REQUIRE(( l_nbq.read_avail() == 0 ));
                nbq_read(l_nbq, l_rbuf, BLOCK_SIZE);
                REQUIRE(( l_nbq.read_avail() == 0 ));
                l_nbq.reset_write();
                l_nbq.reset_read();
                REQUIRE(( l_nbq.read_avail() == 0 ));
                nbq_read(l_nbq, l_rbuf, BLOCK_SIZE);
                REQUIRE(( l_nbq.read_avail() == 0 ));
                nbq_write(l_nbq, l_buf, 888, BLOCK_SIZE);
                REQUIRE(( l_nbq.read_avail() == 888 ));
                nbq_read(l_nbq, l_rbuf, BLOCK_SIZE);
                REQUIRE(( l_nbq.read_avail() == 0 ));
                if(l_buf)
                {
                        free(l_buf);
                        l_buf = NULL;
                }
                if(l_rbuf)
                {
                        free(l_rbuf);
                        l_rbuf = NULL;
                }
        }
        SECTION("reset writing then reading") {
                ns_hurl::nbq l_nbq(BLOCK_SIZE);
                char *l_buf = create_buf(888);
                char *l_rbuf = (char *)malloc(888);
                l_nbq.reset();
                REQUIRE(( l_nbq.read_avail() == 0 ));
                nbq_write(l_nbq, l_buf, 888, BLOCK_SIZE);
                REQUIRE(( l_nbq.read_avail() == 888 ));
                nbq_read(l_nbq, l_rbuf, BLOCK_SIZE);
                REQUIRE(( l_nbq.read_avail() == 0 ));
                if(l_buf)
                {
                        free(l_buf);
                        l_buf = NULL;
                }
                if(l_rbuf)
                {
                        free(l_rbuf);
                        l_rbuf = NULL;
                }
        }
        SECTION("Reset Writing/Writing then Reading") {
                ns_hurl::nbq l_nbq(BLOCK_SIZE);
                char *l_buf = create_buf(888);
                char *l_rbuf = (char *)malloc(888);
                l_nbq.reset();
                REQUIRE(( l_nbq.read_avail() == 0 ));
                nbq_write(l_nbq, l_buf, 888, BLOCK_SIZE);
                nbq_write(l_nbq, l_buf, 888, BLOCK_SIZE);
                REQUIRE(( l_nbq.read_avail() == 1776 ));
                //l_nbq.b_display_all());
                nbq_read(l_nbq, l_rbuf, BLOCK_SIZE);
                REQUIRE(( l_nbq.read_avail() == 0 ));
                if(l_buf)
                {
                        free(l_buf);
                        l_buf = NULL;
                }
                if(l_rbuf)
                {
                        free(l_rbuf);
                        l_rbuf = NULL;
                }
        }
        SECTION("split") {
                ns_hurl::nbq l_nbq(BLOCK_SIZE);
                char *l_uni_buf = create_uniform_buf(703);
                char *l_rbuf = (char *)malloc(888);
                l_nbq.reset();
                nbq_write(l_nbq, l_uni_buf, 703, 133);
                REQUIRE(( l_nbq.read_avail() == 703 ));
                int32_t l_s;
                l_s = verify_contents(l_nbq, 703, 0);
                REQUIRE(( l_s == HURL_STATUS_OK ));
                l_nbq.reset_read();
                ns_hurl::nbq *l_nbq_tail;
                // split at > written offset -return nothing
                l_s = l_nbq.split(&l_nbq_tail, 703);
                REQUIRE(( l_s == HURL_STATUS_ERROR ));
                REQUIRE(( l_nbq_tail == NULL ));
                REQUIRE(( l_nbq.read_avail() == 703 ));
                // split at 0 offset -return nothing
                l_s = l_nbq.split(&l_nbq_tail, 0);
                REQUIRE(( l_s == HURL_STATUS_OK ));
                REQUIRE(( l_nbq_tail == NULL ));
                REQUIRE(( l_nbq.read_avail() == 703 ));
                l_s = l_nbq.split(&l_nbq_tail, 400);
                REQUIRE(( l_s == HURL_STATUS_OK ));
                REQUIRE(( l_nbq_tail != NULL ));
                REQUIRE(( l_nbq_tail->read_avail() == 303 ));
                l_nbq.reset_read();
                l_s = verify_contents(l_nbq, 400, 0);
                REQUIRE(( l_s == HURL_STATUS_OK ));
                l_nbq_tail->reset_read();
                l_s = verify_contents(*l_nbq_tail, 303, 0);
                REQUIRE(( l_s == HURL_STATUS_OK ));
                if(l_nbq_tail)
                {
                        delete l_nbq_tail;
                        l_nbq_tail = NULL;
                }
                if(l_uni_buf)
                {
                        free(l_uni_buf);
                        l_uni_buf = NULL;
                }
                if(l_rbuf)
                {
                        free(l_rbuf);
                        l_rbuf = NULL;
                }
        }
        SECTION("join") {
                ns_hurl::nbq *l_nbq = new ns_hurl::nbq(BLOCK_SIZE);
                char *l_uni_buf = create_uniform_buf(703);
                l_nbq->reset();
                nbq_write(*l_nbq, l_uni_buf, 703, 155);
                REQUIRE(( l_nbq->read_avail() == 703 ));
                ns_hurl::nbq *l_nbq_tail = new ns_hurl::nbq(BLOCK_SIZE);
                nbq_write(*l_nbq_tail, l_uni_buf, 400, 200);
                int32_t l_s;
                l_s = l_nbq->join_ref(*l_nbq_tail);
                REQUIRE(( l_s == HURL_STATUS_OK ));
                REQUIRE(( l_nbq->read_avail() == 1103 ));
                l_nbq->reset_read();
                // verify head
                l_s = verify_contents(*l_nbq, 703, 0);
                REQUIRE(( l_s == HURL_STATUS_OK ));
                // verify tail
                l_s = verify_contents(*l_nbq, 400, 0);
                REQUIRE(( l_s == HURL_STATUS_OK ));
                REQUIRE(( l_nbq->read_avail() == 0 ));
                if(l_nbq)
                {
                        delete l_nbq;
                        l_nbq = NULL;
                }
                l_nbq_tail->reset_read();
                l_s = verify_contents(*l_nbq_tail, 400, 0);
                REQUIRE(( l_s == HURL_STATUS_OK ));
                if(l_uni_buf)
                {
                        free(l_uni_buf);
                        l_uni_buf = NULL;
                }
                if(l_nbq_tail)
                {
                        delete l_nbq_tail;
                        l_nbq_tail = NULL;
                }
        }
        SECTION("split and join") {
                ns_hurl::nbq *l_nbq = new ns_hurl::nbq(BLOCK_SIZE);
                char *l_uni_buf = create_uniform_buf(703);
                nbq_write(*l_nbq, l_uni_buf, 703, 133);
                REQUIRE(( l_nbq->read_avail() == 703 ));
                int32_t l_s;
                l_s = verify_contents(*l_nbq, 703, 0);
                REQUIRE(( l_s == HURL_STATUS_OK ));
                l_nbq->reset_read();
                ns_hurl::nbq *l_nbq_tail;
                // split at > written offset -return nothing
                l_s = l_nbq->split(&l_nbq_tail, 703);
                REQUIRE(( l_s == HURL_STATUS_ERROR ));
                REQUIRE(( l_nbq_tail == NULL ));
                REQUIRE(( l_nbq->read_avail() == 703 ));
                // split at 0 offset -return nothing
                l_s = l_nbq->split(&l_nbq_tail, 0);
                REQUIRE(( l_s == HURL_STATUS_OK ));
                REQUIRE(( l_nbq_tail == NULL ));
                REQUIRE(( l_nbq->read_avail() == 703 ));
                l_s = l_nbq->split(&l_nbq_tail, 400);
                REQUIRE(( l_s == HURL_STATUS_OK ));
                REQUIRE(( l_nbq_tail != NULL ));
                REQUIRE(( l_nbq_tail->read_avail() == 303 ));
                l_nbq->reset_read();
                l_s = verify_contents(*l_nbq, 400, 0);
                REQUIRE(( l_s == HURL_STATUS_OK ));
                if(l_nbq)
                {
                        delete l_nbq;
                        l_nbq = NULL;
                }
                l_nbq_tail->reset_read();
                l_s = verify_contents(*l_nbq_tail, 303, 0);
                REQUIRE(( l_s == HURL_STATUS_OK ));
                // join to new
                ns_hurl::nbq *l_nbq_1 = new ns_hurl::nbq(BLOCK_SIZE);
                nbq_write(*l_nbq_1, l_uni_buf, 300, 155);
                REQUIRE(( l_nbq_1->read_avail() == 300 ));
                l_s = l_nbq_1->join_ref(*l_nbq_tail);
                REQUIRE(( l_s == HURL_STATUS_OK ));
                REQUIRE(( l_nbq_1->read_avail() == 603 ));
                // verify head
                l_s = verify_contents(*l_nbq_1, 300, 0);
                REQUIRE(( l_s == HURL_STATUS_OK ));
                // verify tail
                l_s = verify_contents(*l_nbq_1, 303, 0);
                REQUIRE(( l_s == HURL_STATUS_OK ));
                if(l_nbq_1)
                {
                        delete l_nbq_1;
                        l_nbq_1 = NULL;
                }
                if(l_nbq_tail)
                {
                        delete l_nbq_tail;
                        l_nbq_tail = NULL;
                }
                if(l_uni_buf)
                {
                        free(l_uni_buf);
                        l_uni_buf = NULL;
                }
        }
        SECTION("write read write on boundaries") {
                ns_hurl::nbq l_nbq(4096);
                char *l_buf = create_uniform_buf(4*4096);
                char *l_rbuf = (char *)malloc(4096);
                nbq_write(l_nbq, l_buf, 4*4096, 4096);
                REQUIRE(( l_nbq.b_read_avail() == (4096)));
                REQUIRE(( l_nbq.read_avail() == (4*4096)));
                int32_t l_s;
                //l_nbq.b_display_all();
                l_s = verify_contents(l_nbq, 4*4096, 0);
                REQUIRE(( l_s == HURL_STATUS_OK ));
                l_nbq.reset_read();
                nbq_read(l_nbq, l_rbuf, 4096);
                //ns_hurl::mem_display((const uint8_t *)l_rbuf, 4*4096, true);
                l_s = verify_contents(l_rbuf, 4*4096, 0);
                REQUIRE(( l_s == HURL_STATUS_OK ));
                REQUIRE(( l_nbq.b_read_avail() == (0)));
                REQUIRE(( l_nbq.read_avail() == (0)));
                nbq_write(l_nbq, l_buf, 4*4096, 4096);
                REQUIRE(( l_nbq.b_read_avail() == (4096)));
                REQUIRE(( l_nbq.read_avail() == (4*4096)));
                //l_nbq.b_display_all());
                //nbq_read(l_nbq, l_buf, BLOCK_SIZE);
                //REQUIRE(( l_nbq.read_avail() == 0 ));
                if(l_rbuf)
                {
                        free(l_rbuf);
                        l_rbuf = NULL;
                }
                if(l_buf)
                {
                        free(l_rbuf);
                        l_rbuf = NULL;
                }
        }
}
