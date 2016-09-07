#!/bin/sh
# Get the OS costs
ps -e | nawk '/oracle/ {
for (i = 4; i <= NF; i++)
    if ($i == "oracle")
        break
i -= 1
nf = split($i,arr,":")
cpu = 0
for (j = 1; j <= nf; j++)
{
    cpu *= 60
    cpu += arr[j]
}
print $1 " " cpu
}' | sort > pid.lis
# Get the ORACLE activity
sqldba mode=line 2>/dev/null << EOF > sess1.lis
connect internal
select a.spid, sum(b.block_gets + b.consistent_gets), sum(b.physical_reads)
from v\$process a, v\$sess_io b, v\$session c
where a.addr = c.paddr and b.sid =c.sid
and c.sid > 6
group by a.spid
order by 1;
exit
EOF
sed '/^[1-9]/ !d' sess1.lis > sess.lis
# Merge the two
join pid.lis sess.lis> test.lis
nawk 'BEGIN {
x = 0
x2 = 0
y = 0
z = 0
z2 = 0
xz = 0
xy = 0
yz = 0
}
{
#
# y => CPU = $2
# x => Buffer Gets = $3
# z => Disk Reads = $4
y += $2
x += $3
z += $4
x2 += $3 * $3
z2 += $4 * $4
xy += $2 * $3
xz += $3 * $4
yz += $2 * $4
}
function calc_res(){
#
# The formula is y = mx +mnz + c
#
p =  m*($3 + $4*n) + c
r += ($2 - p)*($2 - p)
print $2 " " p " " $3 " " $4
    return
}
function show_res() {
    print "Lines " NR " Average " y/NR " SD " sqrt(r/NR) " Total Residue " r
    return
}
END {
OFMT="%.10f"

    x2bar = x2 - x*x/NR
    xzbar = xz - x*z/NR
    z2bar = z2 - z*z/NR
    xybar = xy - x*y/NR
    yzbar = yz - y*z/NR

    m = (-xzbar*yzbar + z2bar*xybar)/(x2bar*z2bar - xzbar*xzbar)
    n = (yzbar - m*xzbar)/m/z2bar
    c = (y - m*x - m*n*z)/NR

#    print "xzbar: " xzbar
#    print "xybar: " xybar
#    print "yzbar: " yzbar
#    print "x2bar: " x2bar
#    print "z2bar: " z2bar
#    print "xzbar*yzbar: " xzbar*yzbar
#    print "xybar*z2bar: " xybar*z2bar
#    print "x2bar*z2bar: " x2bar*z2bar
#    print "xzbar*xzbar: " xzbar*xzbar
   print "Buffer get cost: " m " Disk Read Ratio: " n " Session Overhead: " c
#
# Now do it again
#
#    m = (-xz*yz + z2*xy)/(x2*z2 - xz*xz)
#    n = (yz - m*xz)/m/z2
#    c = 0
#    print "Buffer get cost: " m " Disk Read Ratio: " n " Session Overhead: " c
#
# Now do it again
#
#    m = xy/x2
#    n = yz/z2
#    n = n/m
#    c = 0
#    print "Buffer get cost: " m " Disk Read Ratio: " n " Session Overhead: " c
#
# Now do it again
#
#   m = (xy - x*y/NR)/(x2 - x*x/NR)
#   n = (yz - y*z/NR)/(z2 - z*z/NR)
#   n = n/m
#   c = (y - m*x - m*n*z)/NR
#   print "Buffer get cost: " m " Disk Read Ratio: " n " Session Overhead: " c

    r = 0

    while((getline<"test.lis")>0)
        calc_res()
    show_res()
}' < test.lis >  cost.lis
