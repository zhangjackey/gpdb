--create normal tables
create table t1(c1 int, c2 int);
create table r1(c1 int, c2 int) distributed randomly;

insert into t1 select i,i from generate_series(1,2)i;
insert into r1 select i,i from generate_series(1,2)i;
--check the plan
explain update t1 set c2 = t1.c2 + 1 from r1 where t1.c1 = r1.c1;
update t1 set c2 = t1.c2 + 1 from r1 where t1.c1 = r1.c1;
select * from t1;
drop table t1;
drop table r1;

-- create inherits tables
create table t1(c1 int, c2 int);
create table t1h(c1 int, c2 int) inherits (t1);
create table r1(c1 int, c2 int) distributed randomly;
insert into t1 select i,i from generate_series(1,2)i;
insert into t1h select i,i from generate_series(1,2)i;
insert into r1 select i,i from generate_series(1,2)i;
explain update t1 set c2 = t1.c2 + 1 from r1 where t1.c1 = r1.c1;
update t1 set c2 = t1.c2 + 1 from r1 where t1.c1 = r1.c1;
select * from t1;
drop table t1h;
drop table t1;
drop table r1;

-- create partition tables

create table part_t1(c1 int, c2 int, c3 int) partition by range(c3) 
	( START (1) END (2) EVERY (1),
    DEFAULT PARTITION other_c3);
create table r1(c1 int, c2 int) distributed randomly;
insert into r1 select i,i from generate_series(1,2)i;
insert into part_t1 select i,i,i from generate_series(1,2)i;
explain update part_t1 set c2 = part_t1.c2 + 1 from r1 where part_t1.c1 = r1.c1;
update part_t1 set c2 = part_t1.c2 + 1 from r1 where part_t1.c1 = r1.c1;
select * from part_t1;
drop table r1;
drop table part_t1;

