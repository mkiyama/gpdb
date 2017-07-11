-- test1: test gp_toolkit.gp_resgroup_status and pg_stat_activity
-- create a resource group when gp_resource_manager is queue
DROP ROLE IF EXISTS role_concurrency_test;
-- start_ignore
DROP RESOURCE GROUP rg_concurrency_test;
-- end_ignore
CREATE RESOURCE GROUP rg_concurrency_test WITH (concurrency=2, cpu_rate_limit=20, memory_limit=20);
CREATE ROLE role_concurrency_test RESOURCE GROUP rg_concurrency_test;

-- no query has been assigned to the this group

SELECT r.rsgname, num_running, num_queueing, num_queued, num_executed FROM gp_toolkit.gp_resgroup_status s, pg_resgroup r WHERE s.groupid=r.oid AND r.rsgname='rg_concurrency_test';
2:SET ROLE role_concurrency_test;
2:BEGIN;
3:SET ROLE role_concurrency_test;
3:BEGIN;
4:SET ROLE role_concurrency_test;
4&:BEGIN;

-- new transaction will be blocked when the concurrency limit of the resource group is reached.
SELECT r.rsgname, num_running, num_queueing, num_queued, num_executed FROM gp_toolkit.gp_resgroup_status s, pg_resgroup r WHERE s.groupid=r.oid AND r.rsgname='rg_concurrency_test';
SELECT waiting_reason, rsgqueueduration > '0'::interval as time from pg_stat_activity where current_query = 'BEGIN;' and rsgname = 'rg_concurrency_test';
2:END;
3:END;
4<:
4:END;
2q:
3q:
4q:
SELECT r.rsgname, num_running, num_queueing, num_queued, num_executed FROM gp_toolkit.gp_resgroup_status s, pg_resgroup r WHERE s.groupid=r.oid AND r.rsgname='rg_concurrency_test';
DROP ROLE role_concurrency_test;
DROP RESOURCE GROUP rg_concurrency_test;

-- test2: test alter concurrency
-- Create a resource group with concurrency=2. Prepare 2 running transactions and 1 queueing transactions.
-- Alter concurrency 2->3, the queueing transaction will be woken up, the 'value' and 'proposed' of pg_resgroupcapability will be set to 3.
DROP ROLE IF EXISTS role_concurrency_test;
-- start_ignore
DROP RESOURCE GROUP rg_concurrency_test;
-- end_ignore
CREATE RESOURCE GROUP rg_concurrency_test WITH (concurrency=2, cpu_rate_limit=20, memory_limit=20);
CREATE ROLE role_concurrency_test RESOURCE GROUP rg_concurrency_test;
12:SET ROLE role_concurrency_test;
12:BEGIN;
13:SET ROLE role_concurrency_test;
13:BEGIN;
14:SET ROLE role_concurrency_test;
14&:BEGIN;
SELECT r.rsgname, num_running, num_queueing, num_queued, num_executed FROM gp_toolkit.gp_resgroup_status s, pg_resgroup r WHERE s.groupid=r.oid AND r.rsgname='rg_concurrency_test';
SELECT concurrency,proposed_concurrency FROM gp_toolkit.gp_resgroup_config WHERE groupname='rg_concurrency_test';
ALTER RESOURCE GROUP rg_concurrency_test SET CONCURRENCY 3;
SELECT r.rsgname, num_running, num_queueing, num_queued, num_executed FROM gp_toolkit.gp_resgroup_status s, pg_resgroup r WHERE s.groupid=r.oid AND r.rsgname='rg_concurrency_test';
SELECT concurrency,proposed_concurrency FROM gp_toolkit.gp_resgroup_config WHERE groupname='rg_concurrency_test';
12:END;
13:END;
14<:
14:END;
12q:
13q:
14q:
DROP ROLE role_concurrency_test;
DROP RESOURCE GROUP rg_concurrency_test;

