/*
 * test_records.cpp - Unit tests for the record manager modules.
 *
 * These tests exercise the case, suspect, evidence and witness registers
 * through their public APIs. No file or console interaction is involved, so
 * every test is independent and repeatable.
 */

extern "C" {
#include "case_manager.h"
#include "casefile.h"
#include "evidence_manager.h"
#include "suspect_manager.h"
#include "validation.h"
#include "witness_manager.h"
}

#include <gtest/gtest.h>

#include <cstring>
#include <string>

namespace
{

/*
 * Fixture that owns a database for the duration of one test.
 *
 * The database is allocated on the heap because it is far too large to live
 * on the stack. Creating it in SetUp and releasing it in TearDown guarantees
 * that each test starts from an empty, known state.
 */
class RecordTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        db = casefile_createDatabase();
        ASSERT_NE(db, nullptr);
    }

    void TearDown() override
    {
        casefile_destroyDatabase(db);
        db = nullptr;
    }

    CasefileDb_t *db = nullptr;
};

/*
 * Build a valid case record for use as a fixture.
 */
Case_t makeValidCase()
{
    Case_t record;

    std::memset(&record, 0, sizeof(record));
    std::strcpy(record.title, "Warehouse break-in");
    record.crimeType = CRIME_TYPE_THEFT;
    record.priority = CASE_PRIORITY_HIGH;
    record.status = CASE_STATUS_OPEN;
    std::strcpy(record.incidentDate, "2026-01-14");
    std::strcpy(record.location, "Sector 9 Industrial Estate");
    std::strcpy(record.assignedOfficer, "Insp. R. Kadam");
    std::strcpy(record.summary, "Cash register emptied overnight.");

    return record;
}

/*
 * Register one case and return its generated identifier.
 */
std::string registerOneCase(CasefileDb_t *db)
{
    Case_t record = makeValidCase();

    EXPECT_EQ(caseManager_addCase(db, &record), CF_SUCCESS);

    return std::string(record.caseId);
}

/* ------------------------------------------------------------------ */
/* Case manager                                                        */
/* ------------------------------------------------------------------ */

TEST_F(RecordTest, AddCaseStoresRecordAndGeneratesIdentifier)
{
    Case_t record = makeValidCase();

    ASSERT_EQ(caseManager_addCase(db, &record), CF_SUCCESS);
    ASSERT_EQ(db->caseCount, 1U);
    EXPECT_EQ(std::string(record.caseId).substr(0, 3), "CF-");
    EXPECT_EQ(caseManager_findById(db, record.caseId), 0);
}

TEST_F(RecordTest, GeneratedIdentifiersAreUniqueAcrossCases)
{
    std::string firstId;
    std::string secondId;

    firstId = registerOneCase(db);
    secondId = registerOneCase(db);

    EXPECT_NE(firstId, secondId);
    EXPECT_EQ(db->caseCount, 2U);
}

TEST_F(RecordTest, AddCaseRejectsEmptyTitle)
{
    Case_t record = makeValidCase();

    record.title[0] = '\0';

    EXPECT_EQ(caseManager_addCase(db, &record), CF_ERR_EMPTY_INPUT);
    EXPECT_EQ(db->caseCount, 0U);
}

TEST_F(RecordTest, AddCaseRejectsInvalidDate)
{
    Case_t record = makeValidCase();

    std::strcpy(record.incidentDate, "2026-02-30");

    EXPECT_EQ(caseManager_addCase(db, &record), CF_ERR_INVALID_DATE);
    EXPECT_EQ(db->caseCount, 0U);
}

TEST_F(RecordTest, AddCaseRejectsInvalidCrimeType)
{
    Case_t record = makeValidCase();

    record.crimeType = static_cast<CrimeType_t>(99);

    EXPECT_EQ(caseManager_addCase(db, &record), CF_ERR_INVALID_ENUM);
    EXPECT_EQ(db->caseCount, 0U);
}

TEST_F(RecordTest, AddCaseRejectsNullDatabase)
{
    Case_t record = makeValidCase();

    EXPECT_EQ(caseManager_addCase(nullptr, &record), CF_ERR_NULL_POINTER);
}

TEST_F(RecordTest, FindByIdReturnsMinusOneForUnknownIdentifier)
{
    registerOneCase(db);

    EXPECT_EQ(caseManager_findById(db, "CF-2026-9999"), -1);
}

