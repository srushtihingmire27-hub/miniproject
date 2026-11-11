/*
 * evidence_manager.h - Forensic evidence register, analysis attributes and
 * chain-of-custody history.
 */

#ifndef CASEFILE_EVIDENCE_MANAGER_H
#define CASEFILE_EVIDENCE_MANAGER_H

#include "casefile.h"

/**
 * Register a forensic evidence item against an existing case.
 *
 * The evidence identifier is generated automatically in the form EV-NNNN and is
 * guaranteed not to collide with an existing identifier.
 *
 * @param db          Database to modify (must not be NULL).
 * @param evidenceData Evidence to store; the evidenceId field is overwritten
 *                     with the generated identifier (must not be NULL).
 * @return CF_SUCCESS on success, CF_ERR_NULL_POINTER, CF_ERR_INVALID_ARGUMENT,
 *         CF_ERR_NOT_FOUND when the parent case does not exist,
 *         CF_ERR_EMPTY_INPUT, CF_ERR_INVALID_LENGTH, CF_ERR_INVALID_DATE,
 *         CF_ERR_INVALID_ENUM, CF_ERR_CAPACITY_FULL or CF_ERR_DUPLICATE_ID.
 */
CfStatus_t evidenceManager_add(CasefileDb_t *db, Evidence_t *evidenceData);

/**
 * Find an evidence item by identifier.
 *
 * @param db         Database to search (must not be NULL).
 * @param evidenceId Identifier to search for (must not be NULL).
 * @return Index of the matching evidence, or -1 when not found.
 */
int evidenceManager_findById(const CasefileDb_t *db, const char *evidenceId);

/**
 * Copy an evidence item into the supplied buffer.
 *
 * @param db         Database to search (must not be NULL).
 * @param evidenceId Identifier to search for (must not be NULL).
 * @param result     Receives a copy of the evidence (must not be NULL).
 * @return CF_SUCCESS on success, CF_ERR_NULL_POINTER, CF_ERR_INVALID_ARGUMENT
 *         or CF_ERR_NOT_FOUND.
 */
CfStatus_t evidenceManager_get(const CasefileDb_t *db, const char *evidenceId, Evidence_t *result);

/**
 * Copy the evidence that belongs to the given case into the supplied array.
 *
 * @param db       Database to read (must not be NULL).
 * @param caseId   Identifier of the parent case (must not be NULL).
 * @param result   Destination array (must not be NULL).
 * @param capacity Number of elements the destination array can hold.
 * @param count    Receives the number of evidence items copied (must not be NULL).
 * @return CF_SUCCESS on success, CF_ERR_NULL_POINTER, CF_ERR_INVALID_ARGUMENT
 *         or CF_ERR_CAPACITY_FULL.
 */
CfStatus_t evidenceManager_listByCase(const CasefileDb_t *db, const char *caseId,
                                      Evidence_t *result, size_t capacity, size_t *count);

/**
 * Copy the evidence whose description contains the given text into the array.
 *
 * The comparison ignores case. An empty search text matches every item.
 *
 * @param db         Database to read (must not be NULL).
 * @param searchText Text to search for (must not be NULL).
 * @param result     Destination array (must not be NULL).
 * @param capacity   Number of elements the destination array can hold.
 * @param count      Receives the number of evidence items copied (must not be NULL).
 * @return CF_SUCCESS on success, CF_ERR_NULL_POINTER, CF_ERR_INVALID_ARGUMENT
 *         or CF_ERR_CAPACITY_FULL.
 */
CfStatus_t evidenceManager_searchByDescription(const CasefileDb_t *db, const char *searchText,
                                               Evidence_t *result, size_t capacity, size_t *count);

/**
 * Copy the evidence that has the given category into the supplied array.
 *
 * @param db       Database to read (must not be NULL).
 * @param category Category to filter by.
 * @param result   Destination array (must not be NULL).
 * @param capacity Number of elements the destination array can hold.
 * @param count    Receives the number of evidence items copied (must not be NULL).
 * @return CF_SUCCESS on success, CF_ERR_NULL_POINTER, CF_ERR_INVALID_ARGUMENT
 *         or CF_ERR_CAPACITY_FULL.
 */
CfStatus_t evidenceManager_filterByCategory(const CasefileDb_t *db, EvidenceCategory_t category,
                                            Evidence_t *result, size_t capacity, size_t *count);

/**
 * Copy the evidence that has the given examination status into the array.
 *
 * @param db       Database to read (must not be NULL).
 * @param status   Status to filter by.
 * @param result   Destination array (must not be NULL).
 * @param capacity Number of elements the destination array can hold.
 * @param count    Receives the number of evidence items copied (must not be NULL).
 * @return CF_SUCCESS on success, CF_ERR_NULL_POINTER, CF_ERR_INVALID_ARGUMENT
 *         or CF_ERR_CAPACITY_FULL.
 */
CfStatus_t evidenceManager_filterByStatus(const CasefileDb_t *db, EvidenceStatus_t status,
                                          Evidence_t *result, size_t capacity, size_t *count);

