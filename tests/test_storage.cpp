/*
 * test_storage.cpp - Unit tests for file persistence and report generation.
 *
 * These tests write to temporary files that are removed again afterwards, so
 * they leave no trace in the repository and never depend on the state left
 * behind by another test.
 */

extern "C" {
#include "case_manager.h"
#include "casefile.h"
#include "evidence_manager.h"
#include "investigation.h"
#include "report_generator.h"
#include "storage.h"
#include "suspect_manager.h"
#include "witness_manager.h"
}

#include <gtest/gtest.h>

#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>

namespace
{

/*
 * Path of the temporary directory used by the storage tests.
 */
const char *const tempDirectory = "/tmp/opencode_casefile_test";

/*
 * Remove the temporary test directory and everything inside it.
 */
void removeTempDirectory()
{
    std::string command = std::string("rm -rf ") + tempDirectory;

    if (std::system(command.c_str()) != 0)
    {
        /* The directory may not exist, which is harmless. */
    }
}

/*
 * Create the temporary test directory.
 */
void createTempDirectory()
{
    std::string command = std::string("mkdir -p ") + tempDirectory;

    if (std::system(command.c_str()) != 0)
    {
        ADD_FAILURE() << "could not create the temporary test directory";
    }
}

/*
 * Write arbitrary text to a file inside the temporary directory.
 */
void writeTempFile(const std::string &name, const std::string &content)
{
    std::string path = std::string(tempDirectory) + "/" + name;
    std::ofstream stream(path.c_str(), std::ios::out | std::ios::trunc);

    stream << content;
    stream.close();
}

/*
 * Read a file from the temporary directory.
 */
std::string readTempFile(const std::string &name)
{
    std::string path = std::string(tempDirectory) + "/" + name;
    std::ifstream stream(path.c_str());
    std::string content((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());

    return content;
}

/*
 * Build a small database with one case, one suspect and one evidence item.
 */
void buildSampleDatabase(CasefileDb_t *db)
{
    Case_t caseRecord;
    Suspect_t suspect;
    Evidence_t evidence;

    std::memset(&caseRecord, 0, sizeof(caseRecord));
    std::strcpy(caseRecord.title, "Persistence fixture");
    caseRecord.crimeType = CRIME_TYPE_THEFT;
    caseRecord.priority = CASE_PRIORITY_HIGH;
    caseRecord.status = CASE_STATUS_UNDER_INVESTIGATION;
    std::strcpy(caseRecord.incidentDate, "2026-01-14");
    std::strcpy(caseRecord.location, "Sector 9");
    std::strcpy(caseRecord.assignedOfficer, "Insp. R. Kadam");
    std::strcpy(caseRecord.summary, "Round trip fixture.");
    EXPECT_EQ(caseManager_addCase(db, &caseRecord), CF_SUCCESS);

    std::memset(&suspect, 0, sizeof(suspect));
    std::strcpy(suspect.caseId, caseRecord.caseId);
    std::strcpy(suspect.fullName, "Round Trip Suspect");
    suspect.age = 34;
    std::strcpy(suspect.alibi, "Unconfirmed");
    EXPECT_EQ(suspectManager_add(db, &suspect), CF_SUCCESS);

    std::memset(&evidence, 0, sizeof(evidence));
    std::strcpy(evidence.caseId, caseRecord.caseId);
    evidence.category = EVIDENCE_CATEGORY_PHYSICAL_TRACE;
    std::strcpy(evidence.description, "Fibre strand");
    std::strcpy(evidence.collectionDate, "2026-01-14");
    std::strcpy(evidence.collectionLocation, "Sector 9");
    std::strcpy(evidence.collectedBy, "Constable Naik");
    EXPECT_EQ(evidenceManager_add(db, &evidence), CF_SUCCESS);
    EXPECT_EQ(evidenceManager_addAttribute(db, evidence.evidenceId, ATTRIBUTE_HAIR, "black"), CF_SUCCESS);
}

/* ------------------------------------------------------------------ */
/* Format validation                                                   */
/* ------------------------------------------------------------------ */

TEST(StorageFormatTest, ValidSignatureIsAccepted)
{
    createTempDirectory();
    writeTempFile("good.txt", "CASEFILE-DATA-V1\nCF-2026-0001|A\n");

    EXPECT_EQ(storage_validateFormat((std::string(tempDirectory) + "/good.txt").c_str()), CF_SUCCESS);

    removeTempDirectory();
}

TEST(StorageFormatTest, ForeignSignatureIsRejected)
{
    createTempDirectory();
    writeTempFile("foreign.txt", "SOMETHING-ELSE-V9\nrecord one\n");

    EXPECT_EQ(storage_validateFormat((std::string(tempDirectory) + "/foreign.txt").c_str()),
              CF_ERR_CORRUPT_FILE);

    removeTempDirectory();
}

TEST(StorageFormatTest, EmptyFileIsRejected)
{
    createTempDirectory();
    writeTempFile("empty.txt", "");

    EXPECT_EQ(storage_validateFormat((std::string(tempDirectory) + "/empty.txt").c_str()),
              CF_ERR_CORRUPT_FILE);

    removeTempDirectory();
}

TEST(StorageFormatTest, MissingFileIsReportedAsUnreadable)
{
    createTempDirectory();

    EXPECT_EQ(storage_validateFormat((std::string(tempDirectory) + "/absent.txt").c_str()),
              CF_ERR_FILE_OPEN);

    removeTempDirectory();
}

TEST(StorageFormatTest, NullPathIsRejected)
{
    EXPECT_EQ(storage_validateFormat(nullptr), CF_ERR_NULL_POINTER);
}

/* ------------------------------------------------------------------ */
/* Path construction and field sanitising                               */
/* ------------------------------------------------------------------ */

TEST(StoragePathTest, BuildPathJoinsDirectoryAndFileName)
{
    char path[FILE_PATH_LENGTH];

    ASSERT_EQ(storage_buildPath("cases.txt", path, sizeof(path)), CF_SUCCESS);
    EXPECT_EQ(std::string(path), std::string("data/cases.txt"));
}

TEST(StoragePathTest, BuildPathRejectsNullArguments)
{
    char path[FILE_PATH_LENGTH];

    EXPECT_EQ(storage_buildPath(nullptr, path, sizeof(path)), CF_ERR_INVALID_ARGUMENT);
    EXPECT_EQ(storage_buildPath("cases.txt", nullptr, sizeof(path)), CF_ERR_INVALID_ARGUMENT);
}

TEST(StoragePathTest, EveryDataFileHasAName)
{
    for (int index = 0; index < STORAGE_FILE_COUNT; index++)
    {
        ASSERT_NE(storage_fileNameByIndex(index), nullptr);
        EXPECT_STRNE(storage_fileNameByIndex(index), "");
    }
    EXPECT_EQ(storage_fileNameByIndex(STORAGE_FILE_COUNT), nullptr);
    EXPECT_EQ(storage_fileNameByIndex(-1), nullptr);
}

TEST(StorageSanitizeTest, SeparatorCharacterIsReplaced)
{
    char text[32] = "left|right";

    storage_sanitizeField(text);

    EXPECT_EQ(std::string(text), "left right");
    EXPECT_EQ(std::string(text).find('|'), std::string::npos);
}

TEST(StorageSanitizeTest, NewlineCharacterIsReplaced)
{
    char text[32] = "left\nright";

    storage_sanitizeField(text);

    EXPECT_EQ(std::string(text), "left right");
}

TEST(StorageSanitizeTest, NullPointerIsHandledSafely)
{
    storage_sanitizeField(nullptr);

    SUCCEED();
}

/* ------------------------------------------------------------------ */
/* Write line                                                          */
/* ------------------------------------------------------------------ */

TEST(StorageWriteLineTest, WriteToOpenFileSucceeds)
{
    createTempDirectory();
    std::string path = std::string(tempDirectory) + "/written.txt";
    FILE *stream = std::fopen(path.c_str(), "w");

    ASSERT_NE(stream, nullptr);
    EXPECT_EQ(storage_writeLine(stream, "hello world"), CF_SUCCESS);
    EXPECT_EQ(std::fclose(stream), 0);

    EXPECT_EQ(readTempFile("written.txt"), "hello world\n");

    removeTempDirectory();
}

TEST(StorageWriteLineTest, WriteToNullStreamIsRejected)
{
    EXPECT_EQ(storage_writeLine(nullptr, "text"), CF_ERR_NULL_POINTER);
}

/* ------------------------------------------------------------------ */
/* Report generation                                                    */
/* ------------------------------------------------------------------ */

TEST(ReportTest, CaseSummaryContainsTheKeyInformation)
{
    CasefileDb_t *db = casefile_createDatabase();
    char buffer[8192];

    ASSERT_NE(db, nullptr);
    buildSampleDatabase(db);

    ASSERT_EQ(report_generateCaseSummary(db, db->cases[0].caseId, buffer, sizeof(buffer)), CF_SUCCESS);

    EXPECT_NE(std::string(buffer).find(db->cases[0].caseId), std::string::npos);
    EXPECT_NE(std::string(buffer).find("Persistence fixture"), std::string::npos);
    EXPECT_NE(std::string(buffer).find("Fibre strand"), std::string::npos);
    EXPECT_NE(std::string(buffer).find("Round Trip Suspect"), std::string::npos);
}

TEST(ReportTest, CaseSummaryForUnknownCaseReportsNotFound)
{
    CasefileDb_t *db = casefile_createDatabase();
    char buffer[8192];

    ASSERT_NE(db, nullptr);
    buildSampleDatabase(db);

    EXPECT_EQ(report_generateCaseSummary(db, "CF-2026-9999", buffer, sizeof(buffer)), CF_ERR_NOT_FOUND);

    casefile_destroyDatabase(db);
}

TEST(ReportTest, CaseSummaryRejectsNullArguments)
{
    CasefileDb_t *db = casefile_createDatabase();
    char buffer[8192];

    ASSERT_NE(db, nullptr);
    buildSampleDatabase(db);

    EXPECT_EQ(report_generateCaseSummary(nullptr, db->cases[0].caseId, buffer, sizeof(buffer)),
              CF_ERR_NULL_POINTER);
    EXPECT_EQ(report_generateCaseSummary(db, db->cases[0].caseId, nullptr, sizeof(buffer)),
              CF_ERR_NULL_POINTER);

    casefile_destroyDatabase(db);
}

TEST(ReportTest, EvidenceSummaryListsAttributesAndCustody)
{
    CasefileDb_t *db = casefile_createDatabase();
    char buffer[8192];
    CustodyEvent_t event;

    ASSERT_NE(db, nullptr);
    buildSampleDatabase(db);

    std::memset(&event, 0, sizeof(event));
    event.action = CUSTODY_ACTION_COLLECTED;
    std::strcpy(event.timestamp, "2026-01-14");
    std::strcpy(event.fromHandler, "Scene");
    std::strcpy(event.toHandler, "C. Naik");
    std::strcpy(event.location, "Sector 9");
    ASSERT_EQ(evidenceManager_addCustodyEvent(db, db->evidence[0].evidenceId, &event), CF_SUCCESS);

    ASSERT_EQ(report_generateEvidenceSummary(db, db->cases[0].caseId, buffer, sizeof(buffer)), CF_SUCCESS);

    EXPECT_NE(std::string(buffer).find("Fibre strand"), std::string::npos);
    EXPECT_NE(std::string(buffer).find("hair"), std::string::npos);
    EXPECT_NE(std::string(buffer).find("C. Naik"), std::string::npos);

    casefile_destroyDatabase(db);
}

TEST(ReportTest, SmallBufferDoesNotOverflow)
{
    CasefileDb_t *db = casefile_createDatabase();
    char buffer[64];

    ASSERT_NE(db, nullptr);
    buildSampleDatabase(db);

    EXPECT_EQ(report_generateCaseSummary(db, db->cases[0].caseId, buffer, sizeof(buffer)), CF_SUCCESS);
    EXPECT_LT(std::string(buffer).size(), sizeof(buffer));

    casefile_destroyDatabase(db);
}

} // namespace