-- test3: test alter concurrency
-- Create a resource group with concurrency=3. Prepare 3 running transactions, and 1 queueing transaction.
DROP ROLE IF EXISTS role_concurrency_test;
-- start_ignore
DROP RESOURCE GROUP rg_concurrency_test;
-- end_ignore
CREATE RESOURCE GROUP rg_concurrency_test WITH (concurrency=3, cpu_rate_limit=20, memory_limit=20);
CREATE ROLE role_concurrency_test RESOURCE GROUP rg_concurrency_test;
22:SET ROLE role_concurrency_test;
22:BEGIN;
23:SET ROLE role_concurrency_test;
23:BEGIN;
24:SET ROLE role_concurrency_test;
24:BEGIN;
25:SET ROLE role_concurrency_test;
25&:BEGIN;
SELECT r.rsgname, num_running, num_queueing, num_queued, num_executed FROM gp_toolkit.gp_resgroup_status s, pg_resgroup r WHERE s.groupid=r.oid AND r.rsgname='rg_concurrency_test';
SELECT concurrency,proposed_concurrency FROM gp_toolkit.gp_resgroup_config WHERE groupname='rg_concurrency_test';
-- Alter concurrency 3->2, the 'proposed' of pg_resgroupcapability will be set to 2.
ALTER RESOURCE GROUP rg_concurrency_test SET CONCURRENCY 2;
SELECT concurrency,proposed_concurrency FROM gp_toolkit.gp_resgroup_config WHERE groupname='rg_concurrency_test';
-- When one transaction is finished, queueing transaction won't be woken up. There're 2 running transactions and 1 queueing transaction.
24:END;
SELECT r.rsgname, num_running, num_queueing, num_queued, num_executed FROM gp_toolkit.gp_resgroup_status s, pg_resgroup r WHERE s.groupid=r.oid AND r.rsgname='rg_concurrency_test';
-- New transaction will be queued, there're 2 running and 2 queueing transactions.
24&:BEGIN;
SELECT r.rsgname, num_running, num_queueing, num_queued, num_executed FROM gp_toolkit.gp_resgroup_status s, pg_resgroup r WHERE s.groupid=r.oid AND r.rsgname='rg_concurrency_test';
-- Finish another transaction, one queueing transaction will be woken up, there're 2 running transactions and 1 queueing transaction.
22:END;
SELECT r.rsgname, num_running, num_queueing, num_queued, num_executed FROM gp_toolkit.gp_resgroup_status s, pg_resgroup r WHERE s.groupid=r.oid AND r.rsgname='rg_concurrency_test';
-- Alter concurrency 2->2, the 'value' and 'proposed' of pg_resgroupcapability will be set to 2.
ALTER RESOURCE GROUP rg_concurrency_test SET CONCURRENCY 2;
SELECT concurrency,proposed_concurrency FROM gp_toolkit.gp_resgroup_config WHERE groupname='rg_concurrency_test';
-- Finish another transaction, one queueing transaction will be woken up, there're 2 running transactions and 0 queueing transaction.
23:END;
SELECT r.rsgname, num_running, num_queueing, num_queued, num_executed FROM gp_toolkit.gp_resgroup_status s, pg_resgroup r WHERE s.groupid=r.oid AND r.rsgname='rg_concurrency_test';
24<:
25<:
25:END;
24:END;
22q:
23q:
24q:
25q:
DROP ROLE role_concurrency_test;
DROP RESOURCE GROUP rg_concurrency_test;

-- test4: concurrently drop resource group

DROP ROLE IF EXISTS role_concurrency_test;
-- start_ignore
DROP RESOURCE GROUP rg_concurrency_test;
-- end_ignore
CREATE RESOURCE GROUP rg_concurrency_test WITH (concurrency=2, cpu_rate_limit=20, memory_limit=20);
CREATE ROLE role_concurrency_test RESOURCE GROUP rg_concurrency_test;

-- DROP should fail if there're running transactions
32:SET ROLE role_concurrency_test;
32:BEGIN;
BEGIN;
DROP ROLE role_concurrency_test;
DROP RESOURCE GROUP rg_concurrency_test;
END;
32:END;
32:RESET ROLE;

-- DROP is abortted
BEGIN;
DROP ROLE role_concurrency_test;
DROP RESOURCE GROUP rg_concurrency_test;
SELECT r.rsgname, num_running, num_queueing, num_queued, num_executed FROM gp_toolkit.gp_resgroup_status s, pg_resgroup r WHERE s.groupid=r.oid AND r.rsgname='rg_concurrency_test';
32:SET ROLE role_concurrency_test;
32&:BEGIN;
ABORT;
32<:
32:SELECT r.rsgname, num_running, num_queueing, num_queued, num_executed FROM gp_toolkit.gp_resgroup_status s, pg_resgroup r WHERE s.groupid=r.oid AND r.rsgname='rg_concurrency_test';
32:END;
32:RESET ROLE;

