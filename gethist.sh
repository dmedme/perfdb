#!/bin/sh
# Convert oracomms.lis into a number of users histogram
#
if [ $# -lt 1 ]
then
    echo Provide a quantisation period in seconds
    exit
fi
quant=$1
gawk -F\| '$2 == "sqlnet" {
dy = int($3/86400)
s = $3 - dy*86400
hr = int(s/3600)
s -= hr*3600
min = int(s/60)
s -= min*60
if (hr+0 < 10)
    hr = "0" hr
if (s+0 < 10)
    s = "0" s
if (min+0 < 10)
    min = "0" min
dy += 1
print "user" NR " " int($5) " " int($3) " Tue Nov " dy " " hr ":" min ":" s " 1998 GMT"
}' oracomms.lis | sort -n +2 | /e2soft/perfdb/usehist $quant | tee junk |
gawk '
function adv(nt, nd) {
lt++
if (lt > 23)
{
    lt = 0
    dy++
}
while(lt < nt || dy < nd)
{
    print dy " Nov 1998 " lt ":00 0"
    lt++
    if (lt > 23)
    {
        lt = 0
        dy++
    }
}
    return
}
/Date:/ { if (nd != "")
{
nd = $2 
nt = 0
adv(nt,nd)
}
else
{
    dy=$2
    nd = $2
    lt = 0
}
next
}
/No/ {
split($9, arr, ":")
nt = arr[1]
nd = $6
adv(nt, nd)
next
}
NF == 5 {
if ($1 ~ "=")
    next
split($1,arr,":")
nt = arr[1]
if (nt < lt)
{
    nd = dy+ 1
}
else
    nd = dy
adv(nt,nd)
print dy " Nov 1998 " $1 " " int($2/'$quant' +.5)
}'
