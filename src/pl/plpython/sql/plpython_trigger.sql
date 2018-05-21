-- these triggers are dedicated to HPHC of RI who
-- decided that my kid's name was william not willem, and
-- vigorously resisted all efforts at correction.  they have
-- since gone bankrupt...

CREATE FUNCTION users_insert() returns trigger
	AS
'if TD["new"]["fname"] == None or TD["new"]["lname"] == None:
	return "SKIP"
if TD["new"]["username"] == None:
	TD["new"]["username"] = TD["new"]["fname"][:1] + "_" + TD["new"]["lname"]
	rv = "MODIFY"
else:
	rv = None
if TD["new"]["fname"] == "william":
	TD["new"]["fname"] = TD["args"][0]
	rv = "MODIFY"
return rv'
	LANGUAGE plpythonu;


CREATE FUNCTION users_update() returns trigger
	AS
'if TD["event"] == "UPDATE":
	if TD["old"]["fname"] != TD["new"]["fname"] and TD["old"]["fname"] == TD["args"][0]:
		return "SKIP"
return None'
	LANGUAGE plpythonu;


CREATE FUNCTION users_delete() RETURNS trigger
	AS
'if TD["old"]["fname"] == TD["args"][0]:
	return "SKIP"
return None'
	LANGUAGE plpythonu;


CREATE TRIGGER users_insert_trig BEFORE INSERT ON users FOR EACH ROW
	EXECUTE PROCEDURE users_insert ('willem');

CREATE TRIGGER users_update_trig BEFORE UPDATE ON users FOR EACH ROW
	EXECUTE PROCEDURE users_update ('willem');

CREATE TRIGGER users_delete_trig BEFORE DELETE ON users FOR EACH ROW
	EXECUTE PROCEDURE users_delete ('willem');


-- quick peek at the table
--
SELECT * FROM users;

-- should fail
--
UPDATE users SET fname = 'william' WHERE fname = 'willem';

-- should modify william to willem and create username
--
INSERT INTO users (fname, lname) VALUES ('william', 'smith');
INSERT INTO users (fname, lname, username) VALUES ('charles', 'darwin', 'beagle');

SELECT * FROM users;


-- dump trigger data

CREATE TABLE trigger_test
	(i int, v text );

CREATE FUNCTION trigger_data() RETURNS trigger LANGUAGE plpythonu AS $$

if 'relid' in TD:
	TD['relid'] = "bogus:12345"

skeys = list(TD.keys())
skeys.sort()
for key in skeys:
	val = TD[key]
	plpy.notice("TD[" + key + "] => " + str(val))

return None

$$;

CREATE TRIGGER show_trigger_data_trig_before
BEFORE INSERT OR UPDATE OR DELETE ON trigger_test
FOR EACH ROW EXECUTE PROCEDURE trigger_data(23,'skidoo');

CREATE TRIGGER show_trigger_data_trig_after
AFTER INSERT OR UPDATE OR DELETE ON trigger_test
FOR EACH ROW EXECUTE PROCEDURE trigger_data(23,'skidoo');

CREATE TRIGGER show_trigger_data_trig_stmt
BEFORE INSERT OR UPDATE OR DELETE OR TRUNCATE ON trigger_test
FOR EACH STATEMENT EXECUTE PROCEDURE trigger_data(23,'skidoo');

insert into trigger_test values(1,'insert');
update trigger_test set v = 'update' where i = 1;
delete from trigger_test;
truncate table trigger_test;

DROP TRIGGER show_trigger_data_trig_stmt on trigger_test;
DROP TRIGGER show_trigger_data_trig_before on trigger_test;
DROP TRIGGER show_trigger_data_trig_after on trigger_test;

insert into trigger_test values(1,'insert');
CREATE VIEW trigger_test_view AS SELECT * FROM trigger_test;

--start_ignore
-- INSTEAD OF triggers are not yet supported in Greenplum
CREATE TRIGGER show_trigger_data_trig
INSTEAD OF INSERT OR UPDATE OR DELETE ON trigger_test_view
FOR EACH ROW EXECUTE PROCEDURE trigger_data(24,'skidoo view');

insert into trigger_test_view values(2,'insert');
update trigger_test_view set v = 'update' where i = 1;
delete from trigger_test_view;

DROP FUNCTION trigger_data() CASCADE;
--end_ignore
DROP VIEW trigger_test_view;
delete from trigger_test;


--
-- trigger error handling
--

INSERT INTO trigger_test VALUES (0, 'zero');


-- returning non-string from trigger function

CREATE FUNCTION stupid1() RETURNS trigger
AS $$
    return 37
$$ LANGUAGE plpythonu;

CREATE TRIGGER stupid_trigger1
BEFORE INSERT ON trigger_test
FOR EACH ROW EXECUTE PROCEDURE stupid1();

INSERT INTO trigger_test VALUES (1, 'one');

DROP TRIGGER stupid_trigger1 ON trigger_test;


-- returning MODIFY from DELETE trigger

CREATE FUNCTION stupid2() RETURNS trigger
AS $$
    return "MODIFY"
$$ LANGUAGE plpythonu;

CREATE TRIGGER stupid_trigger2
BEFORE DELETE ON trigger_test
FOR EACH ROW EXECUTE PROCEDURE stupid2();

DELETE FROM trigger_test WHERE i = 0;

DROP TRIGGER stupid_trigger2 ON trigger_test;

INSERT INTO trigger_test VALUES (0, 'zero');


-- returning unrecognized string from trigger function

CREATE FUNCTION stupid3() RETURNS trigger
AS $$
    return "foo"
$$ LANGUAGE plpythonu;

CREATE TRIGGER stupid_trigger3
BEFORE UPDATE ON trigger_test
FOR EACH ROW EXECUTE PROCEDURE stupid3();

