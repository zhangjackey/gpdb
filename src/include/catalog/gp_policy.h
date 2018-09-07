/*-------------------------------------------------------------------------
 *
 * gp_policy.h
 *	  definitions for the gp_distribution_policy catalog table
 *
 * Portions Copyright (c) 2005-2011, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/include/catalog/gp_policy.h
 *
 * NOTES
 *
 *-------------------------------------------------------------------------
 */

#ifndef _GP_POLICY_H_
#define _GP_POLICY_H_

#include "access/attnum.h"
#include "catalog/genbki.h"
#include "nodes/pg_list.h"
#include "utils/palloc.h"

/*
 * Defines for gp_policy
 */
#define GpPolicyRelationId  5002

CATALOG(gp_distribution_policy,5002) BKI_WITHOUT_OIDS
{
	Oid			localoid;
	int2		attrnums[1];
	char		policytype; /* distribution policy type */
	int4		numsegments;
} FormData_gp_policy;

/* GPDB added foreign key definitions for gpcheckcat. */
FOREIGN_KEY(localoid REFERENCES pg_class(oid));

#define Natts_gp_policy		4
#define Anum_gp_policy_localoid	1
#define Anum_gp_policy_attrnums	2
#define Anum_gp_policy_type	3
#define Anum_gp_policy_numsegments	4

/*
 * Symbolic values for Anum_gp_policy_type column
 */
#define SYM_POLICYTYPE_PARTITIONED 'p'
#define SYM_POLICYTYPE_REPLICATED 'r'

/*
 * A magic number, setting GpPolicy.numsegments to this value will cause a
 * failed assertion at runtime, which allows developers to debug with gdb.
 */
#define __GP_POLICY_EVIL_NUMSEGMENTS		(666)

/*
 * Default numsegments for each motion type.
 */
#define GP_POLICY_ALL_NUMSEGMENTS			Max(1, getgpsegmentCount())
#define GP_POLICY_ENTRY_NUMSEGMENTS			GP_POLICY_ALL_NUMSEGMENTS
#define GP_POLICY_GATHER_NUMSEGMENTS		(1)
#define GP_POLICY_DIRECT_NUMSEGMENTS		(1)
#define GP_POLICY_UNINITIALIZED_NUMSEGMENTS	(-1)

/*
 * GpPolicyType represents a type of policy under which a relation's
 * tuples may be assigned to a component database.
 */
typedef enum GpPolicyType
{
	POLICYTYPE_PARTITIONED,		/* Tuples partitioned onto segment database. */
	POLICYTYPE_ENTRY,			/* Tuples stored on entry database. */
	POLICYTYPE_REPLICATED		/* Tuples stored a copy on all segment database. */
} GpPolicyType;

/*
 * GpPolicy represents a Greenplum DB data distribution policy. The ptype field
 * is always significant.  Other fields may be specific to a particular
 * type.
 *
 * A GpPolicy is typically palloc'd with space for nattrs integer
 * attribute numbers (attrs) in addition to sizeof(GpPolicy).
 */
typedef struct GpPolicy
{
	NodeTag         type;
	GpPolicyType ptype;
	int4		numsegments;

	/* These fields apply to POLICYTYPE_PARTITIONED. */
	int			nattrs;
	AttrNumber	*attrs;		/* pointer to the first of nattrs attribute numbers.  */
} GpPolicy;

/*
 * GpPolicyCopy -- Return a copy of a GpPolicy object.
 *
 * The copy is palloc'ed in the specified context.
 */
GpPolicy *GpPolicyCopy(MemoryContext mcxt, const GpPolicy *src);

/* GpPolicyEqual
 *
 * A field-by-field comparison just to facilitate comparing IntoClause
 * (which embeds this) in equalFuncs.c
 */
bool GpPolicyEqual(const GpPolicy *lft, const GpPolicy *rgt);

/*
 * GpPolicyFetch
 *
 * Looks up a given Oid in the gp_distribution_policy table.
 * If found, returns an GpPolicy object (palloc'd in the specified
 * context) containing the info from the gp_distribution_policy row.
 * Else returns NULL.
 *
 * The caller is responsible for passing in a valid relation oid.  This
 * function does not check and assigns a policy of type POLICYTYPE_ENTRY
 * for any oid not found in gp_distribution_policy.
 */
GpPolicy *GpPolicyFetch(MemoryContext mcxt, Oid tbloid);

/*
 * GpPolicyStore: sets the GpPolicy for a table.
 */
void GpPolicyStore(Oid tbloid, const GpPolicy *policy);

void GpPolicyReplace(Oid tbloid, const GpPolicy *policy);

void GpPolicyRemove(Oid tbloid);

bool GpPolicyIsRandomPartitioned(const GpPolicy *policy);
bool GpPolicyIsHashPartitioned(const GpPolicy *policy);
bool GpPolicyIsPartitioned(const GpPolicy *policy);
bool GpPolicyIsReplicated(const GpPolicy *policy);
bool GpPolicyIsEntry(const GpPolicy *policy);

extern GpPolicy *makeGpPolicy(MemoryContext mcxt, GpPolicyType ptype, int nattrs, int numsegments);
extern GpPolicy *createReplicatedGpPolicy(MemoryContext mcxt, int numsegments);
extern GpPolicy *createRandomPartitionedPolicy(MemoryContext mcxt, int numsegments);
extern GpPolicy *createHashPartitionedPolicy(MemoryContext mcxt, List *keys, int numsegments);

extern bool IsReplicatedTable(Oid relid);

#endif /*_GP_POLICY_H_*/
