/*
 * evidence_manager.c - Implementation of the forensic evidence register.
 */

#include "evidence_manager.h"
#include "case_manager.h"
#include "validation.h"

#include <stdio.h>
#include <string.h>

CfStatus_t evidenceManager_generateId(const CasefileDb_t *db, char *result, size_t resultSize)
{
    int number;

    if (db == NULL || result == NULL || resultSize < ID_LENGTH)
    {
        return CF_ERR_INVALID_ARGUMENT;
    }

    for (number = 1; number <= 99999; number++)
    {
        char candidate[ID_LENGTH];

        snprintf(candidate, sizeof(candidate), "EV-%05d", number);

        if (!casefileEvidenceIdExists(db, candidate))
        {
            validationCopyField(result, resultSize, candidate);
            return CF_SUCCESS;
        }
    }

    validationCopyField(result, resultSize, "");
    return CF_ERR_CAPACITY_FULL;
}

CfStatus_t evidenceManager_add(CasefileDb_t *db, Evidence_t *evidenceData)
{
    char generatedId[ID_LENGTH];

    if (db == NULL || evidenceData == NULL)
    {
        return CF_ERR_NULL_POINTER;
    }

    if (db->evidenceCount >= (size_t)MAX_EVIDENCE)
    {
        return CF_ERR_CAPACITY_FULL;
    }

    if (!casefileCaseIdExists(db, evidenceData->caseId))
    {
        return CF_ERR_NOT_FOUND;
    }

    if (!validationIsNotEmpty(evidenceData->description))
    {
        return CF_ERR_EMPTY_INPUT;
    }
    if (validationCheckLength(evidenceData->description, EVIDENCE_DESC_LENGTH) != CF_SUCCESS)
    {
        return CF_ERR_INVALID_LENGTH;
    }
    if (!validationIsValidDate(evidenceData->collectionDate))
    {
        return CF_ERR_INVALID_DATE;
    }
    if (evidenceData->category < 0 || evidenceData->category >= EVIDENCE_CATEGORY_COUNT)
    {
        return CF_ERR_INVALID_ENUM;
    }
    if (evidenceData->status < 0 || evidenceData->status >= EVIDENCE_STATUS_COUNT)
    {
        return CF_ERR_INVALID_ENUM;
    }

    if (evidenceManager_generateId(db, generatedId, sizeof(generatedId)) != CF_SUCCESS)
    {
        return CF_ERR_CAPACITY_FULL;
    }

    validationCopyField(evidenceData->evidenceId, ID_LENGTH, generatedId);
    validationCopyField(evidenceData->caseId, ID_LENGTH, evidenceData->caseId);
    validationCopyField(evidenceData->description, EVIDENCE_DESC_LENGTH, evidenceData->description);
    validationCopyField(evidenceData->collectionDate, DATE_LENGTH, evidenceData->collectionDate);
    validationCopyField(evidenceData->collectionLocation, LOCATION_LENGTH, evidenceData->collectionLocation);
    validationCopyField(evidenceData->collectedBy, NAME_LENGTH, evidenceData->collectedBy);

    db->evidence[ db->evidenceCount ] = *evidenceData;
    db->evidence[ db->evidenceCount ].attributeCount = 0;
    db->evidence[ db->evidenceCount ].custodyCount = 0;
    db->evidenceCount++;

    return CF_SUCCESS;
}

int evidenceManager_findById(const CasefileDb_t *db, const char *evidenceId)
{
    size_t index;

    if (db == NULL || evidenceId == NULL)
    {
        return -1;
    }

    for (index = 0U; index < db->evidenceCount; index++)
    {
        if (strcmp(db->evidence[index].evidenceId, evidenceId) == 0)
        {
            return (int)index;
        }
    }
    return -1;
}

CfStatus_t evidenceManager_get(const CasefileDb_t *db, const char *evidenceId, Evidence_t *result)
{
    int index;

    if (db == NULL || evidenceId == NULL || result == NULL)
    {
        return CF_ERR_NULL_POINTER;
    }

    index = evidenceManager_findById(db, evidenceId);
    if (index < 0)
    {
        return CF_ERR_NOT_FOUND;
    }

    *result = db->evidence[ index ];
    return CF_SUCCESS;
}

