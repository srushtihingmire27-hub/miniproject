/*
 * report_generator.c - Composition of the investigation and evidence reports.
 *
 * The reports are built into a caller-supplied buffer so that no dynamic
 * allocation is needed. Every write is bounds checked, so a report that is too
 * long for the buffer is truncated rather than allowed to overflow it.
 */

#include "report_generator.h"
#include "case_manager.h"
#include "evidence_manager.h"
#include "investigation.h"
#include "suspect_manager.h"
#include "validation.h"
#include "witness_manager.h"

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

/*
 * Append text to a report buffer, never writing past the end.
 *
 * @param buffer     Destination buffer.
 * @param bufferSize Size of the destination buffer.
 * @param used       In/out count of characters already written.
 * @param format     printf-style format string.
 */
static void appendText(char *buffer, size_t bufferSize, size_t *used, const char *format, ...)
#if defined(__GNUC__)
    __attribute__((format(printf, 4, 5)))
#endif
;

#include <stdarg.h>

static void appendText(char *buffer, size_t bufferSize, size_t *used, const char *format, ...)
{
    va_list arguments;
    int written;

    if (buffer == NULL || used == NULL || bufferSize == 0U)
    {
        return;
    }
    if (*used >= (bufferSize - 1U))
    {
        return;
    }

    va_start(arguments, format);
    written = vsnprintf(buffer + *used, bufferSize - *used, format, arguments);
    va_end(arguments);

    if (written < 0)
    {
        return;
    }
    if ((size_t)written >= (bufferSize - *used))
    {
        *used = bufferSize - 1U;
        return;
    }
    *used += (size_t)written;
}

