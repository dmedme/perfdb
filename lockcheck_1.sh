#!/bin/sh
while :
do
sqlplus fintest/fintest@TEST << EOF
set serveroutput on
rem
rem There is a limit of 2,000 bytes of info from the thing below with
rem SQL*Plus (note that the dbms_output.enable default is 20,000).
rem
declare
blocking_sid sys.v_\$session.sid%type;
blocked_sid sys.v_\$session.sid%type;
locked_id1 sys.v_\$lock.id1%type;
locked_id2 sys.v_\$lock.id2%type;
app_pid sys.v_\$session.process%type;
agent_pid sys.v_\$process.spid%type;
sql_line sys.v_\$sqltext.sql_text%type;
name_a sys.obj\$.name%type;
name_b sys.obj\$.name%type;
rowf sys.v_\$session.row_wait_file#%type;
rowb sys.v_\$session.row_wait_block#%type;
rowr sys.v_\$session.row_wait_row#%type;
osuser sys.v_\$session.osuser%type;
terminal sys.v_\$session.terminal%type;
program sys.v_\$session.program%type;
cursor c_blocked is
select sid, id1, id2 from sys.v_\$lock where request != 0;
cursor ccur_sql (work_sid sys.v_\$session.sid%type) is
select b.sql_text from sys.v_\$session a, sys.v_\$sqltext b
where b.address = a.sql_address
and a.sid = work_sid
order by a.sid, b.address, b.piece;
cursor blocked_process (work_sid sys.v_\$session.sid%type) is
        select a.process,a.osuser,a.terminal,a.program,b.spid
             from sys.v_\$session a, sys.v_\$process b
             where a.sid=work_sid and a.paddr = b.addr;
cursor blocking_sids( locked_id1 sys.v_\$lock.id1%type, 
locked_id2 sys.v_\$lock.id2%type) is
        select sid
            from sys.v_\$lock
            where request = 0 and id1 = locked_id1 and id2 = locked_id2;
begin
    dbms_output.enable(20000);
    dbms_output.put_line('Starting Lock Tree Search at '||
           to_char(sysdate, 'hh24:mi:ss'));
    open c_blocked;
    loop
        fetch c_blocked into blocked_sid, locked_id1, locked_id2;
        exit when c_blocked%notfound;
        dbms_output.put_line('Blocked SID:' || to_char(blocked_sid));
        open ccur_sql(blocked_sid);
        loop
            fetch ccur_sql into sql_line;
            exit when ccur_sql%notfound;
            dbms_output.put_line(sql_line);
        end loop;
        close ccur_sql;
        open blocked_process(blocked_sid);
        loop
            fetch blocked_process
             into app_pid, osuser,terminal, program, agent_pid;
            exit when blocked_process%notfound;
            dbms_output.put_line('Blocked App. Pid: '|| app_pid ||
                                 '  Agent Pid :' || agent_pid ||
             '('|| osuser ||','|| program ||','||terminal||')');
        end loop;
        close blocked_process;
        open blocking_sids(locked_id1, locked_id2);
        loop
            fetch blocking_sids into blocking_sid;
            exit when blocking_sids%notfound;
            dbms_output.put_line('Blocking SID:' || to_char(blocking_sid));
            open ccur_sql(blocking_sid);
            loop
                fetch ccur_sql into sql_line;
                exit when ccur_sql%notfound;
                dbms_output.put_line(sql_line);
            end loop;
            close ccur_sql;
            open blocked_process(blocking_sid);
            loop
                fetch blocked_process
                 into app_pid, osuser,terminal, program, agent_pid;
                exit when blocked_process%notfound;
                dbms_output.put_line('Blocking App. Pid: '|| app_pid ||
                                     '  Agent Pid :' || agent_pid||
             '('|| osuser ||','|| program ||','||terminal||')');
            end loop;
            close blocked_process;
        end loop;
        close blocking_sids;
    end loop;
    close c_blocked;
    dbms_output.put_line('Finished Lock Tree Search at '||
           to_char(sysdate, 'hh24:mi:ss'));
end;
/
select to_char(a.sid)||
    ' ('|| a.osuser||','||a.terminal||','||a.program||') has waited: '||
       b.name||' '||c.name||' '||to_char(a.row_wait_file#)||' '||
       to_char(a.row_wait_block#)||' '|| to_char(a.row_wait_row#)
from
    sys.obj\$ b,
    sys.obj\$ c,
    sys.uet\$ d,
    sys.tab\$ e,
    sys.v_\$session a
where
(a.row_wait_obj# > 0 or a.row_wait_file# > 0 or
a.row_wait_block# > 0 or a.row_wait_row# > 0)
and a.row_wait_obj# = b.obj#(+)
and a.row_wait_file# = d.file#(+)
and a.row_wait_block# between d.block#(+) and d.block#(+) + d.length(+)
and d.segfile# = e.file#
and d.segblock# = e.block#
and e.obj# = c.obj#
/
EOF
sleep 600
done >huntlock.log 2>&1
exit
