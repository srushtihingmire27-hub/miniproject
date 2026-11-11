/*
 * witness_manager.c - Implementation of the witness register.
 */

#include "witness_manager.h"
#include "case_manager.h"
#include "validation.h"

#include <stdio.h>
#include <string.h>

CfStatus_t witnessManager_generateId(const CasefileDb_t *db, char *result, size_t resultSize)
{
    int number;

    if (db == NULL || result == NULL || resultSize < ID_LENGTH)
    {
        return CF_ERR_INVALID_ARGUMENT;
    }

    for (number = 1; number <= 99999; number++)
    {
        char candidate[ID_LENGTH];

        snprintf(candidate, sizeof(candidate), "WI-%05d", number);

        if (!casefileWitnessIdExists(db, candidate))
        {
            validationCopyField(result, resultSize, candidate);
            return CF_SUCCESS;
        }
    }

    validationCopyField(result, resultSize, "");
    return CF_ERR_CAPACITY_FULL;
}

CfStatus_t witnessManager_generateStatementRef(const CasefileDb_t *db, char *result, size_t resultSize)
{
    int number;

    if (db == NULL || result == NULL || resultSize < ID_LENGTH)
    {
        return CF_ERR_INVALID_ARGUMENT;
    }

    for (number = 1; number <= 99999; number++)
    {
        char candidate[ID_LENGTH];

        snprintf(candidate, sizeof(candidate), "ST-%05d", number);

        if (!casefileStatementRefExists(db, candidate))
        {
            validationCopyField(result, resultSize, candidate);
            return CF_SUCCESS;
        }
    }

    validationCopyField(result, resultSize, "");
    return CF_ERR_CAPACITY_FULL;
}

CfStatus_t witnessManager_add(CasefileDb_t *db, Witness_t *witnessData)
{
    char generatedId[ID_LENGTH];
    char generatedRef[ID_LENGTH];

    if (db == NULL || witnessData == NULL)
    {
        return CF_ERR_NULL_POINTER;
    }

    if (db->witnessCount >= (size_t)MAX_WITNESSES)
    {
        return CF_ERR_CAPACITY_FULL;
    }

    if (!casefileCaseIdExists(db, witnessData->caseId))
    {
        return CF_ERR_NOT_FOUND;
    }

    if (!validationIsNotEmpty(witnessData->fullName))
    {
        return CF_ERR_EMPTY_INPUT;
    }
    if (validationCheckLength(witnessData->fullName, NAME_LENGTH) != CF_SUCCESS)
    {
        return CF_ERR_INVALID_LENGTH;
    }
    if (!validationIsNotEmpty(witnessData->statement))
    {
        return CF_ERR_EMPTY_INPUT;
    }
    if (validationCheckLength(witnessData->statement, STATEMENT_LENGTH) != CF_SUCCESS)
    {
        return CF_ERR_INVALID_LENGTH;
    }
    if (!validationIsValidDate(witnessData->recordedDate))
    {
        return CF_ERR_INVALID_DATE;
    }

    if (witnessManager_generateId(db, generatedId, sizeof(generatedId)) != CF_SUCCESS)
    {
        return CF_ERR_CAPACITY_FULL;
    }
    if (witnessManager_generateStatementRef(db, generatedRef, sizeof(generatedRef)) != CF_SUCCESS)
    {
        return CF_ERR_CAPACITY_FULL;
    }

    validationCopyField(witnessData->witnessId, ID_LENGTH, generatedId);
    validationCopyField(witnessData->statementRef, ID_LENGTH, generatedRef);
    validationCopyField(witnessData->caseId, ID_LENGTH, witnessData->caseId);
    validationCopyField(witnessData->fullName, NAME_LENGTH, witnessData->fullName);
    validationCopyField(witnessData->phone, PHONE_LENGTH, witnessData->phone);
    validationCopyField(witnessData->statement, STATEMENT_LENGTH, witnessData->statement);
    validationCopyField(witnessData->recordedDate, DATE_LENGTH, witnessData->recordedDate);

    db->witnesses[ db->witnessCount ] = *witnessData;
    db->witnessCount++;

    return CF_SUCCESS;
}

int witnessManager_findById(const CasefileDb_t *db, const char *witnessId)
{
    size_t index;

    if (db == NULL || witnessId == NULL)
    {
        return -1;
    }

    for (index = 0U; index < db->witnessCount; index++)
    {
        if (strcmp(db->witnesses[index].witnessId, witnessId) == 0)
        {
            return (int)index;
        }
    }
    return -1;
}

CfStatus_t witnessManager_get(const CasefileDb_t *db, const char *witnessId, Witness_t *result)
{
    int index;

    if (db == NULL || witnessId == NULL || result == NULL)
    {
        return CF_ERR_NULL_POINTER;
    }

    index = witnessManager_findById(db, witnessId);
    if (index < 0)
    {
        return CF_ERR_NOT_FOUND;
    }

    *result = db->witnesses[ index ];
    return CF_SUCCESS;
}

CfStatus_t witnessManager_listByCase(const CasefileDb_t *db, const char *caseId,
                                     Witness_t *result, size_t capacity, size_t *count)
{
    size_t index;
    size_t found = 0U;

    if (db == NULL || caseId == NULL || result == NULL || count == NULL)
    {
        return CF_ERR_NULL_POINTER;
    }

    *count = 0U;

    for (index = 0U; index < db->witnessCount; index++)
    {
        if (strcmp(db->witnesses[index].caseId, caseId) == 0)
        {
            if (found >= capacity)
            {
                return CF_ERR_CAPACITY_FULL;
            }
            result[ found ] = db->witnesses[ index ];
            found++;
        }
    }

    *count = found;
    return CF_SUCCESS;
}

