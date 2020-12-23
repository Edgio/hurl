//! ----------------------------------------------------------------------------
//! Copyright Verizon.
//!
//! \file:    TODO
//! \details: TODO
//!
//! Licensed under the terms of the Apache 2.0 open source license.
//! Please refer to the LICENSE file in the project root for the terms.
//! ----------------------------------------------------------------------------
#ifndef _FILE_UTIL_H
#define _FILE_UTIL_H
//! ----------------------------------------------------------------------------
//! includes
//! ----------------------------------------------------------------------------
#include <stdint.h>
namespace ns_hurl {
//! ----------------------------------------------------------------------------
//! fwd decl's
//! ----------------------------------------------------------------------------
class nbq;
//! ----------------------------------------------------------------------------
//! Prototypes
//! ----------------------------------------------------------------------------
int32_t read_file(const char *a_file, char **a_buf, uint32_t *a_len);
int32_t read_file_nbq(nbq &ao_nbq, uint32_t &ao_len, const char *a_file);
} //namespace ns_hurl {
#endif