TEST_F(RecordTest, GetCaseReportsNotFoundForUnknownIdentifier)
{
    Case_t result;

    registerOneCase(db);

    EXPECT_EQ(caseManager_getCase(db, "CF-2026-9999", &result), CF_ERR_NOT_FOUND);
}

TEST_F(RecordTest, FilterByStatusReturnsOnlyMatchingCases)
{
    Case_t first = makeValidCase();
    Case_t second = makeValidCase();
    Case_t filtered[ MAX_CASES ];
    size_t count = 0U;

    first.status = CASE_STATUS_OPEN;
    second.status = CASE_STATUS_CLOSED;

    ASSERT_EQ(caseManager_addCase(db, &first), CF_SUCCESS);
    ASSERT_EQ(caseManager_addCase(db, &second), CF_SUCCESS);

    ASSERT_EQ(caseManager_filterByStatus(db, CASE_STATUS_CLOSED, filtered, MAX_CASES, &count), CF_SUCCESS);
    ASSERT_EQ(count, 1U);
    EXPECT_EQ(std::string(filtered[0].caseId), std::string(second.caseId));
}

TEST_F(RecordTest, SetStatusChangesStatusAndRejectsInvalidEnum)
{
    std::string caseId = registerOneCase(db);

    ASSERT_EQ(caseManager_setStatus(db, caseId.c_str(), CASE_STATUS_EVIDENCE_ANALYSIS), CF_SUCCESS);
    EXPECT_EQ(db->cases[0].status, CASE_STATUS_EVIDENCE_ANALYSIS);

    EXPECT_EQ(caseManager_setStatus(db, caseId.c_str(), static_cast<CaseStatus_t>(77)),
              CF_ERR_INVALID_ENUM);
    EXPECT_EQ(db->cases[0].status, CASE_STATUS_EVIDENCE_ANALYSIS);
}

TEST_F(RecordTest, UpdatePreservesIdentifierAndChangesFields)
{
    std::string caseId = registerOneCase(db);
    Case_t updated = makeValidCase();

    std::strcpy(updated.title, "Revised title");
    std::strcpy(updated.incidentDate, "2026-03-01");

    ASSERT_EQ(caseManager_update(db, caseId.c_str(), &updated), CF_SUCCESS);
    EXPECT_EQ(std::string(db->cases[0].caseId), caseId);
    EXPECT_EQ(std::string(db->cases[0].title), "Revised title");
    EXPECT_EQ(std::string(db->cases[0].incidentDate), "2026-03-01");
}

TEST_F(RecordTest, UpdateRejectsUnknownIdentifier)
{
    Case_t updated = makeValidCase();

    EXPECT_EQ(caseManager_update(db, "CF-2026-9999", &updated), CF_ERR_NOT_FOUND);
}

TEST_F(RecordTest, DeleteRemovesCaseAndItsLinkedRecords)
{
    std::string caseId = registerOneCase(db);
    Suspect_t suspect;
    Evidence_t evidence;
    Witness_t witness;

    std::memset(&suspect, 0, sizeof(suspect));
    std::strcpy(suspect.caseId, caseId.c_str());
    std::strcpy(suspect.fullName, "Ravi Deshmukh");
    suspect.age = 34;
    ASSERT_EQ(suspectManager_add(db, &suspect), CF_SUCCESS);

    std::memset(&evidence, 0, sizeof(evidence));
    std::strcpy(evidence.caseId, caseId.c_str());
    evidence.category = EVIDENCE_CATEGORY_PHYSICAL_TRACE;
    std::strcpy(evidence.description, "Fibre strand");
    std::strcpy(evidence.collectionDate, "2026-01-14");
    ASSERT_EQ(evidenceManager_add(db, &evidence), CF_SUCCESS);

    std::memset(&witness, 0, sizeof(witness));
    std::strcpy(witness.caseId, caseId.c_str());
    std::strcpy(witness.fullName, "Meera Kulkarni");
    std::strcpy(witness.statement, "Observed a person with a bag.");
    std::strcpy(witness.recordedDate, "2026-01-15");
    ASSERT_EQ(witnessManager_add(db, &witness), CF_SUCCESS);

    ASSERT_EQ(db->caseCount, 1U);
    ASSERT_EQ(db->suspectCount, 1U);
    ASSERT_EQ(db->evidenceCount, 1U);
    ASSERT_EQ(db->witnessCount, 1U);

    ASSERT_EQ(caseManager_delete(db, caseId.c_str()), CF_SUCCESS);

    EXPECT_EQ(db->caseCount, 0U);
    EXPECT_EQ(db->suspectCount, 0U);
    EXPECT_EQ(db->evidenceCount, 0U);
    EXPECT_EQ(db->witnessCount, 0U);
}

