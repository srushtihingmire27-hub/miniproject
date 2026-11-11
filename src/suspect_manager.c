/*
 * suspect_manager.c - Implementation of the suspect register.
 */

#include "suspect_manager.h"
#include "case_manager.h"
#include "validation.h"

#include <stdio.h>
#include <string.h>

CfStatus_t suspectManager_generateId(const CasefileDb_t *db, char *result, size_t resultSize)
{
    int number;

    if (db == NULL || result == NULL || resultSize < ID_LENGTH)
    {
        return CF_ERR_INVALID_ARGUMENT;
    }

    for (number = 1; number <= 99999; number++)
    {
        char candidate[ID_LENGTH];

        snprintf(candidate, sizeof(candidate), "SU-%05d", number);

        if (!casefileSuspectIdExists(db, candidate))
        {
            validationCopyField(result, resultSize, candidate);
            return CF_SUCCESS;
        }
    }

    validationCopyField(result, resultSize, "");
    return CF_ERR_CAPACITY_FULL;
}

CfStatus_t suspectManager_add(CasefileDb_t *db, Suspect_t *suspectData)
{
    char generatedId[ID_LENGTH];

    if (db == NULL || suspectData == NULL)
    {
        return CF_ERR_NULL_POINTER;
    }

    if (db->suspectCount >= (size_t)MAX_SUSPECTS)
    {
        return CF_ERR_CAPACITY_FULL;
    }

    /* The parent case must exist, otherwise the record would be orphaned. */
    if (!casefileCaseIdExists(db, suspectData->caseId))
    {
        return CF_ERR_NOT_FOUND;
    }

    if (!validationIsNotEmpty(suspectData->fullName))
    {
        return CF_ERR_EMPTY_INPUT;
    }
    if (validationCheckLength(suspectData->fullName, NAME_LENGTH) != CF_SUCCESS)
    {
        return CF_ERR_INVALID_LENGTH;
    }
    if (!validationIsValidAge(suspectData->age))
    {
        return CF_ERR_INVALID_ARGUMENT;
    }
    if (suspectData->status < 0 || suspectData->status >= SUSPECT_STATUS_COUNT)
    {
        return CF_ERR_INVALID_ENUM;
    }

    if (suspectManager_generateId(db, generatedId, sizeof(generatedId)) != CF_SUCCESS)
    {
        return CF_ERR_CAPACITY_FULL;
    }

    validationCopyField(suspectData->suspectId, ID_LENGTH, generatedId);
    validationCopyField(suspectData->caseId, ID_LENGTH, suspectData->caseId);
    validationCopyField(suspectData->fullName, NAME_LENGTH, suspectData->fullName);
    validationCopyField(suspectData->phone, PHONE_LENGTH, suspectData->phone);
    validationCopyField(suspectData->address, ADDRESS_LENGTH, suspectData->address);
    validationCopyField(suspectData->alibi, ALIBI_LENGTH, suspectData->alibi);
    validationCopyField(suspectData->motive, MOTIVE_LENGTH, suspectData->motive);

    db->suspects[ db->suspectCount ] = *suspectData;
    db->suspectCount++;

    return CF_SUCCESS;
}

int suspectManager_findById(const CasefileDb_t *db, const char *suspectId)
{
    size_t index;

    if (db == NULL || suspectId == NULL)
    {
        return -1;
    }

    for (index = 0U; index < db->suspectCount; index++)
    {
        if (strcmp(db->suspects[index].suspectId, suspectId) == 0)
        {
            return (int)index;
        }
    }
    return -1;
}

CfStatus_t suspectManager_get(const CasefileDb_t *db, const char *suspectId, Suspect_t *result)
{
    int index;

    if (db == NULL || suspectId == NULL || result == NULL)
    {
        return CF_ERR_NULL_POINTER;
    }

    index = suspectManager_findById(db, suspectId);
    if (index < 0)
    {
        return CF_ERR_NOT_FOUND;
    }

    *result = db->suspects[ index ];
    return CF_SUCCESS;
}

