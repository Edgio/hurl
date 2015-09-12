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
#include <list>

//: ----------------------------------------------------------------------------
//: Constants
//: ----------------------------------------------------------------------------

//: ----------------------------------------------------------------------------
//: Enums
//: ----------------------------------------------------------------------------

//: ----------------------------------------------------------------------------
//: Macros
//: ----------------------------------------------------------------------------
namespace ns_hlx {

//: ----------------------------------------------------------------------------
//: Fwd Decl's
//: ----------------------------------------------------------------------------

//: ----------------------------------------------------------------------------
//: Types
//: ----------------------------------------------------------------------------
typedef struct nb_struct {

        // ptr to alloc'd block
        char *m_data;

        // len of block
        uint32_t m_len;

        nb_struct(uint32_t a_len);
        void init(uint32_t a_len);
        ~nb_struct(void);

private:
        DISALLOW_COPY_AND_ASSIGN(nb_struct)

} nb_t;

typedef std::list <nb_t *> nb_list_t;

//: ----------------------------------------------------------------------------
//: \details: TODO
//: ----------------------------------------------------------------------------
class nbq
{
public:

        // -------------------------------------------------
        // Public methods
        // -------------------------------------------------
        nbq(uint32_t a_bsize);
        ~nbq();

        // -------------------------------------------------
        // Public members
        // -------------------------------------------------
        // Writing...
        int32_t write(const char *a_buf, uint32_t a_len);
        uint32_t write_avail(void);
        char *write_ptr(void);
        void write_incr(uint32_t a_size);

        // Reading...
        uint32_t read_avail(void);
        char *read_ptr(void);
        void read_incr(uint32_t a_size);

        uint32_t add_avail(void);

        // Reset...
        void reset_read(void);
        void reset_write(void);
        void reset(void);

private:

        // -------------------------------------------------
        // Private methods
        // -------------------------------------------------
        DISALLOW_COPY_AND_ASSIGN(nbq)

        // -------------------------------------------------
        // Private members
        // -------------------------------------------------

        char *m_write_ptr;
        uint32_t m_write_avail;
        uint32_t m_write_num;
        nb_list_t::iterator m_write_block;

        char *m_read_ptr;
        uint32_t m_read_avail;
        nb_list_t::iterator m_read_block;

        // block size
        uint32_t m_bsize;

        // The data...
        nb_list_t m_q;

};

} // ns_hlx

#endif



