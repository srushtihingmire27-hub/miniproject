/*
 * validation.h - Reusable validation and input helpers for CASEFILE.
 *
 * Every user-entered value passes through this module before it reaches a
 * record. Centralising the checks here means no two modules can disagree about
 * what a valid date, identifier or enumeration value is, and each check is
 * independently unit-testable.
 */

#ifndef CASEFILE_VALIDATION_H
#define CASEFILE_VALIDATION_H

#include "casefile.h"

/**
 * Read a line of text from standard input into a bounded buffer.
 *
 * The buffer is always NUL-terminated. Any characters beyond the buffer
 * length are discarded, so an over-long input can never overflow the buffer.
 *
 * @param prompt Text displayed before reading.
 * @param buffer Destination buffer.
 * @param bufferSize Size of the destination buffer in bytes.
 * @return CF_SUCCESS when a line was read, CF_ERR_INVALID_ARGUMENT when the
 *         arguments are invalid, CF_ERR_FILE_READ when standard input failed.
 */
CfStatus_t validationReadLine(const char *prompt, char *buffer, size_t bufferSize);

/**
 * Trim leading and trailing whitespace from a string in place.
 *
 * @param text String to trim (must not be NULL).
 */
void validationTrim(char *text);

/**
 * Copy a string into a fixed-size field, truncating safely.
 *
 * The destination is always NUL-terminated and is emptied when the source is
 * NULL. Used when storing into a record field whose size is known.
 *
 * @param destination Destination buffer.
 * @param destinationSize Size of the destination buffer in bytes.
 * @param source Source string.
 */
void validationCopyField(char *destination, size_t destinationSize, const char *source);

/**
 * Determine whether a string contains at least one non-whitespace character.
 *
 * @param text String to test (NULL is treated as empty).
 * @return 1 when the string has content, 0 otherwise.
 */
int validationIsNotEmpty(const char *text);

/**
 * Determine whether a string contains only printable characters.
 *
 * @param text String to test (NULL is treated as invalid).
 * @return 1 when the string is printable, 0 otherwise.
 */
int validationIsPrintable(const char *text);

/**
 * Determine whether a string fits inside a field without truncation.
 *
 * @param text          String to test (NULL is treated as invalid).
 * @param maxFieldLength Maximum permitted length including the terminator.
 * @return CF_SUCCESS when the string fits, CF_ERR_INVALID_LENGTH otherwise.
 */
CfStatus_t validationCheckLength(const char *text, size_t maxFieldLength);

/**
 * Compare two strings ignoring case.
 *
 * @param left  First string (must not be NULL).
 * @param right Second string (must not be NULL).
 * @return A negative value, zero or a positive value when left is less than,
 *         equal to or greater than right, ignoring case.
 */
int validationCompareIgnoreCase(const char *left, const char *right);

/**
 * Determine whether a string contains a substring, ignoring case.
 *
 * @param haystack String to search in (must not be NULL).
 * @param needle   Substring to look for (must not be NULL).
 * @return 1 when the substring is present, 0 otherwise.
 */
int validationContainsIgnoreCase(const char *haystack, const char *needle);

/**
 * Parse a string as a base-10 integer.
 *
 * The whole string must be a valid integer; trailing characters such as
 * "12abc" are rejected. An optional leading '+' or '-' is accepted.
 *
 * @param text   String to parse (must not be NULL).
 * @param result Receives the parsed value (must not be NULL).
 * @return CF_SUCCESS on success, CF_ERR_INVALID_FORMAT when the text is not a
 *         valid integer, CF_ERR_INVALID_ARGUMENT when arguments are invalid.
 */
CfStatus_t validationParseInt(const char *text, int *result);

/**
 * Read an integer from the user, re-prompting until valid input is given.
 *
 * @param prompt        Text displayed before each read attempt.
 * @param minValue      Smallest permitted value.
 * @param maxValue      Largest permitted value.
 * @param result        Receives the parsed value (must not be NULL).
 * @return CF_SUCCESS on success, CF_ERR_INVALID_ARGUMENT when arguments are invalid.
 */
CfStatus_t validationReadInt(const char *prompt, int minValue, int maxValue, int *result);

/**
 * Determine whether a string is a valid calendar date in YYYY-MM-DD form.
 *
 * The year must be four digits, the month 01-12 and the day valid for that
 * month, including leap-year rules.
 *
 * @param text Date string to test (must not be NULL).
 * @return 1 when the date is valid, 0 otherwise.
 */
int validationIsValidDate(const char *text);

/**
 * Determine whether a string is a valid identifier for the given kind.
 *
 * A valid identifier is a non-empty run of uppercase letters, digits and
 * hyphens, beginning with a letter, and no longer than ID_LENGTH - 1.
 *
 * @param text Identifier to test (must not be NULL).
 * @return 1 when the identifier is well formed, 0 otherwise.
 */
int validationIsValidIdentifier(const char *text);

/**
 * Determine whether a phone number is plausible.
 *
 * Accepts an optional leading '+' followed by 7 to 15 digits.
 *
 * @param text Phone number to test (must not be NULL).
 * @return 1 when the number is plausible, 0 otherwise.
 */
int validationIsValidPhone(const char *text);

/**
 * Determine whether an age is within the accepted range.
 *
 * @param age Age in years.
 * @return 1 when the age is plausible, 0 otherwise.
 */