-- DROP is committed
BEGIN;
DROP ROLE role_concurrency_test;
DROP RESOURCE GROUP rg_concurrency_test;
SELECT r.rsgname, num_running, num_queueing, num_queued, num_executed FROM gp_toolkit.gp_resgroup_status s, pg_resgroup r WHERE s.groupid=r.oid AND r.rsgname='rg_concurrency_test';
32:SET ROLE role_concurrency_test;
32&:BEGIN;
END;
32<:
32q:
SELECT r.rsgname, num_running, num_queueing, num_queued, num_executed FROM gp_toolkit.gp_resgroup_status s, pg_resgroup r WHERE s.groupid=r.oid AND r.rsgname='rg_concurrency_test';

DROP ROLE IF EXISTS role_concurrency_test;
DROP RESOURCE GROUP rg_concurrency_test;

-- test5: concurrently alter resource group cpu rate limit

-- start_ignore
DROP RESOURCE GROUP rg1_concurrency_test;
DROP RESOURCE GROUP rg2_concurrency_test;
-- end_ignore

CREATE RESOURCE GROUP rg1_concurrency_test WITH (concurrency=2, cpu_rate_limit=10, memory_limit=20);
CREATE RESOURCE GROUP rg2_concurrency_test WITH (concurrency=2, cpu_rate_limit=20, memory_limit=20);

41:BEGIN;
41:ALTER RESOURCE GROUP rg1_concurrency_test SET CPU_RATE_LIMIT 35;
42:BEGIN;
42&:ALTER RESOURCE GROUP rg2_concurrency_test SET CPU_RATE_LIMIT 35;
41:ABORT;
42<:
42:COMMIT;
SELECT g.rsgname, c.cpu_rate_limit FROM gp_toolkit.gp_resgroup_config c, pg_resgroup g WHERE c.groupid=g.oid ORDER BY g.oid;

DROP RESOURCE GROUP rg1_concurrency_test;
DROP RESOURCE GROUP rg2_concurrency_test;

CREATE RESOURCE GROUP rg1_concurrency_test WITH (concurrency=2, cpu_rate_limit=10, memory_limit=20);
CREATE RESOURCE GROUP rg2_concurrency_test WITH (concurrency=2, cpu_rate_limit=20, memory_limit=20);

41:BEGIN;
41:ALTER RESOURCE GROUP rg1_concurrency_test SET CPU_RATE_LIMIT 35;
42:BEGIN;
42&:ALTER RESOURCE GROUP rg2_concurrency_test SET CPU_RATE_LIMIT 35;
41:COMMIT;
42<:
41q:
42q:
SELECT g.rsgname, c.cpu_rate_limit FROM gp_toolkit.gp_resgroup_config c, pg_resgroup g WHERE c.groupid=g.oid ORDER BY g.oid;

DROP RESOURCE GROUP rg1_concurrency_test;
DROP RESOURCE GROUP rg2_concurrency_test;


-- test6: cancel a query that is waiting for a slot
DROP ROLE IF EXISTS role_concurrency_test;
-- start_ignore
DROP RESOURCE GROUP rg_concurrency_test;
-- end_ignore

CREATE RESOURCE GROUP rg_concurrency_test WITH (concurrency=1, cpu_rate_limit=20, memory_limit=20);
CREATE ROLE role_concurrency_test RESOURCE GROUP rg_concurrency_test;
51:SET ROLE role_concurrency_test;
51:BEGIN;
52:SET ROLE role_concurrency_test;
52&:BEGIN;
SELECT pg_cancel_backend(procpid) FROM pg_stat_activity WHERE waiting_reason='resgroup' AND rsgname='rg_concurrency_test';
52<:
52&:BEGIN;
SELECT pg_cancel_backend(procpid) FROM pg_stat_activity WHERE waiting_reason='resgroup' AND rsgname='rg_concurrency_test';
52<:
51q:
52q:
DROP ROLE role_concurrency_test;
DROP RESOURCE GROUP rg_concurrency_test;

-- test7: terminate a query that is waiting for a slot
DROP ROLE IF EXISTS role_concurrency_test;
-- start_ignore
DROP RESOURCE GROUP rg_concurrency_test;
-- end_ignore

CREATE RESOURCE GROUP rg_concurrency_test WITH (concurrency=1, cpu_rate_limit=20, memory_limit=20);
CREATE ROLE role_concurrency_test RESOURCE GROUP rg_concurrency_test;
61:SET ROLE role_concurrency_test;
61:BEGIN;
62:SET ROLE role_concurrency_test;
62&:BEGIN;
SELECT pg_terminate_backend(procpid) FROM pg_stat_activity WHERE waiting_reason='resgroup' AND rsgname='rg_concurrency_test';
62<:
61q:
62q:
DROP ROLE role_concurrency_test;
DROP RESOURCE GROUP rg_concurrency_test;
