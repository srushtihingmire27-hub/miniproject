/*
 * investigation.h - Rule-based evidence analysis and progress computation.
 *
 * The scoring engine compares the analysis attributes recorded against the
 * evidence of a case with the physical profile of each suspect of that case.
 * Every comparison is a fixed, documented rule; no randomness is involved and
 * the result is always explainable.
 *
 * IMPORTANT: the resulting score is a decision-support indicator that helps an
 * investigator prioritise where to look next. It is not a forensic conclusion,
 * not a probability, and not evidence of guilt.
 */

#ifndef CASEFILE_INVESTIGATION_H
#define CASEFILE_INVESTIGATION_H

#include "casefile.h"

/**
 * Analyse a case: score every suspect against the case evidence and rank the
 * results by descending score.
 *
 * The result reports how many evidence items carried at least one comparable
 * attribute. When that count is zero, or when the case has no suspects, the
 * function returns CF_ERR_INSUFFICIENT_EVIDENCE and no scores are produced.
 *
 * @param db     Database to read (must not be NULL).
 * @param caseId Identifier of the case to analyse (must not be NULL).
 * @param result Receives the analysis result (must not be NULL).
 * @return CF_SUCCESS on success, CF_ERR_NULL_POINTER, CF_ERR_INVALID_ARGUMENT,
 *         CF_ERR_NOT_FOUND when the case does not exist, or
 *         CF_ERR_INSUFFICIENT_EVIDENCE when the evidence cannot support a score.
 */
CfStatus_t investigation_analyseCase(const CasefileDb_t *db, const char *caseId,
                                      AnalysisResult_t *result);

/**
 * Compare a single evidence attribute with the matching suspect attribute.
 *
 * Exposed separately so that the individual rules can be unit-tested in
 * isolation from the full analysis.
 *
 * @param key      Attribute key being compared.
 * @param evidenceValue Value recorded against the evidence (must not be NULL).
 * @param suspectValue  Value recorded against the suspect (must not be NULL).
 * @param matched  Receives 1 when the values coincide, 0 otherwise (must not be NULL).
 * @return The weight contributed by a match, or 0 when the values do not match
 *         or cannot be compared.
 */
int investigation_compareAttribute(AttributeKey_t key, const char *evidenceValue,
                                   const char *suspectValue, int *matched);

/**
 * Return the weight applied to a given attribute key.
 *
 * @param key Attribute key.
 * @return The weight, or 0 when the key is not comparable.
 */
int investigation_weightForKey(AttributeKey_t key);

/**
 * Rank scored suspects by descending score.
 *
 * The ranking is stable: suspects with equal scores keep their original
 * relative order.
 *
 * @param scores Array of scored suspects (must not be NULL).
 * @param count  Number of elements in the array.
 */
void investigation_rankSuspects(SuspectScore_t *scores, int count);

/**
 * Compute the investigation progress of a case.
 *
 * The progress percentage is derived from real record counts: how many
 * suspects are recorded, how much of the evidence has been analysed and how
 * many witness statements have been verified.
 *
 * @param db       Database to read (must not be NULL).
 * @param caseId   Identifier of the case (must not be NULL).
 * @param percent  Receives the progress percentage 0-100 (must not be NULL).
 * @param detail   Optional buffer that receives a short textual breakdown;
 *                 may be NULL. Must be at least TEXT_LENGTH bytes when supplied.
 * @param detailSize Size of the detail buffer.
 * @return CF_SUCCESS on success, CF_ERR_NULL_POINTER, CF_ERR_INVALID_ARGUMENT
 *         or CF_ERR_NOT_FOUND when the case does not exist.
 */
CfStatus_t investigation_computeProgress(const CasefileDb_t *db, const char *caseId,
                                         int *percent, char *detail, size_t detailSize);

/**
 * Return the educational disclaimer that accompanies every score.
 *
 * @return Static string describing the purpose and limits of the score.
 */
const char *investigation_getDisclaimer(void);

/**
 * Return a short label describing a suspicion score.
 *
 * @param score Score between 0 and MAX_SUSPICION_SCORE.
 * @return Static string describing the score band.
 */
const char *investigation_describeScore(int score);

#endif /* CASEFILE_INVESTIGATION_H */
