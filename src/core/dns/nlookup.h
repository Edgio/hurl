//! ----------------------------------------------------------------------------
//! Copyright Edgio Inc.
//!
//! \file:    TODO
//! \details: TODO
//!
//! Licensed under the terms of the Apache 2.0 open source license.
//! Please refer to the LICENSE file in the project root for the terms.
//! ----------------------------------------------------------------------------
#ifndef _LOOKUP_INLINE_H
#define _LOOKUP_INLINE_H
//! ----------------------------------------------------------------------------
//! includes
//! ----------------------------------------------------------------------------
#include <stdint.h>
#include <string>
#include <arpa/inet.h>
namespace ns_hurl {
//! ----------------------------------------------------------------------------
//! Fwd Decl's
//! ----------------------------------------------------------------------------
struct host_info;
//! ----------------------------------------------------------------------------
//! Lookup inline
//! ----------------------------------------------------------------------------
int32_t nlookup(const std::string &a_host, uint16_t a_port, host_info &ao_host_info, int = AF_UNSPEC);
} //namespace ns_hurl {
#endif