UPDATE trigger_test SET v = 'null' WHERE i = 0;

DROP TRIGGER stupid_trigger3 ON trigger_test;


-- Unicode variant

CREATE FUNCTION stupid3u() RETURNS trigger
AS $$
    return u"foo"
$$ LANGUAGE plpythonu;

CREATE TRIGGER stupid_trigger3
BEFORE UPDATE ON trigger_test
FOR EACH ROW EXECUTE PROCEDURE stupid3u();

UPDATE trigger_test SET v = 'null' WHERE i = 0;

DROP TRIGGER stupid_trigger3 ON trigger_test;


-- deleting the TD dictionary

CREATE FUNCTION stupid4() RETURNS trigger
AS $$
    del TD["new"]
    return "MODIFY";
$$ LANGUAGE plpythonu;

CREATE TRIGGER stupid_trigger4
BEFORE UPDATE ON trigger_test
FOR EACH ROW EXECUTE PROCEDURE stupid4();

UPDATE trigger_test SET v = 'null' WHERE i = 0;

DROP TRIGGER stupid_trigger4 ON trigger_test;


-- TD not a dictionary

CREATE FUNCTION stupid5() RETURNS trigger
AS $$
    TD["new"] = ['foo', 'bar']
    return "MODIFY";
$$ LANGUAGE plpythonu;

CREATE TRIGGER stupid_trigger5
BEFORE UPDATE ON trigger_test
FOR EACH ROW EXECUTE PROCEDURE stupid5();

UPDATE trigger_test SET v = 'null' WHERE i = 0;

DROP TRIGGER stupid_trigger5 ON trigger_test;


-- TD not having string keys

CREATE FUNCTION stupid6() RETURNS trigger
AS $$
    TD["new"] = {1: 'foo', 2: 'bar'}
    return "MODIFY";
$$ LANGUAGE plpythonu;

CREATE TRIGGER stupid_trigger6
BEFORE UPDATE ON trigger_test
FOR EACH ROW EXECUTE PROCEDURE stupid6();

UPDATE trigger_test SET v = 'null' WHERE i = 0;

DROP TRIGGER stupid_trigger6 ON trigger_test;


-- TD keys not corresponding to row columns

CREATE FUNCTION stupid7() RETURNS trigger
AS $$
    TD["new"] = {'a': 'foo', 'b': 'bar'}
    return "MODIFY";
$$ LANGUAGE plpythonu;

CREATE TRIGGER stupid_trigger7
BEFORE UPDATE ON trigger_test
FOR EACH ROW EXECUTE PROCEDURE stupid7();

UPDATE trigger_test SET v = 'null' WHERE i = 0;

DROP TRIGGER stupid_trigger7 ON trigger_test;


-- Unicode variant

CREATE FUNCTION stupid7u() RETURNS trigger
AS $$
    TD["new"] = {u'a': 'foo', u'b': 'bar'}
    return "MODIFY"
$$ LANGUAGE plpythonu;

CREATE TRIGGER stupid_trigger7
BEFORE UPDATE ON trigger_test
FOR EACH ROW EXECUTE PROCEDURE stupid7u();

UPDATE trigger_test SET v = 'null' WHERE i = 0;

DROP TRIGGER stupid_trigger7 ON trigger_test;


-- calling a trigger function directly

SELECT stupid7();


--
-- Null values
--

SELECT * FROM trigger_test;

CREATE FUNCTION test_null() RETURNS trigger
AS $$
    TD["new"]['v'] = None
    return "MODIFY"
$$ LANGUAGE plpythonu;

CREATE TRIGGER test_null_trigger
BEFORE UPDATE ON trigger_test
FOR EACH ROW EXECUTE PROCEDURE test_null();

UPDATE trigger_test SET v = 'null' WHERE i = 0;

DROP TRIGGER test_null_trigger ON trigger_test;

SELECT * FROM trigger_test;


--
-- Test that triggers honor typmod when assigning to tuple fields,
-- as per an early 9.0 bug report
--

SET DateStyle = 'ISO';

CREATE FUNCTION set_modif_time() RETURNS trigger AS $$
    TD['new']['modif_time'] = '2010-10-13 21:57:28.930486'
    return 'MODIFY'
$$ LANGUAGE plpythonu;

-- Add 'DISTRIBUTED RANDOMLY' to avoid "ERROR:  Cannot parallelize an UPDATE statement that updates the distribution columns"
CREATE TABLE pb (a TEXT, modif_time TIMESTAMP(0) WITHOUT TIME ZONE) DISTRIBUTED RANDOMLY;

CREATE TRIGGER set_modif_time BEFORE UPDATE ON pb
  FOR EACH ROW EXECUTE PROCEDURE set_modif_time();

INSERT INTO pb VALUES ('a', '2010-10-09 21:57:33.930486');
SELECT * FROM pb;
UPDATE pb SET a = 'b';
SELECT * FROM pb;


-- triggers for tables with composite types

CREATE TABLE comp1 (i integer, j boolean);
CREATE TYPE comp2 AS (k integer, l boolean);

CREATE TABLE composite_trigger_test (f1 comp1, f2 comp2);

CREATE FUNCTION composite_trigger_f() RETURNS trigger AS $$
    TD['new']['f1'] = (3, False)
    TD['new']['f2'] = {'k': 7, 'l': 'yes', 'ignored': 10}
    return 'MODIFY'
$$ LANGUAGE plpythonu;

CREATE TRIGGER composite_trigger BEFORE INSERT ON composite_trigger_test
  FOR EACH ROW EXECUTE PROCEDURE composite_trigger_f();

INSERT INTO composite_trigger_test VALUES (NULL, NULL);
SELECT * FROM composite_trigger_test;