TEST_F(RecordTest, DeleteReportsNotFoundForUnknownIdentifier)
{
    EXPECT_EQ(caseManager_delete(db, "CF-2026-9999"), CF_ERR_NOT_FOUND);
}

TEST_F(RecordTest, AddCaseReportsCapacityReachedAtLimit)
{
    Case_t record = makeValidCase();

    db->caseCount = MAX_CASES;

    EXPECT_EQ(caseManager_addCase(db, &record), CF_ERR_CAPACITY_FULL);
    EXPECT_EQ(db->caseCount, static_cast<size_t>(MAX_CASES));
}

/* ------------------------------------------------------------------ */
/* Suspect manager                                                     */
/* ------------------------------------------------------------------ */

TEST_F(RecordTest, AddSuspectRequiresAnExistingCase)
{
    Suspect_t suspect;

    std::memset(&suspect, 0, sizeof(suspect));
    std::strcpy(suspect.caseId, "CF-2026-0001");
    std::strcpy(suspect.fullName, "Orphan Suspect");
    suspect.age = 40;

    EXPECT_EQ(suspectManager_add(db, &suspect), CF_ERR_NOT_FOUND);
    EXPECT_EQ(db->suspectCount, 0U);
}

TEST_F(RecordTest, AddSuspectStoresRecordAndGeneratesIdentifier)
{
    std::string caseId = registerOneCase(db);
    Suspect_t suspect;

    std::memset(&suspect, 0, sizeof(suspect));
    std::strcpy(suspect.caseId, caseId.c_str());
    std::strcpy(suspect.fullName, "Imran Shaikh");
    suspect.age = 29;

    ASSERT_EQ(suspectManager_add(db, &suspect), CF_SUCCESS);
    ASSERT_EQ(db->suspectCount, 1U);
    EXPECT_EQ(std::string(suspect.suspectId).substr(0, 3), "SU-");
}

TEST_F(RecordTest, AddSuspectRejectsInvalidAge)
{
    std::string caseId = registerOneCase(db);
    Suspect_t suspect;

    std::memset(&suspect, 0, sizeof(suspect));
    std::strcpy(suspect.caseId, caseId.c_str());
    std::strcpy(suspect.fullName, "No Age");
    suspect.age = 0;

    EXPECT_EQ(suspectManager_add(db, &suspect), CF_ERR_INVALID_ARGUMENT);
    EXPECT_EQ(db->suspectCount, 0U);
}

TEST_F(RecordTest, SearchByNameMatchesPartialTextIgnoringCase)
{
    std::string caseId = registerOneCase(db);
    Suspect_t first;
    Suspect_t second;
    Suspect_t found[ MAX_SUSPECTS ];
    size_t count = 0U;

    std::memset(&first, 0, sizeof(first));
    std::strcpy(first.caseId, caseId.c_str());
    std::strcpy(first.fullName, "Ravi Deshmukh");
    first.age = 34;
    ASSERT_EQ(suspectManager_add(db, &first), CF_SUCCESS);

    std::memset(&second, 0, sizeof(second));
    std::strcpy(second.caseId, caseId.c_str());
    std::strcpy(second.fullName, "Imran Shaikh");
    second.age = 29;
    ASSERT_EQ(suspectManager_add(db, &second), CF_SUCCESS);

    ASSERT_EQ(suspectManager_searchByName(db, "desh", found, MAX_SUSPECTS, &count), CF_SUCCESS);
    EXPECT_EQ(count, 1U);
    EXPECT_EQ(std::string(found[0].fullName), "Ravi Deshmukh");
}

