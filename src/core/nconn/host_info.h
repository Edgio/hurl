//! ----------------------------------------------------------------------------
//! Copyright Edgio Inc.
//!
//! \file:    TODO
//! \details: TODO
//!
//! Licensed under the terms of the Apache 2.0 open source license.
//! Please refer to the LICENSE file in the project root for the terms.
//! ----------------------------------------------------------------------------
#ifndef _HOST_INFO_H
#define _HOST_INFO_H
//! ----------------------------------------------------------------------------
//! includes
//! ----------------------------------------------------------------------------
#include <sys/socket.h>
namespace ns_hurl {
//! ----------------------------------------------------------------------------
//! \details: Host info
//! ----------------------------------------------------------------------------
struct host_info {
        struct sockaddr_storage m_sa;
        int m_sa_len;
        int m_sock_family;
        int m_sock_type;
        int m_sock_protocol;
        unsigned int m_expires_s;
        host_info();
        void show(void);
};
}
#endif

