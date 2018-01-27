//: ----------------------------------------------------------------------------
//: Copyright (C) 2016 Verizon.  All Rights Reserved.
//: All Rights Reserved
//:
//: \file:    uri.cc
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

// Uri encode and decode.
// RFC1630, RFC1738, RFC2396

//: ----------------------------------------------------------------------------
//: Includes
//: ----------------------------------------------------------------------------
#include "hurl/support/uri.h"
#include <string>
#include <assert.h>
#include <stdint.h>

namespace ns_hurl
{
//: ----------------------------------------------------------------------------
//: Globals
//: ----------------------------------------------------------------------------
const int8_t G_HEX2DEC[256] =
{
        /*       0  1  2  3   4  5  6  7   8  9  A  B   C  D  E  F */
        /* 0 */ -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
        /* 1 */ -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
        /* 2 */ -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
        /* 3 */  0, 1, 2, 3,  4, 5, 6, 7,  8, 9,-1,-1, -1,-1,-1,-1,

        /* 4 */ -1,10,11,12, 13,14,15,-1, -1,-1,-1,-1, -1,-1,-1,-1,
        /* 5 */ -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
        /* 6 */ -1,10,11,12, 13,14,15,-1, -1,-1,-1,-1, -1,-1,-1,-1,
        /* 7 */ -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,

        /* 8 */ -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
        /* 9 */ -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
        /* A */ -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
        /* B */ -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,

        /* C */ -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
        /* D */ -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
        /* E */ -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
        /* F */ -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1
};


// Only alphanum is safe.
const int8_t G_SAFE[256] =
{
        /*      0 1 2 3  4 5 6 7  8 9 A B  C D E F */
        /* 0 */ 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
        /* 1 */ 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
        /* 2 */ 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
        /* 3 */ 1,1,1,1, 1,1,1,1, 1,1,0,0, 0,0,0,0,

        /* 4 */ 0,1,1,1, 1,1,1,1, 1,1,1,1, 1,1,1,1,
        /* 5 */ 1,1,1,1, 1,1,1,1, 1,1,1,0, 0,0,0,0,
        /* 6 */ 0,1,1,1, 1,1,1,1, 1,1,1,1, 1,1,1,1,
        /* 7 */ 1,1,1,1, 1,1,1,1, 1,1,1,0, 0,0,0,0,

        /* 8 */ 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
        /* 9 */ 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
        /* A */ 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
        /* B */ 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,

        /* C */ 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
        /* D */ 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
        /* E */ 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
        /* F */ 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0
};


//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
std::string uri_decode(const std::string & a_src)
{
        // Note from RFC1630:  "Sequences which start with a percent sign
        // but are not followed by two hexadecimal characters (0-9, A-F) are reserved
        // for future extension"

        const unsigned char * pSrc = (const unsigned char *) a_src.c_str();
        const int SRC_LEN = a_src.length();
        const unsigned char * const SRC_END = pSrc + SRC_LEN;
        const unsigned char * const SRC_LAST_DEC = SRC_END - 2; // last decodable '%'

        char * const pStart = new char[SRC_LEN];
        char * pEnd = pStart;

        while (pSrc < SRC_LAST_DEC)
        {
                if(*pSrc == '+')
                {
                        *pEnd = ' ';
                }
                else
                {
                        if(*pSrc == '%')
                        {
                                char dec1;
                                char dec2;
                                if ((-1 != (dec1 = (char)G_HEX2DEC[*(pSrc + 1)])) &&
                                    (-1 != (dec2 = (char)G_HEX2DEC[*(pSrc + 2)])))
                                {
                                        *pEnd = (dec1 << 4) + dec2;
                                        ++pEnd;
                                        pSrc += 3;
                                        continue;
                                }
                        }
                        *pEnd = *pSrc;
                }
                ++pEnd;
                ++pSrc;
        }

        // the last 2- chars
        while (pSrc < SRC_END)
        {
                *pEnd = *pSrc;
                ++pEnd;
                ++pSrc;
        }

        std::string sResult(pStart, pEnd);
        delete[] pStart;
        return sResult;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
std::string uri_encode(const std::string & a_src)
{
        const char DEC2HEX[16 + 1] = "0123456789ABCDEF";
        const unsigned char * pSrc = (const unsigned char *) a_src.c_str();
        const int SRC_LEN = a_src.length();
        unsigned char * const pStart = new unsigned char[SRC_LEN * 3];
        unsigned char * pEnd = pStart;
        const unsigned char * const SRC_END = pSrc + SRC_LEN;

        for (; pSrc < SRC_END; ++pSrc)
        {
                if (G_SAFE[*pSrc])
                        *pEnd++ = *pSrc;
                else
                {
                        // escape this char
                        *pEnd++ = '%';
                        *pEnd++ = DEC2HEX[*pSrc >> 4];
                        *pEnd++ = DEC2HEX[*pSrc & 0x0F];
                }
        }

        std::string sResult((char *) pStart, (char *) pEnd);
        delete[] pStart;
        return sResult;
}

}

#if 0
//////////////////////////////////////////////////////////////
// Test codes start
#ifndef NDEBUG

class UriCodecTest
{
public:
        UriCodecTest();
};

static UriCodecTest test; // auto run the test

#include <stdlib.h>
#include <time.h>

UriCodecTest::UriCodecTest()
{
        assert(uri_encode("ABC") == "ABC");

        const std::string ORG("\0\1\2", 3);
        const std::string ENC("%00%01%02");
        assert(uri_encode(ORG) == ENC);
        assert(uri_decode(ENC) == ORG);

        assert(uri_encode("\xFF") == "%FF");
        assert(uri_decode("%FF") == "\xFF");
        assert(uri_decode("%ff") == "\xFF");

        // unsafe chars test, RFC1738
        const std::string UNSAFE(" <>#{}|\\^~[]`");
        std::string sUnsafeEnc = uri_encode(UNSAFE);
        assert(std::string::npos == sUnsafeEnc.find_first_of(UNSAFE));
        assert(uri_decode(sUnsafeEnc) == UNSAFE);

        // random test
        const int MAX_LEN = 128;
        char a[MAX_LEN];
        srand((unsigned) time(NULL));
        for (int i = 0; i < 100; i++)
        {
                for (int j = 0; j < MAX_LEN; j++)
                        a[j] = rand() % (1 << 8);
                int nLen = rand() % MAX_LEN;
                std::string sOrg(a, nLen);
                std::string sEnc = uri_encode(sOrg);
                assert(sOrg == uri_decode(sEnc));
        }
}
#endif  // !NDEBUG
// Test codes end
//////////////////////////////////////////////////////////////
#endif