int validationIsValidAge(int age);

/**
 * Determine whether a height in centimetres is within the accepted range.
 *
 * @param heightCm Height in centimetres.
 * @return 1 when the height is plausible, 0 otherwise.
 */
int validationIsValidHeight(int heightCm);

/**
 * Determine whether a shoe size (European) is within the accepted range.
 *
 * @param shoeSize Shoe size.
 * @return 1 when the size is plausible, 0 otherwise.
 */
int validationIsValidShoeSize(int shoeSize);

/**
 * Convert a case status name to its enumeration value.
 *
 * The comparison ignores case and surrounding whitespace.
 *
 * @param text  Name to convert (must not be NULL).
 * @param result Receives the enumeration value (must not be NULL).
 * @return CF_SUCCESS on success, CF_ERR_INVALID_ENUM when the name is unknown.
 */
CfStatus_t validationParseCaseStatus(const char *text, CaseStatus_t *result);

/**
 * Convert a case priority name to its enumeration value.
 *
 * @param text  Name to convert (must not be NULL).
 * @param result Receives the enumeration value (must not be NULL).
 * @return CF_SUCCESS on success, CF_ERR_INVALID_ENUM when the name is unknown.
 */
CfStatus_t validationParseCasePriority(const char *text, CasePriority_t *result);

/**
 * Convert a crime type name to its enumeration value.
 *
 * @param text  Name to convert (must not be NULL).
 * @param result Receives the enumeration value (must not be NULL).
 * @return CF_SUCCESS on success, CF_ERR_INVALID_ENUM when the name is unknown.
 */
CfStatus_t validationParseCrimeType(const char *text, CrimeType_t *result);

/**
 * Convert a suspect status name to its enumeration value.
 *
 * @param text  Name to convert (must not be NULL).
 * @param result Receives the enumeration value (must not be NULL).
 * @return CF_SUCCESS on success, CF_ERR_INVALID_ENUM when the name is unknown.
 */
CfStatus_t validationParseSuspectStatus(const char *text, SuspectStatus_t *result);

/**
 * Convert an evidence category name to its enumeration value.
 *
 * @param text  Name to convert (must not be NULL).
 * @param result Receives the enumeration value (must not be NULL).
 * @return CF_SUCCESS on success, CF_ERR_INVALID_ENUM when the name is unknown.
 */
CfStatus_t validationParseEvidenceCategory(const char *text, EvidenceCategory_t *result);

/**
 * Convert an evidence status name to its enumeration value.
 *
 * @param text  Name to convert (must not be NULL).
 * @param result Receives the enumeration value (must not be NULL).
 * @return CF_SUCCESS on success, CF_ERR_INVALID_ENUM when the name is unknown.
 */
CfStatus_t validationParseEvidenceStatus(const char *text, EvidenceStatus_t *result);

/**
 * Convert a custody action name to its enumeration value.
 *
 * @param text  Name to convert (must not be NULL).
 * @param result Receives the enumeration value (must not be NULL).
 * @return CF_SUCCESS on success, CF_ERR_INVALID_ENUM when the name is unknown.
 */
CfStatus_t validationParseCustodyAction(const char *text, CustodyAction_t *result);

/**
 * Convert an attribute key name to its enumeration value.
 *
 * @param text  Name to convert (must not be NULL).
 * @param result Receives the enumeration value (must not be NULL).
 * @return CF_SUCCESS on success, CF_ERR_INVALID_ENUM when the name is unknown.
 */
CfStatus_t validationParseAttributeKey(const char *text, AttributeKey_t *result);

/**
 * Return the display name of a case status.
 *
 * @param status Status value.
 * @return Static string describing the status.
 */
const char *validationCaseStatusName(CaseStatus_t status);

/**
 * Return the display name of a case priority.
 *
 * @param priority Priority value.
 * @return Static string describing the priority.
 */
const char *validationCasePriorityName(CasePriority_t priority);

/**
 * Return the display name of a crime type.
 *
 * @param crimeType Crime type value.
 * @return Static string describing the crime type.
 */
const char *validationCrimeTypeName(CrimeType_t crimeType);

/**
 * Return the display name of a suspect status.
 *
 * @param status Status value.
 * @return Static string describing the status.
 */
const char *validationSuspectStatusName(SuspectStatus_t status);

/**
 * Return the display name of an evidence category.
 *
 * @param category Category value.
 * @return Static string describing the category.
 */
const char *validationEvidenceCategoryName(EvidenceCategory_t category);

/**
 * Return the display name of an evidence status.
 *
 * @param status Status value.
 * @return Static string describing the status.
 */
const char *validationEvidenceStatusName(EvidenceStatus_t status);

/**
 * Return the display name of a custody action.
 *
 * @param action Action value.
 * @return Static string describing the action.
 */
const char *validationCustodyActionName(CustodyAction_t action);

/**
 * Return the display name of an attribute key.
 *
 * @param key Attribute key value.
 * @return Static string describing the key.
 */
const char *validationAttributeKeyName(AttributeKey_t key);

/**
 * Return a human-readable message describing a status code.
 *
 * @param status Status value.
 * @return Static string describing the status.
 */
const char *validationStatusMessage(CfStatus_t status);

#endif /* CASEFILE_VALIDATION_H */
