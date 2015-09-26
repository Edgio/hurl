//: ----------------------------------------------------------------------------
//: \file:    nbq_test.cc
//: \details: TODO
//: \author:  Reed P. Morrison
//: \date:    08/08/2015
//: \notes:   compile with:
//:   g++ -ggdb -Wall -Weffc++ ./nbq_test.cc ./nbq.cc ../ndebug/ndebug.cc -I./ -I../ndebug -o nbq_test
//: ----------------------------------------------------------------------------

//: ----------------------------------------------------------------------------
//: Includes
//: ----------------------------------------------------------------------------
#include "nbq.h"
#include <stdlib.h>
#include <stdio.h>
#include <getopt.h> // For getopt_long
#include <string>

//: ----------------------------------------------------------------------------
//: Constants
//: ----------------------------------------------------------------------------
#define BLOCK_SIZE 256

//: ----------------------------------------------------------------------------
//: \details: nbq test
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
char *create_buf(uint32_t a_size)
{
        char *l_buf = (char *)malloc(a_size);
        for(uint32_t i_c = 0; i_c < a_size; ++i_c)
        {
                l_buf[i_c] = (char)(0xFF&i_c);
        }
        return l_buf;
}

//: ----------------------------------------------------------------------------
//: \details: nbq test
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void nbq_write(ns_hlx::nbq &a_nbq, char *a_buf, uint32_t a_write_size, uint32_t a_write_per)
{
        uint64_t l_write_size = a_write_size;
        printf(": Big Write: Write size: %lu B\n", l_write_size);
        uint64_t l_left = l_write_size;
        uint64_t l_written = 0;
        while(l_left)
        {
                int32_t l_status = 0;
                uint32_t l_write_size = ((a_write_per) > l_left)?l_left:(a_write_per);
                l_status = a_nbq.write(a_buf, l_write_size);
                if(l_status > 0)
                {
                        l_written += l_status;
                        l_left -= l_status;
                }
        }
        printf(": Big Write: Wrote:      %lu B\n", l_written);
        printf(": Big Write: Read Avail: %u B\n", a_nbq.read_avail());
}

//: ----------------------------------------------------------------------------
//: \details: nbq test
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void nbq_read(ns_hlx::nbq &a_nbq, char *a_buf, uint32_t a_read_per)
{
        printf(": Big Write: Read Avail: %u B\n", a_nbq.read_avail());
        char *l_rd_buf = (char *)malloc(a_read_per);
        uint64_t l_read = 0;
        uint32_t l_per_read_size = (a_read_per);
        while(a_nbq.read_avail())
        {
                int32_t l_status = 0;
                NDBG_PRINT(": Try read: %u\n", l_per_read_size);
                l_status = a_nbq.read(l_rd_buf, l_per_read_size);
                if(l_status > 0)
                {
                        l_read += l_status;
                        ns_hlx::mem_display((const uint8_t *)l_rd_buf, l_status);
                }
        }
        printf(": Big Write: Read:       %lu B\n", l_read);
        printf(": Big Write: Read Avail: %u\n", a_nbq.read_avail());
        free(l_rd_buf);
}

//: ----------------------------------------------------------------------------
//: \details: nbq test
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void nbq_test(void)
{
        // Create router
        ns_hlx::nbq l_nbq(BLOCK_SIZE);
        char *l_buf = create_buf(BLOCK_SIZE);

        printf(":-------------------------------------------------------:\n");
        printf(":                      NBQ Test...\n");
        printf(":-------------------------------------------------------:\n");
        nbq_write(l_nbq, l_buf, 331, BLOCK_SIZE);
        l_nbq.b_display_all();
        nbq_read(l_nbq, l_buf, BLOCK_SIZE);
        l_nbq.reset_read();
        nbq_read(l_nbq, l_buf, BLOCK_SIZE);
        l_nbq.reset_write();
        l_nbq.reset_read();
        nbq_read(l_nbq, l_buf, BLOCK_SIZE);
        nbq_write(l_nbq, l_buf, 331, BLOCK_SIZE);
        nbq_read(l_nbq, l_buf, BLOCK_SIZE);
        l_nbq.reset();
        nbq_write(l_nbq, l_buf, 331, BLOCK_SIZE);
        nbq_read(l_nbq, l_buf, BLOCK_SIZE);
        l_nbq.reset();
        nbq_write(l_nbq, l_buf, 331, BLOCK_SIZE);
        nbq_write(l_nbq, l_buf, 331, BLOCK_SIZE);
        l_nbq.b_display_all();
        nbq_read(l_nbq, l_buf, BLOCK_SIZE);

#if 0
        printf(":-------------------------------------------------------:\n");
        printf(":                      Add routes...\n");
        printf(":-------------------------------------------------------:\n");
#endif

        printf(": Done...\n");
}

//: ----------------------------------------------------------------------------
//: \details: Print the version.
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void print_version(FILE* a_stream, int a_exit_code)
{

        // print out the version information
        fprintf(a_stream, "nbq Test.\n");
        exit(a_exit_code);

}

//: ----------------------------------------------------------------------------
//: \details: Print the command line help.
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void print_usage(FILE* a_stream, int a_exit_code)
{
        fprintf(a_stream, "Usage: nbq \n");
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

        // Run test
        nbq_test();

        return 0;
}