TEST_F(RecordTest, ListByCaseReturnsOnlyThatCasesSuspects)
{
    std::string firstCase = registerOneCase(db);
    std::string secondCase = registerOneCase(db);
    Suspect_t suspect;
    Suspect_t found[ MAX_SUSPECTS ];
    size_t count = 0U;

    std::memset(&suspect, 0, sizeof(suspect));
    std::strcpy(suspect.caseId, secondCase.c_str());
    std::strcpy(suspect.fullName, "Linked Suspect");
    suspect.age = 30;
    ASSERT_EQ(suspectManager_add(db, &suspect), CF_SUCCESS);

    ASSERT_EQ(suspectManager_listByCase(db, firstCase.c_str(), found, MAX_SUSPECTS, &count), CF_SUCCESS);
    EXPECT_EQ(count, 0U);

    ASSERT_EQ(suspectManager_listByCase(db, secondCase.c_str(), found, MAX_SUSPECTS, &count), CF_SUCCESS);
    EXPECT_EQ(count, 1U);
}

TEST_F(RecordTest, PhysicalProfileIsStoredAndVehicleListIsBounded)
{
    std::string caseId = registerOneCase(db);
    Suspect_t suspect;
    PhysicalProfile_t profile;
    Suspect_t stored;

    std::memset(&suspect, 0, sizeof(suspect));
    std::strcpy(suspect.caseId, caseId.c_str());
    std::strcpy(suspect.fullName, "Profile Holder");
    suspect.age = 45;
    ASSERT_EQ(suspectManager_add(db, &suspect), CF_SUCCESS);

    std::memset(&profile, 0, sizeof(profile));
    std::strcpy(profile.gender, "male");
    profile.heightCm = 178;
    std::strcpy(profile.hairColour, "black");
    profile.shoeSize = 42;
    std::strcpy(profile.bloodGroup, "O+");

    ASSERT_EQ(suspectManager_setPhysicalProfile(db, suspect.suspectId, &profile), CF_SUCCESS);
    ASSERT_EQ(suspectManager_get(db, suspect.suspectId, &stored), CF_SUCCESS);

    EXPECT_EQ(std::string(stored.physical.gender), "male");
    EXPECT_EQ(stored.physical.heightCm, 178);
    EXPECT_EQ(std::string(stored.physical.hairColour), "black");
    EXPECT_EQ(stored.physical.shoeSize, 42);
    EXPECT_EQ(std::string(stored.physical.bloodGroup), "O+");

    for (int index = 0; index < MAX_VEHICLES; index++)
    {
        ASSERT_EQ(suspectManager_addVehicle(db, suspect.suspectId, "MH-12-AB-1234"), CF_SUCCESS);
    }
    EXPECT_EQ(suspectManager_addVehicle(db, suspect.suspectId, "MH-12-AB-9999"), CF_ERR_CAPACITY_FULL);
}

TEST_F(RecordTest, DeleteRemovesSuspect)
{
    std::string caseId = registerOneCase(db);
    Suspect_t suspect;

    std::memset(&suspect, 0, sizeof(suspect));
    std::strcpy(suspect.caseId, caseId.c_str());
    std::strcpy(suspect.fullName, "Temporary");
    suspect.age = 33;
    ASSERT_EQ(suspectManager_add(db, &suspect), CF_SUCCESS);
    ASSERT_EQ(db->suspectCount, 1U);

    ASSERT_EQ(suspectManager_delete(db, suspect.suspectId), CF_SUCCESS);
    EXPECT_EQ(db->suspectCount, 0U);
    EXPECT_EQ(suspectManager_delete(db, suspect.suspectId), CF_ERR_NOT_FOUND);
}

/* ------------------------------------------------------------------ */
/* Evidence manager                                                    */
/* ------------------------------------------------------------------ */

TEST_F(RecordTest, AddEvidenceRequiresAnExistingCase)
{
    Evidence_t evidence;

    std::memset(&evidence, 0, sizeof(evidence));
    std::strcpy(evidence.caseId, "CF-2026-0001");
    evidence.category = EVIDENCE_CATEGORY_BIOLOGICAL;
    std::strcpy(evidence.description, "Blood stained cloth");
    std::strcpy(evidence.collectionDate, "2026-02-03");

    EXPECT_EQ(evidenceManager_add(db, &evidence), CF_ERR_NOT_FOUND);
    EXPECT_EQ(db->evidenceCount, 0U);
}

