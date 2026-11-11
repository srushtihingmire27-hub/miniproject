/*
 * storage.c - Implementation of the file persistence layer.
 *
 * Record layouts (fields separated by '|'):
 *
 *   cases.txt     caseId|title|crimeType|priority|status|incidentDate|location|officer|summary
 *   suspects.txt  suspectId|caseId|name|age|phone|address|alibi|motive|status|
 *                 gender|height|hair|shoe|blood|vehicles
 *   evidence.txt  evidenceId|caseId|category|description|collectionDate|location|
 *                 collector|status|attributeCount|key=value;key=value
 *   custody.txt   evidenceId|action|timestamp|fromHandler|toHandler|location|remarks
 *   witnesses.txt witnessId|caseId|statementRef|name|phone|statement|recordedDate|verified
 */

#include "storage.h"
#include "validation.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#define FILE_CASES      "cases.txt"
#define FILE_SUSPECTS   "suspects.txt"
#define FILE_EVIDENCE   "evidence.txt"
#define FILE_CUSTODY    "custody.txt"
#define FILE_WITNESSES  "witnesses.txt"

static const char *const fileNames[ STORAGE_FILE_COUNT ] = {
    FILE_CASES, FILE_SUSPECTS, FILE_EVIDENCE, FILE_CUSTODY, FILE_WITNESSES
};

/* ------------------------------------------------------------------ */
/* Low level helpers                                                   */
/* ------------------------------------------------------------------ */

void storage_sanitizeField(char *text)
{
    size_t index;

    if (text == NULL)
    {
        return;
    }

    for (index = 0U; text[ index ] != '\0'; index++)
    {
        char current = text[ index ];

        if (current == STORAGE_FIELD_SEPARATOR || current == '\n' || current == '\r')
        {
            text[ index ] = ' ';
        }
    }
}

CfStatus_t storage_writeLine(FILE *stream, const char *text)
{
    if (stream == NULL || text == NULL)
    {
        return CF_ERR_NULL_POINTER;
    }

    if (fputs(text, stream) == EOF)
    {
        return CF_ERR_FILE_WRITE;
    }
    if (fputc('\n', stream) == EOF)
    {
        return CF_ERR_FILE_WRITE;
    }
    return CF_SUCCESS;
}

const char *storage_fileNameByIndex(int index)
{
    if (index < 0 || index >= STORAGE_FILE_COUNT)
    {
        return NULL;
    }
    return fileNames[ index ];
}

CfStatus_t storage_buildPath(const char *fileName, char *result, size_t resultSize)
{
    if (fileName == NULL || result == NULL || resultSize == 0U)
    {
        return CF_ERR_INVALID_ARGUMENT;
    }

    snprintf(result, resultSize, "%s/%s", STORAGE_DATA_DIRECTORY, fileName);
    return CF_SUCCESS;
}

CfStatus_t storage_ensureDataDirectory(void)
{
    struct stat info;

    if (stat(STORAGE_DATA_DIRECTORY, &info) == 0)
    {
        if ((info.st_mode & S_IFDIR) != 0)
        {
            return CF_SUCCESS;
        }
        return CF_ERR_FILE_OPEN;
    }

#if defined(_WIN32)
    if (mkdir(STORAGE_DATA_DIRECTORY) == 0)
#else
    if (mkdir(STORAGE_DATA_DIRECTORY, 0755) == 0)
#endif
    {
        return CF_SUCCESS;
    }

    /* The directory may have been created by another run in the meantime. */
    if (stat(STORAGE_DATA_DIRECTORY, &info) == 0 && (info.st_mode & S_IFDIR) != 0)
    {
        return CF_SUCCESS;
    }
    return CF_ERR_FILE_OPEN;
}

/*
 * Open a data file for writing and write the signature line.
 */
