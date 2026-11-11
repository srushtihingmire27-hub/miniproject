/*
 * case_manager.c - Implementation of the crime case register.
 */

#include "case_manager.h"
#include "validation.h"

#include <stdio.h>
#include <string.h>

/*
 * Remove every child record that belongs to the given case.
 *
 * Called by caseManager_delete so that no suspect, evidence item or witness
 * is left pointing at a case identifier that no longer exists.
 */
static void cascadeDeleteChildren(CasefileDb_t *db, const char *caseId)
{
    size_t index;

    for (index = 0U; index < db->suspectCount; index++)
    {
        if (strcmp(db->suspects[index].caseId, caseId) == 0)
        {
            size_t move;

            for (move = index; (move + 1U) < db->suspectCount; move++)
            {
                db->suspects[move] = db->suspects[move + 1U];
            }
            db->suspectCount--;
            index--;
        }
    }

    for (index = 0U; index < db->evidenceCount; index++)
    {
        if (strcmp(db->evidence[index].caseId, caseId) == 0)
        {
            size_t move;

            for (move = index; (move + 1U) < db->evidenceCount; move++)
            {
                db->evidence[move] = db->evidence[move + 1U];
            }
            db->evidenceCount--;
            index--;
        }
    }

    for (index = 0U; index < db->witnessCount; index++)
    {
        if (strcmp(db->witnesses[index].caseId, caseId) == 0)
        {
            size_t move;

            for (move = index; (move + 1U) < db->witnessCount; move++)
            {
                db->witnesses[move] = db->witnesses[move + 1U];
            }
            db->witnessCount--;
            index--;
        }
    }
}

CfStatus_t caseManager_generateId(const CasefileDb_t *db, int year, char *result, size_t resultSize)
{
    int number;

    if (db == NULL || result == NULL || resultSize < ID_LENGTH)
    {
        return CF_ERR_INVALID_ARGUMENT;
    }

    for (number = 1; number <= 9999; number++)
    {
        char candidate[ID_LENGTH];

        snprintf(candidate, sizeof(candidate), "CF-%04d-%04d", year, number);

        if (!casefileCaseIdExists(db, candidate))
        {
            validationCopyField(result, resultSize, candidate);
            return CF_SUCCESS;
        }
    }

    validationCopyField(result, resultSize, "");
    return CF_ERR_CAPACITY_FULL;
}

CfStatus_t caseManager_validateNewId(const CasefileDb_t *db, const char *caseId)
{
    if (db == NULL || caseId == NULL)
    {
        return CF_ERR_INVALID_ARGUMENT;
    }
    if (!validationIsValidIdentifier(caseId))
    {
        return CF_ERR_INVALID_FORMAT;
    }
    if (casefileCaseIdExists(db, caseId))
    {
        return CF_ERR_DUPLICATE_ID;
    }
    return CF_SUCCESS;
}

