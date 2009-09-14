#!/bin/sh

for tool in autoconf autom4te automake bash bison dot fiona flex gcc gmake gengetopt help2man libtool lola-statespace make makeinfo sh; do
    echo "$tool (`which $tool || echo "not found"`)"
    echo "------------------------------------------------------------------------"
    echo `($tool --version &> /dev/null && $tool --version) || ($tool -V &> /dev/null && $tool -V)  || ($tool -v &> /dev/null && $tool -v) || echo "$tool was not found"` | fmt
    echo
done
