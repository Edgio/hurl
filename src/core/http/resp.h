//! ----------------------------------------------------------------------------
//! Copyright Verizon.
//!
//! \file:    TODO
//! \details: TODO
//!
//! Licensed under the terms of the Apache 2.0 open source license.
//! Please refer to the LICENSE file in the project root for the terms.
//! ----------------------------------------------------------------------------
#ifndef _RESP_H
#define _RESP_H
//! ----------------------------------------------------------------------------
//! includes
//! ----------------------------------------------------------------------------
#include "http/hmsg.h"
#include "http/http_status.h"
namespace ns_hurl {
//! ----------------------------------------------------------------------------
//! \details: TODO
//! ----------------------------------------------------------------------------
class resp : public hmsg
{
public:
        // -------------------------------------------------
        // Public methods
        // -------------------------------------------------
        resp();
        ~resp();
        // Getters
        uint16_t get_status(void);
        // Setters
        void set_status(http_status_t a_code);
        void clear(void);
        void init(bool a_save);
        // Debug
        void show(bool a_color = false);
        // -------------------------------------------------
        // Public members
        // -------------------------------------------------
        // ---------------------------------------
        // raw http request offsets
        // ---------------------------------------
        cr_t m_p_status;
        // TODO REMOVE
        const char *m_tls_info_protocol_str;
        const char *m_tls_info_cipher_str;
private:
        // -------------------------------------------------
        // Private methods
        // -------------------------------------------------
        // Disallow copy/assign
        resp& operator=(const resp &);
        resp(const resp &);
        // -------------------------------------------------
        // Private members
        // -------------------------------------------------
        http_status_t m_status;
};
}
#endif
