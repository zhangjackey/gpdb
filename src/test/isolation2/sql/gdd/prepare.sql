-- start_ignore
! gpconfig -c gp_enable_global_deadlock_detector -v on;
! gpstop -rai;
-- end_ignore
-- t0r is the reference table to provide the data distribution info.
DROP TABLE IF EXISTS t0p;
CREATE TABLE t0p (id int, val int);
INSERT INTO t0p (id, val) SELECT i, i FROM generate_series(1, 20) i;

DROP TABLE IF EXISTS t0r;
CREATE TABLE t0r (id int, val int, segid int) DISTRIBUTED REPLICATED;
INSERT INTO t0r (id, val, segid) SELECT id, val, gp_segment_id from t0p;

-- GDD tests rely on the data distribution, but depends on the number of
-- the segments the distribution might be different.
-- so we provide this helper function to return the nth id on a segment.
-- * `seg` is the segment id, starts from 0;
-- * `idx` is the index on the segment, starts from 1;
CREATE OR REPLACE FUNCTION segid(seg int, idx int)
RETURNS int AS $$
  SELECT id FROM t0r
  WHERE segid=$1
  ORDER BY id LIMIT 1 OFFSET ($2-1)
$$ LANGUAGE sql;

-- In some of the testcases the execution order of two background queries
-- must be enforced not only on master but also on segments, for example
-- in below case the order of 10 and 20 on segments results in different
-- waiting relations:
--
--     30: UPDATE t SET val=val WHERE id=1;
--     10&: UPDATE t SET val=val WHERE val=1;
--     20&: UPDATE t SET val=val WHERE val=1;
--
-- There is no perfect way to ensure this.  The '&' command in the isolation2
-- framework only ensures that the QD is being blocked, but this might not be
-- true on segments.  In fact on slow machines this exception occurs quite
-- offen on heave load. (e.g. when multiple testcases are executed in parallel)
--
-- So we provide this barrier function to ensure the execution order.
-- It's implemented with sleep now, but should at least work.
CREATE OR REPLACE FUNCTION barrier()
RETURNS void AS $$
  SELECT pg_sleep(4)
$$ LANGUAGE sql;

-- verify the function
-- Data distribution is sensitive to the underlying hash algorithm.
SELECT segid(0,1);
SELECT segid(0,2);
SELECT segid(1,1);
SELECT segid(1,2);

-- start_ignore
! gpconfig -c gp_global_deadlock_detector_period -v 10;
! gpstop -u;
-- end_ignore

-- the new setting need some time to be loaded
SELECT pg_sleep(2);

SHOW gp_global_deadlock_detector_period;
