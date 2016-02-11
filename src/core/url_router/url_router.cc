//: ----------------------------------------------------------------------------
//: \file:    url_router.cc
//: \details: TODO
//: \author:  Reed P. Morrison
//: \date:    08/08/2015
//: ----------------------------------------------------------------------------

//: ----------------------------------------------------------------------------
//: Includes
//: ----------------------------------------------------------------------------
#include "hlx/url_router.h"

// For fixed width types...
#include <stdint.h>
#include <string.h>
#include <list>
#include <stdio.h>

//: ----------------------------------------------------------------------------
//: Macros
//: ----------------------------------------------------------------------------
#ifndef NDBG_PRINT
#define NDBG_PRINT(...) \
        do { \
                fprintf(stdout, "%s:%s.%d: ", __FILE__, __FUNCTION__, __LINE__); \
                fprintf(stdout, __VA_ARGS__);               \
                fflush(stdout); \
        } while(0)
#endif

#ifndef DISALLOW_ASSIGN
#define DISALLOW_ASSIGN(class_name)\
    class_name& operator=(const class_name &);
#endif

#ifndef DISALLOW_COPY
#define DISALLOW_COPY(class_name)\
    class_name(const class_name &);
#endif

#ifndef DISALLOW_COPY_AND_ASSIGN
#define DISALLOW_COPY_AND_ASSIGN(class_name)\
    DISALLOW_COPY(class_name)\
    DISALLOW_ASSIGN(class_name)
#endif

//: ----------------------------------------------------------------------------
//: Constants
//: ----------------------------------------------------------------------------
//: ----------------------------------------------------------------------------
//: ANSI Color Code Strings
//:
//: Taken from:
//: http://pueblo.sourceforge.net/doc/manual/ansi_color_codes.html
//: ----------------------------------------------------------------------------
#define ANSI_COLOR_OFF          "\033[0m"
#define ANSI_COLOR_FG_BLACK     "\033[01;30m"
#define ANSI_COLOR_FG_RED       "\033[01;31m"
#define ANSI_COLOR_FG_GREEN     "\033[01;32m"
#define ANSI_COLOR_FG_YELLOW    "\033[01;33m"
#define ANSI_COLOR_FG_BLUE      "\033[01;34m"
#define ANSI_COLOR_FG_MAGENTA   "\033[01;35m"
#define ANSI_COLOR_FG_CYAN      "\033[01;36m"
#define ANSI_COLOR_FG_WHITE     "\033[01;37m"
#define ANSI_COLOR_FG_DEFAULT   "\033[01;39m"
#define ANSI_COLOR_BG_BLACK     "\033[01;40m"
#define ANSI_COLOR_BG_RED       "\033[01;41m"
#define ANSI_COLOR_BG_GREEN     "\033[01;42m"
#define ANSI_COLOR_BG_YELLOW    "\033[01;43m"
#define ANSI_COLOR_BG_BLUE      "\033[01;44m"
#define ANSI_COLOR_BG_MAGENTA   "\033[01;45m"
#define ANSI_COLOR_BG_CYAN      "\033[01;46m"
#define ANSI_COLOR_BG_WHITE     "\033[01;47m"
#define ANSI_COLOR_BG_DEFAULT   "\033[01;49m"

