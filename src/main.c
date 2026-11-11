/*
 * main.c - Application entry point for CASEFILE.
 *
 * This file contains only the high-level workflow: initialisation, menu
 * presentation, dispatching a menu choice to the appropriate module, printing
 * the result and saving the data on exit. Every business rule lives in one of
 * the manager, investigation, report or storage modules.
 */

#include "case_manager.h"
#include "casefile.h"
#include "evidence_manager.h"
#include "investigation.h"
#include "report_generator.h"
#include "storage.h"
#include "suspect_manager.h"
#include "validation.h"
#include "witness_manager.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define REPORT_BUFFER_SIZE 8192

/* ------------------------------------------------------------------ */
/* Shared menu helpers                                                 */
/* ------------------------------------------------------------------ */

static void printHeading(const char *text)
{
    printf("\n");
    printf("=====================================================================\n");
    printf(" %s\n", text);
    printf("=====================================================================\n");
}

static void printStatus(CfStatus_t status, const char *successMessage)
{
    if (status == CF_SUCCESS)
    {
        if (successMessage != NULL)
        {
            printf("  [OK] %s\n", successMessage);
        }
    }
    else
    {
        printf("  [!] %s\n", validationStatusMessage(status));
    }
}

/*
 * Copy a value into a fixed-width display field.
 *
 * A value that does not fit is shortened and marked with an ellipsis, so that
 * the columns of a table stay aligned whatever the user typed.
 *
 * @param destination Destination buffer.
 * @param destinationSize Size of the destination buffer.
 * @param source Value to display (may be NULL).
 * @param width Column width in characters.
 */
static void displayField(char *destination, size_t destinationSize, const char *source, size_t width)
{
    size_t length;

    if (destination == NULL || destinationSize == 0U)
    {
        return;
    }

    destination[0] = '\0';

    if (source == NULL)
    {
        return;
    }

    length = strlen(source);

    if (length <= width)
    {
        validationCopyField(destination, destinationSize, source);
        return;
    }

    if (width < 4U || destinationSize <= width)
    {
        validationCopyField(destination, destinationSize, source);
        return;
    }

    memcpy(destination, source, width - 3U);
    destination[ width - 3U ] = '.';
    destination[ width - 2U ] = '.';
    destination[ width - 1U ] = '.';
    destination[ width ] = '\0';
}

/*
 * Ask the user to confirm a destructive action.
 *
 * @param question Question to present.
 * @return 1 when the user confirmed, 0 otherwise.
 */
static int confirmAction(const char *question)
{
    char buffer[FILE_LINE_LENGTH];

    printf("  %s (y/n): ", question);
    fflush(stdout);

    if (validationReadLine(NULL, buffer, sizeof(buffer)) != CF_SUCCESS)
    {
        return 0;
    }
    validationTrim(buffer);

    if (validationCompareIgnoreCase(buffer, "y") == 0 || validationCompareIgnoreCase(buffer, "yes") == 0)
    {
        return 1;
    }
    return 0;
}

/*
 * Read a required case identifier from the user.
 *
 * @return 1 when a valid identifier was entered, 0 otherwise.
 */
static int readCaseId(char *buffer, size_t bufferSize)
{
    if (validationReadLine("Case ID: ", buffer, bufferSize) != CF_SUCCESS)
    {
        return 0;
    }
    validationTrim(buffer);

    if (!validationIsNotEmpty(buffer))
    {
        printf("  [!] %s\n", validationStatusMessage(CF_ERR_EMPTY_INPUT));
        return 0;
    }
    if (!validationIsValidIdentifier(buffer))
    {
        printf("  [!] '%s' is not a valid identifier. Use letters, digits and hyphens,\n"
               "      beginning with a letter.\n", buffer);
        return 0;
    }
    return 1;
}

/* ------------------------------------------------------------------ */
/* Case menu                                                           */
/* ------------------------------------------------------------------ */

static void listCases(const CasefileDb_t *db)
{
    size_t index;

    printHeading("CASE REGISTER");

    if (db->caseCount == 0U)
    {
        printf("  No cases have been registered yet.\n");
        return;
    }

    printf("%-16s %-36s %-18s %-20s %s\n", "CASE ID", "TITLE", "CRIME", "STATUS", "PRIORITY");
    printf("-----------------------------------------------------------------------------------------------\n");

    for (index = 0U; index < db->caseCount; index++)
    {
        const Case_t *record = &db->cases[ index ];
        char title[TITLE_LENGTH + 1U];
        char crime[NAME_LENGTH + 1U];
        char status[NAME_LENGTH + 1U];

        displayField(title, sizeof(title), record->title, 36U);
        displayField(crime, sizeof(crime), validationCrimeTypeName(record->crimeType), 18U);
        displayField(status, sizeof(status), validationCaseStatusName(record->status), 20U);

        printf("%-16s %-36s %-18s %-20s %s\n", record->caseId, title, crime, status,
               validationCasePriorityName(record->priority));
    }

    printf("\n  %zu case(s) on record.\n", db->caseCount);
}

static void showCaseDetail(const CasefileDb_t *db)
{
    char caseId[ID_LENGTH];
    Case_t record;
    size_t suspects;
    size_t evidence;
    size_t witnesses;
    int progress = 0;
    char detail[TEXT_LENGTH];

    printHeading("CASE DETAIL");

    if (!readCaseId(caseId, sizeof(caseId)))
    {
        return;
    }

    if (caseManager_getCase(db, caseId, &record) != CF_SUCCESS)
    {
        printf("  [!] %s\n", validationStatusMessage(CF_ERR_NOT_FOUND));
        return;
    }

    suspects = suspectManager_countByCase(db, caseId);
    evidence = evidenceManager_countByCase(db, caseId);
    witnesses = witnessManager_countByCase(db, caseId);

    detail[0] = '\0';
    (void)investigation_computeProgress(db, caseId, &progress, detail, sizeof(detail));

    printf("  Case ID          : %s\n", record.caseId);
    printf("  Title            : %s\n", record.title);
    printf("  Crime type       : %s\n", validationCrimeTypeName(record.crimeType));
    printf("  Priority         : %s\n", validationCasePriorityName(record.priority));
    printf("  Status           : %s\n", validationCaseStatusName(record.status));
    printf("  Incident date    : %s\n", record.incidentDate);
    printf("  Location         : %s\n", record.location);
    printf("  Assigned officer : %s\n", record.assignedOfficer);
    printf("  Summary          : %s\n", record.summary);
    printf("  Progress         : %d%% (%s)\n", progress, detail);
    printf("  Linked records   : %zu suspect(s), %zu evidence item(s), %zu witness(es)\n",
           suspects, evidence, witnesses);
}

