/*-------------------------------------------------------------------------
 *
 * nodeReshuffle.c
 *	  Support for reshuffling data in different segments size.
 *
 * DESCRIPTION
 *
 *		When we add new segments into the cluster, the table
 *		data need to reshuffle.
 *
 * Portions Copyright (c) 2005-2018, Greenplum inc.
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 * Portions Copyright (c) 1996-2011, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * IDENTIFICATION
 *	  src/backend/executor/nodeReshuffle.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "executor/executor.h"
#include "executor/nodeReshuffle.h"
#include "utils/memutils.h"

#include "cdb/cdbhash.h"
#include "cdb/cdbvars.h"
#include "cdb/memquota.h"

/*
 *  EvalHashSegID
 *
 * 	compute the Hash keys
 */
static int
EvalHashSegID(Datum *values, bool *nulls, List *policyAttrs,
			  List *targetlist, int nsegs)
{
    CdbHash *hnew = makeCdbHash(nsegs);
    uint32 newSeg;
    ListCell *lc;

    Assert(policyAttrs);
    Assert(targetlist);

    cdbhashinit(hnew);

    foreach(lc, policyAttrs)
    {
        AttrNumber attidx = lfirst_int(lc);

        if (nulls[attidx - 1])
        {
            cdbhashnull(hnew);
        }
        else
        {
            TargetEntry *entry = list_nth(targetlist, attidx - 1);
            cdbhash(hnew, values[attidx - 1], exprType((Node *) entry->expr));
        }
    }

    newSeg = cdbhashreduce(hnew);

    return newSeg;

}

/*
 * copyReshuffleSlot
 *
 * TargetList of Reshuffle Node and SplitUpdate Node is equal,
 * We can copy the values directly.
 */
void
copyReshuffleSlot(TupleTableSlot *slot, Datum *values, bool *nulls)
{
	TupleDesc desc = slot->tts_tupleDescriptor;
	int i = 0;
    Datum *temp_values = slot_get_values(slot);
    bool *temp_nulls = slot_get_isnull(slot);

	for(i = 0; i < desc->natts; i++)
	{
        temp_values[i] = values[i];
        temp_nulls[i] = nulls[i];
	}

	return;
}

/* ----------------------------------------------------------------
 *		ExecReshuffle(node)
 *
 *  For hash distributed tables:
 *  	we compute the destination segment with Hash methods and
 *  	new segments count.
 *
 *  For random distributed tables:
 *  	we get an random value [0, newSeg# - oldSeg#), then the
 *  	destination segment is (random value + oldSeg#)
 *
 *  For replicated tables:
 *  	if there are 3 old segments in the cluster and we add 4
 *  	new segments:
 *  	old segments: 0,1,2
 *  	new segments: 3,4,5,6
 *  	the seg#0 is responsible to copy data to seg#3 and seg#6
 *  	the seg#1 is responsible to copy data to seg#4
 *  	the seg#2 is responsible to copy data to seg#5
 *
 * ----------------------------------------------------------------
 */
