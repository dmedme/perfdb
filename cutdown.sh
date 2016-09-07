#!/bin/bash
#
# Deal with performance monitor output with more than 256 columns that
# needs to go into multiple files
#
for i in *.csv
do
gawk 'BEGIN {
    getline
    nf = split($0,arr,"\",\"")
    ofn = FILENAME
    gsub(" ","_",ofn)
    fcnt = 0;
    if (nf <= 255)
    {
        sub(/\./,"_run.",ofn)
        print $0>ofn
    }
    else
    {
        lim = 256;
        bse = 2;
        do
        {
            ofn_p[fcnt] = ofn
            sub(/\./,"_run_" fcnt ".",ofn_p[fcnt])
            head = arr[1]
            for (i = bse; i < lim; i++)
               head = head "\",\"" arr[i]
            print   head >ofn_p[fcnt]
            bse += 254
            lim += 254
            if (lim > nf + 1)
                lim = nf + 1
            fcnt++
        }
        while(bse < nf)
    }
}
/^"11\/27\/2006 2[01]:/ {
    if (fcnt == 0)
        print $0>ofn
    else
    {
        cols = split($0, arr, ",")
        if (cols != nf)
            print FILENAME " row " NR " names " nf " columns " cols
        lim = 256;
        bse = 2;
        j = 0
        do
        {
            head = arr[1]
            for (i = bse; i < lim; i++)
               head = head "," arr[i]
            print  head >ofn_p[j]
            bse += 254
            lim += 254
            if (lim > nf)
                lim = nf + 1
            j++
        }
        while(bse < nf)
    }
}' "$i"
done