static CfStatus_t openForWrite(const char *fileName, FILE **stream)
{
    char path[FILE_PATH_LENGTH];
    FILE *handle;

    if (storage_buildPath(fileName, path, sizeof(path)) != CF_SUCCESS)
    {
        return CF_ERR_INVALID_ARGUMENT;
    }

    handle = fopen(path, "w");
    if (handle == NULL)
    {
        return CF_ERR_FILE_OPEN;
    }

    if (storage_writeLine(handle, STORAGE_FILE_SIGNATURE) != CF_SUCCESS)
    {
        fclose(handle);
        return CF_ERR_FILE_WRITE;
    }

    *stream = handle;
    return CF_SUCCESS;
}

/*
 * Open a data file for reading and verify its signature.
 *
 * A file that does not exist is reported with CF_ERR_FILE_OPEN and the
 * "missing" flag set, which the caller treats as "nothing to load".
 */
static CfStatus_t openForRead(const char *fileName, FILE **stream, int *missing)
{
    char path[FILE_PATH_LENGTH];
    char line[FILE_LINE_LENGTH];
    FILE *handle;

    if (missing != NULL)
    {
        *missing = 0;
    }

    if (storage_buildPath(fileName, path, sizeof(path)) != CF_SUCCESS)
    {
        return CF_ERR_INVALID_ARGUMENT;
    }

    handle = fopen(path, "r");
    if (handle == NULL)
    {
        if (missing != NULL)
        {
            *missing = 1;
        }
        return CF_ERR_FILE_OPEN;
    }

    if (fgets(line, (int)sizeof(line), handle) == NULL)
    {
        fclose(handle);
        return CF_ERR_CORRUPT_FILE;
    }

    line[ strcspn(line, "\r\n") ] = '\0';
    if (strcmp(line, STORAGE_FILE_SIGNATURE) != 0)
    {
        fclose(handle);
        return CF_ERR_CORRUPT_FILE;
    }

    *stream = handle;
    return CF_SUCCESS;
}

/*
 * Split a stored line into its fields.
 *
 * @param line  Line to split (modified in place).
 * @param fields Array that receives the field pointers.
 * @param maxFields Maximum number of fields to extract.
 * @return The number of fields found.
 */
static int splitFields(char *line, char **fields, int maxFields)
{
    int count = 0;
    char *cursor = line;

    if (line == NULL || fields == NULL || maxFields <= 0)
    {
        return 0;
    }

    fields[ count ] = cursor;
    count++;

    while (count < maxFields)
    {
        char *separator = strchr(cursor, STORAGE_FIELD_SEPARATOR);

        if (separator == NULL)
        {
            break;
        }
        *separator = '\0';
        cursor = separator + 1;
        fields[ count ] = cursor;
        count++;
    }

    return count;
}

/*
 * Parse an integer field, returning the fallback when the field is unusable.
 */
static int parseIntOr(const char *text, int fallback)
{
    int value = 0;

    if (text == NULL || validationParseInt(text, &value) != CF_SUCCESS)
    {
        return fallback;
    }
    return value;
}

/* ------------------------------------------------------------------ */
/* Writing                                                             */
/* ------------------------------------------------------------------ */

static CfStatus_t saveCases(const CasefileDb_t *db)
{
    FILE *stream = NULL;
    size_t index;

    if (openForWrite(FILE_CASES, &stream) != CF_SUCCESS)
    {
        return CF_ERR_FILE_OPEN;
    }

    for (index = 0U; index < db->caseCount; index++)
    {
        const Case_t *record = &db->cases[ index ];
        char title[TITLE_LENGTH];
        char location[LOCATION_LENGTH];
        char officer[OFFICER_LENGTH];
        char summary[TEXT_LENGTH];
        char line[FILE_LINE_LENGTH];

        validationCopyField(title, sizeof(title), record->title);
        validationCopyField(location, sizeof(location), record->location);
        validationCopyField(officer, sizeof(officer), record->assignedOfficer);
        validationCopyField(summary, sizeof(summary), record->summary);
        storage_sanitizeField(title);
        storage_sanitizeField(location);
        storage_sanitizeField(officer);
        storage_sanitizeField(summary);

        snprintf(line, sizeof(line), "%s|%s|%d|%d|%d|%s|%s|%s|%s",
                 record->caseId, title, (int)record->crimeType, (int)record->priority,
                 (int)record->status, record->incidentDate, location, officer, summary);

        if (storage_writeLine(stream, line) != CF_SUCCESS)
        {
            fclose(stream);
            return CF_ERR_FILE_WRITE;
        }
    }

    fclose(stream);
    return CF_SUCCESS;
}

