BEGIN { flag = 0
fun=0
old_icl = 0
nd=0
outf=""
}
function switch_files(x) {
    if (outf != "")
        close(outf)
    outf=x
    return
}
#
# Switch off extraction
#
fun == 2 && $0 ~ "Average" {
    out_line()
}
/Average/ {
getline
getline
if ($2 == "Unix")
   next
fun = 0
next
}
#
# CPU
#
/%idle/ { fun = 1
switch_files("sar_cpu.txt")}
fun == 1 && NF == 5 {
print $1 "\t" $2 "\t" $3 "\t" $4 "\t" $5 "\r">outf
}
fun == 1 && NF == 6 {
old_icl = 1
print $1 "\t" $2 "\t" $3 "\t" $5 "\t" $6 "\r">outf
}
#
# Free memory
#
/freemem/ { fun = 3
switch_files("sar_mem.txt")}
fun == 3 && NF == 3 {
print $1 "\t" $2 "\t" $3 "\r">outf
}
fun == 3 && NF == 4 {
print $1 "\t" $2 "\t" $3 "\r">outf
}
#
# Scan Rate - SUN and old ICL
#
/pgscan/ { fun = 4
switch_files("sar_scr.txt")}
fun == 4 && NF == 6 {
print $1 "\t" $5 "\r">outf
}
fun == 4 && NF == 5 {
print $1 "\t" $5 "\r">outf
}
#
# Scan Rate - ICL
#
/vscan/ { fun = 8
switch_files("sar_scr.txt")}
fun == 8 && NF == 6 {
print $1 "\t" $6 "\r">outf
}
#
# Disk
#
/avwait/ { fun = 2
switch_files("sar_td.txt")
getline
next
}
#
# Terminal
#
/rawch/ { fun = 7
switch_files("sar_tty.txt")}
fun == 7 && NF == 7 { print $1 "\t" $2 "\t" $4 "\r">outf }
#
# Print out all the Disk stuff for this time, or print the headings
#
fun == 2 && NF == 0 {
   if (flag == 0 || flag == 2)
   {
       heading()
       flag = 1
   }
   if (t != "")
       out_line()
   next
}
#
# Remember the disk name
#
fun == 2 && flag == 0 {
    if ($1 ~ ":")
    {
        dsk[nd] = $2
        xref[$2] = nd
    }
    else
    {
        dsk[nd] = $1
        xref[$1] = nd
    }
    nd++
}
#
# Output the headings
#
function heading() {
    printf "Time" >outf
    for (j = 0; j < 2; j++)
    for (i = 0; i < nd; i++)
    {
       printf "\t%s", dsk[i] >outf
    }
    printf "\r\n">outf
    return
}
#
# Output a line
#
function out_line() {
    printf t  >outf
    for (i = 0; i < nd; i++)
    {
       nm = dsk[i]
       printf "\t%f", blks[nm] >outf
       blks[nm] = 0
    }
    for (i = 0; i < nd; i++)
    {
       nm = dsk[i]
       printf "\t%f",  avs[nm] >outf
       avs[nm] = 0
    }
    printf "\r\n">outf
    t = ""
    return
}
fun == 2 && $1 ~ ":" {
    if (t != "")
        out_line()
    t = $1
    if (xref[$2] == "")
    {
        dsk[nd] = $2
        xref[$2] = nd
        nd++
        flag = 2
    }
    blks[$2] = $6
    avs[$2] = $7 + $8
    next
}
fun == 2 {
    if (xref[$1] == "")
    {
        dsk[nd] = $1
        xref[$1] = nd
        nd++
        flag = 2
    }
    blks[$1] = $5
    avs[$1] = $6 + $7
}
END {
    switch_files("sar_dsk.txt")
    heading()
    inf = "sar_td.txt"
    while((getline<inf)>0)
    {
        if ($1 == "Time")
            continue
        if (NF ==  (1 +2 * nd))
            print>outf
        else
        {
            nf = (NF - 1)/2
            ns = nd - nf
            printf "%s", $1>outf
            for (i = 2; i <= (nf + 1); i++)
                 printf "\t%f",  $i >outf
            for (i = 0; i < ns; i++)
                 printf "\t0" >outf
            
            for (i = nf + 2; i <= NF; i++)
                 printf "\t%f",  $i >outf
            for (i = 0; i < ns; i++)
                 printf "\t0" >outf
            printf "\r\n" >outf
        }
    }
    close(outf)
}