TEST_F(RecordTest, AddEvidenceStoresRecordAndGeneratesIdentifier)
{
    std::string caseId = registerOneCase(db);
    Evidence_t evidence;

    std::memset(&evidence, 0, sizeof(evidence));
    std::strcpy(evidence.caseId, caseId.c_str());
    evidence.category = EVIDENCE_CATEGORY_BIOLOGICAL;
    std::strcpy(evidence.description, "Blood stained cloth");
    std::strcpy(evidence.collectionDate, "2026-02-03");

    ASSERT_EQ(evidenceManager_add(db, &evidence), CF_SUCCESS);
    ASSERT_EQ(db->evidenceCount, 1U);
    EXPECT_EQ(std::string(evidence.evidenceId).substr(0, 3), "EV-");
}

TEST_F(RecordTest, AddAttributeReplacesAnExistingKey)
{
    std::string caseId = registerOneCase(db);
    Evidence_t evidence;

    std::memset(&evidence, 0, sizeof(evidence));
    std::strcpy(evidence.caseId, caseId.c_str());
    evidence.category = EVIDENCE_CATEGORY_PHYSICAL_TRACE;
    std::strcpy(evidence.description, "Fibre strand");
    std::strcpy(evidence.collectionDate, "2026-01-14");
    ASSERT_EQ(evidenceManager_add(db, &evidence), CF_SUCCESS);

    ASSERT_EQ(evidenceManager_addAttribute(db, evidence.evidenceId, ATTRIBUTE_HAIR, "black"), CF_SUCCESS);
    ASSERT_EQ(evidenceManager_addAttribute(db, evidence.evidenceId, ATTRIBUTE_HAIR, "brown"), CF_SUCCESS);

    ASSERT_EQ(db->evidence[0].attributeCount, 1);
    EXPECT_EQ(std::string(db->evidence[0].attributes[0].value), "brown");
}

TEST_F(RecordTest, AddAttributeRejectsUnknownEvidenceAndInvalidKey)
{
    std::string caseId = registerOneCase(db);
    Evidence_t evidence;

    std::memset(&evidence, 0, sizeof(evidence));
    std::strcpy(evidence.caseId, caseId.c_str());
    evidence.category = EVIDENCE_CATEGORY_PHYSICAL_TRACE;
    std::strcpy(evidence.description, "Fibre strand");
    std::strcpy(evidence.collectionDate, "2026-01-14");
    ASSERT_EQ(evidenceManager_add(db, &evidence), CF_SUCCESS);

    EXPECT_EQ(evidenceManager_addAttribute(db, "EV-99999", ATTRIBUTE_HAIR, "black"), CF_ERR_NOT_FOUND);
    EXPECT_EQ(evidenceManager_addAttribute(db, evidence.evidenceId,
                                           static_cast<AttributeKey_t>(88), "black"),
              CF_ERR_INVALID_ENUM);
    EXPECT_EQ(evidenceManager_addAttribute(db, evidence.evidenceId, ATTRIBUTE_HAIR, ""),
              CF_ERR_EMPTY_INPUT);
}

TEST_F(RecordTest, CustodyEventsAreAppendedInOrder)
{
    std::string caseId = registerOneCase(db);
    Evidence_t evidence;
    CustodyEvent_t event;
    CustodyEvent_t history[ MAX_CUSTODY_EVENTS ];
    size_t count = 0U;

    std::memset(&evidence, 0, sizeof(evidence));
    std::strcpy(evidence.caseId, caseId.c_str());
    evidence.category = EVIDENCE_CATEGORY_VEHICLE;
    std::strcpy(evidence.description, "Tyre impression");
    std::strcpy(evidence.collectionDate, "2026-01-14");
    ASSERT_EQ(evidenceManager_add(db, &evidence), CF_SUCCESS);

    std::memset(&event, 0, sizeof(event));
    event.action = CUSTODY_ACTION_COLLECTED;
    std::strcpy(event.timestamp, "2026-01-14");
    std::strcpy(event.fromHandler, "Scene");
    std::strcpy(event.toHandler, "C. Naik");
    ASSERT_EQ(evidenceManager_addCustodyEvent(db, evidence.evidenceId, &event), CF_SUCCESS);

    event.action = CUSTODY_ACTION_TRANSFERRED;
    std::strcpy(event.timestamp, "2026-01-15");
    std::strcpy(event.fromHandler, "C. Naik");
    std::strcpy(event.toHandler, "Evidence Store");
    ASSERT_EQ(evidenceManager_addCustodyEvent(db, evidence.evidenceId, &event), CF_SUCCESS);

    ASSERT_EQ(evidenceManager_getCustodyHistory(db, evidence.evidenceId, history,
                                                MAX_CUSTODY_EVENTS, &count), CF_SUCCESS);
    ASSERT_EQ(count, 2U);
    EXPECT_EQ(history[0].action, CUSTODY_ACTION_COLLECTED);
    EXPECT_EQ(history[1].action, CUSTODY_ACTION_TRANSFERRED);
    EXPECT_EQ(std::string(history[1].toHandler), "Evidence Store");
}

