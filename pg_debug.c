#include "postgres.h"
#include "fmgr.h"
#include "funcapi.h"
#include "optimizer/planner.h"
#include "nodes/plannodes.h"
#include "nodes/parsenodes.h"
#include "nodes/params.h"
#include "nodes/print.h"
#include "tcop/utility.h"
#include "parser/analyze.h"
#include "storage/s_lock.h"
#include "utils/hsearch.h"
#include "storage/lwlock.h"
#include "miscadmin.h"
#include "utils/builtins.h"

PG_MODULE_MAGIC;

#define MAX_SQLSTMT 128

typedef struct SQLStmt
{
    uint32 sql_id;
    const char *sql_str;
    const char *parse_tree;
    const char *rewritten_parse_tree;
    const char *plan;
} SQLStmt;

static SQLStmt sql_stmt[MAX_SQLSTMT];
static uint32 seq_id = 0;

static post_parse_analyze_hook_type prev_post_parse_analyze_hook = NULL;
static planner_hook_type prev_planner_hook = NULL;

void _PG_init(void);
void _PG_fini(void);

PG_FUNCTION_INFO_V1(sql_info);
PG_FUNCTION_INFO_V1(sql_info_reset);

static void my_post_parse_analyze_hook(ParseState *pstate,
                                       Query *query);
static PlannedStmt *my_planner_hook(Query *parse,
                                    int cursorOptions,
                                    ParamListInfo boundParams);

void _PG_init(void)
{
    /* Install hooks. */
    prev_post_parse_analyze_hook = post_parse_analyze_hook;
    post_parse_analyze_hook = my_post_parse_analyze_hook;
    prev_planner_hook = planner_hook;
    planner_hook = my_planner_hook;
}

void _PG_fini(void)
{
    post_parse_analyze_hook = prev_post_parse_analyze_hook;
    planner_hook = prev_planner_hook;
}

static void my_post_parse_analyze_hook(ParseState *pstate,
                                       Query *query)
{
    if (prev_post_parse_analyze_hook)
        prev_post_parse_analyze_hook(pstate, query);

    seq_id++;
    query->queryId = seq_id;
    sql_stmt[seq_id].sql_id = seq_id;
    sql_stmt[seq_id].sql_str = pstrdup(pstate->p_sourcetext);
    sql_stmt[seq_id].parse_tree = NULL;
    sql_stmt[seq_id].rewritten_parse_tree = NULL;
    sql_stmt[seq_id].plan = NULL;

    char *s = nodeToString(query);
    char *f = format_node_dump(s);
    pfree(s);
    sql_stmt[seq_id].parse_tree = pstrdup(f);
    pfree(f);
}

static PlannedStmt *my_planner_hook(Query *parse,
                                    int cursorOptions,
                                    ParamListInfo boundParams)
{
    char *s = nodeToString(parse);
    char *f = format_node_dump(s);
    pfree(s);
    sql_stmt[seq_id].rewritten_parse_tree = pstrdup(f);
    pfree(f);

    PlannedStmt *plan = NULL;
    if (prev_planner_hook)
        plan = prev_planner_hook(parse, cursorOptions, boundParams);
    else
        plan = standard_planner(parse, cursorOptions, boundParams);

    char *s2 = nodeToString(plan);
    char *f2 = format_node_dump(s2);
    pfree(s2);
    sql_stmt[seq_id].plan = pstrdup(f2);
    pfree(f2);

    return plan;
}

Datum sql_info(PG_FUNCTION_ARGS)
{
    ReturnSetInfo *rsinfo = (ReturnSetInfo *)fcinfo->resultinfo;
    TupleDesc tupdesc;
    Tuplestorestate *tupstore;
    MemoryContext per_query_ctx;
    MemoryContext oldcontext;
    uint32 sql_id = PG_GETARG_UINT32(0);

    /* check to see if caller supports us returning a tuplestore */
    if (rsinfo == NULL || !IsA(rsinfo, ReturnSetInfo))
        ereport(ERROR,
                (errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
                 errmsg("set-valued function called in context that cannot accept a set")));
    if (!(rsinfo->allowedModes & SFRM_Materialize))
        ereport(ERROR,
                (errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
                 errmsg("materialize mode required, but it is not allowed in this context")));

    /* Switch into long-lived context to construct returned data structures */
    per_query_ctx = rsinfo->econtext->ecxt_per_query_memory;
    oldcontext = MemoryContextSwitchTo(per_query_ctx);

    /* Build a tuple descriptor for our result type */
    if (get_call_result_type(fcinfo, NULL, &tupdesc) != TYPEFUNC_COMPOSITE)
        elog(ERROR, "return type must be a row type");

    tupstore = tuplestore_begin_heap(true, false, work_mem);
    rsinfo->returnMode = SFRM_Materialize;
    rsinfo->setResult = tupstore;
    rsinfo->setDesc = tupdesc;

    MemoryContextSwitchTo(oldcontext);

    for (int i = 1; i <= seq_id; ++i)
    {
        Datum values[5];
        bool nulls[5];
        memset(values, 0, sizeof(values));
        memset(nulls, 0, sizeof(nulls));

        values[0] = UInt32GetDatum(sql_stmt[i].sql_id);
        values[1] = CStringGetTextDatum(sql_stmt[i].sql_str);
        if (sql_stmt[i].parse_tree == NULL)
            nulls[2] = true;
        else
            values[2] = CStringGetTextDatum(sql_stmt[i].parse_tree);

        if (sql_stmt[i].rewritten_parse_tree == NULL)
            nulls[3] = true;
        else
            values[3] = CStringGetTextDatum(sql_stmt[i].rewritten_parse_tree);

        if (sql_stmt[i].plan == NULL)
            nulls[4] = true;
        else
            values[4] = CStringGetTextDatum(sql_stmt[i].plan);

        if (sql_id == 0 || sql_stmt[i].sql_id == sql_id)
            tuplestore_putvalues(tupstore, tupdesc, values, nulls);
    }

    /* clean up and return the tuplestore */
    tuplestore_donestoring(tupstore);

    PG_RETURN_VOID();
}

Datum sql_info_reset(PG_FUNCTION_ARGS)
{
    PG_RETURN_VOID();
}