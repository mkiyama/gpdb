set allow_system_table_mods=true;

-- Hash distributed tables
Create table t1_reshuffle(a int, b int, c int) distributed by (a);
update gp_distribution_policy  set numsegments=2 where localoid='t1_reshuffle'::regclass;
insert into t1_reshuffle select i,i,0 from generate_series(1,100) I;
Update t1_reshuffle set c = gp_segment_id;

Select gp_segment_id, count(*) from t1_reshuffle group by gp_segment_id;

begin;
Alter table t1_reshuffle set with (reshuffle);
Select gp_segment_id, count(*) from t1_reshuffle group by gp_segment_id;
abort;

Select gp_segment_id, count(*) from t1_reshuffle group by gp_segment_id;

Alter table t1_reshuffle set with (reshuffle);

Select gp_segment_id, count(*) from t1_reshuffle group by gp_segment_id;

select numsegments from gp_distribution_policy where localoid='t1_reshuffle'::regclass;
drop table t1_reshuffle;


Create table t1_reshuffle(a int, b int, c int) with OIDS distributed by (a,b);
update gp_distribution_policy  set numsegments=1 where localoid='t1_reshuffle'::regclass;
insert into t1_reshuffle select i,i,0 from generate_series(1,100) I;
Update t1_reshuffle set c = gp_segment_id;

Select gp_segment_id, count(*) from t1_reshuffle group by gp_segment_id;

begin;
Alter table t1_reshuffle set with (reshuffle);
Select gp_segment_id, count(*) from t1_reshuffle group by gp_segment_id;
abort;

Select gp_segment_id, count(*) from t1_reshuffle group by gp_segment_id;

Alter table t1_reshuffle set with (reshuffle);

Select gp_segment_id, count(*) from t1_reshuffle group by gp_segment_id;

select numsegments from gp_distribution_policy where localoid='t1_reshuffle'::regclass;
drop table t1_reshuffle;

-- Test NULLs.
Create table t1_reshuffle(a int, b int, c int) distributed by (a,b,c);
update gp_distribution_policy  set numsegments=2 where localoid='t1_reshuffle'::regclass;
insert into t1_reshuffle values
  (1,    1,    1   ),
  (null, 2,    2   ),
  (3,    null, 3   ),
  (4,    4,    null),
  (null, null, 5   ),
  (null, 6,    null),
  (7,    null, null),
  (null, null, null);
Select gp_segment_id, count(*) from t1_reshuffle group by gp_segment_id;

begin;
Alter table t1_reshuffle set with (reshuffle);
Select gp_segment_id, count(*) from t1_reshuffle group by gp_segment_id;
abort;

Select gp_segment_id, count(*) from t1_reshuffle group by gp_segment_id;

Alter table t1_reshuffle set with (reshuffle);

Select gp_segment_id, count(*) from t1_reshuffle group by gp_segment_id;

select numsegments from gp_distribution_policy where localoid='t1_reshuffle'::regclass;
drop table t1_reshuffle;

Create table t1_reshuffle(a int, b int, c int) distributed by (a) partition by list(b) (partition t1_reshuffle_1 values(1), partition t1_reshuffle_2 values(2), default partition other);
update gp_distribution_policy set numsegments = 1 where localoid='t1_reshuffle_1_prt_t1_reshuffle_1'::regclass;
update gp_distribution_policy set numsegments = 1 where localoid='t1_reshuffle_1_prt_t1_reshuffle_2'::regclass;
update gp_distribution_policy set numsegments = 1 where localoid='t1_reshuffle_1_prt_other'::regclass;
update gp_distribution_policy set numsegments = 1 where localoid='t1_reshuffle'::regclass;
insert into t1_reshuffle select i,i,0 from generate_series(1,100) I;

Select gp_segment_id, count(*) from t1_reshuffle group by gp_segment_id;

begin;
Alter table t1_reshuffle set with (reshuffle);
Select gp_segment_id, count(*) from t1_reshuffle group by gp_segment_id;
abort;

Select gp_segment_id, count(*) from t1_reshuffle group by gp_segment_id;

Alter table t1_reshuffle set with (reshuffle);

Select gp_segment_id, count(*) from t1_reshuffle group by gp_segment_id;

select numsegments from gp_distribution_policy where localoid='t1_reshuffle'::regclass;
drop table t1_reshuffle;

-- Random distributed tables
Create table r1_reshuffle(a int, b int, c int) distributed randomly;
update gp_distribution_policy  set numsegments=2 where localoid='r1_reshuffle'::regclass;
insert into r1_reshuffle select i,i,0 from generate_series(1,100) I;
Update r1_reshuffle set c = gp_segment_id;

Select count(*) from r1_reshuffle;
Select count(*) > 0 from r1_reshuffle where gp_segment_id=2;

begin;
Alter table r1_reshuffle set with (reshuffle);
Select count(*) from r1_reshuffle;
Select count(*) > 0 from r1_reshuffle where gp_segment_id=2;
abort;

