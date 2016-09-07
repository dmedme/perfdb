
drop table badsort_plan_table;
create table badsort_PLAN_TABLE (
	statement_id 	varchar2(30),
	timestamp    	date,
	remarks      	varchar2(80),
	operation    	varchar2(30),
	options       	varchar2(30),
	object_node  	varchar2(128),
	object_owner 	varchar2(30),
	object_name  	varchar2(30),
	object_instance numeric,
	object_type     varchar2(30),
	optimizer       varchar2(255),
	search_columns  numeric,
	id		numeric,
	parent_id	numeric,
	position	numeric,
        cost            numeric,
        cardinality      numeric,
        bytes           numeric,
        other_tag       varchar2(255),
	other		long);
create public synonym badsort_plan_table for badsort_plan_table;
grant insert, update,delete,select on badsort_plan_table to public;