static CfStatus_t saveSuspects(const CasefileDb_t *db)
{
    FILE *stream = NULL;
    size_t index;

    if (openForWrite(FILE_SUSPECTS, &stream) != CF_SUCCESS)
    {
        return CF_ERR_FILE_OPEN;
    }

    for (index = 0U; index < db->suspectCount; index++)
    {
        const Suspect_t *record = &db->suspects[ index ];
        char name[NAME_LENGTH];
        char phone[PHONE_LENGTH];
        char address[ADDRESS_LENGTH];
        char alibi[ALIBI_LENGTH];
        char motive[MOTIVE_LENGTH];
        char gender[NAME_LENGTH];
        char hair[NAME_LENGTH];
        char blood[NAME_LENGTH];
        char vehicles[ ( MAX_VEHICLES * VEHICLE_LENGTH ) + 8U ];
        char line[FILE_LINE_LENGTH];
        int vehicle;
        size_t offset = 0U;

        validationCopyField(name, sizeof(name), record->fullName);
        validationCopyField(phone, sizeof(phone), record->phone);
        validationCopyField(address, sizeof(address), record->address);
        validationCopyField(alibi, sizeof(alibi), record->alibi);
        validationCopyField(motive, sizeof(motive), record->motive);
        validationCopyField(gender, sizeof(gender), record->physical.gender);
        validationCopyField(hair, sizeof(hair), record->physical.hairColour);
        validationCopyField(blood, sizeof(blood), record->physical.bloodGroup);
        storage_sanitizeField(name);
        storage_sanitizeField(phone);
        storage_sanitizeField(address);
        storage_sanitizeField(alibi);
        storage_sanitizeField(motive);
        storage_sanitizeField(gender);
        storage_sanitizeField(hair);
        storage_sanitizeField(blood);

        vehicles[0] = '\0';
        for (vehicle = 0; vehicle < record->physical.vehicleCount && vehicle < MAX_VEHICLES; vehicle++)
        {
            int written = snprintf(vehicles + offset, sizeof(vehicles) - offset, "%s%s",
                                   (vehicle > 0) ? "," : "", record->physical.vehicles[ vehicle ]);
            if (written < 0 || (size_t)written >= (sizeof(vehicles) - offset))
            {
                break;
            }
            offset += (size_t)written;
        }
        storage_sanitizeField(vehicles);

        snprintf(line, sizeof(line), "%s|%s|%s|%d|%s|%s|%s|%s|%d|%s|%d|%s|%d|%s|%s",
                 record->suspectId, record->caseId, name, record->age, phone, address,
                 alibi, motive, (int)record->status, gender, record->physical.heightCm,
                 hair, record->physical.shoeSize, blood, vehicles);

        if (storage_writeLine(stream, line) != CF_SUCCESS)
        {
            fclose(stream);
            return CF_ERR_FILE_WRITE;
        }
    }

    fclose(stream);
    return CF_SUCCESS;
}