Select count(*) from r1_reshuffle;
Select count(*) > 0 from r1_reshuffle where gp_segment_id=2;

Alter table r1_reshuffle set with (reshuffle);

Select count(*) from r1_reshuffle;
Select count(*) > 0 from r1_reshuffle where gp_segment_id=2;

select numsegments from gp_distribution_policy where localoid='r1_reshuffle'::regclass;
drop table r1_reshuffle;

Create table r1_reshuffle(a int, b int, c int) with OIDS distributed randomly;
update gp_distribution_policy  set numsegments=2 where localoid='r1_reshuffle'::regclass;
insert into r1_reshuffle select i,i,0 from generate_series(1,100) I;
Update r1_reshuffle set c = gp_segment_id;

Select count(*) from r1_reshuffle;
Select count(*) > 0 from r1_reshuffle where gp_segment_id=2;

begin;
Alter table r1_reshuffle set with (reshuffle);
Select count(*) from r1_reshuffle;
Select count(*) > 0 from r1_reshuffle where gp_segment_id=2;
abort;

Select count(*) from r1_reshuffle;
Select count(*) > 0 from r1_reshuffle where gp_segment_id=2;

Alter table r1_reshuffle set with (reshuffle);

Select count(*) from r1_reshuffle;
Select count(*) > 0 from r1_reshuffle where gp_segment_id=2;

select numsegments from gp_distribution_policy where localoid='r1_reshuffle'::regclass;
drop table r1_reshuffle;

Create table r1_reshuffle(a int, b int, c int) distributed randomly partition by list(b) (partition r1_reshuffle_1 values(1), partition r1_reshuffle_2 values(2), default partition other);
update gp_distribution_policy set numsegments = 2 where localoid='r1_reshuffle_1_prt_r1_reshuffle_1'::regclass;
update gp_distribution_policy set numsegments = 2 where localoid='r1_reshuffle_1_prt_r1_reshuffle_2'::regclass;
update gp_distribution_policy set numsegments = 2 where localoid='r1_reshuffle_1_prt_other'::regclass;
update gp_distribution_policy set numsegments = 2 where localoid='r1_reshuffle'::regclass;
insert into r1_reshuffle select i,i,0 from generate_series(1,100) I;

Select count(*) from r1_reshuffle;
Select count(*) > 0 from r1_reshuffle where gp_segment_id=2;

begin;
Alter table r1_reshuffle set with (reshuffle);
Select count(*) from r1_reshuffle;
Select count(*) > 0 from r1_reshuffle where gp_segment_id=2;
abort;

Select count(*) from r1_reshuffle;
Select count(*) > 0 from r1_reshuffle where gp_segment_id=2;

Alter table r1_reshuffle set with (reshuffle);

Select count(*) from r1_reshuffle;
Select count(*) > 0 from r1_reshuffle where gp_segment_id=2;

select numsegments from gp_distribution_policy where localoid='r1_reshuffle'::regclass;
drop table r1_reshuffle;

-- Replicated tables
-- We have to make sure replicated table successfully reshuffled.
-- Currently we could only connect to the specific segments in utility mode
-- to see if the data is consistent there. We create some python function here.
-- The case can only work under the assumption: it's running on 3-segment cluster
-- in a single machine.
-- start_ignore
create language plpythonu;
-- end_ignore
create function update_on_segment(tabname text, segid int, numseg int) returns boolean as
$$
import pygresql.pg as pg
conn = pg.connect(dbname='regression')
port = conn.query("select port from gp_segment_configuration where content = %d and role = 'p'" % segid).getresult()[0][0]
conn.close()

conn = pg.connect(dbname='regression', opt='-c gp_session_role=utility', port=port)
conn.query("set allow_system_table_mods = true")
conn.query("update gp_distribution_policy set numsegments = %d where localoid = '%s'::regclass" % (numseg, tabname))
conn.close()

return True
$$
LANGUAGE plpythonu;

create function select_on_segment(sql text, segid int) returns int as
$$
import pygresql.pg as pg
conn = pg.connect(dbname='regression')
port = conn.query("select port from gp_segment_configuration where content = %d and role = 'p'" % segid).getresult()[0][0]
conn.close()

conn = pg.connect(dbname='regression', opt='-c gp_session_role=utility', port=port)
r = conn.query(sql).getresult()
conn.close()

return r[0][0]
$$
LANGUAGE plpythonu;

Create table r1_reshuffle(a int, b int, c int) distributed replicated;
update gp_distribution_policy  set numsegments=1 where localoid='r1_reshuffle'::regclass;
select update_on_segment('r1_reshuffle', 0, 1);
select update_on_segment('r1_reshuffle', 1, 1);
select update_on_segment('r1_reshuffle', 2, 1);
insert into r1_reshuffle select i,i,0 from generate_series(1,100) I;

Select count(*) from r1_reshuffle;
Select select_on_segment('Select count(*) from r1_reshuffle;', 1);
Select select_on_segment('Select count(*) from r1_reshuffle;', 2);

