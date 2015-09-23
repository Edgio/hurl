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
#include "nbq.h"
#include <string.h>

namespace ns_hlx {

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t nbq::write(const char *a_buf, uint32_t a_len)
{
        //NDBG_PRINT("write: a_buf: %p a_len: %u\n", a_buf, a_len);
        if(!a_len || !a_buf)
        {
                return 0;
        }
        const char *l_buf = a_buf;
        uint32_t l_bytes_written = 0;
        uint32_t l_bytes_to_write = a_len;
        do {
                if(!m_write_avail)
                {
                        //NDBG_PRINT("ADD_AVAIL\n");
                        add_avail();
                }
                uint32_t l_write_bytes = (l_bytes_to_write > m_write_avail)?m_write_avail:l_bytes_to_write;
                memcpy(m_write_ptr, l_buf, l_write_bytes);
                m_write_num += l_write_bytes;
                write_incr(l_write_bytes);
                l_buf += l_write_bytes;
                l_bytes_to_write -= l_write_bytes;
                m_read_avail += l_write_bytes;
                l_bytes_written += l_write_bytes;
        } while(l_bytes_to_write);
        return l_bytes_written;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
uint32_t nbq::write_avail(void)
{
        return m_write_avail;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
char *nbq::write_ptr(void)
{
        return m_write_ptr;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void nbq::write_incr(uint32_t a_size)
{
        //NDBG_PRINT("%sWRITE_INCR%s: a_size: %u m_write_avail: %u\n",
        //                ANSI_COLOR_BG_CYAN, ANSI_COLOR_OFF,
        //                a_size,
        //                m_write_avail);

        if(a_size < m_write_avail)
        {
                m_write_avail -= a_size;
                m_write_ptr += a_size;
        }
        else
        {
                m_write_avail = 0;
                m_write_ptr = NULL;
        }

        if(!m_write_avail)
        {
                if(m_write_block != --(m_q.end()))
                {
                        ++m_write_block;
                        m_write_avail = (*m_write_block)->m_len;
                        m_write_ptr = (*m_write_block)->m_data;
                }
        }

}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
uint32_t nbq::read_avail(void)
{
        return m_read_avail;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
char *nbq::read_ptr(void)
{
        return m_read_ptr;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void nbq::read_incr(uint32_t a_size)
{
        //NDBG_PRINT("%sREAD_INCR%s: a_size: %u\n", ANSI_COLOR_BG_MAGENTA, ANSI_COLOR_OFF, a_size);

        if(a_size > m_read_avail)
        {
                m_read_avail = 0;
        }
        else
        {
                m_read_avail -= a_size;
                m_read_ptr += a_size;
        }

        // Check for next block
        if(m_read_avail == 0)
        {
                // Delete the current block
#if 0
                free(m_read_ptr);
                m_read_ptr = NULL;
                m_q.pop_front();
                m_read_block = m_q.front();
                if(m_read_block != m_q.end())
#else
                if(m_read_block != --m_q.end())
#endif
                {
                        ++m_read_block;
                        m_read_avail = (*m_read_block)->m_len;
                        m_read_ptr = (*m_read_block)->m_data;
                }
        }

}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
uint32_t nbq::add_avail(void)
{
        // Add to queue
        nb_t *l_block = new nb_struct(m_bsize);
        m_q.push_back(l_block);

        if(!m_write_avail)
        {
                m_write_block = --(m_q.end());
                m_write_avail = (*m_write_block)->m_len;
                m_write_ptr = (*m_write_block)->m_data;
        }

        if(m_q.size() == 1)
        {
                m_read_block = m_write_block;
                m_read_ptr = m_write_ptr;
                m_read_avail = 0;
        }

        return m_write_avail;
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
        m_write_ptr = NULL;
        m_write_avail = 0;
        m_write_block = m_q.begin();
        m_read_ptr = NULL;
        m_read_avail = 0;
        m_read_block = m_q.begin();
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void nbq::reset_read(void)
{
        m_read_block = m_q.begin();
        m_read_avail = m_write_num;
        if(!m_q.empty())
        {
                m_read_ptr = (*(m_q.begin()))->m_data;
        }
        else
        {
                m_read_ptr = NULL;
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
                m_write_block = m_q.begin();
                m_write_avail = m_q.size()*m_bsize;
                m_write_ptr = (*(m_q.begin()))->m_data;
                m_write_num = 0;
                reset_read();
        }
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
nbq::nbq(uint32_t a_bsize):
        m_write_ptr(NULL),
        m_write_avail(0),
        m_write_num(0),
        m_write_block(0),
        m_read_ptr(NULL),
        m_read_avail(0),
        m_read_block(0),
        m_bsize(a_bsize),
        m_q()
{
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
nbq::~nbq(void)
{
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

} // ns_hlx
