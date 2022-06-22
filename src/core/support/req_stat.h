//! ----------------------------------------------------------------------------
//! Copyright Edgio Inc.
//!
//! \file:    TODO
//! \details: TODO
//!
//! Licensed under the terms of the Apache 2.0 open source license.
//! Please refer to the LICENSE file in the project root for the terms.
//! ----------------------------------------------------------------------------
#ifndef _REQ_STAT_H
#define _REQ_STAT_H

//! ----------------------------------------------------------------------------
//! includes
//! ----------------------------------------------------------------------------
// Sockets...
#include <stdint.h>
#include <strings.h>

namespace ns_hurl {

//! ----------------------------------------------------------------------------
//! Types
//! ----------------------------------------------------------------------------
// Single stat
typedef struct req_stat_struct
{
        uint32_t m_body_bytes;
        uint32_t m_total_bytes;
        uint64_t m_tt_connect_us;
        uint64_t m_tt_first_read_us;
        uint64_t m_tt_completion_us;
        int32_t m_last_state;
        int32_t m_error;

} req_stat_t;



} //namespace ns_hurl {

#endif



