/*
 * suspect_manager.h - Suspect register and physical profile management.
 */

#ifndef CASEFILE_SUSPECT_MANAGER_H
#define CASEFILE_SUSPECT_MANAGER_H

#include "casefile.h"

/**
 * Add a suspect to an existing case.
 *
 * The suspect identifier is generated automatically in the form SU-NNNN and is
 * guaranteed not to collide with an existing identifier. The parent case must
 * already exist.
 *
 * @param db          Database to modify (must not be NULL).
 * @param suspectData Suspect to store; the suspectId field is overwritten
 *                    with the generated identifier (must not be NULL).
 * @return CF_SUCCESS on success, CF_ERR_NULL_POINTER, CF_ERR_INVALID_ARGUMENT,
 *         CF_ERR_NOT_FOUND when the parent case does not exist,
 *         CF_ERR_EMPTY_INPUT, CF_ERR_INVALID_LENGTH, CF_ERR_INVALID_ENUM,
 *         CF_ERR_CAPACITY_FULL or CF_ERR_DUPLICATE_ID.
 */
CfStatus_t suspectManager_add(CasefileDb_t *db, Suspect_t *suspectData);

/**
 * Find a suspect by identifier.
 *
 * @param db        Database to search (must not be NULL).
 * @param suspectId Identifier to search for (must not be NULL).
 * @return Index of the matching suspect, or -1 when not found.
 */
int suspectManager_findById(const CasefileDb_t *db, const char *suspectId);

/**
 * Copy a suspect into the supplied buffer.
 *
 * @param db        Database to search (must not be NULL).
 * @param suspectId Identifier to search for (must not be NULL).
 * @param result    Receives a copy of the suspect (must not be NULL).
 * @return CF_SUCCESS on success, CF_ERR_NULL_POINTER, CF_ERR_INVALID_ARGUMENT
 *         or CF_ERR_NOT_FOUND.
 */
CfStatus_t suspectManager_get(const CasefileDb_t *db, const char *suspectId, Suspect_t *result);

/**
 * Copy the suspects that belong to the given case into the supplied array.
 *
 * @param db       Database to read (must not be NULL).
 * @param caseId   Identifier of the parent case (must not be NULL).
 * @param result   Destination array (must not be NULL).
 * @param capacity Number of elements the destination array can hold.
 * @param count    Receives the number of suspects copied (must not be NULL).
 * @return CF_SUCCESS on success, CF_ERR_NULL_POINTER, CF_ERR_INVALID_ARGUMENT
 *         or CF_ERR_CAPACITY_FULL.
 */
CfStatus_t suspectManager_listByCase(const CasefileDb_t *db, const char *caseId,
                                     Suspect_t *result, size_t capacity, size_t *count);

/**
 * Copy the suspects whose name contains the given text into the supplied array.
 *
 * The comparison ignores case. An empty search text matches every suspect.
 *
 * @param db       Database to read (must not be NULL).
 * @param searchText Text to search for (must not be NULL).
 * @param result   Destination array (must not be NULL).
 * @param capacity Number of elements the destination array can hold.
 * @param count    Receives the number of suspects copied (must not be NULL).
 * @return CF_SUCCESS on success, CF_ERR_NULL_POINTER, CF_ERR_INVALID_ARGUMENT
 *         or CF_ERR_CAPACITY_FULL.
 */
CfStatus_t suspectManager_searchByName(const CasefileDb_t *db, const char *searchText,
                                       Suspect_t *result, size_t capacity, size_t *count);

/**
 * Update the editable fields of an existing suspect.
 *
 * The identifier and the parent case are preserved.
 *
 * @param db          Database to modify (must not be NULL).
 * @param suspectId   Identifier of the suspect to update (must not be NULL).
 * @param suspectData New values for the editable fields (must not be NULL).
 * @return CF_SUCCESS on success, CF_ERR_NULL_POINTER, CF_ERR_INVALID_ARGUMENT,
 *         CF_ERR_EMPTY_INPUT, CF_ERR_INVALID_LENGTH, CF_ERR_INVALID_ENUM or
 *         CF_ERR_NOT_FOUND.
 */
CfStatus_t suspectManager_update(CasefileDb_t *db, const char *suspectId, const Suspect_t *suspectData);

/**
 * Delete a suspect.
 *
 * @param db        Database to modify (must not be NULL).
 * @param suspectId Identifier of the suspect to delete (must not be NULL).
 * @return CF_SUCCESS on success, CF_ERR_NULL_POINTER, CF_ERR_INVALID_ARGUMENT
 *         or CF_ERR_NOT_FOUND.
 */
CfStatus_t suspectManager_delete(CasefileDb_t *db, const char *suspectId);

/**
 * Replace the physical profile of a suspect.
 *
 * Height, shoe size and the vehicle list may be left unknown by passing -1 for
 * the numeric fields and an empty vehicle list.
 *
 * @param db        Database to modify (must not be NULL).
 * @param suspectId Identifier of the suspect (must not be NULL).
 * @param profile   Profile to store (must not be NULL).
 * @return CF_SUCCESS on success, CF_ERR_NULL_POINTER, CF_ERR_INVALID_ARGUMENT,
 *         CF_ERR_INVALID_LENGTH or CF_ERR_NOT_FOUND.
 */
CfStatus_t suspectManager_setPhysicalProfile(CasefileDb_t *db, const char *suspectId,
                                              const PhysicalProfile_t *profile);

/**
 * Add a known vehicle registration to a suspect's profile.
 *
 * @param db          Database to modify (must not be NULL).
 * @param suspectId   Identifier of the suspect (must not be NULL).
 * @param registration Registration to add (must not be NULL).
 * @return CF_SUCCESS on success, CF_ERR_NULL_POINTER, CF_ERR_INVALID_ARGUMENT,
 *         CF_ERR_INVALID_LENGTH, CF_ERR_CAPACITY_FULL or CF_ERR_NOT_FOUND.
 */
CfStatus_t suspectManager_addVehicle(CasefileDb_t *db, const char *suspectId,
                                      const char *registration);

/**
 * Generate the next unused suspect identifier.
 *
 * @param db         Database to read (must not be NULL).
 * @param result     Buffer that receives the identifier (must not be NULL).
 * @param resultSize Size of the result buffer.
 * @return CF_SUCCESS on success, CF_ERR_NULL_POINTER, CF_ERR_INVALID_ARGUMENT
 *         or CF_ERR_CAPACITY_FULL.
 */
CfStatus_t suspectManager_generateId(const CasefileDb_t *db, char *result, size_t resultSize);

/**
 * Count the suspects that belong to the given case.
 *
 * @param db     Database to read (must not be NULL).
 * @param caseId Identifier of the parent case (must not be NULL).
 * @return Number of matching suspects, or 0 on error.
 */
size_t suspectManager_countByCase(const CasefileDb_t *db, const char *caseId);

#endif /* CASEFILE_SUSPECT_MANAGER_H */
