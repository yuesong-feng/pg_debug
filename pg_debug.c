#include "postgres.h"
#include "fmgr.h"
#include "optimizer/planner.h"
#include "nodes/plannodes.h"
#include "nodes/parsenodes.h"
#include "nodes/params.h"
#include "tcop/utility.h"

PG_MODULE_MAGIC;

static planner_hook_type prev_planner_hook = NULL;
static ProcessUtility_hook_type prev_ProcessUtility_hook = NULL;

void _PG_init(void);
void _PG_fini(void);

static PlannedStmt *my_planner_hook(Query *parse,
                                    int cursorOptions,

                                    ParamListInfo boundParams);

static void my_ProcessUtility_hook(Node *parsetree,
                                   const char *queryString, ProcessUtilityContext context,
                                   ParamListInfo params,
                                   DestReceiver *dest, char *completionTag);

void _PG_init(void)
{
    /* Install hooks. */
    prev_planner_hook = planner_hook;
    planner_hook = my_planner_hook;
    prev_ProcessUtility_hook = ProcessUtility_hook;
    ProcessUtility_hook = my_ProcessUtility_hook;
}

void _PG_fini(void)
{
    planner_hook = prev_planner_hook;
}

static PlannedStmt *my_planner_hook(Query *parse,
                                    int cursorOptions,

                                    ParamListInfo boundParams)
{
    PlannedStmt *ret_plan_stmt = NULL;
    if (prev_planner_hook)
        ret_plan_stmt = prev_planner_hook(parse, cursorOptions, boundParams);
    else
        ret_plan_stmt = standard_planner(parse, cursorOptions, boundParams);
    return ret_plan_stmt;
}

static void my_ProcessUtility_hook(Node *parsetree,
                                   const char *queryString, ProcessUtilityContext context,
                                   ParamListInfo params,
                                   DestReceiver *dest, char *completionTag)
{
    NodeTag tag = nodeTag(parsetree);
    if (prev_ProcessUtility_hook)
        prev_ProcessUtility_hook(parsetree, queryString, context, params, dest, completionTag);
    else
        standard_ProcessUtility(parsetree, queryString, context, params, dest, completionTag);
}