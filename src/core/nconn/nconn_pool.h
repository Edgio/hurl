//: ----------------------------------------------------------------------------
//: Copyright (C) 2014 Verizon.  All Rights Reserved.
//: All Rights Reserved
//:
//: \file:    nconn.h
//: \details: TODO
//: \author:  Reed P. Morrison
//: \date:    02/07/2014
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
#ifndef _NCONN_POOL_H
#define _NCONN_POOL_H

//: ----------------------------------------------------------------------------
//: Includes
//: ----------------------------------------------------------------------------
#include "ndebug.h"
#include "nconn.h"

#include <list>
#include <unordered_set>
#include <unordered_map>
#include <vector>

//: ----------------------------------------------------------------------------
//: Constants
//: ----------------------------------------------------------------------------
#define NCACHE_DEFAULT_MAX_ENTRIES 64

namespace ns_hlx {

//: ----------------------------------------------------------------------------
//: Fwd Decl's
//: ----------------------------------------------------------------------------

//: ----------------------------------------------------------------------------
//: Types
//: ----------------------------------------------------------------------------
typedef std::vector<nconn *> nconn_vector_t;
typedef std::list<uint32_t> conn_id_list_t;
typedef std::unordered_set<uint32_t> conn_id_set_t;

//: ----------------------------------------------------------------------------
//: Enums
//: ----------------------------------------------------------------------------
typedef enum
{
        NCACHE_LAST = 0,
        NCACHE_LRU,
} ncache_evict_t;

//: ----------------------------------------------------------------------------
//: Cache
//: ----------------------------------------------------------------------------
template<class _Tp>
class nlru
{
public:
        typedef uint64_t id_t;
        typedef std::string label_t;
        typedef std::unordered_set<id_t> id_set_t;
        typedef std::map<label_t, id_set_t> label_id_set_map_t;
        typedef std::pair<id_t, _Tp> id_entry_pair_t;
        typedef std::list<id_entry_pair_t> item_list_t;
        typedef std::unordered_map<id_t, typename item_list_t::iterator> item_list_map_t;
        typedef std::unordered_map<id_t, label_t> id_label_map_t;

        // delete callback
        typedef int (*delete_cb_t) (void* o_1, void *a_2);

        // ---------------------------------------
        // Constructor
        // ---------------------------------------
        nlru(uint32_t a_max_entries = NCACHE_DEFAULT_MAX_ENTRIES,
             ncache_evict_t a_evict =NCACHE_LAST) :
                        m_label_id_set_map(),
                        m_item_list_map(),
                        m_item_list(),
                        m_id_label_map(),
                        m_max_entries(a_max_entries),
                        m_evict(a_evict),
                        m_delete_cb(NULL),
                        m_next_id(0),
                        m_o_1(NULL)
        {}

        // ---------------------------------------
        // Delete callback
        // ---------------------------------------
        void set_delete_cb(delete_cb_t a_delete_cb, void *a_o_1)
        {
                m_delete_cb = a_delete_cb;
                m_o_1 = a_o_1;
        }

        // ---------------------------------------
        // evict...
        // ---------------------------------------
        void evict(void)
        {
                if(m_item_list.empty())
                {
                        return;
                }
                _Tp &l_ref = m_item_list.back().second;
                if(m_delete_cb)
                {
                        m_delete_cb(m_o_1, &l_ref);
                }

                // Get label
                m_item_list_map.erase(m_item_list.back().first);
                typename id_label_map_t::iterator i_label = m_id_label_map.find(m_item_list.back().first);
                if(i_label == m_id_label_map.end())
                {
                        // Error couldn't find...
                        return;
                }

                typename label_id_set_map_t::iterator i_set = m_label_id_set_map.find(i_label->second);
                if(i_set == m_label_id_set_map.end())
                {
                        return;
                }

                // Find in set
                typename id_set_t::iterator i_idx = i_set->second.find(m_item_list.back().first);
                if(i_idx != i_set->second.end())
                {
                        // Remove
                        i_set->second.erase(i_idx);
                }
                if(i_set->second.empty())
                {
                        m_label_id_set_map.erase(i_set);
                }

                m_id_label_map.erase(i_label);
                m_item_list.pop_back();
        }

        // ---------------------------------------
        // Insert...
        // ---------------------------------------
        id_t insert(const label_t &a_label, const _Tp &a_new_entry)
        {
                id_t l_id = m_next_id++;

                m_item_list.push_front(std::make_pair(l_id, a_new_entry));
                label_id_set_map_t::iterator i_set = m_label_id_set_map.find(a_label);
                if(i_set == m_label_id_set_map.end())
                {
                        id_set_t l_set;
                        l_set.insert(l_id);
                        m_label_id_set_map[a_label] = l_set;
                }
                else
                {
                        i_set->second.insert(l_id);
                }
                m_item_list_map[l_id] = m_item_list.begin();
                m_id_label_map[l_id] = a_label;
                if (m_item_list.size() > m_max_entries)
                {
                        evict();
                }

                return l_id;
        }