static void addCase(CasefileDb_t *db)
{
    Case_t record;
    char crimeType[NAME_LENGTH];
    char priority[NAME_LENGTH];
    CfStatus_t status;

    printHeading("REGISTER NEW CASE");

    memset(&record, 0, sizeof(record));
    record.crimeType = CRIME_TYPE_OTHER;
    record.priority = CASE_PRIORITY_MEDIUM;
    record.status = CASE_STATUS_OPEN;

    printf("  Crime types      : homicide, theft, assault, fraud, vandalism,\n");
    printf("                     cybercrime, traffic_fatality, arson, other\n");
    printf("  Priorities       : low, medium, high, critical\n");

    if (validationReadLine("  Title: ", record.title, TITLE_LENGTH) != CF_SUCCESS
        || !validationIsNotEmpty(record.title))
    {
        printf("  [!] %s\n", validationStatusMessage(CF_ERR_EMPTY_INPUT));
        return;
    }

    if (validationReadLine("  Crime type: ", crimeType, sizeof(crimeType)) != CF_SUCCESS
        || validationParseCrimeType(crimeType, &record.crimeType) != CF_SUCCESS)
    {
        printf("  [!] %s\n", validationStatusMessage(CF_ERR_INVALID_ENUM));
        return;
    }

    if (validationReadLine("  Priority: ", priority, sizeof(priority)) != CF_SUCCESS
        || validationParseCasePriority(priority, &record.priority) != CF_SUCCESS)
    {
        printf("  [!] %s\n", validationStatusMessage(CF_ERR_INVALID_ENUM));
        return;
    }

    if (validationReadLine("  Incident date (YYYY-MM-DD): ", record.incidentDate, DATE_LENGTH) != CF_SUCCESS
        || !validationIsValidDate(record.incidentDate))
    {
        printf("  [!] %s\n", validationStatusMessage(CF_ERR_INVALID_DATE));
        return;
    }

    if (validationReadLine("  Location: ", record.location, LOCATION_LENGTH) != CF_SUCCESS)
    {
        return;
    }

    if (validationReadLine("  Assigned officer: ", record.assignedOfficer, OFFICER_LENGTH) != CF_SUCCESS)
    {
        return;
    }

    if (validationReadLine("  Summary: ", record.summary, TEXT_LENGTH) != CF_SUCCESS)
    {
        return;
    }

    status = caseManager_addCase(db, &record);
    printStatus(status, "Case registered.");
    if (status == CF_SUCCESS)
    {
        printf("  Assigned case ID : %s\n", record.caseId);
    }
}

static void updateCase(CasefileDb_t *db)
{
    char caseId[ID_LENGTH];
    Case_t current;
    Case_t updated;
    char buffer[FILE_LINE_LENGTH];
    char crimeType[NAME_LENGTH];
    char priority[NAME_LENGTH];
    CfStatus_t status;

    printHeading("UPDATE CASE");

    if (!readCaseId(caseId, sizeof(caseId)))
    {
        return;
    }

    if (caseManager_getCase(db, caseId, &current) != CF_SUCCESS)
    {
        printf("  [!] %s\n", validationStatusMessage(CF_ERR_NOT_FOUND));
        return;
    }

    updated = current;

    printf("  Leave a field blank to keep the current value.\n");

    if (validationReadLine("  Title: ", buffer, sizeof(buffer)) == CF_SUCCESS)
    {
        validationTrim(buffer);
        if (validationIsNotEmpty(buffer))
        {
            validationCopyField(updated.title, TITLE_LENGTH, buffer);
        }
    }

    if (validationReadLine("  Crime type: ", crimeType, sizeof(crimeType)) == CF_SUCCESS)
    {
        validationTrim(crimeType);
        if (validationIsNotEmpty(crimeType)
            && validationParseCrimeType(crimeType, &updated.crimeType) != CF_SUCCESS)
        {
            printf("  [!] %s\n", validationStatusMessage(CF_ERR_INVALID_ENUM));
            return;
        }
    }

    if (validationReadLine("  Priority: ", priority, sizeof(priority)) == CF_SUCCESS)
    {
        validationTrim(priority);
        if (validationIsNotEmpty(priority)
            && validationParseCasePriority(priority, &updated.priority) != CF_SUCCESS)
        {
            printf("  [!] %s\n", validationStatusMessage(CF_ERR_INVALID_ENUM));
            return;
        }
    }

    if (validationReadLine("  Incident date (YYYY-MM-DD): ", buffer, sizeof(buffer)) == CF_SUCCESS)
    {
        validationTrim(buffer);
        if (validationIsNotEmpty(buffer))
        {
            if (!validationIsValidDate(buffer))
            {
                printf("  [!] %s\n", validationStatusMessage(CF_ERR_INVALID_DATE));
                return;
            }
            validationCopyField(updated.incidentDate, DATE_LENGTH, buffer);
        }
    }

    if (validationReadLine("  Location: ", buffer, sizeof(buffer)) == CF_SUCCESS)
    {
        validationTrim(buffer);
        if (validationIsNotEmpty(buffer))
        {
            validationCopyField(updated.location, LOCATION_LENGTH, buffer);
        }
    }

    if (validationReadLine("  Assigned officer: ", buffer, sizeof(buffer)) == CF_SUCCESS)
    {
        validationTrim(buffer);
        if (validationIsNotEmpty(buffer))
        {
            validationCopyField(updated.assignedOfficer, OFFICER_LENGTH, buffer);
        }
    }

    if (validationReadLine("  Summary: ", buffer, sizeof(buffer)) == CF_SUCCESS)
    {
        validationTrim(buffer);
        if (validationIsNotEmpty(buffer))
        {
            validationCopyField(updated.summary, TEXT_LENGTH, buffer);
        }
    }

    status = caseManager_update(db, caseId, &updated);
    printStatus(status, "Case updated.");
}

static void setCaseStatus(CasefileDb_t *db)
{
    char caseId[ID_LENGTH];
    char statusName[NAME_LENGTH];
    CaseStatus_t status;
    CfStatus_t result;

    printHeading("UPDATE INVESTIGATION STATUS");

    if (!readCaseId(caseId, sizeof(caseId)))
    {
        return;
    }

    printf("  Valid statuses   : open, under_investigation, evidence_analysis, closed, archived\n");

    if (validationReadLine("  New status: ", statusName, sizeof(statusName)) != CF_SUCCESS
        || validationParseCaseStatus(statusName, &status) != CF_SUCCESS)
    {
        printf("  [!] %s\n", validationStatusMessage(CF_ERR_INVALID_ENUM));
        return;
    }

    result = caseManager_setStatus(db, caseId, status);
    printStatus(result, "Investigation status updated.");
}

static void deleteCase(CasefileDb_t *db)
{
    char caseId[ID_LENGTH];
    size_t suspects;
    size_t evidence;
    size_t witnesses;

    printHeading("DELETE CASE");

    if (!readCaseId(caseId, sizeof(caseId)))
    {
        return;
    }

    if (caseManager_findById(db, caseId) < 0)
    {
        printf("  [!] %s\n", validationStatusMessage(CF_ERR_NOT_FOUND));
        return;
    }

    suspects = suspectManager_countByCase(db, caseId);
    evidence = evidenceManager_countByCase(db, caseId);
    witnesses = witnessManager_countByCase(db, caseId);

    printf("  This will also remove %zu suspect(s), %zu evidence item(s) and %zu witness(es).\n",
           suspects, evidence, witnesses);

    if (!confirmAction("Delete this case and all of its records?"))
    {
        printf("  Deletion cancelled.\n");
        return;
    }

    printStatus(caseManager_delete(db, caseId), "Case and its linked records deleted.");
}

