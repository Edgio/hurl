//: ----------------------------------------------------------------------------
//: Copyright (C) 2015 Verizon.  All Rights Reserved.
//: All Rights Reserved
//:
//: \file:    nbq.h
//: \details: TODO
//: \author:  Reed P. Morrison
//: \date:    07/20/2015
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
#ifndef _NBQ_H
#define _NBQ_H

//: ----------------------------------------------------------------------------
//: Includes
//: ----------------------------------------------------------------------------
#include "ndebug.h"
#include <stdint.h>
#include <list>

namespace ns_hlx {

//: ----------------------------------------------------------------------------
//: Types
//: ----------------------------------------------------------------------------
typedef struct nb_struct {
        char *m_data;
        uint32_t m_len;

        nb_struct(uint32_t a_len);
        void init(uint32_t a_len);
        ~nb_struct(void);
private:
        DISALLOW_COPY_AND_ASSIGN(nb_struct)

} nb_t;

typedef std::list <nb_t *> nb_list_t;

//: ----------------------------------------------------------------------------
//: \details: nbq
//: ----------------------------------------------------------------------------
class nbq
{
public:
        // -------------------------------------------------
        // Public methods
        // -------------------------------------------------
        nbq(uint32_t a_bsize);
        ~nbq();

        // Writing...
        int32_t write(const char *a_buf, uint32_t a_len);

        // Reading
        int32_t read(char *a_buf, uint32_t a_len);
        uint32_t read_avail(void) {return m_total_read_avail;}

        // Resetting...
        void reset_read(void);
        void reset_write(void);
        void reset(void);

        // Block Writing...
        char *   b_write_ptr(void) {return m_cur_block_write_ptr;}
        uint32_t b_write_avail(void) {return m_cur_block_write_avail;}
        int32_t  b_write_add_avail();
        void     b_write_incr(uint32_t a_len);

        // Block Reading...
        char *   b_read_ptr(void) {return m_cur_block_read_ptr;}
        int32_t  b_read_avail(void);
        void     b_read_incr(uint32_t a_len);

        // Debugging display all
        void     b_display_all(void);
private:
        // -------------------------------------------------
        // Private methods
        // -------------------------------------------------
        DISALLOW_COPY_AND_ASSIGN(nbq)

        // -------------------------------------------------
        // Private members
        // -------------------------------------------------
        // Write...
        char *m_cur_block_write_ptr;
        uint32_t m_cur_block_write_avail;
        nb_list_t::iterator m_cur_write_block;

        // Read...
        char *m_cur_block_read_ptr;
        nb_list_t::iterator m_cur_read_block;

        // Totals
        uint32_t m_total_read_avail;

        // Block size
        uint32_t m_bsize;

        // Block list
        nb_list_t m_q;
};

} // ns_hlx

#endif



