/*
 * witness_manager.h - Witness register and statement reference management.
 */

#ifndef CASEFILE_WITNESS_MANAGER_H
#define CASEFILE_WITNESS_MANAGER_H

#include "casefile.h"

/**
 * Register a witness against an existing case.
 *
 * Both the witness identifier and the statement reference identifier are
 * generated automatically and are guaranteed to be unique.
 *
 * @param db          Database to modify (must not be NULL).
 * @param witnessData Witness to store; the witnessId and statementRef fields
 *                    are overwritten with generated values (must not be NULL).
 * @return CF_SUCCESS on success, CF_ERR_NULL_POINTER, CF_ERR_INVALID_ARGUMENT,
 *         CF_ERR_NOT_FOUND when the parent case does not exist,
 *         CF_ERR_EMPTY_INPUT, CF_ERR_INVALID_LENGTH, CF_ERR_INVALID_DATE,
 *         CF_ERR_CAPACITY_FULL or CF_ERR_DUPLICATE_ID.
 */
CfStatus_t witnessManager_add(CasefileDb_t *db, Witness_t *witnessData);

/**
 * Find a witness by identifier.
 *
 * @param db        Database to search (must not be NULL).
 * @param witnessId Identifier to search for (must not be NULL).
 * @return Index of the matching witness, or -1 when not found.
 */
int witnessManager_findById(const CasefileDb_t *db, const char *witnessId);

/**
 * Copy a witness into the supplied buffer.
 *
 * @param db        Database to search (must not be NULL).
 * @param witnessId Identifier to search for (must not be NULL).
 * @param result    Receives a copy of the witness (must not be NULL).
 * @return CF_SUCCESS on success, CF_ERR_NULL_POINTER, CF_ERR_INVALID_ARGUMENT
 *         or CF_ERR_NOT_FOUND.
 */
CfStatus_t witnessManager_get(const CasefileDb_t *db, const char *witnessId, Witness_t *result);

/**
 * Copy the witnesses that belong to the given case into the supplied array.
 *
 * @param db       Database to read (must not be NULL).
 * @param caseId   Identifier of the parent case (must not be NULL).
 * @param result   Destination array (must not be NULL).
 * @param capacity Number of elements the destination array can hold.
 * @param count    Receives the number of witnesses copied (must not be NULL).
 * @return CF_SUCCESS on success, CF_ERR_NULL_POINTER, CF_ERR_INVALID_ARGUMENT
 *         or CF_ERR_CAPACITY_FULL.
 */
CfStatus_t witnessManager_listByCase(const CasefileDb_t *db, const char *caseId,
                                     Witness_t *result, size_t capacity, size_t *count);

/**
 * Copy the witnesses whose name contains the given text into the supplied array.
 *
 * The comparison ignores case. An empty search text matches every witness.
 *
 * @param db         Database to read (must not be NULL).
 * @param searchText Text to search for (must not be NULL).
 * @param result     Destination array (must not be NULL).
 * @param capacity   Number of elements the destination array can hold.
 * @param count      Receives the number of witnesses copied (must not be NULL).
 * @return CF_SUCCESS on success, CF_ERR_NULL_POINTER, CF_ERR_INVALID_ARGUMENT
 *         or CF_ERR_CAPACITY_FULL.
 */
CfStatus_t witnessManager_searchByName(const CasefileDb_t *db, const char *searchText,
                                       Witness_t *result, size_t capacity, size_t *count);

/**
 * Update the editable fields of an existing witness.
 *
 * The identifier, the statement reference and the parent case are preserved.
 *
 * @param db          Database to modify (must not be NULL).
 * @param witnessId   Identifier of the witness to update (must not be NULL).
 * @param witnessData New values for the editable fields (must not be NULL).
 * @return CF_SUCCESS on success, CF_ERR_NULL_POINTER, CF_ERR_INVALID_ARGUMENT,
 *         CF_ERR_EMPTY_INPUT, CF_ERR_INVALID_LENGTH, CF_ERR_INVALID_DATE or
 *         CF_ERR_NOT_FOUND.
 */
CfStatus_t witnessManager_update(CasefileDb_t *db, const char *witnessId, const Witness_t *witnessData);

/**
 * Mark a witness statement as verified or unverified.
 *
 * @param db        Database to modify (must not be NULL).
 * @param witnessId Identifier of the witness (must not be NULL).
 * @param verified  1 to mark the statement as verified, 0 to clear it.
 * @return CF_SUCCESS on success, CF_ERR_NULL_POINTER, CF_ERR_INVALID_ARGUMENT
 *         or CF_ERR_NOT_FOUND.
 */
CfStatus_t witnessManager_setVerified(CasefileDb_t *db, const char *witnessId, int verified);

/**
 * Delete a witness.
 *
 * @param db        Database to modify (must not be NULL).
 * @param witnessId Identifier of the witness to delete (must not be NULL).
 * @return CF_SUCCESS on success, CF_ERR_NULL_POINTER, CF_ERR_INVALID_ARGUMENT
 *         or CF_ERR_NOT_FOUND.
 */
CfStatus_t witnessManager_delete(CasefileDb_t *db, const char *witnessId);

/**
 * Generate the next unused witness identifier.
 *
 * @param db         Database to read (must not be NULL).
 * @param result     Buffer that receives the identifier (must not be NULL).
 * @param resultSize Size of the result buffer.
 * @return CF_SUCCESS on success, CF_ERR_NULL_POINTER, CF_ERR_INVALID_ARGUMENT
 *         or CF_ERR_CAPACITY_FULL.
 */
CfStatus_t witnessManager_generateId(const CasefileDb_t *db, char *result, size_t resultSize);

/**
 * Generate the next unused statement reference identifier.
 *
 * @param db         Database to read (must not be NULL).
 * @param result     Buffer that receives the identifier (must not be NULL).
 * @param resultSize Size of the result buffer.
 * @return CF_SUCCESS on success, CF_ERR_NULL_POINTER, CF_ERR_INVALID_ARGUMENT
 *         or CF_ERR_CAPACITY_FULL.
 */
CfStatus_t witnessManager_generateStatementRef(const CasefileDb_t *db, char *result, size_t resultSize);

/**
 * Count the witnesses that belong to the given case.
 *
 * @param db     Database to read (must not be NULL).
 * @param caseId Identifier of the parent case (must not be NULL).
 * @return Number of matching witnesses, or 0 on error.
 */
size_t witnessManager_countByCase(const CasefileDb_t *db, const char *caseId);

/**
 * Count the witnesses of a case whose statement has been verified.
 *
 * @param db     Database to read (must not be NULL).
 * @param caseId Identifier of the parent case (must not be NULL).
 * @return Number of verified statements, or 0 on error.
 */
size_t witnessManager_countVerified(const CasefileDb_t *db, const char *caseId);

#endif /* CASEFILE_WITNESS_MANAGER_H */
