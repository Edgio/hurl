//: ----------------------------------------------------------------------------
//: Copyright (C) 2015 Verizon.  All Rights Reserved.
//: All Rights Reserved
//:
//: \file:    nbq.cc
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

//: ----------------------------------------------------------------------------
//: Includes
//: ----------------------------------------------------------------------------
#include "hlx/trace.h"
#include "hlx/status.h"
#include "nbq.h"
#include "ndebug.h"
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

namespace ns_hlx {

//: ----------------------------------------------------------------------------
//: Macros
//: ----------------------------------------------------------------------------
#define CHECK_FOR_NULL_AND_LEN(_buf, _len)\
        do{\
                if(!_buf) {\
                        return -1;\
                }\
                if(!_len) {\
                        return 0;\
                }\
        }while(0)\

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
nbq::nbq(uint32_t a_bsize):
        m_cur_write_offset(0),
        m_cur_block_write_ptr(NULL),
        m_cur_block_write_avail(0),
        m_cur_write_block(),
        m_cur_read_offset(0),
        m_cur_block_read_ptr(NULL),
        m_cur_read_block(),
        m_total_read_avail(0),
        m_bsize(a_bsize),
        m_q(),
        m_idx(0)
{
        //NDBG_PRINT("%sCONSTR%s: this: %p\n", ANSI_COLOR_FG_GREEN, ANSI_COLOR_OFF, this);
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
nbq::~nbq(void)
{
        //NDBG_PRINT("%sDELETE%s: this: %p\n", ANSI_COLOR_FG_RED, ANSI_COLOR_OFF, this);
        for(nb_list_t::iterator i_block = m_q.begin(); i_block != m_q.end(); ++i_block)
        {
                if(*i_block)
                {
                        delete *i_block;
                }
        }
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int64_t nbq::write(const char *a_buf, uint64_t a_len)
{
        CHECK_FOR_NULL_AND_LEN(a_buf, a_len);
        uint64_t l_left = a_len;
        uint64_t l_written = 0;
        const char *l_buf = a_buf;
        while(l_left)
        {
                if(b_write_avail() <= 0)
                {
                        int32_t l_status = b_write_add_avail();
                        if(l_status <= 0)
                        {
                                // TODO error...
                                return -1;
                        }
                }
                //NDBG_PRINT("l_left: %lu\n", l_left);
                uint32_t l_write_avail = b_write_avail();
                uint32_t l_write = (l_left > l_write_avail)?l_write_avail:l_left;
                //NDBG_PRINT("WRITIN bytes: %u\n", l_write);
                //mem_display((const uint8_t *)l_buf, l_write);
                memcpy(b_write_ptr(), l_buf, l_write);
                b_write_incr(l_write);
                l_left -= l_write;
                l_buf += l_write;
                l_written += l_write;
        }
        return l_written;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int64_t nbq::write_fd(int a_fd, uint64_t a_len, ssize_t &a_status)
{
        if(!a_len)
        {
                return 0;
        }
        uint64_t l_left = a_len;
        uint64_t l_written = 0;
        while(l_left)
        {
                if(b_write_avail() <= 0)
                {
                        int32_t l_status = b_write_add_avail();
                        if(l_status <= 0)
                        {
                                // TODO error...
                                return -1;
                        }
                }
                //NDBG_PRINT("l_left: %lu\n", l_left);
                uint32_t l_write_avail = b_write_avail();
                uint32_t l_write = (l_left > l_write_avail)?l_write_avail:l_left;
                errno = 0;
                a_status = ::read(a_fd, b_write_ptr(), l_write);
                if(a_status > 0)
                {
                        b_write_incr(a_status);
                        b_read_incr(0);
                        l_left -= a_status;
                        l_written += a_status;
                        //NDBG_PRINT("%sREAD%s[%p][%p]: a_status = %ld l_write: %u avail: %u\n",
                        //                ANSI_COLOR_FG_BLUE, ANSI_COLOR_OFF, this, b_write_ptr(), a_status, l_write, b_write_avail());
                }
                else if((a_status == 0) ||
                   ((a_status < 0) && (errno == EAGAIN)))
                {
                        break;
                }
                else if(a_status < 0)
                {
                        return HLX_STATUS_ERROR;
                }
        }
        return l_written;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int64_t nbq::write_q(nbq &a_q)
{
        uint64_t l_left = a_q.read_avail();
        uint64_t l_written = 0;
        while(l_left)
        {
                if(b_write_avail() <= 0)
                {
                        int32_t l_status = b_write_add_avail();
                        if(l_status <= 0)
                        {
                                TRC_ERROR("b_write_add_avail()\n");
                                return HLX_STATUS_ERROR;
                        }
                }
                //NDBG_PRINT("l_left: %u\n", l_left);
                uint32_t l_write_avail = b_write_avail();
                uint32_t l_write = (l_left > l_write_avail)?l_write_avail:l_left;

                // Read from q
                ssize_t l_status = a_q.read(b_write_ptr(), l_write);
                if(l_status < 0)
                {
                        TRC_ERROR("a_q.read()\n");
                        return HLX_STATUS_ERROR;
                }
                if(l_status == 0)
                {
                        break;
                }
                b_write_incr(l_status);
                l_left -= l_status;
                l_written += l_status;
        }
        return l_written;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int64_t nbq::read(char *a_buf, uint64_t a_len)
{
        if(!a_len)
        {
                return 0;
        }
        uint64_t l_read = 0;
        char *l_buf = a_buf;
        uint64_t l_total_read_avail = read_avail();
        uint64_t l_left = (a_len > l_total_read_avail)?l_total_read_avail:a_len;
        while(l_left)
        {
                uint32_t l_read_avail = b_read_avail();
                uint32_t l_read_size = (l_left > l_read_avail)?l_read_avail:l_left;
                //NDBG_PRINT("l_left:       %u\n", l_left);
                //NDBG_PRINT("l_read_avail: %u\n", l_read_avail);
                //NDBG_PRINT("l_read_size:  %u\n", l_read_size);
                if(l_buf)
                {
                        //NDBG_PRINT("l_buf: %p b_read_ptr(): %p l_read_size: %u\n", l_buf, b_read_ptr(), l_read_size);
                        memcpy(l_buf, b_read_ptr(), l_read_size);
                        l_buf += l_read_size;
                }
                b_read_incr(l_read_size);
                l_left -= l_read_size;
                l_read += l_read_size;
        }
        return l_read;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
uint64_t nbq::read_seek(uint64_t a_off)
{
        uint64_t l_d = 0;
        if(a_off > m_cur_read_offset)
        {
                l_d = a_off - m_cur_read_offset;
        }
        else if(a_off < m_cur_read_offset)
        {
                reset_read();
                l_d = a_off;
        }
        if(l_d)
        {
                // fast fwd
                read(NULL, l_d);
                // TODO check for errors?
        }
        return a_off;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
uint64_t nbq::read_from(uint64_t a_off, char *a_buf, uint64_t a_len)
{
        read_seek(a_off);
        return read(a_buf, a_len);
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void nbq::reset_read(void)
{
        m_cur_read_offset = 0;
        m_cur_read_block = m_q.begin();
        m_cur_block_read_ptr = NULL;
        m_total_read_avail = 0;
        if(m_q.size())
        {
                m_cur_block_read_ptr = (*m_cur_read_block)->m_data;
                m_total_read_avail = (std::distance(m_q.begin(), m_cur_write_block)+1)*m_bsize
                                     - m_cur_block_write_avail;
        }
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void nbq::reset_write(void)
{
        if(!m_q.empty())
        {
                m_cur_write_offset = 0;
                m_cur_write_block = m_q.begin();
                m_cur_block_write_avail = (*m_cur_write_block)->m_len;
                m_cur_block_write_ptr = (*m_cur_write_block)->m_data;
                reset_read();
        }
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void nbq::reset(void)
{
        for(nb_list_t::iterator i_block = m_q.begin(); i_block != m_q.end(); ++i_block)
        {
                if(*i_block)
                {
                        //NDBG_PRINT("DELETING.\n");
                        delete *i_block;
                }
        }
        m_q.clear();
        m_cur_write_offset = 0;
        m_cur_write_block = m_q.begin();
        m_cur_block_write_ptr = NULL;
        m_cur_block_write_avail = 0;
        m_cur_block_read_ptr = NULL;
        m_cur_read_offset = 0;
        m_cur_read_block = m_q.begin();
        m_total_read_avail = 0;
}


//: ----------------------------------------------------------------------------
//: \details: Free all read
//: \return:  NA
//: \param:   NA
//: ----------------------------------------------------------------------------
void nbq::shrink(void)
{
        while(m_q.begin() != m_cur_read_block)
        {
                nb_t &l_nb = *(m_q.front());
                m_q.pop_front();
                m_cur_write_offset -= l_nb.m_len;
                m_cur_read_offset -= l_nb.m_len;
                delete &l_nb;
        }
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void nbq::print(void)
{
        uint32_t l_total_read_avail = read_avail();
        uint32_t l_left = l_total_read_avail;
        while(l_left)
        {
                uint32_t l_read_avail = b_read_avail();
                uint32_t l_read_size = (l_left > l_read_avail)?l_read_avail:l_left;
                TRC_OUTPUT("%.*s", l_read_size, b_read_ptr());
                b_read_incr(l_read_size);
                l_left -= l_read_size;
        }
        reset_read();
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t nbq::b_write_add_avail(void)
{
        nb_t *l_block = new nb_struct(m_bsize);
        //NDBG_PRINT("%s[[ADD_AVAIL]]%s: %u\n", ANSI_COLOR_BG_MAGENTA, ANSI_COLOR_OFF, m_bsize);
        m_q.push_back(l_block);
        if(m_q.size() == 1)
        {
                m_cur_read_block = m_q.begin();
                m_cur_write_block = m_q.begin();
                m_cur_block_write_ptr = (*m_cur_write_block)->m_data;
                m_cur_block_read_ptr = (*m_cur_read_block)->m_data;
        }
        else
        {
                if(!m_cur_block_write_avail &&
                  (m_cur_write_block != --m_q.end()))
                {
                        ++m_cur_write_block;
                        m_cur_block_write_ptr = (*m_cur_write_block)->m_data;
                }
        }

        m_cur_block_write_avail += m_bsize;
        return m_bsize;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void nbq::b_write_incr(uint32_t a_len)
{
        //NDBG_PRINT("%sWRITE_INCR%s[%p]: a_len: %u m_cur_block_write_avail: %d m_cur_block_write_ptr: %p m_total_read_avail: %d\n",
        //                ANSI_COLOR_BG_BLUE, ANSI_COLOR_OFF,
        //                this,
        //                a_len,
        //                (int)m_cur_block_write_avail, m_cur_block_write_ptr, (int)m_total_read_avail);
        m_cur_write_offset += a_len;
        m_cur_block_write_avail -= a_len;
        m_cur_block_write_ptr += a_len;
        m_total_read_avail += a_len;

        //NDBG_PRINT("%sWRITE_INCR%s[%p]: a_len: %u m_cur_block_write_avail: %d m_cur_block_write_ptr: %p m_total_read_avail: %d\n",
        //                ANSI_COLOR_FG_BLUE, ANSI_COLOR_OFF,
        //                this,
        //                a_len,
        //                (int)m_cur_block_write_avail, m_cur_block_write_ptr, (int)m_total_read_avail);

        if(!m_cur_block_write_avail &&
           (m_cur_write_block != --m_q.end()))
        {
                ++m_cur_write_block;
                m_cur_block_write_ptr = (*m_cur_write_block)->m_data;
                m_cur_block_write_avail = (*m_cur_write_block)->m_len;
        }
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void nbq::b_read_incr(uint32_t a_len)
{
        uint32_t l_avail = b_read_avail();
        //NDBG_PRINT("%sREAD_INCR%s[%p]: a_len: %6u l_avail: %6d m_total_read_avail: %6d\n", ANSI_COLOR_BG_CYAN, ANSI_COLOR_OFF, this, a_len, (int)l_avail, (int)m_total_read_avail);
        m_total_read_avail -= a_len;
        l_avail -= a_len;
        m_cur_block_read_ptr += a_len;
        m_cur_read_offset += a_len;
        //NDBG_PRINT("%sREAD_INCR%s[%p]: a_len: %6u l_avail: %6d m_total_read_avail: %6d\n", ANSI_COLOR_FG_CYAN, ANSI_COLOR_OFF, this, a_len, (int)l_avail, (int)m_total_read_avail);
        if(!l_avail && m_total_read_avail)
        {
                ++m_cur_read_block;
                m_cur_block_read_ptr = (*m_cur_read_block)->m_data;
                m_cur_read_offset += a_len;
        }
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t nbq::b_read_avail(void)
{
        if(m_cur_read_block == m_cur_write_block)
        {
                return m_total_read_avail;
        }
        else
        {
                return (*m_cur_read_block)->m_len - (m_cur_block_read_ptr - (*m_cur_read_block)->m_data);
        }
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void nbq::b_display_written(void)
{
        if(m_q.empty())
        {
                return;
        }
        uint32_t i_block_num = 0;
        nb_list_t::iterator i_block;
        for(i_block = m_q.begin(); i_block != m_cur_write_block; ++i_block, ++i_block_num)
        {
                NDBG_OUTPUT("+------------------------------------+\n");
                NDBG_OUTPUT("| Block: %d\n", i_block_num);
                NDBG_OUTPUT("+------------------------------------+\n");
                mem_display((const uint8_t *)((*i_block)->m_data), (*i_block)->m_len);
        }
        NDBG_OUTPUT("+------------------------------------+\n");
        NDBG_OUTPUT("| Block: %d\n", i_block_num);
        NDBG_OUTPUT("+------------------------------------+\n");
        mem_display((const uint8_t *)((*i_block)->m_data), (*i_block)->m_len - m_cur_block_write_avail);
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void nbq::b_display_all(void)
{
        uint32_t i_block_num = 0;
        for(nb_list_t::iterator i_block = m_q.begin(); i_block != m_q.end(); ++i_block, ++i_block_num)
        {
                NDBG_OUTPUT("+------------------------------------+\n");
                NDBG_OUTPUT("| Block: %d\n", i_block_num);
                NDBG_OUTPUT("+------------------------------------+\n");
                mem_display((const uint8_t *)((*i_block)->m_data), (*i_block)->m_len);
        }
}

//: ----------------------------------------------------------------------------
//: Block...
//: ----------------------------------------------------------------------------

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
nb_struct::nb_struct(uint32_t a_len):
                m_data(NULL),
                m_len(0)
{
        init(a_len);
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void nb_struct::init(uint32_t a_len)
{
        if(m_data)
        {
                free(m_data);
                m_data = NULL;
        }
        m_len = 0;
        m_data = (char *)malloc(a_len);
        //NDBG_PRINT("%sALLOC%s PTR[%p] len: %u\n", ANSI_COLOR_FG_GREEN, ANSI_COLOR_OFF, m_data, a_len);
        m_len = a_len;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
nb_struct::~nb_struct()
{
        if(m_data)
        {
                //NDBG_PRINT("%sFREE%s  PTR[%p]\n", ANSI_COLOR_FG_RED, ANSI_COLOR_OFF, m_data);
                free(m_data);
                m_data = NULL;
        }
        m_len = 0;
}

//: ----------------------------------------------------------------------------
//: Utils...
//: ----------------------------------------------------------------------------
//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
char *copy_part(nbq &a_nbq, uint64_t a_off, uint64_t a_len)
{
        char *l_buf = NULL;
        l_buf = (char *)calloc(1, sizeof(char)*a_len + 1);
        if(!l_buf)
        {
                return NULL;
        }
        a_nbq.read_from(a_off, l_buf, a_len);
        l_buf[a_len] = '\0';
        return l_buf;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void print_part(nbq &a_nbq, uint64_t a_off, uint64_t a_len)
{
        char *l_buf = copy_part(a_nbq, a_off, a_len);
        TRC_OUTPUT("%.*s", (int)a_len, l_buf);
        if(l_buf)
        {
                free(l_buf);
                l_buf = NULL;
        }
}

} // ns_hlx