namespace ns_hlx {

//: ----------------------------------------------------------------------------
//: fwd decl's
//: ----------------------------------------------------------------------------
class node;
class edge;

//: ----------------------------------------------------------------------------
//: Types
//: ----------------------------------------------------------------------------
typedef enum {
        PART_TYPE_STRING = 0,
        PART_TYPE_PARAMETER,
        PART_TYPE_TERMINAL,
        PART_TYPE_NONE
} part_type_t;

typedef struct part_struct {
        part_type_t m_type;
        std::string m_str;

        part_struct():
                m_type(PART_TYPE_NONE),
                m_str()
        {};

} part_t;

typedef std::list <part_t> pattern_t;

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
static inline std::string pattern_str(const pattern_t &a_pattern)
{
        std::string l_retval;

        for(pattern_t::const_iterator i_part = a_pattern.begin();
            i_part != a_pattern.end();
            ++i_part)
        {
                if(i_part->m_type == PART_TYPE_PARAMETER)
                {
                        l_retval += "<";
                        l_retval += i_part->m_str;
                        l_retval += ">";
                }
                else if(i_part->m_type == PART_TYPE_STRING)
                {
                        l_retval += i_part->m_str;
                }
                else if(i_part->m_type == PART_TYPE_TERMINAL)
                {
                        l_retval += "*";
                }
        }

        return l_retval;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
static inline uint32_t pattern_len(const pattern_t &a_pattern)
{
        uint32_t l_retval = 0;

        for(pattern_t::const_iterator i_part = a_pattern.begin();
            i_part != a_pattern.end();
            ++i_part)
        {
                if(i_part->m_type == PART_TYPE_PARAMETER)
                {
                        l_retval += 2 + i_part->m_str.length();
                }
                else if(i_part->m_type == PART_TYPE_STRING)
                {
                        l_retval += i_part->m_str.length();
                }
                else if(i_part->m_type == PART_TYPE_TERMINAL)
                {
                        l_retval += 1;
                }
        }

        return l_retval;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
static inline int32_t string_diff(const std::string &a1, const std::string &a2)
{
        const char *l_d1 = a1.data();
        const char *l_d2 = a2.data();
        uint32_t l_d2_len = a2.length();
        const char *l_o = l_d1;
        while ( (*l_d1 == *l_d2) &&
                (l_d2_len-- > 0) )
        {
                l_d1++;
                l_d2++;
        }
        return l_d1 - l_o;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
static inline int32_t pattern_diff(const pattern_t &a1, const pattern_t &a2)
{
        pattern_t::const_iterator i_a1;
        pattern_t::const_iterator i_a2;
        int32_t l_len = 0;
        for(i_a1 = a1.begin(), i_a2 = a2.begin();
            i_a1 != a1.end() && i_a2 != a2.end();
            ++i_a1, ++i_a2)
        {
                //NDBG_PRINT("Checking: %s :: %s\n",
                //           i_a1->m_str.c_str(), i_a2->m_str.c_str());
                int32_t l_diff = string_diff(i_a1->m_str, i_a2->m_str);
                //NDBG_PRINT("Diff:     %d\n", l_diff);
                if((l_diff > 0) &&
                   ((uint32_t)l_diff) != i_a1->m_str.length())
                {
                        l_len += l_diff;
                        break;
                }
                else
                {
                        if(!l_diff)
                        {
                                break;
                        }

                        l_len += l_diff;
                        if(i_a1->m_type != i_a2->m_type)
                        {
                                //NDBG_PRINT("Error type mismatch for pattern: %s and %s\n",
                                //                pattern_str(a1).c_str(),
                                //                pattern_str(a2).c_str());
                                return -1;
                        }
                        if(i_a1->m_type == PART_TYPE_PARAMETER)
                        {
                                l_len += 2;
                        }
                }
        }

        return l_len;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
static inline pattern_t pattern_sub(const pattern_t &a_pattern,
                                    uint32_t a_offset,
                                    uint32_t a_len)
{
        //NDBG_PRINT("%sSUB%s: a_pattern: %s\n",
        //           ANSI_COLOR_BG_RED, ANSI_COLOR_OFF,
        //           pattern_str(a_pattern).c_str());
        //NDBG_PRINT("%sSUB%s: a_offset:  %u\n",
        //           ANSI_COLOR_BG_RED, ANSI_COLOR_OFF, a_offset);
        //NDBG_PRINT("%sSUB%s: a_len:     %u\n",
        //           ANSI_COLOR_BG_RED, ANSI_COLOR_OFF, a_len);
        pattern_t l_retval;
        bool l_found = (a_offset == 0);
        uint32_t l_off = 0;
        uint32_t l_len = 0;
        part_t l_part;
        for(pattern_t::const_iterator i_part = a_pattern.begin();
            i_part != a_pattern.end();
            ++i_part)
        {
                if(i_part->m_type == PART_TYPE_PARAMETER)
                {
                        l_off += 1;
                        if(l_found)
                        {
                                l_len += 1;
                        }
                }

                for(uint32_t i_char = 0;
                    (i_char < i_part->m_str.length()) && (l_len < a_len);
                    ++i_char, ++l_off)
                {
                        if(!l_found && (l_off >= a_offset))
                        {
                                l_found = true;
                        }

                        if(l_found)
                        {
                                l_part.m_str += i_part->m_str[i_char];
                                l_len += 1;
                        }
                }

                if(i_part->m_type == PART_TYPE_PARAMETER)
                {
                        l_off += 1;
                        if(l_found)
                        {
                                l_len += 1;
                        }
                }

                if(l_found)
                {
                        l_part.m_type = i_part->m_type;
                        l_retval.push_back(l_part);
                        l_part.m_str.clear();
                        l_part.m_type = PART_TYPE_NONE;
                }

                if(l_len >= a_len)
                {
                        break;
                }
        }
        //NDBG_PRINT("%sSUB%s: l_retval:  %s\n",
        //           ANSI_COLOR_BG_RED, ANSI_COLOR_OFF,
        //           pattern_str(l_retval).c_str());
        return l_retval;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
static inline bool pattern_match(const pattern_t &a1, const pattern_t &a2)
{
        pattern_t::const_iterator i_a1;
        pattern_t::const_iterator i_a2;
        for(i_a1 = a1.begin(), i_a2 = a2.begin();
            i_a1 != a1.end() && i_a2 != a2.end();
            ++i_a1, ++i_a2)
        {
                if(i_a1->m_type != i_a2->m_type)
                {
                        return false;
                }
                if(i_a1->m_str != i_a2->m_str)
                {
                        return false;
                }
        }
        if(i_a1 != a1.end() || i_a2 != a2.end())
        {
                return false;
        }

        return true;
}

//: ----------------------------------------------------------------------------
//: node
//: ----------------------------------------------------------------------------
class node
{
public:
        // -------------------------------------------------
        // Public methods
        // -------------------------------------------------
        node();
        ~node();

        node *insert_route(const pattern_t &a_pattern, const void *a_data);
        int32_t find_longest_common_prefix(const pattern_t &a_pattern,
                                   uint32_t &ao_prefix_len,
                                   edge **ao_common_edge);
        edge *connect(node *a_node, const pattern_t &a_pattern, bool a_dupe);
        edge *find_edge(node *a_node, const pattern_t &a_pattern);
        edge *append_edge(node *a_node, const pattern_t &a_pattern);
        void append_edge(edge *a_edge);
        const void *find_route(const std::string &a_route, url_pmap_t &ao_url_pmap);
        int32_t collision_test(pattern_t a_pattern);
        void display_trie(uint32_t a_indent);

        // -------------------------------------------------
        // Public members
        // -------------------------------------------------
        uint32_t m_endpoint;
        edge_list_t m_edge_list;
        const void *m_data;

private:
        // -------------------------------------------------
        // Private methods
        // -------------------------------------------------
        DISALLOW_COPY_AND_ASSIGN(node)

        // -------------------------------------------------
        // Private members
        // -------------------------------------------------

};

//: ----------------------------------------------------------------------------
//: edge
//: ----------------------------------------------------------------------------
class edge
{
public:
        // -------------------------------------------------
        // Public methods
        // -------------------------------------------------
        edge(const pattern_t &a_pattern, node *a_child);
        ~edge();
        node *branch(uint32_t a_offset);
        bool match_route(const std::string &a_route,
                         url_pmap_t &ao_url_pmap,
                         std::string &ao_suffix);

        // -------------------------------------------------
        // Public members
        // -------------------------------------------------
        pattern_t m_pattern;
        node *m_child;           // practically, this is never null, as that'd be an edge with no endpoint

private:
        // -------------------------------------------------
        // Private methods
        // -------------------------------------------------
        DISALLOW_COPY_AND_ASSIGN(edge)

        // -------------------------------------------------
        // Private members
        // -------------------------------------------------

};

//: ----------------------------------------------------------------------------
//: \details: branch the edge pattern at "a_pattern_offset" offset,
//:           insert a dummy child between the edges.
//:           ex:
//:           A -> [prefix..suffix] -> B
//:           A -> [prefix] -> B -> [suffix] -> New Child (Copy Data, Edges from B)
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
node *edge::branch(uint32_t a_offset)
{
        //NDBG_PRINT("%sBRANCH%s: m_pattern:        %s\n",
        //           ANSI_COLOR_FG_YELLOW, ANSI_COLOR_OFF,
        //           pattern_str(m_pattern).c_str());
        //NDBG_PRINT("%sBRANCH%s: a_pattern_offset: %u\n",
        //           ANSI_COLOR_FG_YELLOW, ANSI_COLOR_OFF,
        //           a_offset);
        // Create new node starting at pattern offset
        node *l_new_child = new node();

        // Create new edge to new node
        edge *l_new_edge = new edge(pattern_sub(m_pattern,
                                                a_offset,
                                                pattern_len(m_pattern)),
                                    l_new_child);
        // Migrate child edges to new node
        for(edge_list_t::const_iterator i_edge = m_child->m_edge_list.begin();
            i_edge != m_child->m_edge_list.end();
            ++i_edge)
        {
                l_new_child->append_edge(*i_edge);
        }
        m_child->m_edge_list.clear();
        // Migrate the endpoint
        l_new_child->m_endpoint = m_child->m_endpoint;
        m_child->m_endpoint = 0;
        // Migrate the data
        l_new_child->m_data = m_child->m_data;
        m_child->m_data = NULL;
        // Append edge to old child
        m_child->append_edge(l_new_edge);
        // Truncate original edge pattern
        m_pattern = pattern_sub(m_pattern, 0, a_offset);
        //NDBG_PRINT("%sBRANCH%s: m_pattern:        %s\n",
        //           ANSI_COLOR_BG_YELLOW, ANSI_COLOR_OFF,
        //           pattern_str(m_pattern).c_str());
        return l_new_child;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
bool edge::match_route(const std::string &a_route,
                       url_pmap_t &ao_url_pmap,
                       std::string &ao_suffix)
{

        //NDBG_PRINT("%sMATCH_ROUTE%s: route: %s edge: %s\n",
        //                ANSI_COLOR_FG_CYAN, ANSI_COLOR_OFF,
        //                pattern_str(m_pattern).c_str(), a_route.c_str());

        // Match parameters if has one
        const char *l_route_ptr = a_route.c_str();
        uint32_t l_len = 0;
        for(pattern_t::const_iterator i_part = m_pattern.begin();
            i_part != m_pattern.end();
            ++i_part)
        {

                //NDBG_PRINT("%sCHECK%s:       %s %s\n",
                //           ANSI_COLOR_BG_CYAN, ANSI_COLOR_OFF,
                //           i_part->m_str.c_str(), l_route_ptr);

                if(i_part->m_type == PART_TYPE_PARAMETER)
                {
                        // Scan ahead to slash and store results
                        uint32_t i_char = 0;
                        while((l_route_ptr[i_char] != '/') && (l_len < a_route.length()))
                        {
                                ++l_len;
                                //NDBG_PRINT("Scan: %c l_len: %d\n", l_route_ptr[i_char], l_len);
                                ++i_char;
                        }
                        std::string l_val = std::string(l_route_ptr, i_char);
                        //NDBG_PRINT("%sSETKX%s:       %s %s\n",
                        //           ANSI_COLOR_BG_RED, ANSI_COLOR_OFF,
                        //           i_part->m_str.c_str(), l_val.c_str());
                        ao_url_pmap[i_part->m_str] = l_val;
                        l_route_ptr += i_char;
                }
                else if(i_part->m_type == PART_TYPE_STRING)
                {

                        //NDBG_PRINT("COMPARE: %s %.*s\n",
                        //           i_part->m_str.c_str(),
                        //           (int)i_part->m_str.length(),
                        //           l_route_ptr);

                        if(strncmp(i_part->m_str.c_str(),
                                   l_route_ptr,
                                   i_part->m_str.length()) != 0)
                        {
                                //NDBG_PRINT("COMPARE: FAIL!!!!\n");
                                return false;
                        }
                        else
                        {
                                l_route_ptr += i_part->m_str.length();
                                l_len += i_part->m_str.length();

                                //NDBG_PRINT("Stri: %s l_len: %d\n",
                                //           i_part->m_str.c_str(), l_len);

                        }
                }
                else if(i_part->m_type == PART_TYPE_TERMINAL)
                {

                        // capture everything after the terminal in a special parameter map '*'
                        ao_url_pmap["*"] = std::string(l_route_ptr);
                        ao_suffix.clear();
                        return true;
                }
        }

        //NDBG_PRINT("LEN: %u -- a_route.length(): %d\n", l_len, (int)a_route.length());

        if(l_len == a_route.length())
        {
                ao_suffix.clear();
        }
        else
        {
                ao_suffix.assign(l_route_ptr);
        }

        //NDBG_PRINT("%sMATCH!!!!%s: ao_suffix: %s\n",
        //           ANSI_COLOR_BG_GREEN, ANSI_COLOR_OFF, ao_suffix.c_str());

        return true;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
edge::edge(const pattern_t &a_pattern, node *a_child):
        m_pattern(a_pattern),
        m_child(a_child)
{
        // TODO Compile???
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
edge::~edge()
{

}

//: ----------------------------------------------------------------------------
//: \details: Find existing edge with specified pattern (including parameter)
//:           if "a_route" parameter, compare with specified pattern.
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
edge *node::find_edge(node *a_node, const pattern_t &a_pattern)
{
        for(edge_list_t::const_iterator i_edge = m_edge_list.begin();
            i_edge != m_edge_list.end();
            ++i_edge)
        {
                // there is a case: "{foo}" vs "{foo:xxx}",
                // we should return the match result: full-match or partial-match
                if(pattern_match((*i_edge)->m_pattern, a_pattern))
                {
                        return *i_edge;
                }
        }

        return NULL;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
edge *node::append_edge(node *a_node, const pattern_t &a_pattern)
{
        // Create new edge...
        edge *l_edge = new edge(a_pattern, a_node);

        // parameters???
        //e->has_parameter = r3_path_contains_parameter_char(e->pattern);

        // opcodes???
        //e->opcode = 0;

        // Append...
        m_edge_list.push_back(l_edge);

        return l_edge;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void node::append_edge(edge *a_edge)
{
        m_edge_list.push_back(a_edge);
}

//: ----------------------------------------------------------------------------
//: \details: Connect two node objects, and create an edge object between them.
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
edge *node::connect(node *a_node, const pattern_t &a_pattern, bool a_dupe)
{
        edge *l_edge = NULL;

        l_edge = find_edge(a_node, a_pattern);
        if (l_edge)
        {
                return l_edge;
        }

        l_edge = append_edge(a_node, a_pattern);
        if (!l_edge)
        {
                //printf("Error performing append_edge\n");
                return NULL;
        }

        return l_edge;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
static int32_t convert_path_to_pattern(std::string &a_route, pattern_t &ao_pattern)
{

        std::string l_path = a_route;

        // Enforce path starts with '/'
        if(l_path[0] != '/')
        {
                l_path = "/" + l_path;
        }

        // Enforce path end w no /
        if(l_path.length() > 1 && (l_path[l_path.length() - 1] == '/'))
        {
                l_path = l_path.substr(0, l_path.length() - 1);
        }

        // Check all parameters start after '/' ie '/<'
        // Check all parameters end with '/' ie '>/'  except at end of path
        part_t l_part;
        l_part.m_type = PART_TYPE_STRING;
        for(uint32_t i_char = 0; i_char < l_path.length(); ++i_char)
        {
                // Check for parameter beginning
                if(l_path[i_char] == '<')
                {
                        // Start of parameter...

                        // Already in parameter???
                        if(l_part.m_type == PART_TYPE_PARAMETER)
                        {
                                //NDBG_PRINT("Error cannot start a parameter within existing parameter.\n");
                                return -1;
                        }

                        // Preceded by '/'
                        if(l_path[i_char - 1] != '/')
                        {
                                //NDBG_PRINT("Error parameters must be preceded by '\'.\n");
                                return -1;
                        }

                        // Add last part to pattern
                        ao_pattern.push_back(l_part);

                        // Set up parameter
                        l_part.m_type = PART_TYPE_PARAMETER;
                        l_part.m_str.clear();

                }
                else if(l_path[i_char] == '>')
                {
                        if(l_part.m_type != PART_TYPE_PARAMETER)
                        {
                                //NDBG_PRINT("Error cannot end a parameter without start char '>'.\n");
                                return -1;
                        }

                        if(i_char != (l_path.length() - 1))
                        {
                                if(l_path[i_char + 1] != '/')
                                {
                                        //NDBG_PRINT("Error parameters must end with '\'.\n");
                                        return -1;
                                }
                        }

                        // Add last part to pattern
                        ao_pattern.push_back(l_part);

                        // Set up string
                        l_part.m_type = PART_TYPE_STRING;
                        l_part.m_str.clear();

                }
                else if(l_path[i_char] == '*')
                {
                        // Preceded by '/'
                        //if(l_path[i_char - 1] != '/')
                        //{
                        //        //NDBG_PRINT("Error terminals must be preceded by '\'.\n");
                        //        return -1;
                        //}
                        // Add last part to pattern
                        ao_pattern.push_back(l_part);

                        // Set up parameter
                        l_part.m_type = PART_TYPE_TERMINAL;
                        l_part.m_str.clear();

                        // Add last part to pattern
                        ao_pattern.push_back(l_part);

                        // We outtie after terminal...
                        goto convert_path_to_pattern_done;

                }
                else
                {
                        l_part.m_str += l_path[i_char];
                }

        }


convert_path_to_pattern_done:

        // Still in parameter???
        if(l_part.m_type == PART_TYPE_PARAMETER)
        {
                //NDBG_PRINT("Error parameter not terminated.\n");
                return -1;
        }

        if(!l_part.m_str.empty())
        {
                ao_pattern.push_back(l_part);
        }

        return 0;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
node* node::insert_route(const pattern_t &a_pattern, const void *a_data)
{
        //NDBG_PRINT("%s: ---------------------------------------------------------- :%s\n",
        //           ANSI_COLOR_FG_GREEN, ANSI_COLOR_OFF);
        //NDBG_PRINT("%s: a_path%s: %s\n",
        //           ANSI_COLOR_FG_GREEN, ANSI_COLOR_OFF, (pattern_str(a_pattern)).c_str());
        //NDBG_PRINT("%s: ---------------------------------------------------------- :%s\n",
        //           ANSI_COLOR_FG_GREEN, ANSI_COLOR_OFF);
        // Test for collisions
        if(collision_test(a_pattern) != 0)
        {
                return NULL;
        }
        //NDBG_PRINT("%s: PASSED %s\n",
        //           ANSI_COLOR_BG_GREEN, ANSI_COLOR_OFF);
        int32_t l_status;

        // length of common prefix
        uint32_t l_prefix_len = 0;

        // common edge
        edge *l_common_edge = NULL;

        l_status = find_longest_common_prefix(a_pattern, l_prefix_len, &l_common_edge);
        if(l_status != 0)
        {
                //printf("Error performing find_longest_common_prefix\n");
                return NULL;
        }
        //NDBG_PRINT("l_prefix_len:                      %d\n", l_prefix_len);
        //if(l_common_edge)
        //{
        //        NDBG_PRINT("l_common_edge->m_pattern.length(): %d\n",
        //                   (int)(pattern_len(l_common_edge->m_pattern)));
        //        NDBG_PRINT("l_common_edge->m_pattern:          %s\n",
        //                   (pattern_str(l_common_edge->m_pattern)).c_str());
        //}

        // common prefix not found, insert a new edge for this pattern
        if(l_prefix_len == 0)
        {
                //NDBG_PRINT("%s: PREFIX_LEN == 0 :%s\n", ANSI_COLOR_BG_GREEN, ANSI_COLOR_OFF);
                node * l_child_node = new node();
                ++(l_child_node->m_endpoint);

                // Set data
                if(a_data)
                {
                        l_child_node->m_data = a_data;
                }

                edge *l_new_edge = NULL;
                l_new_edge = connect(l_child_node, a_pattern, true);
                if(!l_new_edge)
                {
                        //printf("Error performing connect\n");
                        return NULL;
                }

                return l_child_node;
        }
        else if(l_prefix_len == pattern_len(l_common_edge->m_pattern))
        {
                //NDBG_PRINT("%s: l_prefix_len == pattern_len(l_common_edge->m_pattern) :%s\n",
                //           ANSI_COLOR_BG_GREEN, ANSI_COLOR_OFF);
                // Get subpath string
                pattern_t l_subpattern = pattern_sub(a_pattern,
                                                     l_prefix_len,
                                                     pattern_len(a_pattern) - l_prefix_len);
                //NDBG_PRINT("%ssub_pattern%s: %s\n",
                //           ANSI_COLOR_BG_GREEN, ANSI_COLOR_OFF,
                //           (pattern_str(l_subpattern)).c_str());
                //NDBG_PRINT("l_prefix_len == l_common_edge->m_pattern.length()\n");
                //NDBG_PRINT("pattern_len(l_subpattern): %u\n", pattern_len(l_subpattern));
                if(!pattern_len(l_subpattern))
                {
                        //NDBG_PRINT("Error performing insert_route: %s -endpoint collision\n",
                        //           (pattern_str(a_pattern)).c_str());
                        return NULL;
                }
                // insert new path to this node
                node *l_new_node;
                //NDBG_PRINT("INSERT_ROUTE: a_data: %p\n", a_data);
                l_new_node = l_common_edge->m_child->insert_route(l_subpattern, a_data);
                if(!l_new_node)
                {
                        //NDBG_PRINT("Error performing insert_route: %s\n",
                        //           (pattern_str(l_subpattern)).c_str());
                        return NULL;
                }
                return l_new_node;
        }
        else if (l_prefix_len < pattern_len(l_common_edge->m_pattern))
        {
                //NDBG_PRINT("%s: l_prefix_len < pattern_len(l_common_edge->m_pattern) :%s l_prefix_len: %u\n",
                //           ANSI_COLOR_BG_GREEN, ANSI_COLOR_OFF, l_prefix_len);
                // partially matched with pattern
                // split endpoint
                // Create new node -branched at prefix
                node *l_new_node;
                l_new_node = l_common_edge->branch(l_prefix_len);
                if(!l_new_node)
                {
                        //NDBG_PRINT("Error performing branch: offset: %u\n", l_prefix_len);
                        return NULL;
                }
                // Get subpath string
                //NDBG_PRINT("ce_pattern:             %s\n", pattern_str(l_common_edge->m_pattern).c_str());
                //NDBG_PRINT("a_pattern:              %s\n", pattern_str(a_pattern).c_str());
                //NDBG_PRINT("l_prefix_len:           %u\n", l_prefix_len);
                //NDBG_PRINT("pattern_len(a_pattern): %u\n", pattern_len(a_pattern));
                //NDBG_PRINT("diff_len:               %u\n", pattern_len(a_pattern) - l_prefix_len);
                if(!(pattern_len(a_pattern) - l_prefix_len))
                {
                        l_common_edge->m_child->m_data = a_data;
                        return l_common_edge->m_child;
                }
                pattern_t l_subpattern = pattern_sub(a_pattern,
                                                     l_prefix_len,
                                                     pattern_len(a_pattern) - l_prefix_len);
                //NDBG_PRINT("%ssub_pattern%s: %s\n",
                //           ANSI_COLOR_BG_GREEN, ANSI_COLOR_OFF,
                //           (pattern_str(l_subpattern)).c_str());
                // Insert the remainder of the path at the new branch
                //NDBG_PRINT("INSERT_ROUTE: a_data: %p\n", a_data);
                l_new_node = l_common_edge->m_child->insert_route(l_subpattern, a_data);
                if(!l_new_node)
                {
                        //NDBG_PRINT("Error performing insert_route: %s\n",
                        //           (pattern_str(l_subpattern)).c_str());
                        return NULL;
                }
                return l_new_node;
        }
        else
        {
                //printf("Error unexpected route.\n");
                return NULL;
        }
        return this;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t node::collision_test(pattern_t a_pattern)
{
        int32_t l_status;
        uint32_t l_prefix_len = 0;
        edge *l_common_edge = NULL;
        l_status = find_longest_common_prefix(a_pattern, l_prefix_len, &l_common_edge);
        if(l_status != 0)
        {
                return -1;
        }
        if(l_prefix_len &&
           (l_prefix_len == pattern_len(l_common_edge->m_pattern)))
        {
                pattern_t l_subpattern = pattern_sub(a_pattern,
                                                     l_prefix_len,
                                                     pattern_len(a_pattern) - l_prefix_len);
                if(!pattern_len(l_subpattern))
                {
                        return -1;
                }
                return l_common_edge->m_child->collision_test(l_subpattern);
        }
        return 0;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t node::find_longest_common_prefix(const pattern_t &a_pattern,
                                 uint32_t &ao_prefix_len,
                                 edge **ao_common_edge)
{

        uint32_t l_longest_common_prefix_len = 0;
        int32_t l_common_prefix_len = 0;
        ao_prefix_len = 0;
        edge *l_edge = NULL;
        for(edge_list_t::const_iterator i_edge = m_edge_list.begin();
            i_edge != m_edge_list.end();
            ++i_edge)
        {
                //NDBG_PRINT("%sloopin on edges compare%s: %s\n",
                //           ANSI_COLOR_FG_MAGENTA, ANSI_COLOR_OFF,
                //           pattern_str((*i_edge)->m_pattern).c_str());
                // ignore all edges with parameter
                l_common_prefix_len = pattern_diff(a_pattern, (*i_edge)->m_pattern);
                //NDBG_PRINT("%sloopin on edges compare%s: l_common_prefix_len: %d\n",
                //           ANSI_COLOR_FG_MAGENTA, ANSI_COLOR_OFF,
                //           l_common_prefix_len);
                if(l_common_prefix_len < 0)
                {
                        //NDBG_PRINT("BAIL.\n");
                        return -1;
                }
                // no common, consider insert a new edge
                if (((uint32_t)l_common_prefix_len) > l_longest_common_prefix_len)
                {
                        l_longest_common_prefix_len = l_common_prefix_len;
                        l_edge = (*i_edge);
                }
        }
        ao_prefix_len = l_common_prefix_len;
        *ao_common_edge = l_edge;
        return 0;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void node::display_trie(uint32_t a_indent)
{
        //printf("data: %p\n", m_data);
        for(edge_list_t::const_iterator i_edge = m_edge_list.begin();
            i_edge != m_edge_list.end();
            ++i_edge)
        {
                // Stupid???
                printf(": ");
                for(uint32_t i=0; i < a_indent; ++i) printf("-");
                printf("%s \n", pattern_str((*i_edge)->m_pattern).c_str());
                if((*i_edge)->m_child != NULL)
                {
                        (*i_edge)->m_child->display_trie(a_indent + 2);
                }
        }
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
const void *node::find_route(const std::string &a_route, url_pmap_t &ao_url_pmap)
{
        //NDBG_PRINT("Find route: %s\n", a_route.c_str());

        for(edge_list_t::const_iterator i_edge = m_edge_list.begin();
            i_edge != m_edge_list.end();
            ++i_edge)
        {

                url_pmap_t l_url_param_map;

                std::string l_suffix;
                const void *l_data;
                //NDBG_PRINT("%sFIND_ROUTE%s[%s]: edge: %s\n",
                //                ANSI_COLOR_FG_YELLOW, ANSI_COLOR_OFF,
                //                a_route.c_str(),
                //                pattern_str((*i_edge)->m_pattern).c_str());
                if((*i_edge)->match_route(a_route, l_url_param_map, l_suffix))
                {
                        //NDBG_PRINT("%sFIND_ROUTE%s: MATCH_ROUTE %s == %s. Remainder: %s len(%d)\n",
                        //                ANSI_COLOR_FG_YELLOW, ANSI_COLOR_OFF,
                        //                pattern_str((*i_edge)->m_pattern).c_str(),
                        //                a_route.c_str(), l_suffix.c_str(), (int)l_suffix.length());
                        if(!l_suffix.length())
                        {
                                // TODO better way to update map???
                                for(url_pmap_t::const_iterator i_arg = l_url_param_map.begin();
                                                i_arg != l_url_param_map.end();
                                                ++i_arg)
                                {
                                        ao_url_pmap[i_arg->first] = i_arg->second;
                                        //NDBG_PRINT("Parameter: %s: %s\n",
                                        //           i_arg->first.c_str(), i_arg->second.c_str());
                                }

                                return (*i_edge)->m_child->m_data;
                        }

                        // if route <
                        l_data = (*i_edge)->m_child->find_route(l_suffix, l_url_param_map);
                        if(l_data)
                        {

                                // TODO better way to update map???
                                for(url_pmap_t::const_iterator i_arg = l_url_param_map.begin();
                                                i_arg != l_url_param_map.end();
                                                ++i_arg)
                                {
                                        ao_url_pmap[i_arg->first] = i_arg->second;
                                        //NDBG_PRINT("Parameter: %s: %s\n",
                                        //           i_arg->first.c_str(), i_arg->second.c_str());
                                }

                                return l_data;
                        }
                }
        }

        return NULL;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
node::node():
        m_endpoint(0),
        m_edge_list(),
        m_data(NULL)
{

}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: -----------------------------------------------------------------------------
node::~node()
{
        // Walk the edge list deleting any nodes
        for(edge_list_t::iterator i_e = m_edge_list.begin();
            i_e != m_edge_list.end();
            ++i_e)
        {
                if(*i_e)
                {
                        delete (*i_e)->m_child;  // m_child is always set
                        delete *i_e;
                }
        }
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t url_router::add_route(const std::string &a_route, const void *a_data)
{
        //NDBG_PRINT("ADD_ROUTE: a_route: %s a_data: %p\n", a_route.c_str(), a_data);
        // Sanitize path
        int32_t l_status;
        std::string l_route = a_route;
        pattern_t l_pattern;
        l_status = convert_path_to_pattern(l_route, l_pattern);
        if(l_status != 0)
        {
                //NDBG_PRINT("Error performing sanitize path: %s\n", l_route.c_str());
                return -1;
        }
        if(!m_root_node)
        {
                m_root_node = new node();
        }
        node * l_node;
        l_node = m_root_node->insert_route(l_pattern, a_data);
        if(!l_node)
        {
                //printf("Error performing insert_route\n");
                return -1;
        }
        return 0;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
const void *url_router::find_route(const std::string &a_route, url_pmap_t &ao_url_pmap)
{
        const void *l_data = NULL;
        if(!m_root_node)
        {
                return NULL;
        }
        l_data = m_root_node->find_route(a_route, ao_url_pmap);
        return l_data;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void url_router::display(void)
{
        for(const_iterator iter = begin(); iter != end(); ++iter)
                printf(": %s%s -> %p\n", std::string(iter.depth<<1, '-').c_str(), iter->first.c_str(), iter->second);
}

//: ----------------------------------------------------------------------------
//: \details: Constructor
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void url_router::display_trie(void)
{
        m_root_node->display_trie(0);
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
const url_router::const_iterator::value_type url_router::const_iterator::operator*() const
{
        return m_cur_value;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
const url_router::const_iterator::value_type* url_router::const_iterator::operator->() const
{
        return &m_cur_value;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
url_router::const_iterator& url_router::const_iterator::operator++()
{
        // -------------------------------------------------
        // overall 2 steps
        // -------------------------------------------------
        // 1.  Move the iterator to the right spot
        // -------------------------------------------------
        // if the current node has a child,
        //   push the child mode onto the stack
        //   go to the first entry in the child node
        // else
        //   go to the next entry in the current node
        //   if we're at the end, return end iterator
        if(
           //!m_edge_iterators_stack.empty() &&
           //(*m_edge_iterators_stack.top().first) &&
           //(*m_edge_iterators_stack.top().first)->m_child &&
           (false == (*m_edge_iterators_stack.top().first)->m_child->m_edge_list.empty())){
                // the current node being pointed _TO_ has children

                // step down a node to get the children
                m_edge_iterators_stack.push(
                        std::make_pair(
                                (*m_edge_iterators_stack.top().first)->m_child->m_edge_list.begin(),
                                (*m_edge_iterators_stack.top().first)->m_child->m_edge_list.end()
                                )
                        );
                // fprintf(stderr, "Stepped down to edge: %p\n", *m_edge_iterators_stack.top().first);
                ++depth;

        } else {
                // go to the next sibling entry

                // fprintf(stderr, "Go to next sibling\n");
                // move to the next point
                if(!m_edge_iterators_stack.empty() &&
                    (++(m_edge_iterators_stack.top().first) == m_edge_iterators_stack.top().second)){
                        // we're at the end of the edge at the top of the stack

                        // fprintf(stderr, "edge ierator pointing at the end of the current node's edge list\n");
                        m_edge_iterators_stack.pop();             // go up the stack one
                        ++(m_edge_iterators_stack.top().first);   // and step forwards
                        --depth;

                        // if we're at the top of the stack we're done
                        if(m_edge_iterators_stack.top().first == m_router.m_root_node->m_edge_list.end()){
                                // we're now at the end element of the
                                // root node's list
                                // this is the end
                                return *this;
                        }
                        // we can keep going
                }
        }

        // -------------------------------------------------
        // 2.  Set m_cur_value from them
        // -------------------------------------------------
        //if(!m_edge_iterators_stack.empty())
        {
                std::string l_pattern = pattern_str((*m_edge_iterators_stack.top().first)->m_pattern);
                m_cur_value = url_router::const_iterator::value_type(l_pattern, (*m_edge_iterators_stack.top().first)->m_child->m_data);
        }
        return *this;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
url_router::const_iterator url_router::const_iterator::operator++(int)
{
        url_router::const_iterator l_retval(*this);
        ++(*this);
        return l_retval;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
bool url_router::const_iterator::operator==(const const_iterator& a_iterator)
{
        return (&m_router == &a_iterator.m_router &&
                m_edge_iterators_stack == a_iterator.m_edge_iterators_stack);
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
bool url_router::const_iterator::operator!=(const const_iterator& a_iterator)
{
        return !(*this == a_iterator);
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
std::string url_router::const_iterator::get_full_url(void) const
{
        // create the full string representing the url to this point
        std::string retval;
        iterator_stack_t l_stack(m_edge_iterators_stack);
        while(false == l_stack.empty()){
                // have stuff on the stack
                // build the string backwards
                retval.insert(0, pattern_str((*l_stack.top().first)->m_pattern));
                l_stack.pop();
        }
        return retval;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
url_router::const_iterator::const_iterator(const const_iterator& a_iterator):
        depth(a_iterator.depth),
        m_router(a_iterator.m_router),
        m_edge_iterators_stack(a_iterator.m_edge_iterators_stack),
        m_cur_value(a_iterator.m_cur_value)
{
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
url_router::const_iterator::const_iterator(const url_router& a_router):
        depth(0),
        m_router(a_router),
        m_edge_iterators_stack(),
        m_cur_value()
{
        if(a_router.m_root_node)
        {
                m_edge_iterators_stack.push(
                        std::make_pair(a_router.m_root_node->m_edge_list.begin(),
                                       a_router.m_root_node->m_edge_list.end()));
        }
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
url_router::const_iterator url_router::begin() const
{
        url_router::const_iterator l_retval(*this);
        // step to the first valid (m_data != nullptr) point in the trie
        ++l_retval;
        return l_retval;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
url_router::const_iterator url_router::end() const
{
        url_router::const_iterator l_retval(*this);

        // point to the end, this is the end of the root node's list
        // as we iterate depth first
        while(false == l_retval.m_edge_iterators_stack.empty())
        {
                l_retval.m_edge_iterators_stack.pop();
        }
        l_retval.m_edge_iterators_stack.push(
                std::make_pair(l_retval.m_router.m_root_node->m_edge_list.end(),
                               l_retval.m_router.m_root_node->m_edge_list.end()
                        )
                );
        return l_retval;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
url_router::url_router(void):
                m_root_node(NULL)
{

}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
url_router::~url_router(void)
{
        if(m_root_node)
        {
                delete m_root_node;
        }
}

} //namespace ns_hlx {
