#!/bin/sh
if [ $# -ne 1 ]
then
    echo Provide a pstat output file
fi
fname=$1
awk 'BEGIN {bt = 0
ldiff = 0
bcpu = 0
}
function tosecs(tm) {
    nf = split(tm,arr,":")
    mult = 1
    ret = 0
    for (i = nf; i > 0; i--)
    {
        ret = ret + arr[i]*mult
        mult = mult * 60
    }
    return ret
}
/uptime:/ {
    for (i = 0; i < NF; i++)
        if ($i == "uptime:")
            break
    i = i + 1
    if (i <= NF)
    {
        if ($i !~ ":")
            i++
        if (i <= NF)
        {
            t = tosecs($i)
            if (bt == 0)
            {
                bt = t
                diff = 0
            }
            else
            if (t != bt)
            {
                diff = t - bt
            }
        }
    }
    next
}
/Idle Process/ {
    if ($1 == "pid:")
        next
    cpu = tosecs($2)
    if (bcpu != 0 && diff != ldiff)
    {
        print diff "\t" ((diff - ldiff) * 2 - (cpu - bcpu))/(diff - ldiff)*50
        ldiff = diff
    }
    bcpu = cpu
}' $fname

