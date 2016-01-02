#!/usr/bin/python
# ------------------------------------------------------------------------------
# Generate file extension to content type associations.
# ex:
#        { "txt", "text/plain" },
#        { "text", "text/plain" },
#        { "png", "image/png" },
#        { "jpg", "image/jpeg" },
#        ...
# Running:
# ./util/gen_mime_types_h.py -f ./data/mime_types.json -o ./src/core/hlx/_gen_mime_types.h
# ------------------------------------------------------------------------------

# ------------------------------------------------------------------------------
# Imports
# ------------------------------------------------------------------------------
import argparse
import json
import sys
import os

# ------------------------------------------------------------------------------
# main
# ------------------------------------------------------------------------------
def gen_mime_types(a_file, a_out):
    """
    generate output:
    
    //: ----------------------------------------------------------------------------
    //: This is automatically generated
    //: ----------------------------------------------------------------------------
            { "txt", "text/plain" },
            { "text", "text/plain" },
            { "def", "text/plain" },
            { "list", "text/plain" },
            { "in", "text/plain" },
            { "ini", "text/plain" },
            { "css", "text/css" },
            { "mp4", "video/mp4" },
            { "mp4v", "video/mp4" },
            { "mpg4", "video/mp4" },
            { "png", "image/png" },
            { "jpeg", "image/jpeg" },
            { "jpg", "image/jpeg" },
            { "jpe", "image/jpeg" },
    """
    # --------------------------------------------
    # Read json and invert
    # --------------------------------------------
    with open(a_file) as l_file:
        l_mime_json = json.load(l_file)
    l_ext_2_type_dict = {}
    for i_type in l_mime_json:
        for i_ext in l_mime_json[i_type]:
            l_ext_2_type_dict[i_ext] = i_type
    if not a_out:
        print '//: ----------------------------------------------------------------------------'
        print '//: This is automatically generated'
        print '//: ----------------------------------------------------------------------------'
        for i_ext in l_ext_2_type_dict:
            print '{ \"%s\", \"%s\" },'%(i_ext, l_ext_2_type_dict[i_ext])
        return
    with open(a_out, 'w') as l_file:
        l_file.write('//: ----------------------------------------------------------------------------\n')
        l_file.write('//: This is automatically generated\n')
        l_file.write('//: ----------------------------------------------------------------------------\n')
        for i_ext in l_ext_2_type_dict:
            l_file.write('{ \"%s\", \"%s\" },\n'%(i_ext, l_ext_2_type_dict[i_ext]))
    return

# ------------------------------------------------------------------------------
# main
# ------------------------------------------------------------------------------
def main(argv):

    l_arg_parser = argparse.ArgumentParser(
                description='gen_mime_types_h.py.',
                usage= '%(prog)s',
                epilog= '')

    # input
    l_arg_parser.add_argument('-f',
                            '--file',
                            dest='file',
                            help='Mime Types Json.',
                            default='',
                            required=True)
    
        # text
    l_arg_parser.add_argument('-o',
                            '--out',
                            dest='out',
                            help='Output File.',
                            default='',
                            required=False)

    l_args = l_arg_parser.parse_args()

    # Start server
    gen_mime_types(a_file=l_args.file, a_out=l_args.out)

# ------------------------------------------------------------------------------
# main
# ------------------------------------------------------------------------------
if __name__ == '__main__':
    
    main(sys.argv[1:])

