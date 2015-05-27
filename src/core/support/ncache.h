//: ----------------------------------------------------------------------------
//: Copyright (C) 2015 Verizon.  All Rights Reserved.
//: All Rights Reserved
//:
//: \file:    ncache.h
//: \details: TODO
//: \author:  Reed P. Morrison
//: \date:    05/23/2015
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
#ifndef _NCACHE_HPP
#define _NCACHE_HPP

//: ----------------------------------------------------------------------------
//: Includes
//: ----------------------------------------------------------------------------
#include <list>
#include <unordered_map>

//: ----------------------------------------------------------------------------
//: Types
//: ----------------------------------------------------------------------------
typedef enum
{
        NCACHE_LAST = 0,
        NCACHE_LRU,
} ncache_evict_t;

#define NCACHE_DEFAULT_MAX_ENTRIES 64

//: ----------------------------------------------------------------------------
//: Cache
//: ----------------------------------------------------------------------------
template<class _Tp>
class ncache
{

private:
        typedef std::pair<uint64_t, _Tp> hash_entry_pair_t;
        typedef std::list<hash_entry_pair_t> cache_list_t;
        typedef std::unordered_map<uint64_t, typename cache_list_t::iterator> cache_map_t;

        cache_list_t m_cache_list;
        uint32_t m_cache_list_size;
        cache_map_t m_cache_map;
        uint32_t m_max_entries;
        ncache_evict_t m_evict;

public:
        // ---------------------------------------
        // Constructor
        // ---------------------------------------
        ncache(uint32_t a_max_entries = NCACHE_DEFAULT_MAX_ENTRIES,
               ncache_evict_t a_evict =NCACHE_LAST) :
                        m_cache_list(),
                        m_cache_list_size(0),
                        m_cache_map(),
                        m_max_entries(a_max_entries),
                        m_evict(a_evict)
        {}

        // ---------------------------------------
        // Insert...
        // ---------------------------------------
        void insert(uint64_t a_new_entry_hash, const _Tp &a_new_entry)
        {
                m_cache_list.push_front(std::make_pair(a_new_entry_hash, a_new_entry));
                ++m_cache_list_size;
                m_cache_map[a_new_entry_hash] = m_cache_list.begin();
                if (m_cache_list_size > m_max_entries)
                {
                        m_cache_map.erase(m_cache_list.back().first);
                        m_cache_list.pop_back();
                        --m_cache_list_size;
                }
        }

        // ---------------------------------------
        // has...
        // ---------------------------------------
        bool has(uint64_t a_entry_hash)
        {
                typename cache_map_t::iterator i_cache_map_entry;
                if ((i_cache_map_entry = m_cache_map.find(a_entry_hash)) != m_cache_map.end())
                {
                        evict(i_cache_map_entry);
                        return true;
                }
                else
                {
                        return false;
                }
        }

        // ---------------------------------------
        // get...
        // ---------------------------------------
        const _Tp* get(uint64_t a_entry_hash)
        {
                typename cache_map_t::iterator i_cache_map_entry;
                if ((i_cache_map_entry = m_cache_map.find(a_entry_hash)) != m_cache_map.end())
                {
                        evict(i_cache_map_entry);
                        return &(i_cache_map_entry->second->second);
                }
                else
                {
                        return NULL;
                }
        }

        // ---------------------------------------
        // size...
        // ---------------------------------------
        uint64_t size(void)
        {
                return m_cache_list_size;
        };

        // ---------------------------------------
        // remove...
        // ---------------------------------------
        void remove(uint64_t a_entry_hash)
        {
                typename cache_map_t::iterator i_cache_map_entry;
                if ((i_cache_map_entry = m_cache_map.find(a_entry_hash)) != m_cache_map.end())
                {
                        m_cache_list.erase(i_cache_map_entry->second);
                        m_cache_map.erase(i_cache_map_entry);
                        --m_cache_list_size;
                }
                // TODO --error if not found?
        }
private:

        // ---------------------------------------
        // run access policy...
        // ---------------------------------------
        inline void evict(typename cache_map_t::iterator &a_cache_map_entry)
        {
                if (m_evict == NCACHE_LRU)
                {
                        typename cache_list_t::iterator l_cur_list_iter;
                        l_cur_list_iter = a_cache_map_entry->second;
                        m_cache_list.splice(m_cache_list.begin(),
                                            m_cache_list,
                                            l_cur_list_iter);
                        a_cache_map_entry->second = m_cache_list.begin();
                }
        }
};
#endif //#ifndef _NCACHE_HPP
