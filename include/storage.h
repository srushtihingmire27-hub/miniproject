/*
 * storage.h - File persistence for CASEFILE.
 *
 * The application stores its records in plain text files inside the data
 * directory. Each file begins with a signature line so that a file written by
 * another program, or a file that has been damaged, can be detected on load
 * instead of being silently misread.
 *
 * The format is line oriented: one record per line, fields separated by the
 * FIELD_SEPARATOR character. The signature line states the separator that was
 * used, so the reader does not have to guess.
 */

#ifndef CASEFILE_STORAGE_H
#define CASEFILE_STORAGE_H

#include <stdio.h>

#include "casefile.h"

/** Name of the directory holding the data files, relative to the working directory. */
#define STORAGE_DATA_DIRECTORY "data"

/** Signature written at the top of every data file. */
#define STORAGE_FILE_SIGNATURE "CASEFILE-DATA-V1"

/** Character used to separate the fields of a stored record. */
#define STORAGE_FIELD_SEPARATOR '|'

/** Number of data files the application maintains. */
#define STORAGE_FILE_COUNT 5

/**
 * Load every record from the data directory into the database.
 *
 * A missing file is not an error: the application starts with an empty
 * database. A file that exists but carries an unrecognised signature is
 * reported as CF_ERR_CORRUPT_FILE so that the user knows the file was not
 * read.
 *
 * @param db Database to fill (must not be NULL).
 * @return CF_SUCCESS when all files were read, CF_ERR_NULL_POINTER when the
 *         database pointer is invalid, CF_ERR_CORRUPT_FILE when a file has an
 *         unrecognised format, or CF_ERR_FILE_OPEN when a file could not be
 *         opened for reading.
 */
CfStatus_t storage_loadAll(CasefileDb_t *db);

/**
 * Save every record in the database to the data directory.
 *
 * @param db Database to save (must not be NULL).
 * @return CF_SUCCESS when all files were written, CF_ERR_NULL_POINTER,
 *         CF_ERR_FILE_OPEN or CF_ERR_FILE_WRITE on failure.
 */
CfStatus_t storage_saveAll(const CasefileDb_t *db);

/**
 * Check that a file carries the expected format signature.
 *
 * @param path Path of the file to check (must not be NULL).
 * @return CF_SUCCESS when the file is usable, CF_ERR_NULL_POINTER,
 *         CF_ERR_FILE_OPEN when it cannot be read, CF_ERR_CORRUPT_FILE when the
 *         signature does not match.
 */
CfStatus_t storage_validateFormat(const char *path);

/**
 * Return the path of one of the data files.
 *
 * @param fileName Name of the file within the data directory (must not be NULL).
 * @param result   Buffer that receives the path (must not be NULL).
 * @param resultSize Size of the result buffer.
 * @return CF_SUCCESS on success, CF_ERR_NULL_POINTER or CF_ERR_INVALID_ARGUMENT.
 */
CfStatus_t storage_buildPath(const char *fileName, char *result, size_t resultSize);

/**
 * Return the file name of one of the data files by index.
 *
 * @param index Index between 0 and STORAGE_FILE_COUNT - 1.
 * @return The file name, or NULL when the index is out of range.
 */
const char *storage_fileNameByIndex(int index);

/**
 * Create the data directory when it does not yet exist.
 *
 * @return CF_SUCCESS when the directory exists afterwards, CF_ERR_FILE_OPEN
 *         when it could not be created.
 */
CfStatus_t storage_ensureDataDirectory(void);

/**
 * Write a line to a file, checking that the write succeeded.
 *
 * Exposed so that the report generator can reuse the same checked-write
 * behaviour as the persistence layer.
 *
 * @param stream Open stream to write to (must not be NULL).
 * @param text   Text to write (must not be NULL).
 * @return CF_SUCCESS on success, CF_ERR_NULL_POINTER or CF_ERR_FILE_WRITE.
 */
CfStatus_t storage_writeLine(FILE *stream, const char *text);

/**
 * Remove characters that would break the field separator of a stored record.
 *
 * The separator and any newline characters are replaced with a space so that a
 * record can never span more than one line.
 *
 * @param text Text to sanitise in place (must not be NULL).
 */
void storage_sanitizeField(char *text);

#endif /* CASEFILE_STORAGE_H */
