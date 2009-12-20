#! /bin/bash
if ! test -f depend.mak; then
	make depend
fi
make 2>errlog
grep -v "In function" errlog | grep -v "more undefined" | grep -v collect2 | \
sed -e 's/^.*: undefined reference to//g' | sort -u > undef.txt
