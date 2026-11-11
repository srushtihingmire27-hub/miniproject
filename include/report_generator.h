/*
 * report_generator.h - Investigation and evidence report composition.
 */

#ifndef CASEFILE_REPORT_GENERATOR_H
#define CASEFILE_REPORT_GENERATOR_H

#include "casefile.h"

/** Directory in which generated report files are written. */
#define REPORT_OUTPUT_DIRECTORY "reports"

/**
 * Build the investigation summary report for a case.
 *
 * The report contains the case identity and status, the suspects with their
 * scores and the factors behind them, the evidence register, and the witness
 * statements.
 *
 * @param db       Database to read (must not be NULL).
 * @param caseId   Identifier of the case to report on (must not be NULL).
 * @param buffer   Buffer that receives the report (must not be NULL).
 * @param bufferSize Size of the buffer.
 * @return CF_SUCCESS on success, CF_ERR_NULL_POINTER, CF_ERR_INVALID_ARGUMENT,
 *         CF_ERR_NOT_FOUND when the case does not exist, or
 *         CF_ERR_INSUFFICIENT_EVIDENCE when the analysis could not be run.
 */
CfStatus_t report_generateCaseSummary(const CasefileDb_t *db, const char *caseId,
                                      char *buffer, size_t bufferSize);

/**
 * Build the evidence summary report for a case.
 *
 * @param db       Database to read (must not be NULL).
 * @param caseId   Identifier of the case to report on (must not be NULL).
 * @param buffer   Buffer that receives the report (must not be NULL).
 * @param bufferSize Size of the buffer.
 * @return CF_SUCCESS on success, CF_ERR_NULL_POINTER, CF_ERR_INVALID_ARGUMENT
 *         or CF_ERR_NOT_FOUND when the case does not exist.
 */
CfStatus_t report_generateEvidenceSummary(const CasefileDb_t *db, const char *caseId,
                                          char *buffer, size_t bufferSize);

/**
 * Write a report to a file in the report directory.
 *
 * The file is named after the case and the report kind.
 *
 * @param caseId   Identifier of the case the report belongs to (must not be NULL).
 * @param reportKind Short kind name used in the file name (must not be NULL).
 * @param content  Report text to write (must not be NULL).
 * @param pathOut  Optional buffer that receives the written path; may be NULL.
 * @param pathOutSize Size of the path buffer.
 * @return CF_SUCCESS on success, CF_ERR_NULL_POINTER, CF_ERR_INVALID_ARGUMENT
 *         or CF_ERR_FILE_WRITE when the file could not be written.
 */
CfStatus_t report_saveToFile(const char *caseId, const char *reportKind,
                             const char *content, char *pathOut, size_t pathOutSize);

#endif /* CASEFILE_REPORT_GENERATOR_H */
