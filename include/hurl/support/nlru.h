//: ----------------------------------------------------------------------------
//: Copyright (C) 2016 Verizon.  All Rights Reserved.
//: All Rights Reserved
//:
//: \file:    nlru.h
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
#ifndef _NLRU_H
#define _NLRU_H

//: ----------------------------------------------------------------------------
//: Includes
//: ----------------------------------------------------------------------------
#include <stdint.h>

#include <string>
#include <list>
#include <map>
#include <unordered_set>
#include <unordered_map>

#define __STDC_FORMAT_MACROS 1
#include <inttypes.h>

namespace ns_hurl {

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
        typedef std::pair<id_t, _Tp *> id_entry_pair_t;
        typedef std::list<id_entry_pair_t> item_list_t;
        typedef std::unordered_map<id_t, typename item_list_t::iterator> item_list_map_t;
        typedef std::unordered_map<id_t, label_t> id_label_map_t;

        // delete callback
        typedef int (*delete_cb_t) (void* o_1, void *a_2);

        // ---------------------------------------
        // Constructor
        // ---------------------------------------
        nlru(uint32_t a_max_entries):
                        m_label_id_set_map(),
                        m_item_list_map(),
                        m_item_list(),
                        m_item_list_size(0),
                        m_id_label_map(),
                        m_max_entries(a_max_entries),
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
                _Tp *l_ref = m_item_list.back().second;
                if(m_delete_cb)
                {
                        m_delete_cb(m_o_1, l_ref);
                }
                remove(m_item_list.back().first);
        }

        // ---------------------------------------
        // Insert...
        // ---------------------------------------
        id_t insert(const label_t &a_label, _Tp *a_new_entry)
        {
                id_t l_id = m_next_id++;
                //printf("%s.%s.%d: a_label: %s --size: %lu a_new_entry: %p\n", __FILE__,__FUNCTION__,__LINE__, a_label.c_str(), size(), a_new_entry);

                ++m_item_list_size;
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

                //printf("%s.%s.%d: ID: %d, m_item_list.size(): %d m_max_entries: %d\n", __FILE__,__FUNCTION__,__LINE__,(int)l_id, (int)m_item_list.size(), (int)m_max_entries);
                if (m_item_list_size > m_max_entries)
                {
                        //printf("Evict\n");
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
        _Tp* get(const label_t &a_label)
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

                _Tp* l_retval = ((i_item->second->second));
                //printf("%s.%s.%d: **REMOVE FROM LIST**: ID: %d\n", __FILE__,__FUNCTION__,__LINE__,(int)*i_id);
                remove(*i_id);
                return l_retval;
        }

        // ---------------------------------------
        // size...
        // ---------------------------------------
        uint64_t size(void)
        {
                return m_item_list_size;
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
                if(i_idx == i_set->second.end())
                {
                        return;
                }

                typename item_list_map_t::iterator i_id = m_item_list_map.find(a_id);
                if(i_id == m_item_list_map.end())
                {
                        return;
                }

                // Remove
                i_set->second.erase(i_idx);
                if(i_set->second.empty())
                {
                        m_label_id_set_map.erase(i_set);
                }

                --m_item_list_size;
                m_item_list.erase(i_id->second);
                m_item_list_map.erase(i_id);
                //printf("%s.%s.%d: delete -i_label: %u\n", __FILE__,__FUNCTION__,__LINE__,(unsigned int)i_label->first);
                m_id_label_map.erase(i_label);
                // TODO --error if not found?
                return;
        }

        void show(void)
        {
                printf("+----------------------------------------------------------+\n");
                printf(": NLRU STATE                                               :\n");
                printf("+-----------------------------------+----------------------+\n");
                printf(": m_label_id_set_map                :\n");
                printf("+-----------------------------------+\n");
                for(typename label_id_set_map_t::const_iterator _i = m_label_id_set_map.begin();
                                _i != m_label_id_set_map.end();
                                ++_i)
                {
                        printf(":   LABEL: %s\n", _i->first.c_str());
                        for(typename id_set_t::const_iterator _s = _i->second.begin();
                                        _s != _i->second.end();
                                        ++_s)
                        {
                                printf(":     ID: %" PRIu64 "\n", *_s);

                        }
                }
                printf("+-----------------------------------+\n");
                printf(": m_item_list_map                   :\n");
                printf("+-----------------------------------+\n");
                for(typename item_list_map_t::const_iterator _i = m_item_list_map.begin();
                                _i != m_item_list_map.end();
                                ++_i)
                {
                        printf(":   ID: %16lu PTR: %p\n", _i->first, (_i->second)->second);
                }
                printf("+-----------------------------------+\n");
                printf(": m_item_list                       :\n");
                printf("+-----------------------------------+\n");
                for(typename item_list_t::const_iterator _i = m_item_list.begin();
                                _i != m_item_list.end();
                                ++_i)
                {
                        printf(":   ID: %16lu PTR: %p\n", _i->first, _i->second);
                }
                printf("+-----------------------------------+\n");
                printf(": m_id_label_map                    :\n");
                printf("+-----------------------------------+\n");
                for(typename id_label_map_t::const_iterator _i = m_id_label_map.begin();
                                _i != m_id_label_map.end();
                                ++_i)
                {
                        printf(":   ID: %16lu LABEL: %s\n", _i->first, _i->second.c_str());
                }

        }

        void get_label_cnts(std::map<std::string, uint32_t> &ao_label_cnts) const
        {
                for(typename label_id_set_map_t::const_iterator _i = m_label_id_set_map.begin();
                                _i != m_label_id_set_map.end();
                                ++_i)
                {
                        for(typename id_set_t::const_iterator _s = _i->second.begin();
                                        _s != _i->second.end();
                                        ++_s)
                        {
                                ++ao_label_cnts[_i->first];
                        }
                }
        }
private:
        // Disallow copy/assign
        nlru& operator=(const nlru &);
        nlru(const nlru &);

        label_id_set_map_t m_label_id_set_map;
        item_list_map_t m_item_list_map;
        item_list_t m_item_list;
        uint64_t m_item_list_size;
        id_label_map_t m_id_label_map;

        uint32_t m_max_entries;
        delete_cb_t m_delete_cb;
        id_t m_next_id;

        void *m_o_1;
};

} //namespace ns_hurl {

#endif



