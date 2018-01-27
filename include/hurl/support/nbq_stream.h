//: ----------------------------------------------------------------------------
//: Copyright (C) 2016 Verizon.  All Rights Reserved.
//: All Rights Reserved
//:
//: \file:    nbq_stream.h
//: \details: TODO
//: \author:  Reed P. Morrison
//: \date:    12/26/2016
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
#ifndef _NBQ_STREAM_H
#define _NBQ_STREAM_H

//: ----------------------------------------------------------------------------
//: Includes
//: ----------------------------------------------------------------------------
#include "hurl/support/nbq.h"
#include <assert.h>

namespace ns_hurl {

//: ----------------------------------------------------------------------------
//: \details: nbq_stream
//: ----------------------------------------------------------------------------
class nbq_stream
{
public:
        // -------------------------------------------------
        // Public types
        // -------------------------------------------------
        typedef char Ch;

        // -------------------------------------------------
        // Public methods
        // -------------------------------------------------
        nbq_stream(nbq &a_nbq):
                m_nbq(a_nbq),
                m_idx(0)
        {}
        ~nbq_stream()
        {}

        // -------------------------------------------------
        // Peek
        // -------------------------------------------------
        Ch Peek() const
        {
                if(m_nbq.read_avail())
                {
                        return m_nbq.peek();
                }
                return '\0';
        }

        // -------------------------------------------------
        // Take
        // -------------------------------------------------
        Ch Take()
        {
                char l_c;
                int64_t l_s;
                l_s = m_nbq.read(&l_c, 1);
                if(l_s != 1)
                {
                        return '\0';
                }
                ++m_idx;
                return l_c;
        }

        // -------------------------------------------------
        // Tell
        // -------------------------------------------------
        size_t Tell() const
        {
                return m_idx;
        }

        Ch* PutBegin() { assert(false); return 0; }
        void Put(Ch) { assert(false); }
        void Flush() { assert(false); }
        size_t PutEnd(Ch*) { assert(false); return 0; }

private:
        // -------------------------------------------------
        // Private methods
        // -------------------------------------------------
        // Disallow copy/assign
        nbq_stream& operator=(const nbq_stream &);
        nbq_stream(const nbq_stream &);

        // -------------------------------------------------
        // Private members
        // -------------------------------------------------
        nbq &m_nbq;
        size_t m_idx;
};


} // ns_hurl

#endif