CfStatus_t evidenceManager_listByCase(const CasefileDb_t *db, const char *caseId,
                                      Evidence_t *result, size_t capacity, size_t *count)
{
    size_t index;
    size_t found = 0U;

    if (db == NULL || caseId == NULL || result == NULL || count == NULL)
    {
        return CF_ERR_NULL_POINTER;
    }

    *count = 0U;

    for (index = 0U; index < db->evidenceCount; index++)
    {
        if (strcmp(db->evidence[index].caseId, caseId) == 0)
        {
            if (found >= capacity)
            {
                return CF_ERR_CAPACITY_FULL;
            }
            result[ found ] = db->evidence[ index ];
            found++;
        }
    }

    *count = found;
    return CF_SUCCESS;
}

CfStatus_t evidenceManager_searchByDescription(const CasefileDb_t *db, const char *searchText,
                                               Evidence_t *result, size_t capacity, size_t *count)
{
    size_t index;
    size_t found = 0U;

    if (db == NULL || searchText == NULL || result == NULL || count == NULL)
    {
        return CF_ERR_NULL_POINTER;
    }

    *count = 0U;

    for (index = 0U; index < db->evidenceCount; index++)
    {
        if (validationContainsIgnoreCase(db->evidence[index].description, searchText) != 0)
        {
            if (found >= capacity)
            {
                return CF_ERR_CAPACITY_FULL;
            }
            result[ found ] = db->evidence[ index ];
            found++;
        }
    }

    *count = found;
    return CF_SUCCESS;
}

CfStatus_t evidenceManager_filterByCategory(const CasefileDb_t *db, EvidenceCategory_t category,
                                            Evidence_t *result, size_t capacity, size_t *count)
{
    size_t index;
    size_t found = 0U;

    if (db == NULL || result == NULL || count == NULL)
    {
        return CF_ERR_NULL_POINTER;
    }
    if (category < 0 || category >= EVIDENCE_CATEGORY_COUNT)
    {
        return CF_ERR_INVALID_ENUM;
    }

    *count = 0U;

    for (index = 0U; index < db->evidenceCount; index++)
    {
        if (db->evidence[index].category == category)
        {
            if (found >= capacity)
            {
                return CF_ERR_CAPACITY_FULL;
            }
            result[ found ] = db->evidence[ index ];
            found++;
        }
    }

    *count = found;
    return CF_SUCCESS;
}

CfStatus_t evidenceManager_filterByStatus(const CasefileDb_t *db, EvidenceStatus_t status,
                                          Evidence_t *result, size_t capacity, size_t *count)
{
    size_t index;
    size_t found = 0U;

    if (db == NULL || result == NULL || count == NULL)
    {
        return CF_ERR_NULL_POINTER;
    }
    if (status < 0 || status >= EVIDENCE_STATUS_COUNT)
    {
        return CF_ERR_INVALID_ENUM;
    }

    *count = 0U;

    for (index = 0U; index < db->evidenceCount; index++)
    {
        if (db->evidence[index].status == status)
        {
            if (found >= capacity)
            {
                return CF_ERR_CAPACITY_FULL;
            }
            result[ found ] = db->evidence[ index ];
            found++;
        }
    }

    *count = found;
    return CF_SUCCESS;
}