CfStatus_t report_generateCaseSummary(const CasefileDb_t *db, const char *caseId,
                                      char *buffer, size_t bufferSize)
{
    Case_t caseRecord;
    AnalysisResult_t analysis;
    CfStatus_t status;
    size_t used = 0U;
    int progress = 0;
    char progressDetail[TEXT_LENGTH];
    int index;

    if (db == NULL || caseId == NULL || buffer == NULL || bufferSize == 0U)
    {
        return CF_ERR_NULL_POINTER;
    }

    buffer[0] = '\0';

    status = caseManager_getCase(db, caseId, &caseRecord);
    if (status != CF_SUCCESS)
    {
        return status;
    }

    progressDetail[0] = '\0';
    (void)investigation_computeProgress(db, caseId, &progress, progressDetail, sizeof(progressDetail));

    appendText(buffer, bufferSize, &used, "=====================================================================\n");
    appendText(buffer, bufferSize, &used, " CASEFILE INVESTIGATION SUMMARY\n");
    appendText(buffer, bufferSize, &used, "=====================================================================\n");
    appendText(buffer, bufferSize, &used, "Case ID          : %s\n", caseRecord.caseId);
    appendText(buffer, bufferSize, &used, "Title            : %s\n", caseRecord.title);
    appendText(buffer, bufferSize, &used, "Crime type       : %s\n",
               validationCrimeTypeName(caseRecord.crimeType));
    appendText(buffer, bufferSize, &used, "Priority         : %s\n",
               validationCasePriorityName(caseRecord.priority));
    appendText(buffer, bufferSize, &used, "Status           : %s\n",
               validationCaseStatusName(caseRecord.status));
    appendText(buffer, bufferSize, &used, "Incident date    : %s\n", caseRecord.incidentDate);
    appendText(buffer, bufferSize, &used, "Location         : %s\n", caseRecord.location);
    appendText(buffer, bufferSize, &used, "Assigned officer : %s\n", caseRecord.assignedOfficer);
    appendText(buffer, bufferSize, &used, "Progress         : %d%% (%s)\n", progress, progressDetail);
    appendText(buffer, bufferSize, &used, "Summary          : %s\n", caseRecord.summary);

    appendText(buffer, bufferSize, &used, "\n-------------------------------------------------------------------\n");
    appendText(buffer, bufferSize, &used, " SUSPECTS AND EVIDENCE COMPARISON\n");
    appendText(buffer, bufferSize, &used, "-------------------------------------------------------------------\n");

    status = investigation_analyseCase(db, caseId, &analysis);

    if (status == CF_ERR_INSUFFICIENT_EVIDENCE)
    {
        appendText(buffer, bufferSize, &used,
                   "Insufficient evidence: the recorded evidence for this case carries no\n"
                   "comparable attribute, so no suspicion score can be produced.\n");
    }
    else if (status != CF_SUCCESS)
    {
        return status;
    }
    else
    {
        for (index = 0; index < analysis.suspectCount; index++)
        {
            const SuspectScore_t *score = &analysis.suspects[ index ];
            int factor;

            appendText(buffer, bufferSize, &used, "\nRank %d: %s (%s)\n", index + 1, score->suspectName,
                       score->suspectId);
            appendText(buffer, bufferSize, &used, "  Score: %d / %d  (%s)\n", score->score,
                       MAX_SUSPICION_SCORE, investigation_describeScore(score->score));
            appendText(buffer, bufferSize, &used, "  Matched %d of %d comparable attribute(s)\n",
                       score->matchedCount, score->checkedCount);

            if (score->factorCount == 0)
            {
                appendText(buffer, bufferSize, &used, "  No comparable attribute was recorded.\n");
                continue;
            }

            for (factor = 0; factor < score->factorCount; factor++)
            {
                const MatchFactor_t *item = &score->factors[ factor ];

                if (item->matched != 0)
                {
                    appendText(buffer, bufferSize, &used,
                               "  [MATCH]   %-11s evidence %s = %-12s vs suspect %-12s (+%d)\n",
                               validationAttributeKeyName(item->key), item->evidenceId,
                               item->evidenceValue, item->suspectValue, item->weight);
                }
                else
                {
                    appendText(buffer, bufferSize, &used,
                               "  [no match] %-11s evidence %s = %-12s vs suspect %-12s\n",
                               validationAttributeKeyName(item->key), item->evidenceId,
                               item->evidenceValue, item->suspectValue);
                }
            }
        }
    }

    appendText(buffer, bufferSize, &used, "\n-------------------------------------------------------------------\n");
    appendText(buffer, bufferSize, &used, " EVIDENCE REGISTER\n");
    appendText(buffer, bufferSize, &used, "-------------------------------------------------------------------\n");

    {
        size_t index;
        int found = 0;

        /*
         * The evidence register is streamed directly from the database rather
         * than copied into a local array. A local array of every evidence
         * item would be far too large for the stack.
         */
        for (index = 0U; index < db->evidenceCount; index++)
        {
            const Evidence_t *record = &db->evidence[ index ];

            if (strcmp(record->caseId, caseId) != 0)
            {
                continue;
            }

            found = 1;
            appendText(buffer, bufferSize, &used, "  %s  [%s]  %s\n", record->evidenceId,
                       validationEvidenceCategoryName(record->category), record->description);
            appendText(buffer, bufferSize, &used, "       Collected %s at %s by %s | Status: %s\n",
                       record->collectionDate, record->collectionLocation, record->collectedBy,
                       validationEvidenceStatusName(record->status));
            appendText(buffer, bufferSize, &used, "       Custody events: %d\n", record->custodyCount);
        }

        if (found == 0)
        {
            appendText(buffer, bufferSize, &used, "  No evidence has been registered for this case.\n");
        }
    }

    appendText(buffer, bufferSize, &used, "\n-------------------------------------------------------------------\n");
    appendText(buffer, bufferSize, &used, " WITNESS STATEMENTS\n");
    appendText(buffer, bufferSize, &used, "-------------------------------------------------------------------\n");

    {
        size_t index;
        int found = 0;

        for (index = 0U; index < db->witnessCount; index++)
        {
            const Witness_t *record = &db->witnesses[ index ];

            if (strcmp(record->caseId, caseId) != 0)
            {
                continue;
            }

            found = 1;
            appendText(buffer, bufferSize, &used, "  %s (Statement %s)  %s  [verified: %s]\n",
                       record->witnessId, record->statementRef, record->fullName,
                       (record->verified != 0) ? "yes" : "no");
            appendText(buffer, bufferSize, &used, "       %s\n", record->statement);
        }

        if (found == 0)
        {
            appendText(buffer, bufferSize, &used, "  No witness has been registered for this case.\n");
        }
    }

    appendText(buffer, bufferSize, &used, "\n=====================================================================\n");
    appendText(buffer, bufferSize, &used, " %s\n", investigation_getDisclaimer());
    appendText(buffer, bufferSize, &used, "=====================================================================\n");

    return CF_SUCCESS;
}

