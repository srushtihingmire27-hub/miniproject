/*
 * case_manager.h - Crime case register management.
 *
 * Owns the lifecycle of Case_t records: creation with an auto-generated unique
 * identifier, retrieval, filtering, update, status change and deletion with
 * cascade to the child records of that case.
 */

#ifndef CASEFILE_CASE_MANAGER_H
#define CASEFILE_CASE_MANAGER_H

#include "casefile.h"

/**
 * Register a new case in the database.
 *
 * The case identifier is generated automatically in the form CF-YYYY-NNNN and
 * is guaranteed not to collide with an existing identifier. Every other field
 * is validated before the record is stored.
 *
 * @param db       Database to modify (must not be NULL).
 * @param caseData Case to store; the caseId field is overwritten with the
 *                 generated identifier (must not be NULL).
 * @return CF_SUCCESS on success, CF_ERR_NULL_POINTER, CF_ERR_INVALID_ARGUMENT,
 *         CF_ERR_INVALID_LENGTH, CF_ERR_INVALID_DATE, CF_ERR_CAPACITY_FULL,
 *         CF_ERR_DUPLICATE_ID or CF_ERR_INTERNAL.
 */
CfStatus_t caseManager_addCase(CasefileDb_t *db, Case_t *caseData);

/**
 * Find a case by identifier.
 *
 * @param db     Database to search (must not be NULL).
 * @param caseId Identifier to search for (must not be NULL).
 * @return Index of the matching case, or -1 when not found.
 */
int caseManager_findById(const CasefileDb_t *db, const char *caseId);

/**
 * Copy the case with the given identifier into the supplied buffer.
 *
 * @param db     Database to search (must not be NULL).
 * @param caseId Identifier to search for (must not be NULL).
 * @param result Receives a copy of the case (must not be NULL).
 * @return CF_SUCCESS on success, CF_ERR_NULL_POINTER, CF_ERR_INVALID_ARGUMENT
 *         or CF_ERR_NOT_FOUND.
 */
CfStatus_t caseManager_getCase(const CasefileDb_t *db, const char *caseId, Case_t *result);

/**
 * Copy all cases into the supplied array.
 *
 * @param db     Database to read (must not be NULL).
 * @param result Destination array (must not be NULL).
 * @param capacity Number of elements the destination array can hold.
 * @param count  Receives the number of cases copied (must not be NULL).
 * @return CF_SUCCESS on success, CF_ERR_NULL_POINTER, CF_ERR_INVALID_ARGUMENT
 *         or CF_ERR_CAPACITY_FULL.
 */
CfStatus_t caseManager_listAll(const CasefileDb_t *db, Case_t *result, size_t capacity, size_t *count);

/**
 * Copy the cases that have the given status into the supplied array.
 *
 * @param db     Database to read (must not be NULL).
 * @param status Status to filter by.
 * @param result Destination array (must not be NULL).
 * @param capacity Number of elements the destination array can hold.
 * @param count  Receives the number of cases copied (must not be NULL).
 * @return CF_SUCCESS on success, CF_ERR_NULL_POINTER, CF_ERR_INVALID_ARGUMENT
 *         or CF_ERR_CAPACITY_FULL.
 */
CfStatus_t caseManager_filterByStatus(const CasefileDb_t *db, CaseStatus_t status,
                                      Case_t *result, size_t capacity, size_t *count);

/**
 * Copy the cases that have the given crime type into the supplied array.
 *
 * @param db       Database to read (must not be NULL).
 * @param crimeType Crime type to filter by.
 * @param result   Destination array (must not be NULL).
 * @param capacity Number of elements the destination array can hold.
 * @param count    Receives the number of cases copied (must not be NULL).
 * @return CF_SUCCESS on success, CF_ERR_NULL_POINTER, CF_ERR_INVALID_ARGUMENT
 *         or CF_ERR_CAPACITY_FULL.
 */
CfStatus_t caseManager_filterByCrimeType(const CasefileDb_t *db, CrimeType_t crimeType,
                                         Case_t *result, size_t capacity, size_t *count);

/**
 * Update the editable fields of an existing case.
 *
 * The identifier and the registered date are preserved. The incident date and
 * the enumeration values are validated before the change is applied.
 *
 * @param db       Database to modify (must not be NULL).
 * @param caseId   Identifier of the case to update (must not be NULL).
 * @param caseData New values for the editable fields (must not be NULL).
 * @return CF_SUCCESS on success, CF_ERR_NULL_POINTER, CF_ERR_INVALID_ARGUMENT,
 *         CF_ERR_INVALID_DATE, CF_ERR_NOT_FOUND or CF_ERR_INTERNAL.
 */
CfStatus_t caseManager_update(CasefileDb_t *db, const char *caseId, const Case_t *caseData);

/**
 * Change the investigation status of a case.
 *
 * @param db     Database to modify (must not be NULL).
 * @param caseId Identifier of the case to change (must not be NULL).
 * @param status New investigation status.
 * @return CF_SUCCESS on success, CF_ERR_NULL_POINTER, CF_ERR_INVALID_ARGUMENT,
 *         CF_ERR_INVALID_ENUM or CF_ERR_NOT_FOUND.
 */
CfStatus_t caseManager_setStatus(CasefileDb_t *db, const char *caseId, CaseStatus_t status);

/**
 * Delete a case and every record that belongs to it.
 *
 * The linked suspects, evidence items and witnesses are removed as well so
 * that no record is left pointing at a case that no longer exists.
 *
 * @param db     Database to modify (must not be NULL).
 * @param caseId Identifier of the case to delete (must not be NULL).
 * @return CF_SUCCESS on success, CF_ERR_NULL_POINTER, CF_ERR_INVALID_ARGUMENT
 *         or CF_ERR_NOT_FOUND.
 */
CfStatus_t caseManager_delete(CasefileDb_t *db, const char *caseId);

/**
 * Count the cases that have the given status.
 *
 * @param db     Database to read (must not be NULL).
 * @param status Status to count.
 * @return Number of matching cases, or 0 on error.
 */
size_t caseManager_countByStatus(const CasefileDb_t *db, CaseStatus_t status);

/**
 * Determine whether a case identifier is well formed and not already in use.
 *
 * @param db     Database to read (must not be NULL).
 * @param caseId Identifier to test (must not be NULL).
 * @return CF_SUCCESS when the identifier is usable, CF_ERR_INVALID_FORMAT when
 *         it is malformed, CF_ERR_DUPLICATE_ID when it is already in use.
 */
CfStatus_t caseManager_validateNewId(const CasefileDb_t *db, const char *caseId);

/**
 * Build the next unused case identifier for the given year.
 *
 * The identifier has the form CF-YYYY-NNNN and is guaranteed not to collide
 * with an identifier already present in the database.
 *
 * @param db      Database to read (must not be NULL).
 * @param year    Year to embed in the identifier.
 * @param result  Buffer that receives the identifier (must not be NULL).
 * @param resultSize Size of the result buffer.
 * @return CF_SUCCESS on success, CF_ERR_NULL_POINTER or CF_ERR_INVALID_ARGUMENT.
 */
CfStatus_t caseManager_generateId(const CasefileDb_t *db, int year, char *result, size_t resultSize);

#endif /* CASEFILE_CASE_MANAGER_H */