static void caseMenu(CasefileDb_t *db)
{
    int choice = 0;

    for (;;)
    {
        printf("\n--- CASE MANAGEMENT ---\n");
        printf("  1. Register new case\n");
        printf("  2. List all cases\n");
        printf("  3. Show case detail\n");
        printf("  4. Update case details\n");
        printf("  5. Update investigation status\n");
        printf("  6. Delete case\n");
        printf("  0. Back to main menu\n");

        if (validationReadInt("  Select: ", 0, 6, &choice) != CF_SUCCESS)
        {
            choice = 0;
        }

        switch (choice)
        {
            case 1:
                addCase(db);
                break;
            case 2:
                listCases(db);
                break;
            case 3:
                showCaseDetail(db);
                break;
            case 4:
                updateCase(db);
                break;
            case 5:
                setCaseStatus(db);
                break;
            case 6:
                deleteCase(db);
                break;
            default:
                break;
        }

        /* Choice 0 means "back to the previous menu". */
        if (choice == 0)
        {
            break;
        }
    }
}

/* ------------------------------------------------------------------ */
/* Suspect menu                                                        */
/* ------------------------------------------------------------------ */

static void addSuspect(CasefileDb_t *db)
{
    Suspect_t record;
    char buffer[FILE_LINE_LENGTH];
    char statusName[NAME_LENGTH];
    PhysicalProfile_t profile;
    CfStatus_t status;

    printHeading("ADD SUSPECT");

    memset(&record, 0, sizeof(record));
    memset(&profile, 0, sizeof(profile));
    profile.heightCm = -1;
    profile.shoeSize = -1;
    record.status = SUSPECT_STATUS_UNDER_REVIEW;

    if (!readCaseId(record.caseId, ID_LENGTH))
    {
        return;
    }

    if (!casefileCaseIdExists(db, record.caseId))
    {
        printf("  [!] %s\n", validationStatusMessage(CF_ERR_NOT_FOUND));
        return;
    }

    if (validationReadLine("  Full name: ", record.fullName, NAME_LENGTH) != CF_SUCCESS
        || !validationIsNotEmpty(record.fullName))
    {
        printf("  [!] %s\n", validationStatusMessage(CF_ERR_EMPTY_INPUT));
        return;
    }

    if (validationReadInt("  Age: ", 1, 120, &record.age) != CF_SUCCESS)
    {
        return;
    }

    if (validationReadLine("  Phone: ", record.phone, PHONE_LENGTH) != CF_SUCCESS)
    {
        return;
    }

    if (validationReadLine("  Address: ", record.address, ADDRESS_LENGTH) != CF_SUCCESS)
    {
        return;
    }

    if (validationReadLine("  Alibi: ", record.alibi, ALIBI_LENGTH) != CF_SUCCESS)
    {
        return;
    }

    if (validationReadLine("  Possible motive: ", record.motive, MOTIVE_LENGTH) != CF_SUCCESS)
    {
        return;
    }

    printf("  Valid statuses   : under_review, person_of_interest, primary, ruled_out, confirmed\n");
    if (validationReadLine("  Status: ", statusName, sizeof(statusName)) == CF_SUCCESS)
    {
        validationTrim(statusName);
        if (validationIsNotEmpty(statusName)
            && validationParseSuspectStatus(statusName, &record.status) != CF_SUCCESS)
        {
            printf("  [!] %s\n", validationStatusMessage(CF_ERR_INVALID_ENUM));
            return;
        }
    }

    printf("\n  Physical descriptors used for evidence comparison.\n");
    printf("  Enter a value, or press Enter to leave it unknown.\n");

    if (validationReadLine("  Gender: ", profile.gender, NAME_LENGTH) == CF_SUCCESS)
    {
        validationTrim(profile.gender);
    }
    if (validationReadLine("  Height in cm: ", buffer, sizeof(buffer)) == CF_SUCCESS)
    {
        validationTrim(buffer);
        if (validationIsNotEmpty(buffer) && validationParseInt(buffer, &profile.heightCm) != CF_SUCCESS)
        {
            printf("  [!] %s\n", validationStatusMessage(CF_ERR_INVALID_FORMAT));
            return;
        }
    }
    if (validationReadLine("  Hair colour: ", profile.hairColour, NAME_LENGTH) == CF_SUCCESS)
    {
        validationTrim(profile.hairColour);
    }
    if (validationReadLine("  Shoe size: ", buffer, sizeof(buffer)) == CF_SUCCESS)
    {
        validationTrim(buffer);
        if (validationIsNotEmpty(buffer) && validationParseInt(buffer, &profile.shoeSize) != CF_SUCCESS)
        {
            printf("  [!] %s\n", validationStatusMessage(CF_ERR_INVALID_FORMAT));
            return;
        }
    }
    if (validationReadLine("  Blood group: ", profile.bloodGroup, NAME_LENGTH) == CF_SUCCESS)
    {
        validationTrim(profile.bloodGroup);
    }

    status = suspectManager_add(db, &record);
    printStatus(status, "Suspect added to the case.");

    if (status == CF_SUCCESS)
    {
        printf("  Assigned suspect ID: %s\n", record.suspectId);

        if (suspectManager_setPhysicalProfile(db, record.suspectId, &profile) != CF_SUCCESS)
        {
            printf("  [!] The physical profile could not be stored.\n");
        }
        else
        {
            printf("  Physical profile stored.\n");
        }
    }
}

static void listSuspects(const CasefileDb_t *db)
{
    char caseId[ID_LENGTH];
    size_t count = 0U;
    size_t index;

    printHeading("SUSPECTS OF A CASE");

    if (!readCaseId(caseId, sizeof(caseId)))
    {
        return;
    }

    count = suspectManager_countByCase(db, caseId);

    if (count == 0U)
    {
        printf("  No suspects are recorded for this case.\n");
        return;
    }

    printf("%-12s %-28s %-6s %-18s %s\n", "ID", "NAME", "AGE", "STATUS", "ALIBI");
    printf("---------------------------------------------------------------------------------------------------\n");

    /* Streamed from the database so that no large copy is placed on the stack. */
    for (index = 0U; index < db->suspectCount; index++)
    {
        char name[NAME_LENGTH + 1U];
        char status[NAME_LENGTH + 1U];
        char alibi[ALIBI_LENGTH + 1U];

        if (strcmp(db->suspects[index].caseId, caseId) != 0)
        {
            continue;
        }

        displayField(name, sizeof(name), db->suspects[index].fullName, 28U);
        displayField(status, sizeof(status), validationSuspectStatusName(db->suspects[index].status), 18U);
        displayField(alibi, sizeof(alibi), db->suspects[index].alibi, 40U);

        printf("%-12s %-28s %-6d %-18s %s\n", db->suspects[index].suspectId, name,
               db->suspects[index].age, status, alibi);
    }

    printf("\n  %zu suspect(s).\n", count);
}