        // ---------------------------------------
        // has...
        // ---------------------------------------
        // TODO
        //bool has(const label_t &a_label)
        //{
        //        return false;
        //}

        // ---------------------------------------
        // get...
        // ---------------------------------------
        const _Tp* get(const label_t &a_label)
        {
                typename label_id_set_map_t::iterator i_set = m_label_id_set_map.find(a_label);
                if(i_set == m_label_id_set_map.end())
                {
                        return NULL;
                }

                if(i_set->second.empty())
                {
                        return NULL;
                }

                // Pick one from set
                typename id_set_t::iterator i_id = i_set->second.begin();
                typename item_list_map_t::iterator i_item = m_item_list_map.find(*i_id);
                if(i_item == m_item_list_map.end())
                {
                        return NULL;
                }

                access_policy(i_item);
                return &i_item->second->second;
        }

        // ---------------------------------------
        // size...
        // ---------------------------------------
        uint64_t size(void)
        {
                return m_item_list.size();
        };

        // ---------------------------------------
        // remove...
        // ---------------------------------------
        void remove(const id_t &a_id)
        {
                // Get label
                typename id_label_map_t::iterator i_label = m_id_label_map.find(a_id);
                if(i_label == m_id_label_map.end())
                {
                        // Error couldn't find...
                        return;
                }

                typename label_id_set_map_t::iterator i_set = m_label_id_set_map.find(i_label->second);
                if(i_set == m_label_id_set_map.end())
                {
                        return;
                }

                // Find in set
                typename id_set_t::iterator i_idx = i_set->second.find(a_id);
                if(i_idx != i_set->second.end())
                {
                        // Remove
                        i_set->second.erase(i_idx);
                }
                if(i_set->second.empty())
                {
                        m_label_id_set_map.erase(i_set);
                }


                typename item_list_map_t::iterator i_id = m_item_list_map.find(a_id);
                if(i_id == m_item_list_map.end())
                {
                        return;
                }

                m_item_list.erase(i_id->second);
                m_item_list_map.erase(i_id);
                m_id_label_map.erase(i_label);
                // TODO --error if not found?
                return;
        }
private:
        DISALLOW_COPY_AND_ASSIGN(nlru)

        // ---------------------------------------
        // run access policy...
        // ---------------------------------------
        inline void access_policy(typename item_list_map_t::iterator &a_map_entry)
        {
                if (m_evict == NCACHE_LRU)
                {
                        typename item_list_t::iterator l_cur_list_iter;
                        l_cur_list_iter = a_map_entry->second;
                        m_item_list.splice(m_item_list.begin(),
                                           m_item_list,
                                           l_cur_list_iter);
                        a_map_entry->second = m_item_list.begin();
                }
        }

        label_id_set_map_t m_label_id_set_map;
        item_list_map_t m_item_list_map;
        item_list_t m_item_list;
        id_label_map_t m_id_label_map;

        uint32_t m_max_entries;
        ncache_evict_t m_evict;
        delete_cb_t m_delete_cb;
        id_t m_next_id;

        void *m_o_1;
};

typedef nlru <nconn *> idle_conn_lru_t;

//: ----------------------------------------------------------------------------
//: \class: nconn_pool
//: ----------------------------------------------------------------------------
class nconn_pool
{
public:
        // -------------------------------------------------
        // Public methods
        // -------------------------------------------------
        nconn_pool(uint32_t a_size);
        ~nconn_pool();
        // TODO passing settings struct -readonly reference
        int32_t get(nconn::scheme_t a_scheme, nconn **ao_nconn);
        int32_t get_try_idle(const std::string &a_host, nconn::scheme_t a_scheme, nconn **ao_nconn);
        nconn *create_conn(nconn::scheme_t a_scheme);
        int32_t add_idle(nconn *a_nconn);
        int32_t release(nconn *a_nconn);
        int32_t cleanup(nconn *a_nconn);
        uint32_t num_in_use(void) const {return m_conn_idx_used_set.size();}

        // -------------------------------------------------
        // Public static methods
        // -------------------------------------------------
        static int delete_cb(void* o_1, void *a_2);

        // -------------------------------------------------
        // Public members
        // -------------------------------------------------

private:

        // -------------------------------------------------
        // Private methods
        // -------------------------------------------------
        DISALLOW_COPY_AND_ASSIGN(nconn_pool)
        void init(void);

        // -------------------------------------------------
        // Private members
        // -------------------------------------------------
        nconn_vector_t m_nconn_vector;
        conn_id_list_t m_conn_idx_free_list;
        conn_id_set_t m_conn_idx_used_set;
        idle_conn_lru_t m_idle_conn_ncache;
        bool m_initd;

protected:
        // -------------------------------------------------
        // Protected members
        // -------------------------------------------------

};

} //namespace ns_hlx {

#endif