CfStatus_t suspectManager_listByCase(const CasefileDb_t *db, const char *caseId,
                                     Suspect_t *result, size_t capacity, size_t *count)
{
    size_t index;
    size_t found = 0U;

    if (db == NULL || caseId == NULL || result == NULL || count == NULL)
    {
        return CF_ERR_NULL_POINTER;
    }

    *count = 0U;

    for (index = 0U; index < db->suspectCount; index++)
    {
        if (strcmp(db->suspects[index].caseId, caseId) == 0)
        {
            if (found >= capacity)
            {
                return CF_ERR_CAPACITY_FULL;
            }
            result[ found ] = db->suspects[ index ];
            found++;
        }
    }

    *count = found;
    return CF_SUCCESS;
}

CfStatus_t suspectManager_searchByName(const CasefileDb_t *db, const char *searchText,
                                       Suspect_t *result, size_t capacity, size_t *count)
{
    size_t index;
    size_t found = 0U;

    if (db == NULL || searchText == NULL || result == NULL || count == NULL)
    {
        return CF_ERR_NULL_POINTER;
    }

    *count = 0U;

    for (index = 0U; index < db->suspectCount; index++)
    {
        if (validationContainsIgnoreCase(db->suspects[index].fullName, searchText) != 0)
        {
            if (found >= capacity)
            {
                return CF_ERR_CAPACITY_FULL;
            }
            result[ found ] = db->suspects[ index ];
            found++;
        }
    }

    *count = found;
    return CF_SUCCESS;
}

CfStatus_t suspectManager_update(CasefileDb_t *db, const char *suspectId, const Suspect_t *suspectData)
{
    int index;
    char preservedId[ID_LENGTH];
    char preservedCaseId[ID_LENGTH];

    if (db == NULL || suspectId == NULL || suspectData == NULL)
    {
        return CF_ERR_NULL_POINTER;
    }

    index = suspectManager_findById(db, suspectId);
    if (index < 0)
    {
        return CF_ERR_NOT_FOUND;
    }

    if (!validationIsNotEmpty(suspectData->fullName))
    {
        return CF_ERR_EMPTY_INPUT;
    }
    if (validationCheckLength(suspectData->fullName, NAME_LENGTH) != CF_SUCCESS)
    {
        return CF_ERR_INVALID_LENGTH;
    }
    if (!validationIsValidAge(suspectData->age))
    {
        return CF_ERR_INVALID_ARGUMENT;
    }
    if (suspectData->status < 0 || suspectData->status >= SUSPECT_STATUS_COUNT)
    {
        return CF_ERR_INVALID_ENUM;
    }

    validationCopyField(preservedId, ID_LENGTH, db->suspects[ index ].suspectId);
    validationCopyField(preservedCaseId, ID_LENGTH, db->suspects[ index ].caseId);

    validationCopyField(db->suspects[ index ].fullName, NAME_LENGTH, suspectData->fullName);
    validationCopyField(db->suspects[ index ].phone, PHONE_LENGTH, suspectData->phone);
    validationCopyField(db->suspects[ index ].address, ADDRESS_LENGTH, suspectData->address);
    validationCopyField(db->suspects[ index ].alibi, ALIBI_LENGTH, suspectData->alibi);
    validationCopyField(db->suspects[ index ].motive, MOTIVE_LENGTH, suspectData->motive);
    db->suspects[ index ].age = suspectData->age;
    db->suspects[ index ].status = suspectData->status;

    validationCopyField(db->suspects[ index ].suspectId, ID_LENGTH, preservedId);
    validationCopyField(db->suspects[ index ].caseId, ID_LENGTH, preservedCaseId);

    return CF_SUCCESS;
}

