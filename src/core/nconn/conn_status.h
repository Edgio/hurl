//! ----------------------------------------------------------------------------
//! Copyright Edgio Inc.
//!
//! \file:    TODO
//! \details: TODO
//!
//! Licensed under the terms of the Apache 2.0 open source license.
//! Please refer to the LICENSE file in the project root for the terms.
//! ----------------------------------------------------------------------------
#ifndef _CONN_STATUS_H
#define _CONN_STATUS_H
//! ----------------------------------------------------------------------------
//! includes
//! ----------------------------------------------------------------------------
#include <string>
//! ----------------------------------------------------------------------------
//! Fwd Decl
//! ----------------------------------------------------------------------------
typedef struct ssl_st SSL;
namespace ns_hurl {
// ---------------------------------------
// Connection status
// ---------------------------------------
typedef enum {
        CONN_STATUS_NONE                        =  1,
        CONN_STATUS_OK                          =  0,
        CONN_STATUS_ERROR_INTERNAL              = -1,   // generic internal failure
        CONN_STATUS_ERROR_ADDR_LOOKUP_FAILURE   = -1,   // failed to resolve, explicit
        CONN_STATUS_ERROR_ADDR_LOOKUP_TIMEOUT   = -2,   // failed to resolve, timed out
        CONN_STATUS_ERROR_CONNECT               = -3,   // resolved but failed to TCP connect, explicit
        // CONN_STATUS_ERROR_CONNECT_TIMEOUT       = -4,   // resolved but failed to TCP connect, timed out
        CONN_STATUS_ERROR_CONNECT_TLS           = -5,   // TCP connected but TLS error, generic
        CONN_STATUS_ERROR_CONNECT_TLS_HOST      = -6,   // TCP connected but TLS error, specific error for hostname validation failure because SSL makes us do it ourselves
        CONN_STATUS_ERROR_SEND                  = -7,   // connected but send error, explicit
        // CONN_STATUS_ERROR_SEND_TIMEOUT          = -8,   // connected but send error, timed out
        CONN_STATUS_ERROR_RECV                  = -9,   // connected, sent, error receiving, explicit
        // CONN_STATUS_ERROR_RECV_TIMEOUT          = -10,  // connected, sent, error receiving, timed out
        CONN_STATUS_ERROR_TIMEOUT               = -11,   // got a timeout waiting for something, generic.  TODO: deprecate after independent connect/send/recv timeout support
        CONN_STATUS_CANCELLED                   = -100
} conn_status_t;
//! ----------------------------------------------------------------------------
//! nconn_utils
//! ----------------------------------------------------------------------------
class nconn;
int nconn_get_fd(nconn &a_nconn);
SSL *nconn_get_SSL(nconn &a_nconn);
long nconn_get_last_SSL_err(nconn &a_nconn);
conn_status_t nconn_get_status(nconn &a_nconn);
const std::string &nconn_get_last_error_str(nconn &a_nconn);

}
#endif