/**
 * Update the editable fields of an existing evidence item.
 *
 * The identifier, the parent case, the analysis attributes and the custody
 * history are preserved.
 *
 * @param db          Database to modify (must not be NULL).
 * @param evidenceId  Identifier of the evidence to update (must not be NULL).
 * @param evidenceData New values for the editable fields (must not be NULL).
 * @return CF_SUCCESS on success, CF_ERR_NULL_POINTER, CF_ERR_INVALID_ARGUMENT,
 *         CF_ERR_EMPTY_INPUT, CF_ERR_INVALID_LENGTH, CF_ERR_INVALID_DATE,
 *         CF_ERR_INVALID_ENUM or CF_ERR_NOT_FOUND.
 */
CfStatus_t evidenceManager_update(CasefileDb_t *db, const char *evidenceId, const Evidence_t *evidenceData);

/**
 * Change the examination status of an evidence item.
 *
 * @param db         Database to modify (must not be NULL).
 * @param evidenceId Identifier of the evidence (must not be NULL).
 * @param status     New examination status.
 * @return CF_SUCCESS on success, CF_ERR_NULL_POINTER, CF_ERR_INVALID_ARGUMENT,
 *         CF_ERR_INVALID_ENUM or CF_ERR_NOT_FOUND.
 */
CfStatus_t evidenceManager_setStatus(CasefileDb_t *db, const char *evidenceId, EvidenceStatus_t status);

/**
 * Attach an analysis attribute to an evidence item.
 *
 * The attribute key must be one of the supported comparison keys. A key that
 * is already present on the item is replaced rather than duplicated.
 *
 * @param db         Database to modify (must not be NULL).
 * @param evidenceId Identifier of the evidence (must not be NULL).
 * @param key        Attribute key to attach.
 * @param value      Attribute value (must not be NULL).
 * @return CF_SUCCESS on success, CF_ERR_NULL_POINTER, CF_ERR_INVALID_ARGUMENT,
 *         CF_ERR_EMPTY_INPUT, CF_ERR_INVALID_LENGTH, CF_ERR_INVALID_ENUM,
 *         CF_ERR_CAPACITY_FULL or CF_ERR_NOT_FOUND.
 */
CfStatus_t evidenceManager_addAttribute(CasefileDb_t *db, const char *evidenceId,
                                        AttributeKey_t key, const char *value);

/**
 * Append a chain-of-custody event to an evidence item.
 *
 * Events are stored in the order they are added, forming the custody history.
 *
 * @param db         Database to modify (must not be NULL).
 * @param evidenceId Identifier of the evidence (must not be NULL).
 * @param event      Event to append (must not be NULL).
 * @return CF_SUCCESS on success, CF_ERR_NULL_POINTER, CF_ERR_INVALID_ARGUMENT,
 *         CF_ERR_EMPTY_INPUT, CF_ERR_INVALID_LENGTH, CF_ERR_INVALID_DATE,
 *         CF_ERR_INVALID_ENUM, CF_ERR_CAPACITY_FULL or CF_ERR_NOT_FOUND.
 */
CfStatus_t evidenceManager_addCustodyEvent(CasefileDb_t *db, const char *evidenceId,
                                            const CustodyEvent_t *event);

/**
 * Copy the chain-of-custody history of an evidence item into the array.
 *
 * @param db         Database to read (must not be NULL).
 * @param evidenceId Identifier of the evidence (must not be NULL).
 * @param result     Destination array (must not be NULL).
 * @param capacity   Number of elements the destination array can hold.
 * @param count      Receives the number of events copied (must not be NULL).
 * @return CF_SUCCESS on success, CF_ERR_NULL_POINTER, CF_ERR_INVALID_ARGUMENT
 *         or CF_ERR_NOT_FOUND.
 */
CfStatus_t evidenceManager_getCustodyHistory(const CasefileDb_t *db, const char *evidenceId,
                                              CustodyEvent_t *result, size_t capacity, size_t *count);

/**
 * Delete an evidence item.
 *
 * @param db         Database to modify (must not be NULL).
 * @param evidenceId Identifier of the evidence to delete (must not be NULL).
 * @return CF_SUCCESS on success, CF_ERR_NULL_POINTER, CF_ERR_INVALID_ARGUMENT
 *         or CF_ERR_NOT_FOUND.
 */
CfStatus_t evidenceManager_delete(CasefileDb_t *db, const char *evidenceId);

/**
 * Generate the next unused evidence identifier.
 *
 * @param db         Database to read (must not be NULL).
 * @param result     Buffer that receives the identifier (must not be NULL).
 * @param resultSize Size of the result buffer.
 * @return CF_SUCCESS on success, CF_ERR_NULL_POINTER, CF_ERR_INVALID_ARGUMENT
 *         or CF_ERR_CAPACITY_FULL.
 */
CfStatus_t evidenceManager_generateId(const CasefileDb_t *db, char *result, size_t resultSize);

/**
 * Count the evidence that belongs to the given case.
 *
 * @param db     Database to read (must not be NULL).
 * @param caseId Identifier of the parent case (must not be NULL).
 * @return Number of matching evidence items, or 0 on error.
 */
size_t evidenceManager_countByCase(const CasefileDb_t *db, const char *caseId);

/**
 * Count the evidence of a case that has reached the analysed or reported stage.
 *
 * @param db     Database to read (must not be NULL).
 * @param caseId Identifier of the parent case (must not be NULL).
 * @return Number of analysed evidence items, or 0 on error.
 */
size_t evidenceManager_countAnalysed(const CasefileDb_t *db, const char *caseId);

#endif /* CASEFILE_EVIDENCE_MANAGER_H */