TEST_F(RecordTest, CustodyEventRejectsInvalidDate)
{
    std::string caseId = registerOneCase(db);
    Evidence_t evidence;
    CustodyEvent_t event;

    std::memset(&evidence, 0, sizeof(evidence));
    std::strcpy(evidence.caseId, caseId.c_str());
    evidence.category = EVIDENCE_CATEGORY_VEHICLE;
    std::strcpy(evidence.description, "Tyre impression");
    std::strcpy(evidence.collectionDate, "2026-01-14");
    ASSERT_EQ(evidenceManager_add(db, &evidence), CF_SUCCESS);

    std::memset(&event, 0, sizeof(event));
    event.action = CUSTODY_ACTION_COLLECTED;
    std::strcpy(event.timestamp, "not-a-date");
    std::strcpy(event.fromHandler, "Scene");
    std::strcpy(event.toHandler, "C. Naik");

    EXPECT_EQ(evidenceManager_addCustodyEvent(db, evidence.evidenceId, &event), CF_ERR_INVALID_DATE);
    EXPECT_EQ(db->evidence[0].custodyCount, 0);
}

TEST_F(RecordTest, CustodyEventReportsNotFoundForUnknownEvidence)
{
    std::string caseId = registerOneCase(db);
    CustodyEvent_t event;

    std::memset(&event, 0, sizeof(event));
    event.action = CUSTODY_ACTION_COLLECTED;
    std::strcpy(event.timestamp, "2026-01-14");
    std::strcpy(event.fromHandler, "Scene");
    std::strcpy(event.toHandler, "C. Naik");

    EXPECT_EQ(evidenceManager_addCustodyEvent(db, "EV-99999", &event), CF_ERR_NOT_FOUND);
}

TEST_F(RecordTest, CustodyEventRejectsEmptyHandlers)
{
    std::string caseId = registerOneCase(db);
    Evidence_t evidence;
    CustodyEvent_t event;

    std::memset(&evidence, 0, sizeof(evidence));
    std::strcpy(evidence.caseId, caseId.c_str());
    evidence.category = EVIDENCE_CATEGORY_VEHICLE;
    std::strcpy(evidence.description, "Tyre impression");
    std::strcpy(evidence.collectionDate, "2026-01-14");
    ASSERT_EQ(evidenceManager_add(db, &evidence), CF_SUCCESS);

    std::memset(&event, 0, sizeof(event));
    event.action = CUSTODY_ACTION_COLLECTED;
    std::strcpy(event.timestamp, "2026-01-14");
    std::strcpy(event.fromHandler, "");
    std::strcpy(event.toHandler, "C. Naik");

    EXPECT_EQ(evidenceManager_addCustodyEvent(db, evidence.evidenceId, &event), CF_ERR_EMPTY_INPUT);
}

TEST_F(RecordTest, CustodyLogReportsCapacityReachedAtLimit)
{
    std::string caseId = registerOneCase(db);
    Evidence_t evidence;
    CustodyEvent_t event;

    std::memset(&evidence, 0, sizeof(evidence));
    std::strcpy(evidence.caseId, caseId.c_str());
    evidence.category = EVIDENCE_CATEGORY_VEHICLE;
    std::strcpy(evidence.description, "Tyre impression");
    std::strcpy(evidence.collectionDate, "2026-01-14");
    ASSERT_EQ(evidenceManager_add(db, &evidence), CF_SUCCESS);

    std::memset(&event, 0, sizeof(event));
    event.action = CUSTODY_ACTION_TRANSFERRED;
    std::strcpy(event.timestamp, "2026-01-15");
    std::strcpy(event.fromHandler, "A");
    std::strcpy(event.toHandler, "B");

    for (int index = 0; index < MAX_CUSTODY_EVENTS; index++)
    {
        ASSERT_EQ(evidenceManager_addCustodyEvent(db, evidence.evidenceId, &event), CF_SUCCESS);
    }
    EXPECT_EQ(evidenceManager_addCustodyEvent(db, evidence.evidenceId, &event), CF_ERR_CAPACITY_FULL);
}