CfStatus_t caseManager_addCase(CasefileDb_t *db, Case_t *caseData)
{
    int year = 2026;
    char generatedId[ID_LENGTH];

    if (db == NULL || caseData == NULL)
    {
        return CF_ERR_NULL_POINTER;
    }

    if (db->caseCount >= (size_t)MAX_CASES)
    {
        return CF_ERR_CAPACITY_FULL;
    }

    if (!validationIsNotEmpty(caseData->title))
    {
        return CF_ERR_EMPTY_INPUT;
    }
    if (validationCheckLength(caseData->title, TITLE_LENGTH) != CF_SUCCESS)
    {
        return CF_ERR_INVALID_LENGTH;
    }
    if (!validationIsValidDate(caseData->incidentDate))
    {
        return CF_ERR_INVALID_DATE;
    }
    if (caseData->crimeType < 0 || caseData->crimeType >= CRIME_TYPE_COUNT)
    {
        return CF_ERR_INVALID_ENUM;
    }
    if (caseData->priority < 0 || caseData->priority >= CASE_PRIORITY_COUNT)
    {
        return CF_ERR_INVALID_ENUM;
    }
    if (caseData->status < 0 || caseData->status >= CASE_STATUS_COUNT)
    {
        return CF_ERR_INVALID_ENUM;
    }

    /* Use the incident year when it is known, otherwise the current year. */
    if (strlen(caseData->incidentDate) >= 4U)
    {
        year = (caseData->incidentDate[0] - '0') * 1000
             + (caseData->incidentDate[1] - '0') * 100
             + (caseData->incidentDate[2] - '0') * 10
             + (caseData->incidentDate[3] - '0');
    }

    if (caseManager_generateId(db, year, generatedId, sizeof(generatedId)) != CF_SUCCESS)
    {
        return CF_ERR_CAPACITY_FULL;
    }

    validationCopyField(caseData->caseId, ID_LENGTH, generatedId);
    validationCopyField(caseData->title, TITLE_LENGTH, caseData->title);
    validationCopyField(caseData->incidentDate, DATE_LENGTH, caseData->incidentDate);
    validationCopyField(caseData->location, LOCATION_LENGTH, caseData->location);
    validationCopyField(caseData->assignedOfficer, OFFICER_LENGTH, caseData->assignedOfficer);
    validationCopyField(caseData->summary, TEXT_LENGTH, caseData->summary);

    db->cases[ db->caseCount ] = *caseData;
    db->caseCount++;

    return CF_SUCCESS;
}

int caseManager_findById(const CasefileDb_t *db, const char *caseId)
{
    size_t index;

    if (db == NULL || caseId == NULL)
    {
        return -1;
    }

    for (index = 0U; index < db->caseCount; index++)
    {
        if (strcmp(db->cases[index].caseId, caseId) == 0)
        {
            return (int)index;
        }
    }
    return -1;
}

CfStatus_t caseManager_getCase(const CasefileDb_t *db, const char *caseId, Case_t *result)
{
    int index;

    if (db == NULL || caseId == NULL || result == NULL)
    {
        return CF_ERR_NULL_POINTER;
    }

    index = caseManager_findById(db, caseId);
    if (index < 0)
    {
        return CF_ERR_NOT_FOUND;
    }

    *result = db->cases[ index ];
    return CF_SUCCESS;
}

CfStatus_t caseManager_listAll(const CasefileDb_t *db, Case_t *result, size_t capacity, size_t *count)
{
    size_t index;

    if (db == NULL || result == NULL || count == NULL)
    {
        return CF_ERR_NULL_POINTER;
    }

    *count = 0U;

    if (db->caseCount > capacity)
    {
        return CF_ERR_CAPACITY_FULL;
    }

    for (index = 0U; index < db->caseCount; index++)
    {
        result[ index ] = db->cases[ index ];
    }

    *count = db->caseCount;
    return CF_SUCCESS;
}

CfStatus_t caseManager_filterByStatus(const CasefileDb_t *db, CaseStatus_t status,
                                      Case_t *result, size_t capacity, size_t *count)
{
    size_t index;
    size_t found = 0U;

    if (db == NULL || result == NULL || count == NULL)
    {
        return CF_ERR_NULL_POINTER;
    }
    if (status < 0 || status >= CASE_STATUS_COUNT)
    {
        return CF_ERR_INVALID_ENUM;
    }

    *count = 0U;

    for (index = 0U; index < db->caseCount; index++)
    {
        if (db->cases[index].status == status)
        {
            if (found >= capacity)
            {
                return CF_ERR_CAPACITY_FULL;
            }
            result[ found ] = db->cases[ index ];
            found++;
        }
    }

    *count = found;
    return CF_SUCCESS;
}

CfStatus_t caseManager_filterByCrimeType(const CasefileDb_t *db, CrimeType_t crimeType,
                                         Case_t *result, size_t capacity, size_t *count)
{
    size_t index;
    size_t found = 0U;

    if (db == NULL || result == NULL || count == NULL)
    {
        return CF_ERR_NULL_POINTER;
    }
    if (crimeType < 0 || crimeType >= CRIME_TYPE_COUNT)
    {
        return CF_ERR_INVALID_ENUM;
    }

    *count = 0U;

    for (index = 0U; index < db->caseCount; index++)
    {
        if (db->cases[index].crimeType == crimeType)
        {
            if (found >= capacity)
            {
                return CF_ERR_CAPACITY_FULL;
            }
            result[ found ] = db->cases[ index ];
            found++;
        }
    }

    *count = found;
    return CF_SUCCESS;
}

