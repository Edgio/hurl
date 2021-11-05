//! ----------------------------------------------------------------------------
//! Copyright Edgecast Inc.
//!
//! \file:    TODO
//! \details: TODO
//!
//! Licensed under the terms of the Apache 2.0 open source license.
//! Please refer to the LICENSE file in the project root for the terms.
//! ----------------------------------------------------------------------------
#ifndef _HTTP_STATUS_H
#define _HTTP_STATUS_H
namespace ns_hurl {
//! ----------------------------------------------------------------------------
//! Enums
//! ----------------------------------------------------------------------------
// Status
typedef enum http_status_enum {

        HTTP_STATUS_NONE                                = 0,

        HTTP_STATUS_CONTINUE                            = 100,
        HTTP_STATUS_SWITCHING_PROTOCOLS                 = 101,
        HTTP_STATUS_PROCESSING                          = 102, /* WEBDAV */

        HTTP_STATUS_OK                                  = 200,
        HTTP_STATUS_CREATED                             = 201,
        HTTP_STATUS_ACCEPTED                            = 202,
        HTTP_STATUS_NON_AUTHORITATIVE_INFORMATION       = 203,
        HTTP_STATUS_NO_CONTENT                          = 204,
        HTTP_STATUS_RESET_CONTENT                       = 205,
        HTTP_STATUS_PARTIAL_CONTENT                     = 206,
        HTTP_STATUS_MULTI_STATUS                        = 207, /* WEBDAV */

        HTTP_STATUS_MULTIPLE_CHOICES                    = 300,
        HTTP_STATUS_MOVED_PERMANENTLY                   = 301,
        HTTP_STATUS_FOUND                               = 302,
        HTTP_STATUS_SEE_OTHER                           = 303,
        HTTP_STATUS_NOT_MODIFIED                        = 304,
        HTTP_STATUS_USE_PROXY                           = 305,
        HTTP_STATUS_UNUSED                              = 306,
        HTTP_STATUS_TEMPORARY_REDIRECT                  = 307,

        HTTP_STATUS_BAD_REQUEST                         = 400,
        HTTP_STATUS_UNAUTHORIZED                        = 401,
        HTTP_STATUS_PAYMENT_REQUIRED                    = 402,
        HTTP_STATUS_FORBIDDEN                           = 403,
        HTTP_STATUS_NOT_FOUND                           = 404,
        HTTP_STATUS_METHOD_NOT_ALLOWED                  = 405,
        HTTP_STATUS_NOT_ACCEPTABLE                      = 406,
        HTTP_STATUS_PROXY_AUTHENTICATION_REQUIRED       = 407,
        HTTP_STATUS_REQUEST_TIMEOUT                     = 408,
        HTTP_STATUS_CONFLICT                            = 409,
        HTTP_STATUS_GONE                                = 410,
        HTTP_STATUS_LENGTH_REQUIRED                     = 411,
        HTTP_STATUS_PRECONDITION_FAILED                 = 412,
        HTTP_STATUS_REQUEST_ENTITY_TOO_LARGE            = 413,
        HTTP_STATUS_REQUEST_URI_TOO_LONG                = 414,
        HTTP_STATUS_UNSUPPORTED_MEDIA_TYPE              = 415,
        HTTP_STATUS_REQUESTED_RANGE_NOT_SATISFIABLE     = 416,
        HTTP_STATUS_EXPECTATION_FAILED                  = 417,
        HTTP_STATUS_UNPROCESSABLE_ENTITY                = 422, /* WEBDAV */
        HTTP_STATUS_LOCKED                              = 423, /* WEBDAV */
        HTTP_STATUS_FAILED_DEPENDENCY                   = 424, /* WEBDAV */
        HTTP_STATUS_UPGRADE_REQUIRED                    = 426, /* TLS */

        HTTP_STATUS_INTERNAL_SERVER_ERROR               = 500,
        HTTP_STATUS_NOT_IMPLEMENTED                     = 501,
        HTTP_STATUS_BAD_GATEWAY                         = 502,
        HTTP_STATUS_SERVICE_NOT_AVAILABLE               = 503,
        HTTP_STATUS_GATEWAY_TIMEOUT                     = 504,
        HTTP_STATUS_HTTP_VERSION_NOT_SUPPORTED          = 505,
        HTTP_STATUS_INSUFFICIENT_STORAGE                = 507, /* WEBDAV */
        HTTP_STATUS_LOOP_DETECTED                       = 508,
        HTTP_STATUS_BANDWIDTH_LIMIT_EXCEEDED            = 509,

} http_status_t;
}
#endif