TupleTableSlot *
ExecReshuffle(ReshuffleState *node)
{
    PlanState *outerNode = outerPlanState(node);
    Reshuffle *reshuffle = (Reshuffle *) node->ps.plan;
    SplitUpdate *splitUpdate;

    TupleTableSlot *slot = NULL;

    Datum *values;
    bool *nulls;

    int dmlAction;

    Assert(outerNode != NULL);
    Assert(IsA(outerNode->plan, SplitUpdate));

    splitUpdate = (SplitUpdate*)outerNode->plan;

    Assert(splitUpdate->actionColIdx > 0);

    if (reshuffle->ptype == POLICYTYPE_PARTITIONED)
    {
        slot = ExecProcNode(outerNode);

        if (TupIsNull(slot)) {
            return NULL;
        }

        slot_getallattrs(slot);
        values = slot_get_values(slot);
        nulls = slot_get_isnull(slot);

        dmlAction = DatumGetInt32(values[splitUpdate->actionColIdx - 1]);

        Assert(dmlAction == DML_INSERT || dmlAction == DML_DELETE);

        if (DML_INSERT == dmlAction)
		{
			/* For hash distributed tables*/
            if (NULL != reshuffle->policyAttrs)
			{
                values[reshuffle->tupleSegIdx - 1] =
                        Int32GetDatum(EvalHashSegID(values,
                                                    nulls,
                                                    reshuffle->policyAttrs,
                                                    reshuffle->plan.targetlist,
                                                    getgpsegmentCount()));
            }
			else
			{
				/* For random distributed tables*/
                int newSegs = getgpsegmentCount();
                int oldSegs = reshuffle->oldSegs;
                values[reshuffle->tupleSegIdx - 1] =
                        Int32GetDatum((random() % (newSegs - oldSegs)) + oldSegs);
            }
        }
#ifdef USE_ASSERT_CHECKING
        else
        {
            if (NULL != reshuffle->policyAttrs)
            {
                Datum oldSegID = values[reshuffle->tupleSegIdx - 1];
                Datum newSegID = Int32GetDatum(
                        EvalHashSegID(values,
                                      nulls,
                                      reshuffle->policyAttrs,
                                      reshuffle->plan.targetlist,
                                      reshuffle->oldSegs));

                Assert(oldSegID == newSegID);
            }
        }

		/* check */
        if (DatumGetInt32(values[reshuffle->tupleSegIdx - 1]) >=
            getgpsegmentCount())
            elog(ERROR, "ERROR SEGMENT ID : %d",
                 DatumGetInt32(values[reshuffle->tupleSegIdx - 1]));
#endif
    }
    else if (reshuffle->ptype == POLICYTYPE_REPLICATED)
    {
		int segIdx;

		/* For replicated tables*/
        if (GpIdentity.segindex >= reshuffle->oldSegs ||
			GpIdentity.segindex + reshuffle->oldSegs >=
            getgpsegmentCount())
            return NULL;

		/*
		 * Each old semgent cound be responsible to copy data to
		 * more than one new segments
		 */
        do
        {
			/* To copy data to the first new segments */
            if (node->prevSegIdx == GpIdentity.segindex)
            {
                slot = ExecProcNode(outerNode);
                if (TupIsNull(slot))
				{
                    return NULL;
                }

				node->prevSlot = slot;
            }
			else
			{
				/* It seems OK without copying the slot*/
				slot = node->prevSlot;
			}

            Assert(!TupIsNull(slot));

            slot_getallattrs(slot);
			values = slot_get_values(slot);

			dmlAction = DatumGetInt32(values[splitUpdate->actionColIdx - 1]);

			Assert(dmlAction == DML_INSERT || dmlAction == DML_DELETE);

			/* Reshuffling replicate table does not need to delete tuple */
			if(dmlAction == DML_DELETE)
				continue;

			/* Get the destination segments */
            segIdx = node->prevSegIdx + reshuffle->oldSegs;
            if (segIdx >= getgpsegmentCount())
            {
				/*
				 * If tuple is copied to all destination segments, we can
				 * process the next tuple now.
				 */
                node->prevSegIdx = GpIdentity.segindex;
				node->prevSlot = NULL;
                continue;
            }

            node->prevSegIdx = segIdx;
            values[reshuffle->tupleSegIdx - 1] = Int32GetDatum(segIdx);

            break;
        }while(1);
    }
	else
	{
		/* Impossible case*/
		Assert(false);
	}

    return slot;
}

/* ----------------------------------------------------------------
 *		ExecInitReshuffle
 *
 * ----------------------------------------------------------------
 */

ReshuffleState *
ExecInitReshuffle(Reshuffle *node, EState *estate, int eflags)
{
    ReshuffleState *reshufflestate;
    bool has_oids;
    TupleDesc tupDesc;

    /* check for unsupported flags */
    Assert(!(eflags & (EXEC_FLAG_MARK | EXEC_FLAG_BACKWARD)) ||
           outerPlan(node) != NULL);

    /*
     * create state structure
     */
    reshufflestate = makeNode(ReshuffleState);
    reshufflestate->ps.plan = (Plan *) node;
    reshufflestate->ps.state = estate;

    /*
     * initialize child expressions
     */
    reshufflestate->ps.qual = (List *)
            ExecInitExpr((Expr *) node->plan.qual,
                         (PlanState *) reshufflestate);

    /*
     * initialize child nodes
     */
    outerPlanState(reshufflestate) = ExecInitNode(outerPlan(node), estate, eflags);

    /*
     * we don't use inner plan
     */
    Assert(innerPlan(node) == NULL);

    /*
     * tuple table initialization
     */
    ExecInitResultTupleSlot(estate, &reshufflestate->ps);


    /*
     * initialize tuple type and projection info
     */
    ExecAssignResultTypeFromTL(&reshufflestate->ps);
    ExecAssignProjectionInfo(&reshufflestate->ps, NULL);

#if 0
    if (!IsResManagerMemoryPolicyNone()
        && IsResultMemoryIntensive(node))
    {
        SPI_ReserveMemory(((Plan *)node)->operatorMemKB * 1024L);
    }
#endif

	/* Init the segments id to current segment id */
    reshufflestate->prevSegIdx = GpIdentity.segindex;
    reshufflestate->prevSlot = NULL;

    return reshufflestate;
}

/* ----------------------------------------------------------------
 *		ExecEndReshuffle
 * ----------------------------------------------------------------
 */
void
ExecEndReshuffle(ReshuffleState *node)
{
    /*
     * Free the exprcontext
     */
    ExecFreeExprContext(&node->ps);

    /*
     * clean out the tuple table
     */
    ExecClearTuple(node->ps.ps_ResultTupleSlot);

    /*
     * shut down subplans
     */
    ExecEndNode(outerPlanState(node));

    EndPlanStateGpmonPkt(&node->ps);

	return;
}

void
ExecReScanReshuffle(ReshuffleState *node)
{
    /*
     * If chgParam of subnode is not null then plan will be re-scanned by
     * first ExecProcNode.
     */
    if (node->ps.lefttree &&
        node->ps.lefttree->chgParam == NULL)
        ExecReScan(node->ps.lefttree);

	return;
}
