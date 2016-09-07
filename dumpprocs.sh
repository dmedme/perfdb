#!/bin/sh
./tabdiff -l240 -o EXEC JCURRAN/corporate/laurel_live/pir_liv_db fred fred << EOF
select text from syscomments
order by id, number, colid2, colid
EOF