static void searchSuspects(const CasefileDb_t *db)
{
    char searchText[NAME_LENGTH];
    size_t count = 0U;
    size_t index;

    printHeading("SEARCH SUSPECTS");

    if (validationReadLine("  Name contains: ", searchText, sizeof(searchText)) != CF_SUCCESS)
    {
        return;
    }
    validationTrim(searchText);

    printf("%-12s %-28s %-16s %-6s %s\n", "ID", "NAME", "CASE", "AGE", "STATUS");
    printf("-----------------------------------------------------------------------------------\n");

    count = 0U;
    for (index = 0U; index < db->suspectCount; index++)
    {
        char name[NAME_LENGTH + 1U];

        if (validationContainsIgnoreCase(db->suspects[index].fullName, searchText) == 0)
        {
            continue;
        }
        count++;

        displayField(name, sizeof(name), db->suspects[index].fullName, 28U);

        printf("  %-12s %-28s %-16s %-6d %s\n", db->suspects[index].suspectId, name,
               db->suspects[index].caseId, db->suspects[index].age,
               validationSuspectStatusName(db->suspects[index].status));
    }

    if (count == 0U)
    {
        printf("  No suspect matched that text.\n");
        return;
    }

    printf("\n  %zu match(es).\n", count);
}

static void updateSuspect(CasefileDb_t *db)
{
    char suspectId[ID_LENGTH];
    Suspect_t current;
    Suspect_t updated;
    char buffer[FILE_LINE_LENGTH];
    char statusName[NAME_LENGTH];
    CfStatus_t status;

    printHeading("UPDATE SUSPECT");

    if (validationReadLine("  Suspect ID: ", suspectId, sizeof(suspectId)) != CF_SUCCESS)
    {
        return;
    }
    validationTrim(suspectId);

    if (suspectManager_get(db, suspectId, &current) != CF_SUCCESS)
    {
        printf("  [!] %s\n", validationStatusMessage(CF_ERR_NOT_FOUND));
        return;
    }

    updated = current;
    printf("  Leave a field blank to keep the current value.\n");

    if (validationReadLine("  Full name: ", buffer, sizeof(buffer)) == CF_SUCCESS)
    {
        validationTrim(buffer);
        if (validationIsNotEmpty(buffer))
        {
            validationCopyField(updated.fullName, NAME_LENGTH, buffer);
        }
    }

    if (validationReadLine("  Age: ", buffer, sizeof(buffer)) == CF_SUCCESS)
    {
        validationTrim(buffer);
        if (validationIsNotEmpty(buffer))
        {
            if (validationParseInt(buffer, &updated.age) != CF_SUCCESS
                || !validationIsValidAge(updated.age))
            {
                printf("  [!] %s\n", validationStatusMessage(CF_ERR_INVALID_ARGUMENT));
                return;
            }
        }
    }

    if (validationReadLine("  Phone: ", buffer, sizeof(buffer)) == CF_SUCCESS)
    {
        validationTrim(buffer);
        if (validationIsNotEmpty(buffer))
        {
            validationCopyField(updated.phone, PHONE_LENGTH, buffer);
        }
    }

    if (validationReadLine("  Alibi: ", buffer, sizeof(buffer)) == CF_SUCCESS)
    {
        validationTrim(buffer);
        if (validationIsNotEmpty(buffer))
        {
            validationCopyField(updated.alibi, ALIBI_LENGTH, buffer);
        }
    }

    if (validationReadLine("  Possible motive: ", buffer, sizeof(buffer)) == CF_SUCCESS)
    {
        validationTrim(buffer);
        if (validationIsNotEmpty(buffer))
        {
            validationCopyField(updated.motive, MOTIVE_LENGTH, buffer);
        }
    }

    if (validationReadLine("  Status: ", statusName, sizeof(statusName)) == CF_SUCCESS)
    {
        validationTrim(statusName);
        if (validationIsNotEmpty(statusName)
            && validationParseSuspectStatus(statusName, &updated.status) != CF_SUCCESS)
        {
            printf("  [!] %s\n", validationStatusMessage(CF_ERR_INVALID_ENUM));
            return;
        }
    }

    status = suspectManager_update(db, suspectId, &updated);
    printStatus(status, "Suspect updated.");
}

static void deleteSuspect(CasefileDb_t *db)
{
    char suspectId[ID_LENGTH];

    printHeading("DELETE SUSPECT");

    if (validationReadLine("  Suspect ID: ", suspectId, sizeof(suspectId)) != CF_SUCCESS)
    {
        return;
    }
    validationTrim(suspectId);

    if (suspectManager_findById(db, suspectId) < 0)
    {
        printf("  [!] %s\n", validationStatusMessage(CF_ERR_NOT_FOUND));
        return;
    }

    if (!confirmAction("Delete this suspect record?"))
    {
        printf("  Deletion cancelled.\n");
        return;
    }

    printStatus(suspectManager_delete(db, suspectId), "Suspect deleted.");
}

static void suspectMenu(CasefileDb_t *db)
{
    int choice = 0;

    for (;;)
    {
        printf("\n--- SUSPECT MANAGEMENT ---\n");
        printf("  1. Add suspect to a case\n");
        printf("  2. List suspects of a case\n");
        printf("  3. Search suspects by name\n");
        printf("  4. Update suspect\n");
        printf("  5. Delete suspect\n");
        printf("  0. Back to main menu\n");

        if (validationReadInt("  Select: ", 0, 5, &choice) != CF_SUCCESS)
        {
            choice = 0;
        }

        switch (choice)
        {
            case 1:
                addSuspect(db);
                break;
            case 2:
                listSuspects(db);
                break;
            case 3:
                searchSuspects(db);
                break;
            case 4:
                updateSuspect(db);
                break;
            case 5:
                deleteSuspect(db);
                break;
            default:
                break;
        }

        /* Choice 0 means "back to the previous menu". */
        if (choice == 0)
        {
            break;
        }
    }
}

/* ------------------------------------------------------------------ */
/* Evidence menu                                                       */
/* ------------------------------------------------------------------ */

static void addEvidence(CasefileDb_t *db)
{
    Evidence_t record;
    char categoryName[NAME_LENGTH];
    char statusName[NAME_LENGTH];
    CfStatus_t status;

    printHeading("REGISTER EVIDENCE");

    memset(&record, 0, sizeof(record));
    record.category = EVIDENCE_CATEGORY_OTHER;
    record.status = EVIDENCE_STATUS_COLLECTED;

    printf("  Categories        : biological, chemical, digital, ballistic,\n");
    printf("                      physical_trace, documentary, vehicle, other\n");

    if (!readCaseId(record.caseId, ID_LENGTH))
    {
        return;
    }

    if (!casefileCaseIdExists(db, record.caseId))
    {
        printf("  [!] %s\n", validationStatusMessage(CF_ERR_NOT_FOUND));
        return;
    }

    if (validationReadLine("  Category: ", categoryName, sizeof(categoryName)) != CF_SUCCESS
        || validationParseEvidenceCategory(categoryName, &record.category) != CF_SUCCESS)
    {
        printf("  [!] %s\n", validationStatusMessage(CF_ERR_INVALID_ENUM));
        return;
    }

    if (validationReadLine("  Description: ", record.description, EVIDENCE_DESC_LENGTH) != CF_SUCCESS
        || !validationIsNotEmpty(record.description))
    {
        printf("  [!] %s\n", validationStatusMessage(CF_ERR_EMPTY_INPUT));
        return;
    }

    if (validationReadLine("  Collection date (YYYY-MM-DD): ", record.collectionDate, DATE_LENGTH) != CF_SUCCESS
        || !validationIsValidDate(record.collectionDate))
    {
        printf("  [!] %s\n", validationStatusMessage(CF_ERR_INVALID_DATE));
        return;
    }

    if (validationReadLine("  Collection location: ", record.collectionLocation, LOCATION_LENGTH) != CF_SUCCESS)
    {
        return;
    }

    if (validationReadLine("  Collected by: ", record.collectedBy, NAME_LENGTH) != CF_SUCCESS)
    {
        return;
    }

    printf("  Valid statuses    : collected, in_transit, under_analysis, analysed, reported, released\n");
    if (validationReadLine("  Status: ", statusName, sizeof(statusName)) == CF_SUCCESS)
    {
        validationTrim(statusName);
        if (validationIsNotEmpty(statusName)
            && validationParseEvidenceStatus(statusName, &record.status) != CF_SUCCESS)
        {
            printf("  [!] %s\n", validationStatusMessage(CF_ERR_INVALID_ENUM));
            return;
        }
    }

    status = evidenceManager_add(db, &record);
    printStatus(status, "Evidence registered.");
    if (status == CF_SUCCESS)
    {
        printf("  Assigned evidence ID: %s\n", record.evidenceId);
    }
}

