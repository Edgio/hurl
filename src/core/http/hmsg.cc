//! ----------------------------------------------------------------------------
//! Copyright Edgio Inc.
//!
//! \file:    TODO
//! \details: TODO
//!
//! Licensed under the terms of the Apache 2.0 open source license.
//! Please refer to the LICENSE file in the project root for the terms.
//! ----------------------------------------------------------------------------
//! ----------------------------------------------------------------------------
//! includes
//! ----------------------------------------------------------------------------
#include "status.h"
#include "http/hmsg.h"
#include "support/nbq.h"
#include "support/ndebug.h"
#include "http_parser/http_parser.h"
#include <stdlib.h>
namespace ns_hurl {
//! ----------------------------------------------------------------------------
//! \details: TODO
//! \return:  TODO
//! \param:   TODO
//! ----------------------------------------------------------------------------
hmsg::hmsg(void):
        m_http_parser_settings(nullptr),
        m_http_parser(nullptr),
        m_expect_resp_body_flag(true),
        m_cur_off(0),
        m_cur_buf(nullptr),
        m_save(false),
        m_p_h_list_key(),
        m_p_h_list_val(),
        m_p_body(),
        m_http_major(),
        m_http_minor(),
        m_complete(false),
        m_supports_keep_alives(false),
        m_type(TYPE_NONE),
        m_q(nullptr),
        m_body_q(nullptr),
        m_idx(0),
        m_headers(nullptr)
{
        m_http_parser_settings = (http_parser_settings *)calloc(1, sizeof(http_parser_settings));
        m_http_parser = (http_parser *)calloc(1, sizeof(http_parser));
        init(false);
}
//! ----------------------------------------------------------------------------
//! \details: TODO
//! \return:  TODO
//! \param:   TODO
//! ----------------------------------------------------------------------------
hmsg::~hmsg(void)
{
        if (m_body_q)
        {
                delete m_body_q;
        }
        if (m_headers)
        {
                delete m_headers;
        }
        if (m_http_parser_settings)
        {
                free(m_http_parser_settings);
        }
        if (m_http_parser)
        {
                free(m_http_parser);
        }
}
//! ----------------------------------------------------------------------------
//! \details: TODO
//! \return:  TODO
//! \param:   TODO
//! ----------------------------------------------------------------------------
void hmsg::init(bool a_save)
{
        m_p_h_list_key.clear();
        m_p_h_list_val.clear();
        m_p_body.clear();
        m_http_major = 0;
        m_http_minor = 0;
        m_complete = false;
        m_supports_keep_alives = false;
        m_save = a_save;
        if (m_headers)
        {
                delete m_headers;
                m_headers = nullptr;
        }
        if (m_body_q)
        {
                delete m_body_q;
                m_body_q = nullptr;
        }
}
//! ----------------------------------------------------------------------------
//!                               Getters
//! ----------------------------------------------------------------------------
//! ----------------------------------------------------------------------------
//! \details: TODO
//! \return:  TODO
//! \param:   TODO
//! ----------------------------------------------------------------------------
hmsg::type_t hmsg::get_type(void) const
{
        return m_type;
}
//! ----------------------------------------------------------------------------
//! \details: TODO
//! \return:  TODO
//! \param:   TODO
//! ----------------------------------------------------------------------------
nbq *hmsg::get_q(void) const
{
        return m_q;
}
//! ----------------------------------------------------------------------------
//! \details: TODO
//! \return:  TODO
//! \param:   TODO
//! ----------------------------------------------------------------------------
nbq *hmsg::get_body_q(void)
{
        if (m_body_q)
        {
                return m_body_q;
        }
        if (!m_p_body.m_off)
        {
                return nullptr;
        }
        int32_t l_s;
        l_s = m_q->split(&m_body_q, m_p_body.m_off);
        if (l_s != STATUS_OK)
        {
                return nullptr;
        }
        return m_body_q;
}
//! ----------------------------------------------------------------------------
//! \details: TODO
//! \return:  TODO
//! \param:   TODO
//! ----------------------------------------------------------------------------
uint64_t hmsg::get_body_len(void) const
{
        return m_p_body.m_len;
}
//! ----------------------------------------------------------------------------
//! \details: TODO
//! \return:  TODO
//! \param:   ao_headers   Pointer to the headers object to populate.
//! \note:    assume is valid
//! ----------------------------------------------------------------------------
void hmsg::get_headers(kv_map_list_t *ao_headers) const
{
        ns_hurl::cr_list_t::const_iterator i_k = m_p_h_list_key.begin();
        ns_hurl::cr_list_t::const_iterator i_v = m_p_h_list_val.begin();
        for(; i_k != m_p_h_list_key.end() &&
                    i_v != m_p_h_list_val.end();
            ++i_k, ++i_v)
        {
                // for each entry in the lists of headers
                char *l_h_k_b = copy_part(*m_q, i_k->m_off, i_k->m_len);
                std::string l_h_k = l_h_k_b;
                if (l_h_k_b)
                {
                        free(l_h_k_b);
                        l_h_k_b = nullptr;
                }
                char *l_h_v_b = copy_part(*m_q, i_v->m_off, i_v->m_len);
                std::string l_h_v = l_h_v_b;
                if (l_h_v_b)
                {
                        free(l_h_v_b);
                        l_h_v_b = nullptr;
                }
                ns_hurl::kv_map_list_t::iterator i_obj = ao_headers->find(l_h_k);
                if (i_obj != ao_headers->end()){
                        i_obj->second.push_back(l_h_v);
                } else {
                        ns_hurl::str_list_t l_list;
                        l_list.push_back(l_h_v);
                        (*ao_headers)[l_h_k] = l_list;
                }
        }
        return;
}
//! ----------------------------------------------------------------------------
//! \details: TODO
//! \return:  TODO
//! \param:   ao_headers   Pointer to the headers object to populate.  ASSUMPTION: is valid
//! ----------------------------------------------------------------------------
const kv_map_list_t &hmsg::get_headers()
{
        if (nullptr == m_headers)
        {
                // need to initialize
                m_headers = new kv_map_list_t();
                get_headers(m_headers);
        }
        return *m_headers;
}
//! ----------------------------------------------------------------------------
//! \details: TODO
//! \return:  TODO
//! \param:   TODO
//! ----------------------------------------------------------------------------
uint64_t hmsg::get_idx(void) const
{
        return m_idx;
}
//! ----------------------------------------------------------------------------
//! \details: TODO
//! \return:  TODO
//! \param:   TODO
//! ----------------------------------------------------------------------------
void hmsg::set_idx(uint64_t a_idx)
{
        m_idx = a_idx;
}
//! ----------------------------------------------------------------------------
//!                               Setters
//! ----------------------------------------------------------------------------
//! ----------------------------------------------------------------------------
//! \details: TODO
//! \return:  TODO
//! \param:   TODO
//! ----------------------------------------------------------------------------
void hmsg::set_q(nbq *a_q)
{
        m_q = a_q;
}
//! ----------------------------------------------------------------------------
//! \details: TODO
//! \return:  TODO
//! \param:   TODO
//! ----------------------------------------------------------------------------
void hmsg::reset_body_q(void)
{
       m_body_q = nullptr;
       m_p_body.m_len = 0;
       m_p_body.m_off = 0;
}
} //namespace ns_hurl {
