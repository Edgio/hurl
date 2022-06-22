//! ----------------------------------------------------------------------------
//! Copyright Edgio Inc.
//!
//! \file:    TODO
//! \details: TODO
//!
//! Licensed under the terms of the Apache 2.0 open source license.
//! Please refer to the LICENSE file in the project root for the terms.
//! ----------------------------------------------------------------------------
#ifndef _TIME_UTIL_H
#define _TIME_UTIL_H
//! ----------------------------------------------------------------------------
//! includes
//! ----------------------------------------------------------------------------
#include <stdint.h>
namespace ns_hurl {
//! ----------------------------------------------------------------------------
//! Prototypes
//! ----------------------------------------------------------------------------
const char *get_date_str(void);
uint64_t get_time_s(void);
uint64_t get_time_ms(void);
uint64_t get_time_us(void);
uint64_t get_delta_time_ms(uint64_t a_start_time_ms);
uint64_t get_delta_time_us(uint64_t a_start_time_us);
} //namespace ns_hurl {
#endif