begin;
Alter table r1_reshuffle set with (reshuffle);
Select count(*) from r1_reshuffle;
abort;

Select count(*) from r1_reshuffle;
Select select_on_segment('Select count(*) from r1_reshuffle;', 1);
Select select_on_segment('Select count(*) from r1_reshuffle;', 2);

Alter table r1_reshuffle set with (reshuffle);

Select count(*) from r1_reshuffle;
Select select_on_segment('Select count(*) from r1_reshuffle;', 1);
Select select_on_segment('Select count(*) from r1_reshuffle;', 2);

select numsegments from gp_distribution_policy where localoid='r1_reshuffle'::regclass;
drop table r1_reshuffle;

Create table r1_reshuffle(a int, b int, c int) with OIDS distributed replicated;
update gp_distribution_policy  set numsegments=2 where localoid='r1_reshuffle'::regclass;
select update_on_segment('r1_reshuffle', 0, 2);
select update_on_segment('r1_reshuffle', 1, 2);
select update_on_segment('r1_reshuffle', 2, 2);
insert into r1_reshuffle select i,i,0 from generate_series(1,100) I;

Select count(*) from r1_reshuffle;
Select select_on_segment('Select count(*) from r1_reshuffle;', 1);
Select select_on_segment('Select count(*) from r1_reshuffle;', 2);

begin;
Alter table r1_reshuffle set with (reshuffle);
Select count(*) from r1_reshuffle;
abort;

Select count(*) from r1_reshuffle;
Select select_on_segment('Select count(*) from r1_reshuffle;', 1);
Select select_on_segment('Select count(*) from r1_reshuffle;', 2);

Alter table r1_reshuffle set with (reshuffle);

Select count(*) from r1_reshuffle;
Select select_on_segment('Select count(*) from r1_reshuffle;', 1);
Select select_on_segment('Select count(*) from r1_reshuffle;', 2);

select numsegments from gp_distribution_policy where localoid='r1_reshuffle'::regclass;
drop table r1_reshuffle;

-- table with update triggers on distributed key column
CREATE OR REPLACE FUNCTION trigger_func() RETURNS trigger LANGUAGE plpgsql AS '
BEGIN
	RAISE NOTICE ''trigger_func(%) called: action = %, when = %, level = %'', TG_ARGV[0], TG_OP, TG_WHEN, TG_LEVEL;
	RETURN NULL;
END;';

CREATE TABLE table_with_update_trigger(a int, b int, c int);
update gp_distribution_policy set numsegments=2 where localoid='table_with_update_trigger'::regclass;
insert into table_with_update_trigger select i,i,0 from generate_series(1,100) I;
select gp_segment_id, count(*) from table_with_update_trigger group by 1 order by 1;

CREATE TRIGGER foo_br_trigger BEFORE INSERT OR UPDATE OR DELETE ON table_with_update_trigger 
FOR EACH ROW EXECUTE PROCEDURE trigger_func('before_stmt');
CREATE TRIGGER foo_ar_trigger AFTER INSERT OR UPDATE OR DELETE ON table_with_update_trigger 
FOR EACH ROW EXECUTE PROCEDURE trigger_func('before_stmt');

CREATE TRIGGER foo_bs_trigger BEFORE INSERT OR UPDATE OR DELETE ON table_with_update_trigger 
FOR EACH STATEMENT EXECUTE PROCEDURE trigger_func('before_stmt');
CREATE TRIGGER foo_as_trigger AFTER INSERT OR UPDATE OR DELETE ON table_with_update_trigger 
FOR EACH STATEMENT EXECUTE PROCEDURE trigger_func('before_stmt');

-- update should fail
update table_with_update_trigger set a = a + 1;
-- reshuffle should success and not hiting any triggers.
Alter table table_with_update_trigger set with (reshuffle);
select gp_segment_id, count(*) from table_with_update_trigger group by 1 order by 1;
--
-- Test reshuffle inheritance parent table, parent table has different numsegments with
-- child tables.
--
create table mix_base_tbl (a int4, b int4) distributed by (a);
update gp_distribution_policy  set numsegments=2 where localoid='mix_base_tbl'::regclass;
insert into mix_base_tbl select g, g from generate_series(1, 3) g;
create table mix_child_a (a int4, b int4) inherits (mix_base_tbl) distributed by (a);
insert into mix_child_a select g, g from generate_series(11, 13) g;
create table mix_child_b (a int4, b int4) inherits (mix_base_tbl) distributed by (b);
update gp_distribution_policy  set numsegments=2 where localoid='mix_child_b'::regclass;
insert into mix_child_b select g, g from generate_series(21, 23) g;
select gp_segment_id, * from mix_base_tbl order by 2, 1;
-- update distributed column, numsegments does not change
update mix_base_tbl set a=a+1;
select gp_segment_id, * from mix_base_tbl order by 2, 1;
-- reshuffle the parent table, both parent and child table will be rebalanced to all
-- segments
Alter table mix_base_tbl set with (reshuffle);
select gp_segment_id, * from mix_base_tbl order by 2, 1;