CfStatus_t evidenceManager_update(CasefileDb_t *db, const char *evidenceId, const Evidence_t *evidenceData)
{
    int index;
    char preservedId[ID_LENGTH];
    char preservedCaseId[ID_LENGTH];
    int attributeCount;
    int custodyCount;

    if (db == NULL || evidenceId == NULL || evidenceData == NULL)
    {
        return CF_ERR_NULL_POINTER;
    }

    index = evidenceManager_findById(db, evidenceId);
    if (index < 0)
    {
        return CF_ERR_NOT_FOUND;
    }

    if (!validationIsNotEmpty(evidenceData->description))
    {
        return CF_ERR_EMPTY_INPUT;
    }
    if (validationCheckLength(evidenceData->description, EVIDENCE_DESC_LENGTH) != CF_SUCCESS)
    {
        return CF_ERR_INVALID_LENGTH;
    }
    if (!validationIsValidDate(evidenceData->collectionDate))
    {
        return CF_ERR_INVALID_DATE;
    }
    if (evidenceData->category < 0 || evidenceData->category >= EVIDENCE_CATEGORY_COUNT)
    {
        return CF_ERR_INVALID_ENUM;
    }
    if (evidenceData->status < 0 || evidenceData->status >= EVIDENCE_STATUS_COUNT)
    {
        return CF_ERR_INVALID_ENUM;
    }

    /* Preserve the identity, the parent link and the accumulated history. */
    validationCopyField(preservedId, ID_LENGTH, db->evidence[ index ].evidenceId);
    validationCopyField(preservedCaseId, ID_LENGTH, db->evidence[ index ].caseId);
    attributeCount = db->evidence[ index ].attributeCount;
    custodyCount = db->evidence[ index ].custodyCount;

    validationCopyField(db->evidence[ index ].description, EVIDENCE_DESC_LENGTH, evidenceData->description);
    validationCopyField(db->evidence[ index ].collectionDate, DATE_LENGTH, evidenceData->collectionDate);
    validationCopyField(db->evidence[ index ].collectionLocation, LOCATION_LENGTH,
                        evidenceData->collectionLocation);
    validationCopyField(db->evidence[ index ].collectedBy, NAME_LENGTH, evidenceData->collectedBy);
    db->evidence[ index ].category = evidenceData->category;
    db->evidence[ index ].status = evidenceData->status;

    validationCopyField(db->evidence[ index ].evidenceId, ID_LENGTH, preservedId);
    validationCopyField(db->evidence[ index ].caseId, ID_LENGTH, preservedCaseId);
    db->evidence[ index ].attributeCount = attributeCount;
    db->evidence[ index ].custodyCount = custodyCount;

    return CF_SUCCESS;
}

CfStatus_t evidenceManager_setStatus(CasefileDb_t *db, const char *evidenceId, EvidenceStatus_t status)
{
    int index;

    if (db == NULL || evidenceId == NULL)
    {
        return CF_ERR_NULL_POINTER;
    }
    if (status < 0 || status >= EVIDENCE_STATUS_COUNT)
    {
        return CF_ERR_INVALID_ENUM;
    }

    index = evidenceManager_findById(db, evidenceId);
    if (index < 0)
    {
        return CF_ERR_NOT_FOUND;
    }

    db->evidence[ index ].status = status;
    return CF_SUCCESS;
}

CfStatus_t evidenceManager_addAttribute(CasefileDb_t *db, const char *evidenceId,
                                        AttributeKey_t key, const char *value)
{
    int index;
    int slot;

    if (db == NULL || evidenceId == NULL || value == NULL)
    {
        return CF_ERR_NULL_POINTER;
    }
    if (key < 0 || key >= ATTRIBUTE_COUNT)
    {
        return CF_ERR_INVALID_ENUM;
    }
    if (!validationIsNotEmpty(value))
    {
        return CF_ERR_EMPTY_INPUT;
    }
    if (validationCheckLength(value, ATTRIBUTE_VALUE_LENGTH) != CF_SUCCESS)
    {
        return CF_ERR_INVALID_LENGTH;
    }

    index = evidenceManager_findById(db, evidenceId);
    if (index < 0)
    {
        return CF_ERR_NOT_FOUND;
    }

    /* Replace an existing attribute with the same key rather than duplicating. */
    for (slot = 0; slot < db->evidence[ index ].attributeCount; slot++)
    {
        if (db->evidence[ index ].attributes[ slot ].key == key)
        {
            validationCopyField(db->evidence[ index ].attributes[ slot ].value,
                                ATTRIBUTE_VALUE_LENGTH, value);
            return CF_SUCCESS;
        }
    }

    if (db->evidence[ index ].attributeCount >= MAX_EVIDENCE_ATTRIBUTES)
    {
        return CF_ERR_CAPACITY_FULL;
    }

    db->evidence[ index ].attributes[ db->evidence[ index ].attributeCount ].key = key;
    validationCopyField(db->evidence[ index ].attributes[ db->evidence[ index ].attributeCount ].value,
                        ATTRIBUTE_VALUE_LENGTH, value);
    db->evidence[ index ].attributeCount++;

    return CF_SUCCESS;
}