static CfStatus_t saveEvidence(const CasefileDb_t *db)
{
    FILE *stream = NULL;
    size_t index;

    if (openForWrite(FILE_EVIDENCE, &stream) != CF_SUCCESS)
    {
        return CF_ERR_FILE_OPEN;
    }

    for (index = 0U; index < db->evidenceCount; index++)
    {
        const Evidence_t *record = &db->evidence[ index ];
        char description[EVIDENCE_DESC_LENGTH];
        char location[LOCATION_LENGTH];
        char collector[NAME_LENGTH];
        char attributes[ ( MAX_EVIDENCE_ATTRIBUTES * ( ATTRIBUTE_VALUE_LENGTH + 16 ) ) + 8U ];
        char line[FILE_LINE_LENGTH];
        int attribute;
        size_t offset = 0U;

        validationCopyField(description, sizeof(description), record->description);
        validationCopyField(location, sizeof(location), record->collectionLocation);
        validationCopyField(collector, sizeof(collector), record->collectedBy);
        storage_sanitizeField(description);
        storage_sanitizeField(location);
        storage_sanitizeField(collector);

        attributes[0] = '\0';
        for (attribute = 0; attribute < record->attributeCount && attribute < MAX_EVIDENCE_ATTRIBUTES; attribute++)
        {
            char value[ATTRIBUTE_VALUE_LENGTH];
            int written;

            validationCopyField(value, sizeof(value), record->attributes[ attribute ].value);
            storage_sanitizeField(value);

            written = snprintf(attributes + offset, sizeof(attributes) - offset, "%s%d=%s",
                               (attribute > 0) ? ";" : "", (int)record->attributes[ attribute ].key, value);
            if (written < 0 || (size_t)written >= (sizeof(attributes) - offset))
            {
                break;
            }
            offset += (size_t)written;
        }

        snprintf(line, sizeof(line), "%s|%s|%d|%s|%s|%s|%s|%d|%d|%s",
                 record->evidenceId, record->caseId, (int)record->category, description,
                 record->collectionDate, location, collector, (int)record->status,
                 record->attributeCount, attributes);

        if (storage_writeLine(stream, line) != CF_SUCCESS)
        {
            fclose(stream);
            return CF_ERR_FILE_WRITE;
        }
    }

    fclose(stream);
    return CF_SUCCESS;
}

static CfStatus_t saveCustody(const CasefileDb_t *db)
{
    FILE *stream = NULL;
    size_t index;

    if (openForWrite(FILE_CUSTODY, &stream) != CF_SUCCESS)
    {
        return CF_ERR_FILE_OPEN;
    }

    for (index = 0U; index < db->evidenceCount; index++)
    {
        const Evidence_t *record = &db->evidence[ index ];
        int event;

        for (event = 0; event < record->custodyCount; event++)
        {
            const CustodyEvent_t *entry = &record->custody[ event ];
            char fromHandler[NAME_LENGTH];
            char toHandler[NAME_LENGTH];
            char location[LOCATION_LENGTH];
            char remarks[CUSTODY_REMARK_LENGTH];
            char line[FILE_LINE_LENGTH];

            validationCopyField(fromHandler, sizeof(fromHandler), entry->fromHandler);
            validationCopyField(toHandler, sizeof(toHandler), entry->toHandler);
            validationCopyField(location, sizeof(location), entry->location);
            validationCopyField(remarks, sizeof(remarks), entry->remarks);
            storage_sanitizeField(fromHandler);
            storage_sanitizeField(toHandler);
            storage_sanitizeField(location);
            storage_sanitizeField(remarks);

            snprintf(line, sizeof(line), "%s|%d|%s|%s|%s|%s|%s",
                     record->evidenceId, (int)entry->action, entry->timestamp,
                     fromHandler, toHandler, location, remarks);

            if (storage_writeLine(stream, line) != CF_SUCCESS)
            {
                fclose(stream);
                return CF_ERR_FILE_WRITE;
            }
        }
    }

    fclose(stream);
    return CF_SUCCESS;
}

static CfStatus_t saveWitnesses(const CasefileDb_t *db)
{
    FILE *stream = NULL;
    size_t index;

    if (openForWrite(FILE_WITNESSES, &stream) != CF_SUCCESS)
    {
        return CF_ERR_FILE_OPEN;
    }

    for (index = 0U; index < db->witnessCount; index++)
    {
        const Witness_t *record = &db->witnesses[ index ];
        char name[NAME_LENGTH];
        char phone[PHONE_LENGTH];
        char statement[STATEMENT_LENGTH];
        char line[FILE_LINE_LENGTH];

        validationCopyField(name, sizeof(name), record->fullName);
        validationCopyField(phone, sizeof(phone), record->phone);
        validationCopyField(statement, sizeof(statement), record->statement);
        storage_sanitizeField(name);
        storage_sanitizeField(phone);
        storage_sanitizeField(statement);

        snprintf(line, sizeof(line), "%s|%s|%s|%s|%s|%s|%s|%d",
                 record->witnessId, record->caseId, record->statementRef, name,
                 phone, statement, record->recordedDate, record->verified);

        if (storage_writeLine(stream, line) != CF_SUCCESS)
        {
            fclose(stream);
            return CF_ERR_FILE_WRITE;
        }
    }

    fclose(stream);
    return CF_SUCCESS;
}