CfStatus_t witnessManager_searchByName(const CasefileDb_t *db, const char *searchText,
                                       Witness_t *result, size_t capacity, size_t *count)
{
    size_t index;
    size_t found = 0U;

    if (db == NULL || searchText == NULL || result == NULL || count == NULL)
    {
        return CF_ERR_NULL_POINTER;
    }

    *count = 0U;

    for (index = 0U; index < db->witnessCount; index++)
    {
        if (validationContainsIgnoreCase(db->witnesses[index].fullName, searchText) != 0)
        {
            if (found >= capacity)
            {
                return CF_ERR_CAPACITY_FULL;
            }
            result[ found ] = db->witnesses[ index ];
            found++;
        }
    }

    *count = found;
    return CF_SUCCESS;
}

CfStatus_t witnessManager_update(CasefileDb_t *db, const char *witnessId, const Witness_t *witnessData)
{
    int index;
    char preservedId[ID_LENGTH];
    char preservedRef[ID_LENGTH];
    char preservedCaseId[ID_LENGTH];

    if (db == NULL || witnessId == NULL || witnessData == NULL)
    {
        return CF_ERR_NULL_POINTER;
    }

    index = witnessManager_findById(db, witnessId);
    if (index < 0)
    {
        return CF_ERR_NOT_FOUND;
    }

    if (!validationIsNotEmpty(witnessData->fullName))
    {
        return CF_ERR_EMPTY_INPUT;
    }
    if (validationCheckLength(witnessData->fullName, NAME_LENGTH) != CF_SUCCESS)
    {
        return CF_ERR_INVALID_LENGTH;
    }
    if (!validationIsNotEmpty(witnessData->statement))
    {
        return CF_ERR_EMPTY_INPUT;
    }
    if (validationCheckLength(witnessData->statement, STATEMENT_LENGTH) != CF_SUCCESS)
    {
        return CF_ERR_INVALID_LENGTH;
    }
    if (!validationIsValidDate(witnessData->recordedDate))
    {
        return CF_ERR_INVALID_DATE;
    }

    validationCopyField(preservedId, ID_LENGTH, db->witnesses[ index ].witnessId);
    validationCopyField(preservedRef, ID_LENGTH, db->witnesses[ index ].statementRef);
    validationCopyField(preservedCaseId, ID_LENGTH, db->witnesses[ index ].caseId);

    validationCopyField(db->witnesses[ index ].fullName, NAME_LENGTH, witnessData->fullName);
    validationCopyField(db->witnesses[ index ].phone, PHONE_LENGTH, witnessData->phone);
    validationCopyField(db->witnesses[ index ].statement, STATEMENT_LENGTH, witnessData->statement);
    validationCopyField(db->witnesses[ index ].recordedDate, DATE_LENGTH, witnessData->recordedDate);

    validationCopyField(db->witnesses[ index ].witnessId, ID_LENGTH, preservedId);
    validationCopyField(db->witnesses[ index ].statementRef, ID_LENGTH, preservedRef);
    validationCopyField(db->witnesses[ index ].caseId, ID_LENGTH, preservedCaseId);

    return CF_SUCCESS;
}

CfStatus_t witnessManager_setVerified(CasefileDb_t *db, const char *witnessId, int verified)
{
    int index;

    if (db == NULL || witnessId == NULL)
    {
        return CF_ERR_NULL_POINTER;
    }

    index = witnessManager_findById(db, witnessId);
    if (index < 0)
    {
        return CF_ERR_NOT_FOUND;
    }

    db->witnesses[ index ].verified = (verified != 0) ? 1 : 0;
    return CF_SUCCESS;
}

CfStatus_t witnessManager_delete(CasefileDb_t *db, const char *witnessId)
{
    int index;
    size_t move;

    if (db == NULL || witnessId == NULL)
    {
        return CF_ERR_NULL_POINTER;
    }

    index = witnessManager_findById(db, witnessId);
    if (index < 0)
    {
        return CF_ERR_NOT_FOUND;
    }

    for (move = (size_t)index; (move + 1U) < db->witnessCount; move++)
    {
        db->witnesses[ move ] = db->witnesses[ move + 1U ];
    }
    db->witnessCount--;

    return CF_SUCCESS;
}

size_t witnessManager_countByCase(const CasefileDb_t *db, const char *caseId)
{
    size_t index;
    size_t count = 0U;

    if (db == NULL || caseId == NULL)
    {
        return 0U;
    }

    for (index = 0U; index < db->witnessCount; index++)
    {
        if (strcmp(db->witnesses[index].caseId, caseId) == 0)
        {
            count++;
        }
    }
    return count;
}

size_t witnessManager_countVerified(const CasefileDb_t *db, const char *caseId)
{
    size_t index;
    size_t count = 0U;

    if (db == NULL || caseId == NULL)
    {
        return 0U;
    }

    for (index = 0U; index < db->witnessCount; index++)
    {
        if (strcmp(db->witnesses[index].caseId, caseId) == 0 && db->witnesses[index].verified != 0)
        {
            count++;
        }
    }
    return count;
}
