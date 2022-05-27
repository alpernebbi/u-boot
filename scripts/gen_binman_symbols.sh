#!/bin/bash
# SPDX-License-Identifier: GPL-2.0+

# Generate a declaration for binman symbols, so that LTO does not strip
# the 'weak' attribute from optional ones.

# The expected parameter of this script is the command requested to have
# the U-Boot symbols to parse, for example: $(NM) $(u-boot-main)

set -e

PATTERN='_binman_\([a-zA-Z0-9_]*\)_prop_\([a-zA-Z0-9_]*\)'

echo '#include <binman_sym.h>'
$@ 2>/dev/null | grep -oe ". $PATTERN" | sort -u | \
    sed -e "s/W $PATTERN/binman_sym_declare_optional(ulong, \1, \2);/" \
        -e "s/. $PATTERN/binman_sym_declare(ulong, \1, \2);/"