CfStatus_t storage_saveAll(const CasefileDb_t *db)
{
    CfStatus_t status;

    if (db == NULL)
    {
        return CF_ERR_NULL_POINTER;
    }

    if (storage_ensureDataDirectory() != CF_SUCCESS)
    {
        return CF_ERR_FILE_OPEN;
    }

    status = saveCases(db);
    if (status != CF_SUCCESS)
    {
        return status;
    }
    status = saveSuspects(db);
    if (status != CF_SUCCESS)
    {
        return status;
    }
    status = saveEvidence(db);
    if (status != CF_SUCCESS)
    {
        return status;
    }
    status = saveCustody(db);
    if (status != CF_SUCCESS)
    {
        return status;
    }
    status = saveWitnesses(db);
    if (status != CF_SUCCESS)
    {
        return status;
    }
    return CF_SUCCESS;
}

/* ------------------------------------------------------------------ */
/* Reading                                                             */
/* ------------------------------------------------------------------ */

static CfStatus_t loadCases(CasefileDb_t *db)
{
    FILE *stream = NULL;
    int missing = 0;
    char line[FILE_LINE_LENGTH];

    if (openForRead(FILE_CASES, &stream, &missing) != CF_SUCCESS)
    {
        return (missing != 0) ? CF_SUCCESS : CF_ERR_CORRUPT_FILE;
    }

    while (fgets(line, (int)sizeof(line), stream) != NULL)
    {
        char *fields[ 10 ];
        int count;

        line[ strcspn(line, "\r\n") ] = '\0';
        if (line[0] == '\0')
        {
            continue;
        }

        count = splitFields(line, fields, 10);
        if (count < 9)
        {
            fclose(stream);
            return CF_ERR_CORRUPT_FILE;
        }

        if (db->caseCount >= (size_t)MAX_CASES)
        {
            fclose(stream);
            return CF_ERR_CAPACITY_FULL;
        }

        {
            Case_t *record = &db->cases[ db->caseCount ];

            validationCopyField(record->caseId, ID_LENGTH, fields[0]);
            validationCopyField(record->title, TITLE_LENGTH, fields[1]);
            record->crimeType = (CrimeType_t)parseIntOr(fields[2], (int)CRIME_TYPE_OTHER);
            record->priority = (CasePriority_t)parseIntOr(fields[3], (int)CASE_PRIORITY_MEDIUM);
            record->status = (CaseStatus_t)parseIntOr(fields[4], (int)CASE_STATUS_OPEN);
            validationCopyField(record->incidentDate, DATE_LENGTH, fields[5]);
            validationCopyField(record->location, LOCATION_LENGTH, fields[6]);
            validationCopyField(record->assignedOfficer, OFFICER_LENGTH, fields[7]);
            validationCopyField(record->summary, TEXT_LENGTH, fields[8]);

            if (!validationIsValidDate(record->incidentDate))
            {
                fclose(stream);
                return CF_ERR_CORRUPT_FILE;
            }
            if (record->crimeType < 0 || record->crimeType >= CRIME_TYPE_COUNT
                || record->priority < 0 || record->priority >= CASE_PRIORITY_COUNT
                || record->status < 0 || record->status >= CASE_STATUS_COUNT)
            {
                fclose(stream);
                return CF_ERR_CORRUPT_FILE;
            }

            db->caseCount++;
        }
    }

    fclose(stream);
    return CF_SUCCESS;
}

