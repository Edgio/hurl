//! ----------------------------------------------------------------------------
//! Copyright Verizon.
//!
//! \file:    TODO
//! \details: TODO
//!
//! Licensed under the terms of the Apache 2.0 open source license.
//! Please refer to the LICENSE file in the project root for the terms.
//! ----------------------------------------------------------------------------
#ifndef _STRING_UTIL_H
#define _STRING_UTIL_H
//! ----------------------------------------------------------------------------
//! includes
//! ----------------------------------------------------------------------------
#include <string>
#include <stdint.h>
namespace ns_hurl {
//! ----------------------------------------------------------------------------
//! Prototypes
//! ----------------------------------------------------------------------------
int32_t break_header_string(const std::string &a_header_str, std::string &ao_header_key, std::string &ao_header_val);
std::string get_file_wo_path(const std::string &a_filename);
std::string get_file_path(const std::string &a_filename);
std::string get_base_filename(const std::string &a_filename);
std::string get_file_ext(const std::string &a_filename);
std::string get_file_wo_ext(const std::string &a_filename);
} //namespace ns_hurl {
#endif
