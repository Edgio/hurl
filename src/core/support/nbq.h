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
        // Disallow copy/assign
        nb_struct& operator=(const nb_struct &);
        nb_struct(const nb_struct &);

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

        // Getters
        uint64_t get_cur_write_offset(void) { return m_cur_write_offset;}

        // Writing...
        int64_t write(const char *a_buf, uint64_t a_len);
        int64_t write_fd(int a_fd, uint64_t a_len);

        // Reading
        int64_t read(char *a_buf, uint64_t a_len);
        uint64_t read_seek(uint64_t a_off);
        uint64_t read_from(uint64_t a_off, char *a_buf, uint64_t a_len);
        uint64_t read_avail(void) {return m_total_read_avail;}

        // Resetting...
        void reset_read(void);
        void reset_write(void);
        void reset(void);

        // Shrink -free all read blocks
        void shrink(void);

        // Print
        void print(void);

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
        void     b_display_written(void);

        // For use with obj pool
        uint64_t get_idx(void) {return m_idx;}
        void set_idx(uint64_t a_idx) {m_idx = a_idx;}

private:
        // -------------------------------------------------
        // Private methods
        // -------------------------------------------------
        // Disallow copy/assign
        nbq& operator=(const nbq &);
        nbq(const nbq &);

        // -------------------------------------------------
        // Private members
        // -------------------------------------------------
        // Write...
        uint64_t m_cur_write_offset;
        char *m_cur_block_write_ptr;
        uint32_t m_cur_block_write_avail;
        nb_list_t::iterator m_cur_write_block;

        // Read...
        uint64_t m_cur_read_offset;
        char *m_cur_block_read_ptr;
        nb_list_t::iterator m_cur_read_block;

        // Totals
        uint64_t m_total_read_avail;

        // Block size
        uint32_t m_bsize;

        // Block list
        nb_list_t m_q;

        // For use with obj pool
        uint64_t m_idx;

};

//: ----------------------------------------------------------------------------
//: Utils
//: ----------------------------------------------------------------------------
char *copy_part(nbq &a_nbq, uint64_t a_off, uint64_t a_len);
void print_part(nbq &a_nbq, uint64_t a_off, uint64_t a_len);

} // ns_hlx

#endif