static CfStatus_t loadSuspects(CasefileDb_t *db)
{
    FILE *stream = NULL;
    int missing = 0;
    char line[FILE_LINE_LENGTH];

    if (openForRead(FILE_SUSPECTS, &stream, &missing) != CF_SUCCESS)
    {
        return (missing != 0) ? CF_SUCCESS : CF_ERR_CORRUPT_FILE;
    }

    while (fgets(line, (int)sizeof(line), stream) != NULL)
    {
        char *fields[ 16 ];
        int count;

        line[ strcspn(line, "\r\n") ] = '\0';
        if (line[0] == '\0')
        {
            continue;
        }

        count = splitFields(line, fields, 16);
        if (count < 9)
        {
            fclose(stream);
            return CF_ERR_CORRUPT_FILE;
        }

        if (db->suspectCount >= (size_t)MAX_SUSPECTS)
        {
            fclose(stream);
            return CF_ERR_CAPACITY_FULL;
        }

        {
            Suspect_t *record = &db->suspects[ db->suspectCount ];

            validationCopyField(record->suspectId, ID_LENGTH, fields[0]);
            validationCopyField(record->caseId, ID_LENGTH, fields[1]);
            validationCopyField(record->fullName, NAME_LENGTH, fields[2]);
            record->age = parseIntOr(fields[3], 0);
            validationCopyField(record->phone, PHONE_LENGTH, fields[4]);
            validationCopyField(record->address, ADDRESS_LENGTH, fields[5]);
            validationCopyField(record->alibi, ALIBI_LENGTH, fields[6]);
            validationCopyField(record->motive, MOTIVE_LENGTH, fields[7]);
            record->status = (SuspectStatus_t)parseIntOr(fields[8], (int)SUSPECT_STATUS_UNDER_REVIEW);

            if (count > 9)
            {
                validationCopyField(record->physical.gender, NAME_LENGTH, fields[9]);
            }
            if (count > 10)
            {
                record->physical.heightCm = parseIntOr(fields[10], -1);
            }
            if (count > 11)
            {
                validationCopyField(record->physical.hairColour, NAME_LENGTH, fields[11]);
            }
            if (count > 12)
            {
                record->physical.shoeSize = parseIntOr(fields[12], -1);
            }
            if (count > 13)
            {
                validationCopyField(record->physical.bloodGroup, NAME_LENGTH, fields[13]);
            }
            if (count > 14 && fields[14][0] != '\0')
            {
                char *token = fields[14];
                int vehicle = 0;

                while (token != NULL && vehicle < MAX_VEHICLES)
                {
                    char *comma = strchr(token, ',');

                    if (comma != NULL)
                    {
                        *comma = '\0';
                    }
                    if (token[0] != '\0')
                    {
                        validationCopyField(record->physical.vehicles[ vehicle ],
                                            VEHICLE_LENGTH, token);
                        vehicle++;
                    }
                    token = (comma != NULL) ? comma + 1 : NULL;
                }
                record->physical.vehicleCount = vehicle;
            }

            if (record->status < 0 || record->status >= SUSPECT_STATUS_COUNT)
            {
                fclose(stream);
                return CF_ERR_CORRUPT_FILE;
            }

            db->suspectCount++;
        }
    }

    fclose(stream);
    return CF_SUCCESS;
}