static void listEvidence(const CasefileDb_t *db)
{
    char caseId[ID_LENGTH];
    size_t count = 0U;
    size_t index;

    printHeading("EVIDENCE OF A CASE");

    if (!readCaseId(caseId, sizeof(caseId)))
    {
        return;
    }

    count = evidenceManager_countByCase(db, caseId);

    if (count == 0U)
    {
        printf("  No evidence is registered for this case.\n");
        return;
    }

    printf("%-12s %-18s %-44s %-18s %s\n", "ID", "CATEGORY", "DESCRIPTION", "STATUS", "COLLECTED");
    printf("------------------------------------------------------------------------------------------------------------------\n");

    /* Streamed from the database so that no large copy is placed on the stack. */
    for (index = 0U; index < db->evidenceCount; index++)
    {
        char description[EVIDENCE_DESC_LENGTH + 1U];
        char category[NAME_LENGTH + 1U];
        char status[NAME_LENGTH + 1U];

        if (strcmp(db->evidence[index].caseId, caseId) != 0)
        {
            continue;
        }

        displayField(description, sizeof(description), db->evidence[index].description, 44U);
        displayField(category, sizeof(category),
                    validationEvidenceCategoryName(db->evidence[index].category), 18U);
        displayField(status, sizeof(status),
                    validationEvidenceStatusName(db->evidence[index].status), 18U);

        printf("%-12s %-18s %-44s %-18s %s\n", db->evidence[index].evidenceId, category,
               description, status, db->evidence[index].collectionDate);
    }

    printf("\n  %zu evidence item(s).\n", count);
}

static void addEvidenceAttribute(CasefileDb_t *db)
{
    char evidenceId[ID_LENGTH];
    char keyName[NAME_LENGTH];
    char value[ATTRIBUTE_VALUE_LENGTH];
    AttributeKey_t key;
    CfStatus_t status;

    printHeading("ATTACH ANALYSIS ATTRIBUTE TO EVIDENCE");

    printf("  Attribute keys   : gender, height, hair, shoe_size, blood_group, vehicle\n");
    printf("  Height and shoe size use a band, for example 175-185.\n");

    if (validationReadLine("  Evidence ID: ", evidenceId, sizeof(evidenceId)) != CF_SUCCESS)
    {
        return;
    }
    validationTrim(evidenceId);

    if (validationReadLine("  Attribute key: ", keyName, sizeof(keyName)) != CF_SUCCESS
        || validationParseAttributeKey(keyName, &key) != CF_SUCCESS)
    {
        printf("  [!] %s\n", validationStatusMessage(CF_ERR_INVALID_ENUM));
        return;
    }

    if (validationReadLine("  Attribute value: ", value, sizeof(value)) != CF_SUCCESS
        || !validationIsNotEmpty(value))
    {
        printf("  [!] %s\n", validationStatusMessage(CF_ERR_EMPTY_INPUT));
        return;
    }

    status = evidenceManager_addAttribute(db, evidenceId, key, value);
    printStatus(status, "Analysis attribute attached to the evidence.");
}

static void addCustodyEvent(CasefileDb_t *db)
{
    char evidenceId[ID_LENGTH];
    char actionName[NAME_LENGTH];
    CustodyEvent_t event;
    CfStatus_t status;

    printHeading("RECORD CHAIN-OF-CUSTODY EVENT");

    memset(&event, 0, sizeof(event));
    event.action = CUSTODY_ACTION_TRANSFERRED;

    printf("  Actions          : collected, transferred, sealed, analysed, released\n");

    if (validationReadLine("  Evidence ID: ", evidenceId, sizeof(evidenceId)) != CF_SUCCESS)
    {
        return;
    }
    validationTrim(evidenceId);

    if (validationReadLine("  Action: ", actionName, sizeof(actionName)) != CF_SUCCESS
        || validationParseCustodyAction(actionName, &event.action) != CF_SUCCESS)
    {
        printf("  [!] %s\n", validationStatusMessage(CF_ERR_INVALID_ENUM));
        return;
    }

    if (validationReadLine("  Date (YYYY-MM-DD): ", event.timestamp, DATE_LENGTH) != CF_SUCCESS
        || !validationIsValidDate(event.timestamp))
    {
        printf("  [!] %s\n", validationStatusMessage(CF_ERR_INVALID_DATE));
        return;
    }

    if (validationReadLine("  From handler: ", event.fromHandler, NAME_LENGTH) != CF_SUCCESS
        || !validationIsNotEmpty(event.fromHandler))
    {
        printf("  [!] %s\n", validationStatusMessage(CF_ERR_EMPTY_INPUT));
        return;
    }

    if (validationReadLine("  To handler: ", event.toHandler, NAME_LENGTH) != CF_SUCCESS
        || !validationIsNotEmpty(event.toHandler))
    {
        printf("  [!] %s\n", validationStatusMessage(CF_ERR_EMPTY_INPUT));
        return;
    }

    if (validationReadLine("  Location: ", event.location, LOCATION_LENGTH) != CF_SUCCESS)
    {
        return;
    }

    if (validationReadLine("  Remarks: ", event.remarks, CUSTODY_REMARK_LENGTH) != CF_SUCCESS)
    {
        return;
    }

    status = evidenceManager_addCustodyEvent(db, evidenceId, &event);
    printStatus(status, "Chain-of-custody event recorded.");
}

static void showCustodyHistory(const CasefileDb_t *db)
{
    char evidenceId[ID_LENGTH];
    CustodyEvent_t events[ MAX_CUSTODY_EVENTS ];
    size_t count = 0U;
    size_t index;

    printHeading("CHAIN-OF-CUSTODY HISTORY");

    if (validationReadLine("  Evidence ID: ", evidenceId, sizeof(evidenceId)) != CF_SUCCESS)
    {
        return;
    }
    validationTrim(evidenceId);

    if (evidenceManager_getCustodyHistory(db, evidenceId, events,
                                          sizeof(events) / sizeof(events[0]), &count) != CF_SUCCESS)
    {
        printf("  [!] %s\n", validationStatusMessage(CF_ERR_NOT_FOUND));
        return;
    }

    if (count == 0U)
    {
        printf("  No custody event has been recorded for this item.\n");
        return;
    }

    for (index = 0U; index < count; index++)
    {
        printf("  %d. %s  %-11s  %s -> %s  at %s\n", (int)index + 1, events[ index ].timestamp,
               validationCustodyActionName(events[ index ].action), events[ index ].fromHandler,
               events[ index ].toHandler, events[ index ].location);
        if (events[ index ].remarks[0] != '\0')
        {
            printf("      %s\n", events[ index ].remarks);
        }
    }
}

