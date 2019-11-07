drop table if exists olap_test;
drop table if exists olap_test_single;

create table olap_test(a int4, b int4, c int4, d int4) distributed by (a, b);
insert into olap_test select g / 5000, g / 1000, g / 500, g from generate_series(1, 10000) g;
analyze olap_test;

create table olap_test_single(a int4, b int4, c int4, d int4) distributed by (a);
insert into olap_test_single select g / 5000, g / 1000, g / 500, g from generate_series(1, 10000) g;
analyze olap_test;

-- If the GROUP BY is a superset of the table's distribution keys, the
-- aggregation can be independently in segments, and just gather the
-- results. (1-phase agg)
explain select a, b, sum(d) from olap_test group by a, b;
select a, b, sum(d) from olap_test group by a, b;

explain select a, b, c, sum(d) from olap_test group by a, b, c;
select a, b, c, sum(d) from olap_test group by a, b, c;

-- If it's not a superset, redistribution is needed.
explain select a, sum(d) from olap_test group by a;
select a, sum(d) from olap_test group by a;

-- Two-phase plain agg
explain select sum(d) from olap_test;
select sum(d) from olap_test;


--
-- DISTINCT aggregates
--

-- If the DISTINCT argument matches the distribution key, two-stage aggregation
-- is possible.

explain select sum(distinct a) from olap_test_single;
select sum(distinct a) from olap_test_single;

-- Otherwise, need a more complicated plans
explain select sum(distinct b) from olap_test_single;
select sum(distinct b) from olap_test_single;