static CfStatus_t loadEvidence(CasefileDb_t *db)
{
    FILE *stream = NULL;
    int missing = 0;
    char line[FILE_LINE_LENGTH];

    if (openForRead(FILE_EVIDENCE, &stream, &missing) != CF_SUCCESS)
    {
        return (missing != 0) ? CF_SUCCESS : CF_ERR_CORRUPT_FILE;
    }

    while (fgets(line, (int)sizeof(line), stream) != NULL)
    {
        char *fields[ 11 ];
        int count;

        line[ strcspn(line, "\r\n") ] = '\0';
        if (line[0] == '\0')
        {
            continue;
        }

        count = splitFields(line, fields, 11);
        if (count < 9)
        {
            fclose(stream);
            return CF_ERR_CORRUPT_FILE;
        }

        if (db->evidenceCount >= (size_t)MAX_EVIDENCE)
        {
            fclose(stream);
            return CF_ERR_CAPACITY_FULL;
        }

        {
            Evidence_t *record = &db->evidence[ db->evidenceCount ];

            validationCopyField(record->evidenceId, ID_LENGTH, fields[0]);
            validationCopyField(record->caseId, ID_LENGTH, fields[1]);
            record->category = (EvidenceCategory_t)parseIntOr(fields[2], (int)EVIDENCE_CATEGORY_OTHER);
            validationCopyField(record->description, EVIDENCE_DESC_LENGTH, fields[3]);
            validationCopyField(record->collectionDate, DATE_LENGTH, fields[4]);
            validationCopyField(record->collectionLocation, LOCATION_LENGTH, fields[5]);
            validationCopyField(record->collectedBy, NAME_LENGTH, fields[6]);
            record->status = (EvidenceStatus_t)parseIntOr(fields[7], (int)EVIDENCE_STATUS_COLLECTED);

            if (!validationIsValidDate(record->collectionDate))
            {
                fclose(stream);
                return CF_ERR_CORRUPT_FILE;
            }
            if (record->category < 0 || record->category >= EVIDENCE_CATEGORY_COUNT
                || record->status < 0 || record->status >= EVIDENCE_STATUS_COUNT)
            {
                fclose(stream);
                return CF_ERR_CORRUPT_FILE;
            }

            if (count > 9 && fields[9][0] != '\0')
            {
                char *token = fields[9];

                while (token != NULL && record->attributeCount < MAX_EVIDENCE_ATTRIBUTES)
                {
                    char *separator = strchr(token, ';');
                    char *equals;

                    if (separator != NULL)
                    {
                        *separator = '\0';
                    }

                    equals = strchr(token, '=');
                    if (equals != NULL)
                    {
                        int keyValue = parseIntOr(token, -1);

                        *equals = '\0';
                        if (keyValue >= 0 && keyValue < (int)ATTRIBUTE_COUNT)
                        {
                            record->attributes[ record->attributeCount ].key = (AttributeKey_t)keyValue;
                            validationCopyField(record->attributes[ record->attributeCount ].value,
                                                ATTRIBUTE_VALUE_LENGTH, equals + 1);
                            record->attributeCount++;
                        }
                    }

                    token = (separator != NULL) ? separator + 1 : NULL;
                }
            }

            db->evidenceCount++;
        }
    }

    fclose(stream);
    return CF_SUCCESS;
}

static CfStatus_t loadCustody(CasefileDb_t *db)
{
    FILE *stream = NULL;
    int missing = 0;
    char line[FILE_LINE_LENGTH];

    if (openForRead(FILE_CUSTODY, &stream, &missing) != CF_SUCCESS)
    {
        return (missing != 0) ? CF_SUCCESS : CF_ERR_CORRUPT_FILE;
    }

    while (fgets(line, (int)sizeof(line), stream) != NULL)
    {
        char *fields[ 8 ];
        int count;
        int index;

        line[ strcspn(line, "\r\n") ] = '\0';
        if (line[0] == '\0')
        {
            continue;
        }

        count = splitFields(line, fields, 8);
        if (count < 7)
        {
            fclose(stream);
            return CF_ERR_CORRUPT_FILE;
        }

        for (index = 0; index < (int)db->evidenceCount; index++)
        {
            if (strcmp(db->evidence[ index ].evidenceId, fields[0]) != 0)
            {
                continue;
            }

            if (db->evidence[ index ].custodyCount >= MAX_CUSTODY_EVENTS)
            {
                break;
            }

            {
                CustodyEvent_t *entry = &db->evidence[ index ].custody[ db->evidence[ index ].custodyCount ];

                entry->action = (CustodyAction_t)parseIntOr(fields[1], (int)CUSTODY_ACTION_COLLECTED);
                validationCopyField(entry->timestamp, DATE_LENGTH, fields[2]);
                validationCopyField(entry->fromHandler, NAME_LENGTH, fields[3]);
                validationCopyField(entry->toHandler, NAME_LENGTH, fields[4]);
                validationCopyField(entry->location, LOCATION_LENGTH, fields[5]);
                validationCopyField(entry->remarks, CUSTODY_REMARK_LENGTH, fields[6]);

                if (!validationIsValidDate(entry->timestamp)
                    || entry->action < 0 || entry->action >= CUSTODY_ACTION_COUNT)
                {
                    fclose(stream);
                    return CF_ERR_CORRUPT_FILE;
                }

                db->evidence[ index ].custodyCount++;
            }
            break;
        }
    }

    fclose(stream);
    return CF_SUCCESS;
}

