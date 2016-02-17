//: ----------------------------------------------------------------------------
//: Copyright (C) 2015 Verizon.  All Rights Reserved.
//: All Rights Reserved
//:
//: \file:    wb_obj_pool.cc
//: \details: TODO
//: \author:  Reed P. Morrison
//: \date:    02/17/2016
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
//: Includes
//: ----------------------------------------------------------------------------
#include <string>
#include "obj_pool.h"
#include "catch/catch.hpp"

//: ----------------------------------------------------------------------------
//:
//: ----------------------------------------------------------------------------
#define UNUSED(x) ( (void)(x) )

//: ----------------------------------------------------------------------------
//:
//: ----------------------------------------------------------------------------
int32_t g_num_delete;

//: ----------------------------------------------------------------------------
//:
//: ----------------------------------------------------------------------------
class animal {
public:
        animal(std::string a_name):
                m_name(a_name),
                m_idx(0)
        {};
        ~animal()
        {
                ++g_num_delete;
        }
        std::string m_name;
        uint32_t get_idx(void) {return m_idx;}
        void set_idx(uint32_t a_id) {m_idx = a_id;}
private:
        animal& operator=(const animal &);
        animal(const animal &);
        uint32_t m_idx;
};
typedef ns_hlx::obj_pool <animal> animal_pool_t;

//: ----------------------------------------------------------------------------
//: Tests
//: ----------------------------------------------------------------------------
TEST_CASE( "obj pool test", "[obj_pool]" ) {

        SECTION("Basic Insertion Test") {
                animal_pool_t *l_animal_pool = new animal_pool_t();
                g_num_delete = 0;
                animal *l_a0 = new animal("Bongo");
                animal *l_a1 = new animal("Binky");
                animal *l_a2 = new animal("Sleepy");
                animal *l_a3 = new animal("Droopy");
                animal *l_a4 = new animal("Slippy");
                REQUIRE(( l_animal_pool->free_size() == 0 ));
                REQUIRE(( l_animal_pool->used_size() == 0 ));
                l_animal_pool->add(l_a0);
                l_animal_pool->add(l_a1);
                l_animal_pool->add(l_a2);
                l_animal_pool->add(l_a3);
                l_animal_pool->add(l_a4);
                REQUIRE(( l_animal_pool->free_size() == 0 ));
                REQUIRE(( l_animal_pool->used_size() == 5 ));
                REQUIRE(( g_num_delete == 0 ));
                l_animal_pool->release(l_a0);
                REQUIRE(( l_animal_pool->free_size() == 1 ));
                REQUIRE(( l_animal_pool->used_size() == 4 ));
                animal *l_af = l_animal_pool->get_free();
                REQUIRE((l_af != NULL));
                REQUIRE((l_af->m_name == "Bongo"));
                l_animal_pool->release(l_a1);
                l_animal_pool->shrink();
                REQUIRE(( g_num_delete == 1 ));
                l_animal_pool->release(l_a2);
                l_animal_pool->release(l_a3);
                REQUIRE(( l_animal_pool->free_size() == 2 ));
                REQUIRE(( l_animal_pool->used_size() == 2 ));
                l_animal_pool->shrink();
                REQUIRE(( g_num_delete == 3 ));
                l_animal_pool->add(NULL);
                l_animal_pool->release(NULL);
                l_animal_pool->release(l_a4);
                REQUIRE(( l_animal_pool->free_size() == 1 ));
                REQUIRE(( l_animal_pool->used_size() == 1 ));
                delete l_animal_pool;
                l_animal_pool = NULL;
                //NDBG_PRINT("g_num_delete: %d\n", g_num_delete);
                REQUIRE(( g_num_delete == 5 ));
        }
}
