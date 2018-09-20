/*-------------------------------------------------------------------------
 *
 * cdbllize.h
 *	  definitions for cdbplan.c utilities
 *
 * Portions Copyright (c) 2005-2008, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/include/cdb/cdbllize.h
 *
 *
 * NOTES
 *
 *-------------------------------------------------------------------------
 */

#ifndef CDBLLIZE_H
#define CDBLLIZE_H

#include "nodes/nodes.h"
#include "nodes/parsenodes.h"
#include "nodes/plannodes.h"
#include "nodes/params.h"

extern Plan *cdbparallelize(struct PlannerInfo *root, Plan *plan, Query *query,
							int cursorOptions, 
							ParamListInfo boundParams);

extern bool is_plan_node(Node *node);

extern Flow *makeFlow(FlowType flotype, int numsegments);

extern Flow *pull_up_Flow(Plan *plan, Plan *subplan);

extern bool focusPlan(Plan *plan, bool stable, bool rescannable);
extern bool repartitionPlan(Plan *plan, bool stable, bool rescannable, List *hashExpr, int numsegments);
extern bool broadcastPlan(Plan *plan, bool stable, bool rescannable,
						  int numsegments);

#endif   /* CDBLLIZE_H */