static CfStatus_t loadWitnesses(CasefileDb_t *db)
{
    FILE *stream = NULL;
    int missing = 0;
    char line[FILE_LINE_LENGTH];

    if (openForRead(FILE_WITNESSES, &stream, &missing) != CF_SUCCESS)
    {
        return (missing != 0) ? CF_SUCCESS : CF_ERR_CORRUPT_FILE;
    }

    while (fgets(line, (int)sizeof(line), stream) != NULL)
    {
        char *fields[ 9 ];
        int count;

        line[ strcspn(line, "\r\n") ] = '\0';
        if (line[0] == '\0')
        {
            continue;
        }

        count = splitFields(line, fields, 9);
        if (count < 8)
        {
            fclose(stream);
            return CF_ERR_CORRUPT_FILE;
        }

        if (db->witnessCount >= (size_t)MAX_WITNESSES)
        {
            fclose(stream);
            return CF_ERR_CAPACITY_FULL;
        }

        {
            Witness_t *record = &db->witnesses[ db->witnessCount ];

            validationCopyField(record->witnessId, ID_LENGTH, fields[0]);
            validationCopyField(record->caseId, ID_LENGTH, fields[1]);
            validationCopyField(record->statementRef, ID_LENGTH, fields[2]);
            validationCopyField(record->fullName, NAME_LENGTH, fields[3]);
            validationCopyField(record->phone, PHONE_LENGTH, fields[4]);
            validationCopyField(record->statement, STATEMENT_LENGTH, fields[5]);
            validationCopyField(record->recordedDate, DATE_LENGTH, fields[6]);
            record->verified = parseIntOr(fields[7], 0);

            if (!validationIsValidDate(record->recordedDate))
            {
                fclose(stream);
                return CF_ERR_CORRUPT_FILE;
            }

            db->witnessCount++;
        }
    }

    fclose(stream);
    return CF_SUCCESS;
}

CfStatus_t storage_loadAll(CasefileDb_t *db)
{
    CfStatus_t status;

    if (db == NULL)
    {
        return CF_ERR_NULL_POINTER;
    }

    db->caseCount = 0U;
    db->suspectCount = 0U;
    db->evidenceCount = 0U;
    db->witnessCount = 0U;

    status = loadCases(db);
    if (status != CF_SUCCESS)
    {
        return status;
    }
    status = loadSuspects(db);
    if (status != CF_SUCCESS)
    {
        return status;
    }
    status = loadEvidence(db);
    if (status != CF_SUCCESS)
    {
        return status;
    }
    status = loadCustody(db);
    if (status != CF_SUCCESS)
    {
        return status;
    }
    status = loadWitnesses(db);
    if (status != CF_SUCCESS)
    {
        return status;
    }
    return CF_SUCCESS;
}

CfStatus_t storage_validateFormat(const char *path)
{
    FILE *handle;
    char line[FILE_LINE_LENGTH];

    if (path == NULL)
    {
        return CF_ERR_NULL_POINTER;
    }

    handle = fopen(path, "r");
    if (handle == NULL)
    {
        return CF_ERR_FILE_OPEN;
    }

    if (fgets(line, (int)sizeof(line), handle) == NULL)
    {
        fclose(handle);
        return CF_ERR_CORRUPT_FILE;
    }

    line[ strcspn(line, "\r\n") ] = '\0';
    fclose(handle);

    if (strcmp(line, STORAGE_FILE_SIGNATURE) != 0)
    {
        return CF_ERR_CORRUPT_FILE;
    }
    return CF_SUCCESS;
}
