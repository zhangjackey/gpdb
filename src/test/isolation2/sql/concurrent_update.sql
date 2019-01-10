-- Test concurrent update a table with a varying length type
CREATE TABLE t_concurrent_update(a int, b int, c char(84));
INSERT INTO t_concurrent_update VALUES(1,1,'test');

1: BEGIN;
1: SET optimizer=off;
1: UPDATE t_concurrent_update SET b=b+10 WHERE a=1;
2: SET optimizer=off;
2&: UPDATE t_concurrent_update SET b=b+10 WHERE a=1;
1: END;
2<:
1: SELECT * FROM t_concurrent_update;

1q:
2q:

DROP TABLE t_concurrent_update;

-- test update hash-column concurrently
CREATE TABLE t_concurrent_update(c1 int);

insert into t_concurrent_update select * from generate_series(1, 10);

1: BEGIN;
1: update t_concurrent_update set c1 = 999 where c1 = 1;

2: BEGIN;
2&: update t_concurrent_update set c1 = 888 where c1 = 1;

1: END;
2<:
2: END;

1q:
2q:

select count(*) from t_concurrent_update;

DROP TABLE t_concurrent_update;

-- test recheck
CREATE TABLE t1_concurrent_update(c1 int, c2 int, c3 int) distributed randomly;
CREATE TABLE t2_concurrent_update(c1 int, c2 int, c3 int) distributed randomly;

insert into t1_concurrent_update select i,i,i from generate_series(1, 10) i;
insert into t2_concurrent_update select i,i,i from generate_series(1, 2) i;

1: BEGIN;
-- lock t1_concurrent_update's c1 = 1 tuple xid
1: update t1_concurrent_update set c3 = 999 where c1 = 1;

2: BEGIN;
2&: update t1_concurrent_update set c3 = 888 from t2_concurrent_update where t1_concurrent_update.c2 = t1_concurrent_update.c2;

1: END;
2<:
2: END;

1q:
2q:

select * from t1_concurrent_update;
select * from t2_concurrent_update;

DROP TABLE t1_concurrent_update;
DROP TABLE t2_concurrent_update;

--

CREATE TABLE t1_concurrent_delete(c1 int, c2 int, c3 int) distributed randomly;
CREATE TABLE t2_concurrent_delete(c1 int, c2 int, c3 int) distributed randomly;

insert into t1_concurrent_delete select i,i,i from generate_series(1, 10) i;
insert into t2_concurrent_delete select i,i,i from generate_series(1, 2) i;

1: BEGIN;
-- lock t1_concurrent_delete's c1 = 1 tuple xid
1: delete from t1_concurrent_delete where c1 = 1;

2: BEGIN;
2&: delete from t1_concurrent_delete where t1_concurrent_delete.c2 = ANY(select c2 from t2_concurrent_delete);

1: END;
2<:
2: END;

1q:
2q:

select * from t1_concurrent_delete;
select * from t2_concurrent_delete;

-- DROP TABLE t1_concurrent_delete;
-- DROP TABLE t2_concurrent_delete;
