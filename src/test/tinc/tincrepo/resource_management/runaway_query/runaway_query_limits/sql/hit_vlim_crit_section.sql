-- @Description One session allocating > VLIM in critical section, can proceed
-- @author George Caragea
-- @vlimMB 900 
-- @slimMB 0

-- content/segment = 0; size = 901MB; sleep = 0 sec; crit_section = true
select gp_allocate_palloc_test_one_seg(0, 901 * 1024 * 1024, 0, true);