TEST_F(RecordTest, FilterByCategoryReturnsOnlyMatchingItems)
{
    std::string caseId = registerOneCase(db);
    Evidence_t biological;
    Evidence_t vehicle;
    Evidence_t filtered[ MAX_EVIDENCE ];
    size_t count = 0U;

    std::memset(&biological, 0, sizeof(biological));
    std::strcpy(biological.caseId, caseId.c_str());
    biological.category = EVIDENCE_CATEGORY_BIOLOGICAL;
    std::strcpy(biological.description, "Blood stained cloth");
    std::strcpy(biological.collectionDate, "2026-02-03");
    ASSERT_EQ(evidenceManager_add(db, &biological), CF_SUCCESS);

    std::memset(&vehicle, 0, sizeof(vehicle));
    std::strcpy(vehicle.caseId, caseId.c_str());
    vehicle.category = EVIDENCE_CATEGORY_VEHICLE;
    std::strcpy(vehicle.description, "Tyre impression");
    std::strcpy(vehicle.collectionDate, "2026-02-04");
    ASSERT_EQ(evidenceManager_add(db, &vehicle), CF_SUCCESS);

    ASSERT_EQ(evidenceManager_filterByCategory(db, EVIDENCE_CATEGORY_VEHICLE,
                                               filtered, MAX_EVIDENCE, &count), CF_SUCCESS);
    ASSERT_EQ(count, 1U);
    EXPECT_EQ(std::string(filtered[0].description), "Tyre impression");
}

TEST_F(RecordTest, UpdatePreservesAttributesAndCustodyHistory)
{
    std::string caseId = registerOneCase(db);
    Evidence_t evidence;
    Evidence_t updated;
    CustodyEvent_t event;

    std::memset(&evidence, 0, sizeof(evidence));
    std::strcpy(evidence.caseId, caseId.c_str());
    evidence.category = EVIDENCE_CATEGORY_PHYSICAL_TRACE;
    std::strcpy(evidence.description, "Fibre strand");
    std::strcpy(evidence.collectionDate, "2026-01-14");
    ASSERT_EQ(evidenceManager_add(db, &evidence), CF_SUCCESS);
    ASSERT_EQ(evidenceManager_addAttribute(db, evidence.evidenceId, ATTRIBUTE_HAIR, "black"), CF_SUCCESS);

    std::memset(&event, 0, sizeof(event));
    event.action = CUSTODY_ACTION_COLLECTED;
    std::strcpy(event.timestamp, "2026-01-14");
    std::strcpy(event.fromHandler, "Scene");
    std::strcpy(event.toHandler, "C. Naik");
    ASSERT_EQ(evidenceManager_addCustodyEvent(db, evidence.evidenceId, &event), CF_SUCCESS);

    updated = db->evidence[0];
    std::strcpy(updated.description, "Revised description");

    ASSERT_EQ(evidenceManager_update(db, evidence.evidenceId, &updated), CF_SUCCESS);
    EXPECT_EQ(std::string(db->evidence[0].description), "Revised description");
    EXPECT_EQ(db->evidence[0].attributeCount, 1);
    EXPECT_EQ(db->evidence[0].custodyCount, 1);
}

/* ------------------------------------------------------------------ */
/* Witness manager                                                     */
/* ------------------------------------------------------------------ */

TEST_F(RecordTest, AddWitnessRequiresAnExistingCase)
{
    Witness_t witness;

    std::memset(&witness, 0, sizeof(witness));
    std::strcpy(witness.caseId, "CF-2026-0001");
    std::strcpy(witness.fullName, "Orphan Witness");
    std::strcpy(witness.statement, "Saw something.");
    std::strcpy(witness.recordedDate, "2026-01-15");

    EXPECT_EQ(witnessManager_add(db, &witness), CF_ERR_NOT_FOUND);
    EXPECT_EQ(db->witnessCount, 0U);
}