CfStatus_t report_generateEvidenceSummary(const CasefileDb_t *db, const char *caseId,
                                          char *buffer, size_t bufferSize)
{
    Case_t caseRecord;
    size_t index;
    size_t itemCount = 0U;
    CfStatus_t status;
    size_t used = 0U;

    if (db == NULL || caseId == NULL || buffer == NULL || bufferSize == 0U)
    {
        return CF_ERR_NULL_POINTER;
    }

    buffer[0] = '\0';

    status = caseManager_getCase(db, caseId, &caseRecord);
    if (status != CF_SUCCESS)
    {
        return status;
    }

    /* Count the evidence of this case without copying any of it. */
    for (index = 0U; index < db->evidenceCount; index++)
    {
        if (strcmp(db->evidence[ index ].caseId, caseId) == 0)
        {
            itemCount++;
        }
    }

    appendText(buffer, bufferSize, &used, "=====================================================================\n");
    appendText(buffer, bufferSize, &used, " CASEFILE EVIDENCE SUMMARY\n");
    appendText(buffer, bufferSize, &used, "=====================================================================\n");
    appendText(buffer, bufferSize, &used, "Case ID    : %s\n", caseRecord.caseId);
    appendText(buffer, bufferSize, &used, "Case title : %s\n", caseRecord.title);
    appendText(buffer, bufferSize, &used, "Status     : %s\n", validationCaseStatusName(caseRecord.status));
    appendText(buffer, bufferSize, &used, "Evidence items: %zu\n", itemCount);
    appendText(buffer, bufferSize, &used, "\n");

    if (itemCount == 0U)
    {
        appendText(buffer, bufferSize, &used, "  No evidence has been registered for this case.\n");
    }

    for (index = 0U; index < db->evidenceCount; index++)
    {
        const Evidence_t *record = &db->evidence[ index ];
        int attribute;
        int event;

        if (strcmp(record->caseId, caseId) != 0)
        {
            continue;
        }

        appendText(buffer, bufferSize, &used, "-------------------------------------------------------------------\n");
        appendText(buffer, bufferSize, &used, "Evidence ID : %s\n", record->evidenceId);
        appendText(buffer, bufferSize, &used, "Category    : %s\n",
                   validationEvidenceCategoryName(record->category));
        appendText(buffer, bufferSize, &used, "Description : %s\n", record->description);
        appendText(buffer, bufferSize, &used, "Collected   : %s at %s\n", record->collectionDate,
                   record->collectionLocation);
        appendText(buffer, bufferSize, &used, "Collector   : %s\n", record->collectedBy);
        appendText(buffer, bufferSize, &used, "Status      : %s\n",
                   validationEvidenceStatusName(record->status));

        appendText(buffer, bufferSize, &used, "Analysis attributes:\n");
        if (record->attributeCount == 0)
        {
            appendText(buffer, bufferSize, &used, "  (none recorded)\n");
        }
        for (attribute = 0; attribute < record->attributeCount; attribute++)
        {
            appendText(buffer, bufferSize, &used, "  %-12s = %s\n",
                       validationAttributeKeyName(record->attributes[ attribute ].key),
                       record->attributes[ attribute ].value);
        }

        appendText(buffer, bufferSize, &used, "Chain of custody:\n");
        if (record->custodyCount == 0)
        {
            appendText(buffer, bufferSize, &used, "  (no custody event recorded)\n");
        }
        for (event = 0; event < record->custodyCount; event++)
        {
            const CustodyEvent_t *entry = &record->custody[ event ];

            appendText(buffer, bufferSize, &used, "  %s  %-11s  %s -> %s  at %s\n", entry->timestamp,
                       validationCustodyActionName(entry->action), entry->fromHandler,
                       entry->toHandler, entry->location);
            if (entry->remarks[0] != '\0')
            {
                appendText(buffer, bufferSize, &used, "               %s\n", entry->remarks);
            }
        }
    }

    appendText(buffer, bufferSize, &used, "=====================================================================\n");
    appendText(buffer, bufferSize, &used, "All case data is fictional and is used for teaching purposes only.\n");
    appendText(buffer, bufferSize, &used, "=====================================================================\n");

    return CF_SUCCESS;
}

CfStatus_t report_saveToFile(const char *caseId, const char *reportKind,
                             const char *content, char *pathOut, size_t pathOutSize)
{
    char path[FILE_PATH_LENGTH];
    FILE *stream;

    if (caseId == NULL || reportKind == NULL || content == NULL)
    {
        return CF_ERR_NULL_POINTER;
    }
    if (pathOut != NULL && pathOutSize > 0U)
    {
        pathOut[0] = '\0';
    }

#if defined(_WIN32)
    if (mkdir(REPORT_OUTPUT_DIRECTORY) != 0)
#else
    if (mkdir(REPORT_OUTPUT_DIRECTORY, 0755) != 0)
#endif
    {
        struct stat info;

        /* The directory may already exist, which is not an error. */
        if (stat(REPORT_OUTPUT_DIRECTORY, &info) != 0 || (info.st_mode & S_IFDIR) == 0)
        {
            return CF_ERR_FILE_WRITE;
        }
    }

    snprintf(path, sizeof(path), "%s/%s_%s.txt", REPORT_OUTPUT_DIRECTORY, caseId, reportKind);

    stream = fopen(path, "w");
    if (stream == NULL)
    {
        return CF_ERR_FILE_WRITE;
    }

    if (fputs(content, stream) == EOF)
    {
        fclose(stream);
        return CF_ERR_FILE_WRITE;
    }

    if (fclose(stream) != 0)
    {
        return CF_ERR_FILE_WRITE;
    }

    if (pathOut != NULL && pathOutSize > 0U)
    {
        snprintf(pathOut, pathOutSize, "%s", path);
    }
    return CF_SUCCESS;
}
