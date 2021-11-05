//! ----------------------------------------------------------------------------
//! Copyright Edgecast Inc.
//!
//! \file:    TODO
//! \details: TODO
//!
//! Licensed under the terms of the Apache 2.0 open source license.
//! Please refer to the LICENSE file in the project root for the terms.
//! ----------------------------------------------------------------------------
#ifndef _CR_H
#define _CR_H
//! ----------------------------------------------------------------------------
//! includes
//! ----------------------------------------------------------------------------
// For fixed size types
#include <stdint.h>
#include <list>
namespace ns_hurl {
//! ----------------------------------------------------------------------------
//! Raw buffer
//! ----------------------------------------------------------------------------
typedef struct cr_struct
{
        uint64_t m_off;
        uint64_t m_len;
        cr_struct():
                m_off(0),
                m_len(0)
        {}
        void clear(void)
        {
                m_off = 0;
                m_len = 0;
        }
} cr_t;
typedef std::list <cr_t> cr_list_t;
}
#endif

