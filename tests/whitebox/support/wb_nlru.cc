//: ----------------------------------------------------------------------------
//: Copyright (C) 2016 Verizon.  All Rights Reserved.
//: All Rights Reserved
//:
//: \file:    wb_nlru.cc
//: \details: TODO
//: \author:  Reed P. Morrison
//: \date:    10/25/2015
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
#include "hurl/support/nlru.h"
#include "catch/catch.hpp"
//: ----------------------------------------------------------------------------
//:
//: ----------------------------------------------------------------------------
#define UNUSED(x) ( (void)(x) )
//: ----------------------------------------------------------------------------
//:
//: ----------------------------------------------------------------------------
class animal {
public:
        animal(std::string a_name):
                m_name(a_name)
        {};
        std::string m_name;
private:
        animal& operator=(const animal &);
        animal(const animal &);
};
typedef ns_hurl::nlru <animal> animal_lru_t;
//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int delete_cb(void* a_1, void *a_2)
{
        //printf("%s.%s.%d: DELETE: %p %p\n", __FILE__,__FUNCTION__,__LINE__,o_1, a_2);
        animal *l_animal = (reinterpret_cast<animal *>(a_2));
        delete l_animal;
        uint32_t *l_num_call = (reinterpret_cast<uint32_t *>(a_1));
        ++(*l_num_call);
        //printf("%s.%s.%d: DELETE: name: %s\n", __FILE__,__FUNCTION__,__LINE__,l_animal->m_name.c_str());
        return 0;
}
#if 0
//: ----------------------------------------------------------------------------
//: main
//: compile standalone example
//: export ASAN_SYMBOLIZER_PATH=/usr/bin/llvm-symbolizer-3.3
//: export ASAN_OPTIONS=symbolize=1
//: g++ ./wb_nlru.cc -I../../src/core/support/ -std=c++0x -g3 -fno-omit-frame-pointer -fsanitize=address -o ./wb_nlru -lasan -Wall -Werror -ggdb
//: ./wb_nlru
//: ----------------------------------------------------------------------------
int main(void)
{
        animal_lru_t l_animal_lru(4);
        const char *l_panda_label = "PANDA";
        const char *l_monkey_label = "MONKEY";
        const char *l_bear_label = "BEAR";
        const char *l_snake_label = "SNAKE";


        l_animal_lru.set_delete_cb(delete_cb, (void *)0xDEADDEAD);

        l_animal_lru.insert(l_monkey_label, new animal("Bongo"));
        l_animal_lru.insert(l_monkey_label, new animal("Binky"));
        l_animal_lru.insert(l_panda_label, new animal("Sleepy"));
        l_animal_lru.insert(l_panda_label, new animal("Droopy"));
        l_animal_lru.show();
        l_animal_lru.insert(l_snake_label, new animal("Slippy"));
        l_animal_lru.show();
        l_animal_lru.insert(l_bear_label, new animal("Honey"));
        l_animal_lru.show();

        animal *l_get = NULL;

        const char *l_animal_label = NULL;
        l_animal_label = l_panda_label;
        printf("* TEST: CALLING GET: %p\n", l_get);
        l_get = l_animal_lru.get(l_animal_label);
        printf("* TEST: GET:         %p\n", l_get);
        if(l_get)
        {
                printf("* TEST: GET: %s: name: %s\n", l_animal_label, l_get->m_name.c_str());
        }
        l_animal_lru.show();

        printf("* TEST: SIZE:        %d\n", (int)l_animal_lru.size());

        return 0;
}
#endif
//: ----------------------------------------------------------------------------
//: Tests
//: ----------------------------------------------------------------------------
TEST_CASE( "nlru test", "[nlru]" ) {

        animal_lru_t l_animal_lru(4);
        const char *l_panda_label = "PANDA";
        const char *l_monkey_label = "MONKEY";
        const char *l_bear_label = "BEAR";
        const char *l_snake_label = "SNAKE";
        uint32_t l_num_call = 0;
        l_animal_lru.set_delete_cb(delete_cb, (void *)&l_num_call);

        SECTION("Basic Insertion Test") {
                INFO("Write 5 entries");
                l_animal_lru.insert(l_monkey_label, new animal("Bongo"));
                l_animal_lru.insert(l_monkey_label, new animal("Binky"));
                l_animal_lru.insert(l_panda_label, new animal("Sleepy"));
                l_animal_lru.insert(l_panda_label, new animal("Droopy"));
                l_animal_lru.insert(l_snake_label, new animal("Slippy"));
                REQUIRE(( l_animal_lru.size() == 4 ));
                REQUIRE(( l_num_call == 1 ));
        }
        SECTION("Basic Get Test") {
                INFO("Write 6 entries");
                l_animal_lru.insert(l_monkey_label, new animal("Bongo"));
                l_animal_lru.insert(l_monkey_label, new animal("Binky"));
                l_animal_lru.insert(l_panda_label, new animal("Droopy"));
                l_animal_lru.insert(l_snake_label, new animal("Slippy"));
                l_animal_lru.insert(l_snake_label, new animal("Slimy"));
                l_animal_lru.insert(l_bear_label, new animal("Honey"));
                REQUIRE(( l_animal_lru.size() == 4 ));
                REQUIRE(( l_num_call == 2 ));
                INFO("Get an entry");
                //l_animal_lru.show();
                animal *l_get = NULL;
                l_get = l_animal_lru.get(l_panda_label);
                //l_animal_lru.show();
                REQUIRE( (l_animal_lru.size() == 3) );
                REQUIRE( (l_get != NULL) );
                printf("l_get->m_name: %s\n", l_get->m_name.c_str());
                REQUIRE( (l_get->m_name == "Droopy") );
                if(l_get)
                {
                        delete l_get;
                        l_get = NULL;
                }
        }
        while(l_animal_lru.size())
        {
                l_animal_lru.evict();
        }
}