static void updateEvidenceStatus(CasefileDb_t *db)
{
    char evidenceId[ID_LENGTH];
    char statusName[NAME_LENGTH];
    EvidenceStatus_t status;
    CfStatus_t result;

    printHeading("UPDATE EVIDENCE EXAMINATION STATUS");

    printf("  Valid statuses   : collected, in_transit, under_analysis, analysed, reported, released\n");

    if (validationReadLine("  Evidence ID: ", evidenceId, sizeof(evidenceId)) != CF_SUCCESS)
    {
        return;
    }
    validationTrim(evidenceId);

    if (validationReadLine("  New status: ", statusName, sizeof(statusName)) != CF_SUCCESS
        || validationParseEvidenceStatus(statusName, &status) != CF_SUCCESS)
    {
        printf("  [!] %s\n", validationStatusMessage(CF_ERR_INVALID_ENUM));
        return;
    }

    result = evidenceManager_setStatus(db, evidenceId, status);
    printStatus(result, "Evidence status updated.");
}

static void evidenceMenu(CasefileDb_t *db)
{
    int choice = 0;

    for (;;)
    {
        printf("\n--- FORENSIC EVIDENCE MANAGEMENT ---\n");
        printf("  1. Register evidence\n");
        printf("  2. List evidence of a case\n");
        printf("  3. Attach analysis attribute\n");
        printf("  4. Record chain-of-custody event\n");
        printf("  5. Show chain-of-custody history\n");
        printf("  6. Update examination status\n");
        printf("  0. Back to main menu\n");

        if (validationReadInt("  Select: ", 0, 6, &choice) != CF_SUCCESS)
        {
            choice = 0;
        }

        switch (choice)
        {
            case 1:
                addEvidence(db);
                break;
            case 2:
                listEvidence(db);
                break;
            case 3:
                addEvidenceAttribute(db);
                break;
            case 4:
                addCustodyEvent(db);
                break;
            case 5:
                showCustodyHistory(db);
                break;
            case 6:
                updateEvidenceStatus(db);
                break;
            default:
                break;
        }

        /* Choice 0 means "back to the previous menu". */
        if (choice == 0)
        {
            break;
        }
    }
}

/* ------------------------------------------------------------------ */
/* Witness menu                                                        */
/* ------------------------------------------------------------------ */

static void addWitness(CasefileDb_t *db)
{
    Witness_t record;
    CfStatus_t status;

    printHeading("REGISTER WITNESS");

    memset(&record, 0, sizeof(record));

    if (!readCaseId(record.caseId, ID_LENGTH))
    {
        return;
    }

    if (!casefileCaseIdExists(db, record.caseId))
    {
        printf("  [!] %s\n", validationStatusMessage(CF_ERR_NOT_FOUND));
        return;
    }

    if (validationReadLine("  Full name: ", record.fullName, NAME_LENGTH) != CF_SUCCESS
        || !validationIsNotEmpty(record.fullName))
    {
        printf("  [!] %s\n", validationStatusMessage(CF_ERR_EMPTY_INPUT));
        return;
    }

    if (validationReadLine("  Phone: ", record.phone, PHONE_LENGTH) != CF_SUCCESS)
    {
        return;
    }

    if (validationReadLine("  Statement: ", record.statement, STATEMENT_LENGTH) != CF_SUCCESS
        || !validationIsNotEmpty(record.statement))
    {
        printf("  [!] %s\n", validationStatusMessage(CF_ERR_EMPTY_INPUT));
        return;
    }

    if (validationReadLine("  Recorded date (YYYY-MM-DD): ", record.recordedDate, DATE_LENGTH) != CF_SUCCESS
        || !validationIsValidDate(record.recordedDate))
    {
        printf("  [!] %s\n", validationStatusMessage(CF_ERR_INVALID_DATE));
        return;
    }

    status = witnessManager_add(db, &record);
    printStatus(status, "Witness registered.");
    if (status == CF_SUCCESS)
    {
        printf("  Assigned witness ID    : %s\n", record.witnessId);
        printf("  Assigned statement ref : %s\n", record.statementRef);
    }
}

static void listWitnesses(const CasefileDb_t *db)
{
    char caseId[ID_LENGTH];
    size_t count = 0U;
    size_t index;

    printHeading("WITNESSES OF A CASE");

    if (!readCaseId(caseId, sizeof(caseId)))
    {
        return;
    }

    count = witnessManager_countByCase(db, caseId);

    if (count == 0U)
    {
        printf("  No witness is registered for this case.\n");
        return;
    }

    for (index = 0U; index < db->witnessCount; index++)
    {
        if (strcmp(db->witnesses[index].caseId, caseId) != 0)
        {
            continue;
        }
        printf("  %-12s (Statement %s)  %-28s verified: %s\n", db->witnesses[index].witnessId,
               db->witnesses[index].statementRef, db->witnesses[index].fullName,
               (db->witnesses[index].verified != 0) ? "yes" : "no");
        printf("      %s\n", db->witnesses[index].statement);
    }

    printf("\n  %zu witness(es).\n", count);
}

static void updateWitness(CasefileDb_t *db)
{
    char witnessId[ID_LENGTH];
    char buffer[FILE_LINE_LENGTH];
    char answer[8];
    Witness_t current;
    Witness_t updated;
    CfStatus_t status;

    printHeading("UPDATE WITNESS");

    if (validationReadLine("  Witness ID: ", witnessId, sizeof(witnessId)) != CF_SUCCESS)
    {
        return;
    }
    validationTrim(witnessId);

    if (witnessManager_get(db, witnessId, &current) != CF_SUCCESS)
    {
        printf("  [!] %s\n", validationStatusMessage(CF_ERR_NOT_FOUND));
        return;
    }

    updated = current;

    if (validationReadLine("  Full name: ", buffer, sizeof(buffer)) == CF_SUCCESS)
    {
        validationTrim(buffer);
        if (validationIsNotEmpty(buffer))
        {
            validationCopyField(updated.fullName, NAME_LENGTH, buffer);
        }
    }

    if (validationReadLine("  Phone: ", buffer, sizeof(buffer)) == CF_SUCCESS)
    {
        validationTrim(buffer);
        if (validationIsNotEmpty(buffer))
        {
            validationCopyField(updated.phone, PHONE_LENGTH, buffer);
        }
    }

    if (validationReadLine("  Statement: ", buffer, sizeof(buffer)) == CF_SUCCESS)
    {
        validationTrim(buffer);
        if (validationIsNotEmpty(buffer))
        {
            validationCopyField(updated.statement, STATEMENT_LENGTH, buffer);
        }
    }

    if (validationReadLine("  Recorded date (YYYY-MM-DD): ", buffer, sizeof(buffer)) == CF_SUCCESS)
    {
        validationTrim(buffer);
        if (validationIsNotEmpty(buffer))
        {
            if (!validationIsValidDate(buffer))
            {
                printf("  [!] %s\n", validationStatusMessage(CF_ERR_INVALID_DATE));
                return;
            }
            validationCopyField(updated.recordedDate, DATE_LENGTH, buffer);
        }
    }

    if (validationReadLine("  Mark statement verified? (y/n): ", answer, sizeof(answer)) == CF_SUCCESS)
    {
        validationTrim(answer);
        if (validationCompareIgnoreCase(answer, "y") == 0 || validationCompareIgnoreCase(answer, "yes") == 0)
        {
            updated.verified = 1;
        }
        else if (validationCompareIgnoreCase(answer, "n") == 0 || validationCompareIgnoreCase(answer, "no") == 0)
        {
            updated.verified = 0;
        }
    }

    status = witnessManager_update(db, witnessId, &updated);
    printStatus(status, "Witness updated.");
}