CfStatus_t caseManager_update(CasefileDb_t *db, const char *caseId, const Case_t *caseData)
{
    int index;
    char preservedId[ID_LENGTH];

    if (db == NULL || caseId == NULL || caseData == NULL)
    {
        return CF_ERR_NULL_POINTER;
    }

    index = caseManager_findById(db, caseId);
    if (index < 0)
    {
        return CF_ERR_NOT_FOUND;
    }

    if (!validationIsNotEmpty(caseData->title))
    {
        return CF_ERR_EMPTY_INPUT;
    }
    if (validationCheckLength(caseData->title, TITLE_LENGTH) != CF_SUCCESS)
    {
        return CF_ERR_INVALID_LENGTH;
    }
    if (!validationIsValidDate(caseData->incidentDate))
    {
        return CF_ERR_INVALID_DATE;
    }
    if (caseData->crimeType < 0 || caseData->crimeType >= CRIME_TYPE_COUNT)
    {
        return CF_ERR_INVALID_ENUM;
    }
    if (caseData->priority < 0 || caseData->priority >= CASE_PRIORITY_COUNT)
    {
        return CF_ERR_INVALID_ENUM;
    }

    /* The identifier must survive an update unchanged. */
    validationCopyField(preservedId, ID_LENGTH, db->cases[ index ].caseId);

    validationCopyField(db->cases[ index ].title, TITLE_LENGTH, caseData->title);
    validationCopyField(db->cases[ index ].incidentDate, DATE_LENGTH, caseData->incidentDate);
    validationCopyField(db->cases[ index ].location, LOCATION_LENGTH, caseData->location);
    validationCopyField(db->cases[ index ].assignedOfficer, OFFICER_LENGTH, caseData->assignedOfficer);
    validationCopyField(db->cases[ index ].summary, TEXT_LENGTH, caseData->summary);
    db->cases[ index ].crimeType = caseData->crimeType;
    db->cases[ index ].priority = caseData->priority;

    validationCopyField(db->cases[ index ].caseId, ID_LENGTH, preservedId);

    return CF_SUCCESS;
}

CfStatus_t caseManager_setStatus(CasefileDb_t *db, const char *caseId, CaseStatus_t status)
{
    int index;

    if (db == NULL || caseId == NULL)
    {
        return CF_ERR_NULL_POINTER;
    }
    if (status < 0 || status >= CASE_STATUS_COUNT)
    {
        return CF_ERR_INVALID_ENUM;
    }

    index = caseManager_findById(db, caseId);
    if (index < 0)
    {
        return CF_ERR_NOT_FOUND;
    }

    db->cases[ index ].status = status;
    return CF_SUCCESS;
}

CfStatus_t caseManager_delete(CasefileDb_t *db, const char *caseId)
{
    int index;
    size_t move;

    if (db == NULL || caseId == NULL)
    {
        return CF_ERR_NULL_POINTER;
    }

    index = caseManager_findById(db, caseId);
    if (index < 0)
    {
        return CF_ERR_NOT_FOUND;
    }

    cascadeDeleteChildren(db, caseId);

    for (move = (size_t)index; (move + 1U) < db->caseCount; move++)
    {
        db->cases[ move ] = db->cases[ move + 1U ];
    }
    db->caseCount--;

    return CF_SUCCESS;
}

size_t caseManager_countByStatus(const CasefileDb_t *db, CaseStatus_t status)
{
    size_t index;
    size_t count = 0U;

    if (db == NULL)
    {
        return 0U;
    }

    for (index = 0U; index < db->caseCount; index++)
    {
        if (db->cases[index].status == status)
        {
            count++;
        }
    }
    return count;
}