CfStatus_t evidenceManager_addCustodyEvent(CasefileDb_t *db, const char *evidenceId,
                                            const CustodyEvent_t *event)
{
    int index;

    if (db == NULL || evidenceId == NULL || event == NULL)
    {
        return CF_ERR_NULL_POINTER;
    }
    if (event->action < 0 || event->action >= CUSTODY_ACTION_COUNT)
    {
        return CF_ERR_INVALID_ENUM;
    }
    if (!validationIsNotEmpty(event->fromHandler))
    {
        return CF_ERR_EMPTY_INPUT;
    }
    if (!validationIsNotEmpty(event->toHandler))
    {
        return CF_ERR_EMPTY_INPUT;
    }
    if (!validationIsValidDate(event->timestamp))
    {
        return CF_ERR_INVALID_DATE;
    }

    index = evidenceManager_findById(db, evidenceId);
    if (index < 0)
    {
        return CF_ERR_NOT_FOUND;
    }

    if (db->evidence[ index ].custodyCount >= MAX_CUSTODY_EVENTS)
    {
        return CF_ERR_CAPACITY_FULL;
    }

    db->evidence[ index ].custody[ db->evidence[ index ].custodyCount ] = *event;
    validationCopyField(db->evidence[ index ].custody[ db->evidence[ index ].custodyCount ].timestamp,
                        DATE_LENGTH, event->timestamp);
    validationCopyField(db->evidence[ index ].custody[ db->evidence[ index ].custodyCount ].fromHandler,
                        NAME_LENGTH, event->fromHandler);
    validationCopyField(db->evidence[ index ].custody[ db->evidence[ index ].custodyCount ].toHandler,
                        NAME_LENGTH, event->toHandler);
    validationCopyField(db->evidence[ index ].custody[ db->evidence[ index ].custodyCount ].location,
                        LOCATION_LENGTH, event->location);
    validationCopyField(db->evidence[ index ].custody[ db->evidence[ index ].custodyCount ].remarks,
                        CUSTODY_REMARK_LENGTH, event->remarks);
    db->evidence[ index ].custodyCount++;

    return CF_SUCCESS;
}

CfStatus_t evidenceManager_getCustodyHistory(const CasefileDb_t *db, const char *evidenceId,
                                              CustodyEvent_t *result, size_t capacity, size_t *count)
{
    int index;
    int slot;

    if (db == NULL || evidenceId == NULL || result == NULL || count == NULL)
    {
        return CF_ERR_NULL_POINTER;
    }

    *count = 0U;

    index = evidenceManager_findById(db, evidenceId);
    if (index < 0)
    {
        return CF_ERR_NOT_FOUND;
    }

    if ((size_t)db->evidence[ index ].custodyCount > capacity)
    {
        return CF_ERR_CAPACITY_FULL;
    }

    for (slot = 0; slot < db->evidence[ index ].custodyCount; slot++)
    {
        result[ slot ] = db->evidence[ index ].custody[ slot ];
    }

    *count = (size_t)db->evidence[ index ].custodyCount;
    return CF_SUCCESS;
}

CfStatus_t evidenceManager_delete(CasefileDb_t *db, const char *evidenceId)
{
    int index;
    size_t move;

    if (db == NULL || evidenceId == NULL)
    {
        return CF_ERR_NULL_POINTER;
    }

    index = evidenceManager_findById(db, evidenceId);
    if (index < 0)
    {
        return CF_ERR_NOT_FOUND;
    }

    for (move = (size_t)index; (move + 1U) < db->evidenceCount; move++)
    {
        db->evidence[ move ] = db->evidence[ move + 1U ];
    }
    db->evidenceCount--;

    return CF_SUCCESS;
}

size_t evidenceManager_countByCase(const CasefileDb_t *db, const char *caseId)
{
    size_t index;
    size_t count = 0U;

    if (db == NULL || caseId == NULL)
    {
        return 0U;
    }

    for (index = 0U; index < db->evidenceCount; index++)
    {
        if (strcmp(db->evidence[index].caseId, caseId) == 0)
        {
            count++;
        }
    }
    return count;
}

size_t evidenceManager_countAnalysed(const CasefileDb_t *db, const char *caseId)
{
    size_t index;
    size_t count = 0U;

    if (db == NULL || caseId == NULL)
    {
        return 0U;
    }

    for (index = 0U; index < db->evidenceCount; index++)
    {
        if (strcmp(db->evidence[index].caseId, caseId) == 0
            && (db->evidence[index].status == EVIDENCE_STATUS_ANALYSED
                || db->evidence[index].status == EVIDENCE_STATUS_REPORTED
                || db->evidence[index].status == EVIDENCE_STATUS_RELEASED))
        {
            count++;
        }
    }
    return count;
}
