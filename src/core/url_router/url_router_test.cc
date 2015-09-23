//: ----------------------------------------------------------------------------
//: \file:    url_router.h
//: \details: TODO
//: \author:  Reed P. Morrison
//: \date:    08/08/2015
//: ----------------------------------------------------------------------------

//: ----------------------------------------------------------------------------
//: Includes
//: ----------------------------------------------------------------------------
#include "url_router.h"
#include <stdlib.h>
#include <stdio.h>
#include <getopt.h> // For getopt_long
#include <string>

//: ----------------------------------------------------------------------------
//: Macros
//: ----------------------------------------------------------------------------
# define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

//: ----------------------------------------------------------------------------
//: \details: Print the version.
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void print_version(FILE* a_stream, int a_exit_code)
{

        // print out the version information
        fprintf(a_stream, "url_router Test.\n");
        exit(a_exit_code);

}

//: ----------------------------------------------------------------------------
//: \details: Print the command line help.
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void print_usage(FILE* a_stream, int a_exit_code)
{
        fprintf(a_stream, "Usage: url_router_test \n");
        fprintf(a_stream, "Options are:\n");
        fprintf(a_stream, "  -h, --help          Display this help and exit.\n");
        fprintf(a_stream, "  -v, --version       Display the version number and exit.\n");
        fprintf(a_stream, "\n");
        exit(a_exit_code);
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int main(int argc, char** argv)
{
        // -------------------------------------------
        // Get args...
        // -------------------------------------------
        char l_opt;
        std::string l_argument;
        int l_option_index = 0;
        struct option l_long_options[] =
                {
                { "help",         0, 0, 'h' },
                { "version",      0, 0, 'v' },

                // list sentinel
                { 0, 0, 0, 0 }
        };

        while ((l_opt = getopt_long_only(argc, argv, "hv", l_long_options, &l_option_index)) != -1)
        {

                if (optarg)
                {
                        l_argument = std::string(optarg);
                }
                else
                {
                        l_argument.clear();
                }

                switch (l_opt)
                {
                // ---------------------------------------
                // Help
                // ---------------------------------------
                case 'h':
                {
                        print_usage(stdout, 0);
                        break;
                }
                // ---------------------------------------
                // Version
                // ---------------------------------------
                case 'v':
                {
                        print_version(stdout, 0);
                        break;
                }
                // ---------------------------------------
                // What???
                // ---------------------------------------
                case '?':
                {
                        // Required argument was missing
                        // '?' is provided when the 3rd arg to getopt_long does not begin with a ':', and is preceeded
                        // by an automatic error message.
                        fprintf(stdout, "  Exiting.\n");
                        print_usage(stdout, -1);
                        break;
                }
                // ---------------------------------------
                // Huh???
                // ---------------------------------------
                default:
                {
                        fprintf(stdout, "Unrecognized option.\n");
                        print_usage(stdout, -1);
                        break;
                }
                }
        }

        // Create router
        ns_hlx::url_router l_url_router;
        int32_t l_status;

        // -------------------------------------------------
        // Add routes
        // -------------------------------------------------
        printf(":-------------------------------------------------------:\n");
        printf(":                      Add routes...\n");
        printf(":-------------------------------------------------------:\n");
        typedef struct router_ptr_pair_struct {
                const char *m_route;
                void *m_data;
        } route_ptr_pair_t;

        route_ptr_pair_t l_route_ptr_pairs[] =
        {
                {"/monkeys/<monkey_name>/banana/<banana_number>", (void *)1},
                {"/monkeez/<monkey_name>/banana/<banana_number>", (void *)33},
                {"/circus/<circus_number>/hot_dog/<hot_dog_name>", (void *)45},
                {"/cats_are/cool/dog/are/smelly", (void *)2},
                {"/sweet/donuts/*", (void *)1337},
                {"/sweet/cakes/*/cupcakes", (void *)5337}
        };

        for(uint32_t i_route = 0; i_route < ARRAY_SIZE(l_route_ptr_pairs); ++i_route)
        {
                printf(": Adding route: %s\n", l_route_ptr_pairs[i_route].m_route);
                l_status = l_url_router.add_route(l_route_ptr_pairs[i_route].m_route, l_route_ptr_pairs[i_route].m_data);
                if(l_status != 0)
                {
                        printf("Error performing add_route: %s\n", l_route_ptr_pairs[i_route].m_route);
                }
        }

        // -------------------------------------------------
        // Display
        // -------------------------------------------------
        printf(":-------------------------------------------------------:\n");
        printf(":                      Display...\n");
        printf(":-------------------------------------------------------:\n");
        l_url_router.display();

        // -------------------------------------------------
        // Find
        // -------------------------------------------------
        printf(":-------------------------------------------------------:\n");
        printf(":                      Do a find....\n");
        printf(":-------------------------------------------------------:\n");

        route_ptr_pair_t l_find_ptr_pairs[] =
        {
                {"/monkeys/bongo/banana/33", (void *)1},
                {"/cats_are/cool/dog/are/smelly", (void *)2},
                {"/sweet/donuts/pinky", (void *)1337},
                {"/sweet/donuts/cavorting/anteaters", (void *)1337},
                {"/sweet/donuts/trash/pandas/are/super/cool", (void *)1337}
        };

        for(uint32_t i_find = 0; i_find < ARRAY_SIZE(l_find_ptr_pairs); ++i_find)
        {
                ns_hlx::url_param_map_t l_url_param_map;
                const void *l_found = NULL;
                std::string l_request_route;
                l_request_route = l_find_ptr_pairs[i_find].m_route;
                //printf("Finding route: %s\n", l_request_route.c_str());
                l_found = l_url_router.find_route(l_request_route, l_url_param_map);
                if(l_found == NULL)
                {
                        printf(": Route: %s not found... :( \n", l_request_route.c_str());
                }
                else
                {
                        printf(": Route: %s found!\n", l_request_route.c_str());
                        for(ns_hlx::url_param_map_t::const_iterator i_param = l_url_param_map.begin();
                                        i_param != l_url_param_map.end();
                                        ++i_param)
                        {
                                printf(": Parameter: %s: %s\n", i_param->first.c_str(), i_param->second.c_str());

                        }
                        if(l_found != l_find_ptr_pairs[i_find].m_data)
                        {
                                printf(": Error: data %p != %p\n", l_found, l_find_ptr_pairs[i_find].m_data);
                        }
                }
        }
        printf(":-------------------------------------------------------:\n");

        return 0;

}
