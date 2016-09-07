#!/bin/bash
gawk 'BEGIN {
    print "drop table minute_bucket;"
    print "create table minute_bucket (minute datetime);"
    print "begin transaction;"
# 2456689.03418772->2456689.04441214
    min_int = int(2456689.03418772*24.0*60.0)
    max_int = int(2456689.04441214*24.0*60.0)
    for (min = min_int; min <= max_int; min += 1)
        print "insert into minute_bucket values (julianday(" min "/24.0/60.0));"
    print "commit;"
    exit;
}' /dev/null | sqlite3 capsess.db
