//: ----------------------------------------------------------------------------
//: Copyright (C) 2016 Verizon.  All Rights Reserved.
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
#include "ndebug.h"
#include "hurl/status.h"
#include "hurl/support/trace.h"
#include "hurl/support/nbq.h"
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

namespace ns_hurl {

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
//: \details: nbq
//: ----------------------------------------------------------------------------
typedef struct nb_struct {

        // -------------------------------------------------
        // std constructor
        // -------------------------------------------------
        nb_struct(uint32_t a_len):
                m_data(NULL),
                m_len(a_len),
                m_written(0),
                m_read(0),
                m_ref(false)
        {
                m_data = (char *)malloc(a_len);
        }
        // -------------------------------------------------
        // std constructor
        // -------------------------------------------------
        nb_struct(char *a_buf, uint32_t a_len):
                m_data(a_buf),
                m_len(a_len),
                m_written(a_len),
                m_read(0),
                m_ref(true)
        {
        }
        // -------------------------------------------------
        // destructor
        // -------------------------------------------------
        ~nb_struct(void)
        {
                if(m_data &&
                  !m_ref)
                {
                        free(m_data);
                }
                m_data = NULL;
                m_len = 0;
        }
        uint32_t write_avail(void) { return m_len - m_written;}
        uint32_t read_avail(void) { return m_written - m_read;}
        char *data(void) { return m_data; }
        bool ref(void) const { return m_ref; }
        uint32_t size(void) { return m_len; }
        uint32_t written(void) { return m_written; }
        char *write_ptr(void) { return m_data + m_written; }
        void write_inc(uint32_t a_inc) { m_written += a_inc; }
        void write_reset(void) { m_written = 0; m_read = 0;}
        char *read_ptr(void) { return m_data + m_read; }
        void read_inc(uint32_t a_inc) { m_read += a_inc; }
        void read_reset(void) { m_read = 0; }

private:
        // Disallow copy/assign
        nb_struct& operator=(const nb_struct &);
        nb_struct(const nb_struct &);

        char *m_data;
        uint32_t m_len;
        uint32_t m_written;
        uint32_t m_read;
        bool m_ref;

} nb_t;

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
nbq::nbq(uint32_t a_bsize):
        m_q(),
        m_bsize(a_bsize),
        m_cur_write_block(),
        m_cur_read_block(),
        m_idx(0),
        m_cur_write_offset(0),
        m_total_read_avail(0)
{
        m_cur_write_block = m_q.end();
        m_cur_read_block = m_q.end();
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
nbq::~nbq(void)
{
        for(nb_list_t::iterator i_b = m_q.begin(); i_b != m_q.end(); ++i_b)
        {
                if(*i_b)
                {
                        delete *i_b;
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
                                NDBG_PRINT("error performing b_write_add_avail\n");
                                return -1;
                        }
                }
                uint32_t l_write_avail = b_write_avail();
                uint32_t l_write = (l_left > l_write_avail)?l_write_avail:l_left;
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
                }
                else if((a_status == 0) ||
                   ((a_status < 0) && (errno == EAGAIN)))
                {
                        break;
                }
                else if(a_status < 0)
                {
                        return HURL_STATUS_ERROR;
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
                                return HURL_STATUS_ERROR;
                        }
                }
                uint32_t l_write_avail = b_write_avail();
                uint32_t l_write = (l_left > l_write_avail)?l_write_avail:l_left;
                ssize_t l_status = a_q.read(b_write_ptr(), l_write);
                if(l_status < 0)
                {
                        TRC_ERROR("a_q.read()\n");
                        return HURL_STATUS_ERROR;
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
char nbq::peek(void) const
{
        if(read_avail())
        {
                return *b_read_ptr();
        }
        return '\0';
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
                if(l_buf)
                {
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
        reset_read();
        return read(NULL, a_off);
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
        // reset read ptrs and recalc read available
        m_total_read_avail = 0;
        for(nb_list_t::const_iterator i_b = m_q.begin();
            i_b != m_q.end();
            ++i_b)
        {
                (*i_b)->read_reset();
                m_total_read_avail += (*i_b)->read_avail();
        }
        m_cur_read_block = m_q.begin();
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void nbq::reset_write(void)
{
        for(nb_list_t::iterator i_b = m_q.begin();
            i_b != m_q.end();
            )
        {
                if(!(*i_b))
                {
                        ++i_b;
                        continue;
                }
                // erase references
                if((*i_b)->ref())
                {
                        delete (*i_b);
                        (*i_b) = NULL;
                        m_q.erase(i_b++);
                }
                else
                {
                        (*i_b)->write_reset();
                        ++i_b;
                }
        }
        m_cur_write_block = m_q.begin();
        m_cur_read_block = m_q.begin();
        m_total_read_avail = 0;
        m_cur_write_offset = 0;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void nbq::reset(void)
{
        for(nb_list_t::iterator i_b = m_q.begin();
            i_b != m_q.end();
            ++i_b)
        {
                if(*i_b)
                {
                        delete *i_b;
                        *i_b = NULL;
                }
        }
        m_q.clear();
        m_cur_write_block = m_q.end();
        m_cur_read_block = m_q.end();
        m_cur_write_offset = 0;
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
                m_cur_write_offset -= l_nb.size();
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
int32_t nbq::split(nbq **ao_nbq_tail, uint64_t a_offset)
{
        *ao_nbq_tail = NULL;
        if(!a_offset)
        {
                return HURL_STATUS_OK;
        }
        if(a_offset >= m_cur_write_offset)
        {
                TRC_ERROR("requested split at offset: %lu > write_offset: %lu\n", a_offset, m_cur_write_offset);
                return HURL_STATUS_ERROR;
        }

        // ---------------------------------------
        // find block at offset
        // ---------------------------------------
        uint64_t i_offset = a_offset;
        nb_list_t::iterator i_b;
        for(i_b = m_q.begin();
            i_b != m_q.end();
            ++i_b)
        {
                if(!(*i_b))
                {
                        TRC_ERROR("block iter in nbq == NULL\n");
                        return HURL_STATUS_ERROR;
                }
                uint32_t l_w = (*i_b)->written();
                if(l_w > i_offset)
                {
                        break;
                }
                i_offset -= l_w;
        }

        // ---------------------------------------
        // create new nbq and append remainder
        // ---------------------------------------
        nbq *l_nbq = new nbq(m_bsize);
        if(i_offset > 0)
        {
                nb_t &l_b = *(*i_b);
                if(i_offset >= l_b.written())
                {
                        TRC_ERROR("i_offset: %lu >= l_b.written(): %u\n", i_offset, l_b.written());
                        return HURL_STATUS_ERROR;
                }
                // write the remainder
                l_nbq->b_write_add_avail();
                l_nbq->write(l_b.data() + i_offset, l_b.written() - i_offset);
                l_b.write_reset();
                l_b.write_inc(i_offset);
        }

        // ---------------------------------------
        // add the tail
        // ---------------------------------------
        ++i_b;
        while (i_b != m_q.end())
        {
                if(!(*i_b))
                {
                        TRC_ERROR("block iter in nbq == NULL\n");
                        return HURL_STATUS_ERROR;
                }
                //NDBG_PRINT("adding tail block\n");
                l_nbq->m_q.push_back(*i_b);
                (*i_b)->read_reset();

                //NDBG_PRINT("removing tail block\n");
                m_cur_write_offset -= (*i_b)->written();
                l_nbq->m_cur_write_offset += (*i_b)->written();
                m_q.erase(i_b++);
        }

        l_nbq->m_cur_write_block = --(l_nbq->m_q.end());
        l_nbq->m_cur_read_block = l_nbq->m_q.begin();
        l_nbq->reset_read();

        m_cur_write_block = --m_q.end();
        reset_read();

        *ao_nbq_tail = l_nbq;
        return HURL_STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t nbq::join_ref(const nbq &ao_nbq_tail)
{
        const nbq &l_nbq_tail = ao_nbq_tail;
        for(nb_list_t::const_iterator i_b = l_nbq_tail.m_q.begin();
            i_b != l_nbq_tail.m_q.end();
            ++i_b)
        {
                if(!(*i_b))
                {
                        return HURL_STATUS_ERROR;
                }
                nb_t &l_b = *(*i_b);
                nb_t *l_b_ref = new nb_t(l_b.data(), l_b.written());
                m_q.push_back(l_b_ref);
                m_cur_write_offset += l_b_ref->written();
                m_total_read_avail += l_b_ref->written();
        }
        m_cur_write_block = m_q.end();
        // Join nbq with reference nbq
        return HURL_STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
char * nbq::b_write_ptr(void)
{
        if(m_cur_write_block == m_q.end())
        {
                return NULL;
        }
        return (*m_cur_write_block)->write_ptr();
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
uint32_t nbq::b_write_avail(void)
{
        if(m_cur_write_block == m_q.end())
        {
                return 0;
        }
        return (*m_cur_write_block)->write_avail();
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
        }
        else
        {
                if(((*m_cur_write_block)->write_avail() == 0) &&
                    (m_cur_write_block != --m_q.end()))
                {
                        ++m_cur_write_block;
                }
        }
        //NDBG_PRINT("write_avail: %u\n", (*m_cur_write_block)->write_avail());
        return (*m_cur_write_block)->write_avail();
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void nbq::b_write_incr(uint32_t a_len)
{
        m_cur_write_offset += a_len;
        (*m_cur_write_block)->write_inc(a_len);
        m_total_read_avail += a_len;
        if(((*m_cur_write_block)->write_avail() == 0) &&
             (m_cur_write_block != --m_q.end()))
        {
                ++m_cur_write_block;
        }
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
char *nbq::b_read_ptr(void) const
{
        if(m_cur_read_block == m_q.end())
        {
                return NULL;
        }
        return (*m_cur_read_block)->read_ptr();
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t nbq::b_read_avail(void) const
{
        if(m_cur_read_block == m_q.end())
        {
                return 0;
        }
        else if(m_cur_read_block == m_cur_write_block)
        {
                return m_total_read_avail;
        }
        else
        {
                return (*m_cur_read_block)->read_avail();
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
        m_total_read_avail -= a_len;
        l_avail -= a_len;
        (*m_cur_read_block)->read_inc(a_len);
        if(!l_avail && m_total_read_avail)
        {
                ++m_cur_read_block;
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
        for(nb_list_t::iterator i_b = m_q.begin(); i_b != m_q.end(); ++i_b, ++i_block_num)
        {
                if(!(*i_b))
                {
                        return;
                }
                NDBG_OUTPUT("+------------------------------------+\n");
                NDBG_OUTPUT("| Block: %d -> %p\n", i_block_num, (*i_b));
                NDBG_OUTPUT("+------------------------------------+\n");
                nb_t &l_b = *(*i_b);
                mem_display((const uint8_t *)(l_b.data()), l_b.written(), false);
                if(i_b == m_cur_write_block)
                {
                        break;
                }
        }
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void nbq::b_display_all(void)
{
        uint32_t i_block_num = 0;
        for(nb_list_t::iterator i_b = m_q.begin(); i_b != m_q.end(); ++i_b, ++i_block_num)
        {
                if(!(*i_b))
                {
                        return;
                }
                NDBG_OUTPUT("+------------------------------------+\n");
                NDBG_OUTPUT("| Block: %d -> %p\n", i_block_num, (*i_b));
                NDBG_OUTPUT("+------------------------------------+\n");
                nb_t &l_b = *(*i_b);
                mem_display((const uint8_t *)(l_b.data()), l_b.size(), false);
        }
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

} // ns_hurl