TEST_F(RecordTest, AddWitnessGeneratesUniqueStatementReferences)
{
    std::string caseId = registerOneCase(db);
    Witness_t first;
    Witness_t second;

    std::memset(&first, 0, sizeof(first));
    std::strcpy(first.caseId, caseId.c_str());
    std::strcpy(first.fullName, "Meera Kulkarni");
    std::strcpy(first.statement, "Observed a person.");
    std::strcpy(first.recordedDate, "2026-01-15");
    ASSERT_EQ(witnessManager_add(db, &first), CF_SUCCESS);

    std::memset(&second, 0, sizeof(second));
    std::strcpy(second.caseId, caseId.c_str());
    std::strcpy(second.fullName, "Harish Bhat");
    std::strcpy(second.statement, "The gate was propped open.");
    std::strcpy(second.recordedDate, "2026-01-16");
    ASSERT_EQ(witnessManager_add(db, &second), CF_SUCCESS);

    EXPECT_EQ(std::string(first.statementRef).substr(0, 3), "ST-");
    EXPECT_NE(std::string(first.statementRef), std::string(second.statementRef));
    EXPECT_EQ(db->witnessCount, 2U);
}

TEST_F(RecordTest, AddWitnessRejectsInvalidRecordedDate)
{
    std::string caseId = registerOneCase(db);
    Witness_t witness;

    std::memset(&witness, 0, sizeof(witness));
    std::strcpy(witness.caseId, caseId.c_str());
    std::strcpy(witness.fullName, "Bad Date");
    std::strcpy(witness.statement, "Saw something.");
    std::strcpy(witness.recordedDate, "2026-13-45");

    EXPECT_EQ(witnessManager_add(db, &witness), CF_ERR_INVALID_DATE);
    EXPECT_EQ(db->witnessCount, 0U);
}

TEST_F(RecordTest, VerificationFlagCanBeSetAndCleared)
{
    std::string caseId = registerOneCase(db);
    Witness_t witness;

    std::memset(&witness, 0, sizeof(witness));
    std::strcpy(witness.caseId, caseId.c_str());
    std::strcpy(witness.fullName, "Anita Rodrigues");
    std::strcpy(witness.statement, "Heard an argument.");
    std::strcpy(witness.recordedDate, "2026-02-04");
    ASSERT_EQ(witnessManager_add(db, &witness), CF_SUCCESS);

    EXPECT_EQ(witnessManager_setVerified(db, witness.witnessId, 1), CF_SUCCESS);
    EXPECT_EQ(db->witnesses[0].verified, 1);
    EXPECT_EQ(witnessManager_countVerified(db, caseId.c_str()), 1U);

    EXPECT_EQ(witnessManager_setVerified(db, witness.witnessId, 0), CF_SUCCESS);
    EXPECT_EQ(db->witnesses[0].verified, 0);
    EXPECT_EQ(witnessManager_countVerified(db, caseId.c_str()), 0U);
}

TEST_F(RecordTest, UpdatePreservesIdentifierAndStatementReference)
{
    std::string caseId = registerOneCase(db);
    Witness_t witness;
    Witness_t updated;
    std::string originalId;
    std::string originalRef;

    std::memset(&witness, 0, sizeof(witness));
    std::strcpy(witness.caseId, caseId.c_str());
    std::strcpy(witness.fullName, "Suresh Iyer");
    std::strcpy(witness.statement, "The register was in the cabinet.");
    std::strcpy(witness.recordedDate, "2026-02-12");
    ASSERT_EQ(witnessManager_add(db, &witness), CF_SUCCESS);

    originalId = witness.witnessId;
    originalRef = witness.statementRef;

    updated = db->witnesses[0];
    std::strcpy(updated.fullName, "Suresh K. Iyer");

    ASSERT_EQ(witnessManager_update(db, originalId.c_str(), &updated), CF_SUCCESS);
    EXPECT_EQ(std::string(db->witnesses[0].witnessId), originalId);
    EXPECT_EQ(std::string(db->witnesses[0].statementRef), originalRef);
    EXPECT_EQ(std::string(db->witnesses[0].fullName), "Suresh K. Iyer");
}

} // namespace