CfStatus_t suspectManager_delete(CasefileDb_t *db, const char *suspectId)
{
    int index;
    size_t move;

    if (db == NULL || suspectId == NULL)
    {
        return CF_ERR_NULL_POINTER;
    }

    index = suspectManager_findById(db, suspectId);
    if (index < 0)
    {
        return CF_ERR_NOT_FOUND;
    }

    for (move = (size_t)index; (move + 1U) < db->suspectCount; move++)
    {
        db->suspects[ move ] = db->suspects[ move + 1U ];
    }
    db->suspectCount--;

    return CF_SUCCESS;
}

CfStatus_t suspectManager_setPhysicalProfile(CasefileDb_t *db, const char *suspectId,
                                              const PhysicalProfile_t *profile)
{
    int index;
    int vehicle;

    if (db == NULL || suspectId == NULL || profile == NULL)
    {
        return CF_ERR_NULL_POINTER;
    }

    index = suspectManager_findById(db, suspectId);
    if (index < 0)
    {
        return CF_ERR_NOT_FOUND;
    }

    if (profile->heightCm != -1 && !validationIsValidHeight(profile->heightCm))
    {
        return CF_ERR_INVALID_ARGUMENT;
    }
    if (profile->shoeSize != -1 && !validationIsValidShoeSize(profile->shoeSize))
    {
        return CF_ERR_INVALID_ARGUMENT;
    }
    if (validationCheckLength(profile->gender, NAME_LENGTH) != CF_SUCCESS)
    {
        return CF_ERR_INVALID_LENGTH;
    }
    if (validationCheckLength(profile->hairColour, NAME_LENGTH) != CF_SUCCESS)
    {
        return CF_ERR_INVALID_LENGTH;
    }
    if (validationCheckLength(profile->bloodGroup, NAME_LENGTH) != CF_SUCCESS)
    {
        return CF_ERR_INVALID_LENGTH;
    }
    if (profile->vehicleCount < 0 || profile->vehicleCount > MAX_VEHICLES)
    {
        return CF_ERR_INVALID_ARGUMENT;
    }

    db->suspects[ index ].physical = *profile;
    db->suspects[ index ].physical.vehicleCount = profile->vehicleCount;

    for (vehicle = 0; vehicle < profile->vehicleCount; vehicle++)
    {
        validationCopyField(db->suspects[ index ].physical.vehicles[ vehicle ],
                            VEHICLE_LENGTH, profile->vehicles[ vehicle ]);
    }

    return CF_SUCCESS;
}

CfStatus_t suspectManager_addVehicle(CasefileDb_t *db, const char *suspectId,
                                      const char *registration)
{
    int index;

    if (db == NULL || suspectId == NULL || registration == NULL)
    {
        return CF_ERR_NULL_POINTER;
    }
    if (!validationIsNotEmpty(registration))
    {
        return CF_ERR_EMPTY_INPUT;
    }
    if (validationCheckLength(registration, VEHICLE_LENGTH) != CF_SUCCESS)
    {
        return CF_ERR_INVALID_LENGTH;
    }

    index = suspectManager_findById(db, suspectId);
    if (index < 0)
    {
        return CF_ERR_NOT_FOUND;
    }

    if (db->suspects[ index ].physical.vehicleCount >= MAX_VEHICLES)
    {
        return CF_ERR_CAPACITY_FULL;
    }

    validationCopyField(db->suspects[ index ].physical.vehicles[ db->suspects[ index ].physical.vehicleCount ],
                        VEHICLE_LENGTH, registration);
    db->suspects[ index ].physical.vehicleCount++;

    return CF_SUCCESS;
}

size_t suspectManager_countByCase(const CasefileDb_t *db, const char *caseId)
{
    size_t index;
    size_t count = 0U;

    if (db == NULL || caseId == NULL)
    {
        return 0U;
    }

    for (index = 0U; index < db->suspectCount; index++)
    {
        if (strcmp(db->suspects[index].caseId, caseId) == 0)
        {
            count++;
        }
    }
    return count;
}
