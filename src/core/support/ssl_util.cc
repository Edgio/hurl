//: ----------------------------------------------------------------------------
//: Copyright (C) 2014 Verizon.  All Rights Reserved.
//: All Rights Reserved
//:
//: \file:    ssl_util.cc
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

//: ----------------------------------------------------------------------------
//:                          OpenSSL Support
//: ----------------------------------------------------------------------------

//: ----------------------------------------------------------------------------
//: Includes
//: ----------------------------------------------------------------------------
#include "ndebug.h"
#include "ssl_util.h"

#include <map>
#include <algorithm>

#include <pthread.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/bio.h>
#include <openssl/rand.h>
#include <openssl/crypto.h>

//: ----------------------------------------------------------------------------
//:
//: ----------------------------------------------------------------------------
struct CRYPTO_dynlock_value
{
        pthread_mutex_t mutex;
};

namespace ns_hlx {

//: ----------------------------------------------------------------------------
//: Globals
//: ----------------------------------------------------------------------------
static pthread_mutex_t *g_lock_cs;

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
static struct CRYPTO_dynlock_value* dyn_create_function(const char* a_file, int a_line)
{
        struct CRYPTO_dynlock_value* value = new CRYPTO_dynlock_value;
        if (!value) return NULL;

        pthread_mutex_init(&value->mutex, NULL);
        return value;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
static void dyn_lock_function(int a_mode,
                              struct CRYPTO_dynlock_value* a_l,
                              const char* a_file,
                              int a_line)
{
        if (a_mode & CRYPTO_LOCK)
        {
                pthread_mutex_lock(&a_l->mutex);
        }
        else
        {
                pthread_mutex_unlock(&a_l->mutex);
        }
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
static void dyn_destroy_function(struct CRYPTO_dynlock_value* a_l,
                                 const char* a_file,
                                 int a_line)
{
        if(a_l)
        {
                pthread_mutex_destroy(&a_l->mutex);
                free(a_l);
        }
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
static void pthreads_locking_callback(int a_mode, int a_type, const char *a_file, int a_line)
{
#if 0
        fprintf(stdout,"thread=%4d mode=%s lock=%s %s:%d\n",
                        (int)CRYPTO_thread_id(),
                        (mode&CRYPTO_LOCK)?"l":"u",
                                        (type&CRYPTO_READ)?"r":"w",a_file,a_line);
#endif

#if 0
        if (CRYPTO_LOCK_SSL_CERT == type)
                fprintf(stdout,"(t,m,f,l) %ld %d %s %d\n",
                                CRYPTO_thread_id(),
                                a_mode,a_file,a_line);
#endif

        if (a_mode & CRYPTO_LOCK)
        {
                pthread_mutex_lock(&(g_lock_cs[a_type]));
        } else
        {
                pthread_mutex_unlock(&(g_lock_cs[a_type]));

        }

}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
static unsigned long pthreads_thread_id(void)
{
        unsigned long ret;

        ret=(unsigned long)pthread_self();
        return(ret);

}
//: ----------------------------------------------------------------------------
//: \details: OpenSSL can safely be used in multi-threaded applications provided
//:           that at least two callback functions are set, locking_function and
//:           threadid_func this function sets those two callbacks.
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
static void init_ssl_locking(void)
{
        int l_num_locks = CRYPTO_num_locks();
        g_lock_cs = (pthread_mutex_t *)OPENSSL_malloc(l_num_locks * sizeof(pthread_mutex_t));
        //g_lock_cs =(pthread_mutex_t*)malloc(        l_num_locks * sizeof(pthread_mutex_t));
        for (int i=0; i<l_num_locks; ++i)
        {
                pthread_mutex_init(&(g_lock_cs[i]),NULL);
        }
        CRYPTO_set_id_callback(pthreads_thread_id);
        CRYPTO_set_locking_callback(pthreads_locking_callback);
        CRYPTO_set_dynlock_create_callback(dyn_create_function);
        CRYPTO_set_dynlock_lock_callback(dyn_lock_function);
        CRYPTO_set_dynlock_destroy_callback(dyn_destroy_function);
}

//: ----------------------------------------------------------------------------
//: \details: Initialize OpenSSL
//: \return:  ctx on success, NULL on failure
//: \param:   TODO
//: ----------------------------------------------------------------------------
SSL_CTX* ssl_init(const std::string &a_cipher_list,
		  long a_options,
		  const std::string &a_ca_file,
		  const std::string &a_ca_path,
		  bool a_server_flag,
		  const std::string &a_tls_key_file,
		  const std::string &a_tls_crt_file)
{
        SSL_CTX *l_server_ctx;

        // Initialize the OpenSSL library
        SSL_library_init();

        // Bring in and register error messages
        ERR_load_crypto_strings();
        SSL_load_error_strings();

        // TODO Deprecated???
        //SSLeay_add_ssl_algorithms();
        OpenSSL_add_all_algorithms();

        // Set up for thread safety
        init_ssl_locking();

        // We MUST have entropy, or else there's no point to crypto.
        if (!RAND_poll())
        {
                return NULL;
        }

        // TODO Old method???
#if 0
        // Random seed
        if (! RAND_status())
        {
                unsigned char bytes[1024];
                for (size_t i = 0; i < sizeof(bytes); ++i)
                        bytes[i] = random() % 0xff;
                RAND_seed(bytes, sizeof(bytes));
        }
#endif

        // TODO Make configurable
        if(a_server_flag)
        {
                l_server_ctx = SSL_CTX_new(SSLv23_server_method());
        }
        else
        {
                l_server_ctx = SSL_CTX_new(SSLv23_client_method());
        }
        if (l_server_ctx == NULL)
        {
                ERR_print_errors_fp(stderr);
                NDBG_PRINT("SSL_CTX_new Error: %s\n", ERR_error_string(ERR_get_error(), NULL));
                return NULL;
        }

        if (!a_cipher_list.empty())
        {
                if (! SSL_CTX_set_cipher_list(l_server_ctx, a_cipher_list.c_str()))
                {
                        NDBG_PRINT("Error cannot set m_cipher list: %s\n", a_cipher_list.c_str());
                        ERR_print_errors_fp(stderr);
                        //close_connection(con, nowP);
                        return NULL;
                }
        }

        const char *l_ca_file = NULL;
        const char *l_ca_path = NULL;
        if(!a_ca_file.empty())
        {
        	l_ca_file = a_ca_file.c_str();
        }
        else if(!a_ca_path.empty())
        {
        	l_ca_path = a_ca_path.c_str();
        }

        int32_t l_status;
        if(l_ca_file || l_ca_path)
        {
		l_status = SSL_CTX_load_verify_locations(l_server_ctx, l_ca_file, l_ca_path);
		if(1 != l_status)
		{
			ERR_print_errors_fp(stdout);
			NDBG_PRINT("Error performing SSL_CTX_load_verify_locations.  Reason: %s\n",
					ERR_error_string(ERR_get_error(), NULL));
			SSL_CTX_free(l_server_ctx);
			return NULL;
		}

		l_status = SSL_CTX_set_default_verify_paths(l_server_ctx);
		if(1 != l_status)
		{
			ERR_print_errors_fp(stdout);
			NDBG_PRINT("Error performing SSL_CTX_set_default_verify_paths.  Reason: %s\n",
					ERR_error_string(ERR_get_error(), NULL));
			SSL_CTX_free(l_server_ctx);
			return NULL;
		}
        }

        if(a_options)
        {
                SSL_CTX_set_options(l_server_ctx, a_options);
                // TODO Check return
                //long l_results = SSL_CTX_set_options(l_server_ctx, a_options);
                //NDBG_PRINT("Set SSL CTX options: 0x%08lX -set to: 0x%08lX \n", l_results, a_options);

        }

        if(!a_tls_crt_file.empty())
        {
                // set the local certificate from CertFile
                if(SSL_CTX_use_certificate_chain_file(l_server_ctx, a_tls_crt_file.c_str()) <= 0)
                {
                        NDBG_PRINT("Error performing SSL_CTX_use_certificate_file.\n");
                        ERR_print_errors_fp(stdout);
                        return NULL;
                }
        }

        if(!a_tls_key_file.empty())
        {
                // set the private key from KeyFile (may be the same as CertFile) */
                if(SSL_CTX_use_PrivateKey_file(l_server_ctx, a_tls_key_file.c_str(), SSL_FILETYPE_PEM) <= 0)
                {
                        NDBG_PRINT("Error performing SSL_CTX_use_PrivateKey_file.\n");
                        ERR_print_errors_fp(stdout);
                        return NULL;
                }
                // verify private key
                if(!SSL_CTX_check_private_key(l_server_ctx))
                {
                        NDBG_PRINT("Error performing SSL_CTX_check_private_key.\n");
                        fprintf(stdout, "Private key does not match the public certificate\n");
                        return NULL;
                }
        }


        //NDBG_PRINT("SSL_CTX_new success\n");
        return l_server_ctx;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void ssl_kill_locks(void)
{
        CRYPTO_set_id_callback(NULL);
        CRYPTO_set_locking_callback(NULL);
        if(g_lock_cs)
        {
                for (int i=0; i<CRYPTO_num_locks(); ++i)
                {
                        pthread_mutex_destroy(&(g_lock_cs[i]));
                }
        }

        OPENSSL_free(g_lock_cs);
        g_lock_cs = NULL;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
typedef std::map <std::string, long>ssl_options_map_t;
ssl_options_map_t g_ssl_options_map;
int32_t get_ssl_options_str_val(const std::string a_options_str, long &ao_val)
{

        std::string l_options_str = a_options_str;

        if(g_ssl_options_map.empty())
        {
                g_ssl_options_map["SSL_OP_NO_SSLv2"] = SSL_OP_NO_SSLv2;
                g_ssl_options_map["SSL_OP_NO_SSLv3"] = SSL_OP_NO_SSLv3;
                g_ssl_options_map["SSL_OP_NO_TLSv1"] = SSL_OP_NO_TLSv1;
                g_ssl_options_map["SSL_OP_NO_TLSv1_2"] = SSL_OP_NO_TLSv1_2;
                g_ssl_options_map["SSL_OP_NO_TLSv1_1"] = SSL_OP_NO_TLSv1_1;
        }

        // Remove whitespace
        l_options_str.erase( std::remove_if( l_options_str.begin(), l_options_str.end(), ::isspace ), l_options_str.end() );

        ao_val = 0;

        std::string l_token;
        std::string l_delim = "|";
        size_t l_start = 0U;
        size_t l_end = l_options_str.find(l_delim);

        while(l_end != std::string::npos)
        {
                l_token = l_options_str.substr(l_start, l_end - l_start);
                l_start = l_end + l_delim.length();
                l_end = l_options_str.find(l_delim, l_start);
                //NDBG_PRINT("TOKEN: %s\n", l_token.c_str());
                ssl_options_map_t::iterator i_option  = g_ssl_options_map.find(l_token);
                if(i_option == g_ssl_options_map.end())
                {
                        NDBG_PRINT("Error unrecognized ssl option: %s\n", l_token.c_str());
                        return STATUS_ERROR;
                }
                ao_val |= i_option->second;
        };
        l_token = l_options_str.substr(l_start, l_options_str.length() - l_start);
        //NDBG_PRINT("TOKEN: %s\n", l_token.c_str());
        ssl_options_map_t::iterator i_option  = g_ssl_options_map.find(l_token);
        if(i_option == g_ssl_options_map.end())
        {
                NDBG_PRINT("Error unrecognized ssl option: %s\n", l_token.c_str());
                return STATUS_ERROR;
        }
        ao_val |= i_option->second;

        //NDBG_PRINT("ao_val: 0x%08lX\n", ao_val);

        return STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
const char *get_ssl_info_cipher_str(SSL *a_ssl)
{
        if(!a_ssl)
        {
            return NULL;
        }
        return SSL_get_cipher_name(a_ssl);
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t get_ssl_info_protocol_num(SSL *a_ssl)
{
        if(!a_ssl)
        {
            return -1;
        }
        SSL_SESSION *m_ssl_session = SSL_get_session(a_ssl);
        if(!m_ssl_session)
        {
                return -1;
        }
        return m_ssl_session->ssl_version;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
const char *get_ssl_info_protocol_str(int32_t a_version)
{
        switch(a_version)
        {
        case SSL2_VERSION:
        {
                return "SSLv2";
        }
        case SSL3_VERSION:
        {
                return "SSLv3";
        }
        case TLS1_2_VERSION:
        {
                return "TLSv1.2";
        }
        case TLS1_1_VERSION:
        {
                return "TLSv1.1";
        }
        case TLS1_VERSION:
        {
                return "TLSv1";
        }
        case DTLS1_VERSION:
        {
                return "DTLSv1";
        }
        case DTLS1_BAD_VER:
        {
                return "DTLSv1-bad";
        }
        default:
        {
                return "unknown";
        }
        }
        return NULL;
}

} //namespace ns_hlx {