static void deleteWitness(CasefileDb_t *db)
{
    char witnessId[ID_LENGTH];

    printHeading("DELETE WITNESS");

    if (validationReadLine("  Witness ID: ", witnessId, sizeof(witnessId)) != CF_SUCCESS)
    {
        return;
    }
    validationTrim(witnessId);

    if (witnessManager_findById(db, witnessId) < 0)
    {
        printf("  [!] %s\n", validationStatusMessage(CF_ERR_NOT_FOUND));
        return;
    }

    if (!confirmAction("Delete this witness record?"))
    {
        printf("  Deletion cancelled.\n");
        return;
    }

    printStatus(witnessManager_delete(db, witnessId), "Witness deleted.");
}

static void witnessMenu(CasefileDb_t *db)
{
    int choice = 0;

    for (;;)
    {
        printf("\n--- WITNESS MANAGEMENT ---\n");
        printf("  1. Register witness\n");
        printf("  2. List witnesses of a case\n");
        printf("  3. Update witness\n");
        printf("  4. Delete witness\n");
        printf("  0. Back to main menu\n");

        if (validationReadInt("  Select: ", 0, 4, &choice) != CF_SUCCESS)
        {
            choice = 0;
        }

        switch (choice)
        {
            case 1:
                addWitness(db);
                break;
            case 2:
                listWitnesses(db);
                break;
            case 3:
                updateWitness(db);
                break;
            case 4:
                deleteWitness(db);
                break;
            default:
                break;
        }

        /* Choice 0 means "back to the previous menu". */
        if (choice == 0)
        {
            break;
        }
    }
}

/* ------------------------------------------------------------------ */
/* Analysis menu                                                       */
/* ------------------------------------------------------------------ */

static void analyseCase(const CasefileDb_t *db)
{
    char caseId[ID_LENGTH];
    AnalysisResult_t *analysis;
    CfStatus_t status;
    int index;

    printHeading("EVIDENCE ANALYSIS - SUSPECT COMPARISON");

    if (!readCaseId(caseId, sizeof(caseId)))
    {
        return;
    }

    /*
     * The analysis result is allocated on the heap because it is far too large
     * to be placed on the stack.
     */
    analysis = (AnalysisResult_t *)malloc(sizeof(AnalysisResult_t));
    if (analysis == NULL)
    {
        printf("  [!] There is not enough memory to run the analysis.\n");
        return;
    }

    status = investigation_analyseCase(db, caseId, analysis);

    if (status == CF_ERR_INSUFFICIENT_EVIDENCE)
    {
        printf("  [!] %s\n", validationStatusMessage(CF_ERR_INSUFFICIENT_EVIDENCE));
        printf("      No comparable evidence attribute was recorded for this case,\n");
        printf("      or the case has no suspects. A score would be misleading, so\n");
        printf("      none is produced.\n");
        free(analysis);
        return;
    }

    if (status != CF_SUCCESS)
    {
        printf("  [!] %s\n", validationStatusMessage(status));
        free(analysis);
        return;
    }

    printf("  %d suspect(s) evaluated against %d comparable evidence item(s).\n\n",
           analysis->suspectCount, analysis->comparableEvidence);

    for (index = 0; index < analysis->suspectCount; index++)
    {
        const SuspectScore_t *score = &analysis->suspects[ index ];
        int factor;

        printf("  Rank %d: %s (%s)\n", index + 1, score->suspectName, score->suspectId);
        printf("    Score %d / %d - %s\n", score->score, MAX_SUSPICION_SCORE,
               investigation_describeScore(score->score));
        printf("    Matched %d of %d comparable attribute(s)\n", score->matchedCount, score->checkedCount);

        for (factor = 0; factor < score->factorCount; factor++)
        {
            const MatchFactor_t *item = &score->factors[ factor ];

            if (item->matched != 0)
            {
                printf("      [MATCH]   %-11s %s = %-12s vs %-12s (+%d)\n",
                       validationAttributeKeyName(item->key), item->evidenceId,
                       item->evidenceValue, item->suspectValue, item->weight);
            }
            else
            {
                printf("      [no match] %-11s %s = %-12s vs %-12s\n",
                       validationAttributeKeyName(item->key), item->evidenceId,
                       item->evidenceValue, item->suspectValue);
            }
        }
        printf("\n");
    }

    printf("  %s\n", investigation_getDisclaimer());

    free(analysis);
}

static void showProgress(const CasefileDb_t *db)
{
    char caseId[ID_LENGTH];
    int progress = 0;
    char detail[TEXT_LENGTH];
    CfStatus_t status;

    printHeading("INVESTIGATION PROGRESS");

    if (!readCaseId(caseId, sizeof(caseId)))
    {
        return;
    }

    detail[0] = '\0';
    status = investigation_computeProgress(db, caseId, &progress, detail, sizeof(detail));

    if (status != CF_SUCCESS)
    {
        printf("  [!] %s\n", validationStatusMessage(status));
        return;
    }

    printf("  Progress : %d%%\n", progress);
    printf("  Detail   : %s\n", detail);
}

static void analysisMenu(const CasefileDb_t *db)
{
    int choice = 0;

    for (;;)
    {
        printf("\n--- EVIDENCE ANALYSIS ---\n");
        printf("  1. Analyse a case (suspect comparison)\n");
        printf("  2. Show investigation progress\n");
        printf("  0. Back to main menu\n");

        if (validationReadInt("  Select: ", 0, 2, &choice) != CF_SUCCESS)
        {
            choice = 0;
        }

        switch (choice)
        {
            case 1:
                analyseCase(db);
                break;
            case 2:
                showProgress(db);
                break;
            default:
                break;
        }

        /* Choice 0 means "back to the previous menu". */
        if (choice == 0)
        {
            break;
        }
    }
}

/* ------------------------------------------------------------------ */
/* Report menu                                                         */
/* ------------------------------------------------------------------ */

