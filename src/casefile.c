/*
 * casefile.c - Shared database lookup helpers.
 *
 * These functions answer the cross-cutting uniqueness questions that several
 * modules need to ask. Each manager keeps its own record-specific lookup
 * function, so no lookup logic is duplicated between modules.
 */

#include "casefile.h"

#include <stdlib.h>
#include <string.h>

CasefileDb_t *casefile_createDatabase(void)
{
    CasefileDb_t *db = (CasefileDb_t *)malloc(sizeof(CasefileDb_t));

    if (db == NULL)
    {
        return NULL;
    }

    memset(db, 0, sizeof(*db));
    return db;
}

void casefile_destroyDatabase(CasefileDb_t *db)
{
    if (db != NULL)
    {
        free(db);
    }
}

int casefileCaseIdExists(const CasefileDb_t *db, const char *caseId)
{
    size_t index;

    if (db == NULL || caseId == NULL)
    {
        return 0;
    }

    for (index = 0U; index < db->caseCount; index++)
    {
        if (strcmp(db->cases[ index ].caseId, caseId) == 0)
        {
            return 1;
        }
    }
    return 0;
}

int casefileSuspectIdExists(const CasefileDb_t *db, const char *suspectId)
{
    size_t index;

    if (db == NULL || suspectId == NULL)
    {
        return 0;
    }

    for (index = 0U; index < db->suspectCount; index++)
    {
        if (strcmp(db->suspects[ index ].suspectId, suspectId) == 0)
        {
            return 1;
        }
    }
    return 0;
}

int casefileEvidenceIdExists(const CasefileDb_t *db, const char *evidenceId)
{
    size_t index;

    if (db == NULL || evidenceId == NULL)
    {
        return 0;
    }

    for (index = 0U; index < db->evidenceCount; index++)
    {
        if (strcmp(db->evidence[ index ].evidenceId, evidenceId) == 0)
        {
            return 1;
        }
    }
    return 0;
}

int casefileWitnessIdExists(const CasefileDb_t *db, const char *witnessId)
{
    size_t index;

    if (db == NULL || witnessId == NULL)
    {
        return 0;
    }

    for (index = 0U; index < db->witnessCount; index++)
    {
        if (strcmp(db->witnesses[ index ].witnessId, witnessId) == 0)
        {
            return 1;
        }
    }
    return 0;
}

int casefileStatementRefExists(const CasefileDb_t *db, const char *statementRef)
{
    size_t index;

    if (db == NULL || statementRef == NULL)
    {
        return 0;
    }

    for (index = 0U; index < db->witnessCount; index++)
    {
        if (strcmp(db->witnesses[ index ].statementRef, statementRef) == 0)
        {
            return 1;
        }
    }
    return 0;
}
