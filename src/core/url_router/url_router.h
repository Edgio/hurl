//: ----------------------------------------------------------------------------
//: \file:    url_router.h
//: \details: TODO
//: \author:  Reed P. Morrison
//: \date:    08/08/2015
//: ----------------------------------------------------------------------------
#ifndef _URL_ROUTER_H
#define _URL_ROUTER_H

//: ----------------------------------------------------------------------------
//: Includes
//: ----------------------------------------------------------------------------
#include <string>
#include <map>
#include <stdint.h>

namespace ns_hlx {

//: ----------------------------------------------------------------------------
//: Types
//: ----------------------------------------------------------------------------
#ifndef URL_PARAM_MAP_T
#define URL_PARAM_MAP_T
typedef std::map <std::string, std::string> url_param_map_t;
#endif

class node;

//: ----------------------------------------------------------------------------
//: url_router
//: ----------------------------------------------------------------------------
class url_router
{
public:
        // -------------------------------------------------
        // Public methods
        // -------------------------------------------------
        url_router();
        ~url_router();

        // -------------------------------------------------
        // Public members
        // -------------------------------------------------
        int32_t add_route(const std::string &a_route, const void *a_data);
        const void *find_route(const std::string &a_route, url_param_map_t &ao_url_param_map);
        void display(void);

private:
        // -------------------------------------------------
        // Private methods
        // -------------------------------------------------
        // Disallow copy and assign
        url_router& operator=(const url_router &);
        url_router(const url_router &);

        // -------------------------------------------------
        // Private members
        // -------------------------------------------------
        node *m_root_node;

};


} //namespace ns_hlx {

#endif // #ifndef _URL_ROUTER_H
