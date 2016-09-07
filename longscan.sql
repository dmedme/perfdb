create or replace
procedure do_sql_search (search_txt in varchar2)
is
sql_text varchar2(32767);
sql_rowid varchar2(32);
flag varchar2(1);
cursor c1 is select sql_text, flag, rowid from badsort_sql;
begin
    open c1;
    fetch c1 into sql_text, flag, sql_rowid; 
    while  c1%found
    loop
        if sql_text like search_txt
        then
             update badsort_sql
                set flag = 'Y'
                   where rowid = sql_rowid;
        elsif flag = 'Y'
        then
             update badsort_sql
                set flag = null
                   where rowid = sql_rowid;
        end if;
        fetch c1 into sql_text, flag, sql_rowid; 
    end loop;
    close c1;
    commit;
end do_sql_search;
/
create or replace
procedure do_plan_search (search_txt in varchar2)
is
plan_text varchar2(32767);
plan_rowid varchar2(32);
flag varchar2(1);
cursor c1 is select plan_text, flag, rowid from badsort_plan;
begin
    open c1;
    fetch c1 into plan_text, flag, plan_rowid; 
    while  c1%found
    loop
        if plan_text like search_txt
        then
             update badsort_plan
                set flag = 'Y'
                   where rowid = plan_rowid;
        elsif flag = 'Y'
        then
             update badsort_plan
                set flag = null
                   where rowid = plan_rowid;
        end if;
        fetch c1 into plan_text, flag, plan_rowid; 
    end loop;
    close c1;
    commit;
end do_plan_search;
/
