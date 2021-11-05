//! ----------------------------------------------------------------------------
//! Copyright Edgecast Inc.
//!
//! \file:    TODO
//! \details: TODO
//!
//! Licensed under the terms of the Apache 2.0 open source license.
//! Please refer to the LICENSE file in the project root for the terms.
//! ----------------------------------------------------------------------------
#ifndef _AI_CACHE_H
#define _AI_CACHE_H
//! ----------------------------------------------------------------------------
//! includes
//! ----------------------------------------------------------------------------
#include <string>
#include <map>
//! ----------------------------------------------------------------------------
//! Constants
//! ----------------------------------------------------------------------------
#define NRESOLVER_DEFAULT_AI_CACHE_FILE "/tmp/addr_info_cache.json"
//! ----------------------------------------------------------------------------
//! Fwd Decl's
//! ----------------------------------------------------------------------------
namespace ns_hurl {
//! ----------------------------------------------------------------------------
//! Fwd Decl's
//! ----------------------------------------------------------------------------
struct host_info;
//! ----------------------------------------------------------------------------
//! \details: TODO
//! ----------------------------------------------------------------------------
class ai_cache
{
public:
        // -------------------------------------------------
        // -Public methods
        // -------------------------------------------------
        ai_cache(const std::string &a_ai_cache_file);
        ~ai_cache();
        host_info *lookup(const std::string a_label);
        host_info *lookup(const std::string a_label, host_info *a_host_info);
        void add(const std::string a_label, host_info *a_host_info);
private:
        // -------------------------------------------------
        // -Types
        // -------------------------------------------------
        typedef std::map <std::string, host_info *> ai_cache_map_t;
        // -------------------------------------------------
        // -Private methods
        // -------------------------------------------------
        // Disallow copy/assign
        ai_cache& operator=(const ai_cache &);
        ai_cache(const ai_cache &);
        static int32_t sync(const std::string &a_ai_cache_file,
                            const ai_cache_map_t &a_ai_cache_map);
        static int32_t read(const std::string &a_ai_cache_file,
                            ai_cache_map_t &ao_ai_cache_map);
        // -------------------------------------------------
        // -Private members
        // -------------------------------------------------
        ai_cache_map_t m_ai_cache_map;
        std::string m_ai_cache_file;
};
} //namespace ns_hurl {
#endif