static void generateReport(const CasefileDb_t *db)
{
    char caseId[ID_LENGTH];
    char buffer[REPORT_BUFFER_SIZE];
    char path[FILE_PATH_LENGTH];
    char answer[8];
    CfStatus_t status;

    printHeading("GENERATE INVESTIGATION REPORT");

    if (!readCaseId(caseId, sizeof(caseId)))
    {
        return;
    }

    status = report_generateCaseSummary(db, caseId, buffer, sizeof(buffer));
    if (status != CF_SUCCESS)
    {
        printf("  [!] %s\n", validationStatusMessage(status));
        return;
    }

    printf("\n%s\n", buffer);

    if (validationReadLine("  Save this report to a file? (y/n): ", answer, sizeof(answer)) == CF_SUCCESS)
    {
        validationTrim(answer);
        if (validationCompareIgnoreCase(answer, "y") == 0 || validationCompareIgnoreCase(answer, "yes") == 0)
        {
            if (report_saveToFile(caseId, "investigation", buffer, path, sizeof(path)) == CF_SUCCESS)
            {
                printf("  [OK] Report saved to %s\n", path);
            }
            else
            {
                printf("  [!] %s\n", validationStatusMessage(CF_ERR_FILE_WRITE));
            }
        }
    }
}

static void generateEvidenceReport(const CasefileDb_t *db)
{
    char caseId[ID_LENGTH];
    char buffer[REPORT_BUFFER_SIZE];
    char path[FILE_PATH_LENGTH];
    char answer[8];
    CfStatus_t status;

    printHeading("GENERATE EVIDENCE SUMMARY REPORT");

    if (!readCaseId(caseId, sizeof(caseId)))
    {
        return;
    }

    status = report_generateEvidenceSummary(db, caseId, buffer, sizeof(buffer));
    if (status != CF_SUCCESS)
    {
        printf("  [!] %s\n", validationStatusMessage(status));
        return;
    }

    printf("\n%s\n", buffer);

    if (validationReadLine("  Save this report to a file? (y/n): ", answer, sizeof(answer)) == CF_SUCCESS)
    {
        validationTrim(answer);
        if (validationCompareIgnoreCase(answer, "y") == 0 || validationCompareIgnoreCase(answer, "yes") == 0)
        {
            if (report_saveToFile(caseId, "evidence", buffer, path, sizeof(path)) == CF_SUCCESS)
            {
                printf("  [OK] Report saved to %s\n", path);
            }
            else
            {
                printf("  [!] %s\n", validationStatusMessage(CF_ERR_FILE_WRITE));
            }
        }
    }
}

static void reportMenu(const CasefileDb_t *db)
{
    int choice = 0;

    for (;;)
    {
        printf("\n--- INVESTIGATION REPORTS ---\n");
        printf("  1. Investigation summary report\n");
        printf("  2. Evidence summary report\n");
        printf("  0. Back to main menu\n");

        if (validationReadInt("  Select: ", 0, 2, &choice) != CF_SUCCESS)
        {
            choice = 0;
        }

        switch (choice)
        {
            case 1:
                generateReport(db);
                break;
            case 2:
                generateEvidenceReport(db);
                break;
            default:
                break;
        }

        /* Choice 0 means "back to the previous menu". */
        if (choice == 0)
        {
            break;
        }
    }
}

/* ------------------------------------------------------------------ */
/* Main                                                                */
/* ------------------------------------------------------------------ */

static void displayMainMenu(const CasefileDb_t *db)
{
    printf("\n");
    printf("=====================================================================\n");
    printf(" CASEFILE - Forensic Evidence Management and Crime Investigation\n");
    printf("-------------------------------------------------------------------\n");
    printf(" 1. Case management\n");
    printf(" 2. Suspect management\n");
    printf(" 3. Forensic evidence management\n");
    printf(" 4. Witness management\n");
    printf(" 5. Evidence analysis\n");
    printf(" 6. Investigation reports\n");
    printf(" 7. Save all data to file\n");
    printf(" 8. Show data file status\n");
    printf(" 0. Exit\n");
    printf("-------------------------------------------------------------------\n");
    printf(" Records: %zu case(s), %zu suspect(s), %zu evidence item(s), %zu witness(es)\n",
           db->caseCount, db->suspectCount, db->evidenceCount, db->witnessCount);
    printf("=====================================================================\n");
}

static void showFileStatus(void)
{
    int index;

    printHeading("DATA FILE STATUS");

    for (index = 0; index < STORAGE_FILE_COUNT; index++)
    {
        const char *name = storage_fileNameByIndex(index);
        char path[FILE_PATH_LENGTH];
        CfStatus_t status;

        if (name == NULL)
        {
            continue;
        }

        if (storage_buildPath(name, path, sizeof(path)) != CF_SUCCESS)
        {
            continue;
        }

        status = storage_validateFormat(path);
        printf("  %-16s %-40s ", name, path);

        if (status == CF_SUCCESS)
        {
            printf("OK\n");
        }
        else if (status == CF_ERR_FILE_OPEN)
        {
            printf("not present yet\n");
        }
        else
        {
            printf("UNREADABLE - %s\n", validationStatusMessage(status));
        }
    }
}

int main(void)
{
    CasefileDb_t *db;
    CfStatus_t status;
    int choice = 0;

    /*
     * The database holds every record of the application and is too large for
     * the stack, so it is allocated on the heap and released at the end.
     */
    db = casefile_createDatabase();
    if (db == NULL)
    {
        printf("  [!] There is not enough memory to start CASEFILE.\n");
        return EXIT_FAILURE;
    }

    printf("=====================================================================\n");
    printf(" CASEFILE - Forensic Evidence Management and Crime Investigation System\n");
    printf("=====================================================================\n");
    printf(" All case data is fictional and is used for teaching purposes only.\n");

    status = storage_loadAll(db);
    if (status == CF_SUCCESS)
    {
        if (db->caseCount == 0U && db->suspectCount == 0U
            && db->evidenceCount == 0U && db->witnessCount == 0U)
        {
            printf(" No stored data was found. Starting with an empty case file.\n");
        }
        else
        {
            printf(" Loaded %zu case(s), %zu suspect(s), %zu evidence item(s), %zu witness(es).\n",
                   db->caseCount, db->suspectCount, db->evidenceCount, db->witnessCount);
        }
    }
    else
    {
        printf(" [!] %s\n", validationStatusMessage(status));
        printf("     The affected data file was not loaded. Starting with an empty case file.\n");
    }

    for (;;)
    {
        displayMainMenu(db);

        if (validationReadInt("  Select: ", 0, 8, &choice) != CF_SUCCESS)
        {
            choice = 0;
        }

        switch (choice)
        {
            case 1:
                caseMenu(db);
                break;
            case 2:
                suspectMenu(db);
                break;
            case 3:
                evidenceMenu(db);
                break;
            case 4:
                witnessMenu(db);
                break;
            case 5:
                analysisMenu(db);
                break;
            case 6:
                reportMenu(db);
                break;
            case 7:
                status = storage_saveAll(db);
                printStatus(status, "All records saved to the data directory.");
                break;
            case 8:
                showFileStatus();
                break;
            default:
                break;
        }

        /* Choice 0 means "exit the application". */
        if (choice == 0)
        {
            break;
        }
    }

    printHeading("EXIT");

    status = storage_saveAll(db);
    if (status == CF_SUCCESS)
    {
        printf("  [OK] All records were saved to the data directory.\n");
    }
    else
    {
        printf("  [!] %s\n", validationStatusMessage(status));
        printf("      Your changes were NOT saved. Check the data directory permissions.\n");
    }

    printf("\n  Thank you for using CASEFILE.\n\n");

    casefile_destroyDatabase(db);

    return (status == CF_SUCCESS) ? EXIT_SUCCESS : EXIT_FAILURE;
}
