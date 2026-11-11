/*
 * test_analysis.cpp - Unit tests for the rule-based evidence analysis engine.
 *
 * The scoring engine is the heart of the investigation feature, so each rule is
 * tested on its own as well as through the full case analysis.
 */

extern "C" {
#include "case_manager.h"
#include "casefile.h"
#include "evidence_manager.h"
#include "investigation.h"
#include "suspect_manager.h"
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
 * The database and the analysis result are both allocated on the heap because
 * they are far too large to live on the stack.
 */
class AnalysisTest : public ::testing::Test
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
 * Create a case with one suspect and one evidence item carrying the given
 * attribute.
 *
 * EXPECT_EQ is used rather than ASSERT_EQ because this function returns a
 * value and the assertion macros expand to a return statement.
 *
 * @param db     Database to populate.
 * @param key    Attribute key to attach to the evidence.
 * @param value  Attribute value to attach.
 * @param evidenceIdOut Receives the identifier of the created evidence item.
 */
void prepareCase(CasefileDb_t *db, AttributeKey_t key, const char *value, std::string *evidenceIdOut)
{
    Case_t caseRecord;
    Suspect_t suspect;
    Evidence_t evidence;

    std::memset(&caseRecord, 0, sizeof(caseRecord));
    std::strcpy(caseRecord.title, "Analysis fixture");
    caseRecord.crimeType = CRIME_TYPE_THEFT;
    caseRecord.priority = CASE_PRIORITY_MEDIUM;
    caseRecord.status = CASE_STATUS_OPEN;
    std::strcpy(caseRecord.incidentDate, "2026-01-14");
    EXPECT_EQ(caseManager_addCase(db, &caseRecord), CF_SUCCESS);

    std::memset(&suspect, 0, sizeof(suspect));
    std::strcpy(suspect.caseId, caseRecord.caseId);
    std::strcpy(suspect.fullName, "Fixture Suspect");
    suspect.age = 34;
    EXPECT_EQ(suspectManager_add(db, &suspect), CF_SUCCESS);

    std::memset(&evidence, 0, sizeof(evidence));
    std::strcpy(evidence.caseId, caseRecord.caseId);
    evidence.category = EVIDENCE_CATEGORY_PHYSICAL_TRACE;
    std::strcpy(evidence.description, "Fixture evidence");
    std::strcpy(evidence.collectionDate, "2026-01-14");
    EXPECT_EQ(evidenceManager_add(db, &evidence), CF_SUCCESS);

    if (key != ATTRIBUTE_COUNT)
    {
        EXPECT_EQ(evidenceManager_addAttribute(db, evidence.evidenceId, key, value), CF_SUCCESS);
    }

    if (evidenceIdOut != nullptr)
    {
        *evidenceIdOut = std::string(evidence.evidenceId);
    }
}

/*
 * Attach a physical profile to the first suspect of the database.
 */
void setFirstSuspectProfile(CasefileDb_t *db, const PhysicalProfile_t *profile)
{
    ASSERT_GT(db->suspectCount, 0U);
    ASSERT_EQ(suspectManager_setPhysicalProfile(db, db->suspects[0].suspectId, profile), CF_SUCCESS);
}
/* ------------------------------------------------------------------ */
/* Individual comparison rules                                         */
/* ------------------------------------------------------------------ */

TEST(InvestigationCompareTest, GenderMatchesIgnoringCase)
{
    int matched = 0;

    EXPECT_EQ(investigation_compareAttribute(ATTRIBUTE_GENDER, "Male", "male", &matched), WEIGHT_GENDER);
    EXPECT_EQ(matched, 1);
}

TEST(InvestigationCompareTest, GenderMismatchContributesNothing)
{
    int matched = 0;

    EXPECT_EQ(investigation_compareAttribute(ATTRIBUTE_GENDER, "female", "male", &matched), 0);
    EXPECT_EQ(matched, 0);
}

TEST(InvestigationCompareTest, HairColourMatchesIgnoringCase)
{
    int matched = 0;

    EXPECT_EQ(investigation_compareAttribute(ATTRIBUTE_HAIR, "BLACK", "black", &matched), WEIGHT_HAIR);
    EXPECT_EQ(matched, 1);
}

TEST(InvestigationCompareTest, BloodGroupMatchesExactly)
{
    int matched = 0;

    EXPECT_EQ(investigation_compareAttribute(ATTRIBUTE_BLOOD_GROUP, "B+", "b+", &matched), WEIGHT_BLOOD_GROUP);
    EXPECT_EQ(matched, 1);
}

TEST(InvestigationCompareTest, HeightInsideBandMatches)
{
    int matched = 0;

    EXPECT_EQ(investigation_compareAttribute(ATTRIBUTE_HEIGHT, "175-185", "178", &matched), WEIGHT_HEIGHT);
    EXPECT_EQ(matched, 1);
}

TEST(InvestigationCompareTest, HeightAtLowerBandBoundaryMatches)
{
    int matched = 0;

    EXPECT_EQ(investigation_compareAttribute(ATTRIBUTE_HEIGHT, "175-185", "175", &matched), WEIGHT_HEIGHT);
    EXPECT_EQ(matched, 1);
}

TEST(InvestigationCompareTest, HeightAtUpperBandBoundaryMatches)
{
    int matched = 0;

    EXPECT_EQ(investigation_compareAttribute(ATTRIBUTE_HEIGHT, "175-185", "185", &matched), WEIGHT_HEIGHT);
    EXPECT_EQ(matched, 1);
}

TEST(InvestigationCompareTest, HeightJustOutsideBandDoesNotMatch)
{
    int matched = 0;

    EXPECT_EQ(investigation_compareAttribute(ATTRIBUTE_HEIGHT, "175-185", "186", &matched), 0);
    EXPECT_EQ(matched, 0);
}

TEST(InvestigationCompareTest, ShoeSizeInsideBandMatches)
{
    int matched = 0;

    EXPECT_EQ(investigation_compareAttribute(ATTRIBUTE_SHOE_SIZE, "41-43", "42", &matched), WEIGHT_SHOE_SIZE);
    EXPECT_EQ(matched, 1);
}

TEST(InvestigationCompareTest, ShoeSizeOutsideBandDoesNotMatch)
{
    int matched = 0;

    EXPECT_EQ(investigation_compareAttribute(ATTRIBUTE_SHOE_SIZE, "41-43", "39", &matched), 0);
    EXPECT_EQ(matched, 0);
}

TEST(InvestigationCompareTest, VehicleMatchesWhenIdentical)
{
    int matched = 0;

    EXPECT_EQ(investigation_compareAttribute(ATTRIBUTE_VEHICLE, "MH-12-AB-4471", "mh-12-ab-4471", &matched),
              WEIGHT_VEHICLE);
    EXPECT_EQ(matched, 1);
}

TEST(InvestigationCompareTest, NullValuesNeverMatch)
{
    int matched = 1;

    EXPECT_EQ(investigation_compareAttribute(ATTRIBUTE_GENDER, nullptr, "male", &matched), 0);
    EXPECT_EQ(matched, 0);

    matched = 1;
    EXPECT_EQ(investigation_compareAttribute(ATTRIBUTE_GENDER, "male", nullptr, &matched), 0);
    EXPECT_EQ(matched, 0);
}

TEST(InvestigationCompareTest, NullMatchedPointerIsHandledSafely)
{
    EXPECT_EQ(investigation_compareAttribute(ATTRIBUTE_GENDER, "male", "male", nullptr), 0);
}

/* ------------------------------------------------------------------ */
/* Weights                                                             */
/* ------------------------------------------------------------------ */

TEST(InvestigationWeightTest, EveryComparableKeyHasAPositiveWeight)
{
    for (int key = ATTRIBUTE_GENDER; key < ATTRIBUTE_COUNT; key++)
    {
        EXPECT_GT(investigation_weightForKey(static_cast<AttributeKey_t>(key)), 0);
    }
}

TEST(InvestigationWeightTest, UnknownKeyHasNoWeight)
{
    EXPECT_EQ(investigation_weightForKey(static_cast<AttributeKey_t>(99)), 0);
}

/* ------------------------------------------------------------------ */
/* Full case analysis                                                  */
/* ------------------------------------------------------------------ */

TEST_F(AnalysisTest, MatchingAttributeProducesExpectedScore)
{
    AnalysisResult_t result;
    PhysicalProfile_t profile;

    prepareCase(db, ATTRIBUTE_GENDER, "male", nullptr);

    std::memset(&profile, 0, sizeof(profile));
    std::strcpy(profile.gender, "male");
    profile.heightCm = -1;
    profile.shoeSize = -1;
    setFirstSuspectProfile(db, &profile);

    ASSERT_EQ(investigation_analyseCase(db, db->cases[0].caseId, &result), CF_SUCCESS);
    ASSERT_EQ(result.suspectCount, 1);
    EXPECT_EQ(result.suspects[0].score, WEIGHT_GENDER);
    EXPECT_EQ(result.suspects[0].matchedCount, 1);
    EXPECT_EQ(result.suspects[0].checkedCount, 1);
    EXPECT_EQ(result.comparableEvidence, 1);
}

TEST_F(AnalysisTest, NonMatchingAttributeProducesZeroScoreButIsStillCounted)
{
    AnalysisResult_t result;
    PhysicalProfile_t profile;

    prepareCase(db, ATTRIBUTE_GENDER, "female", nullptr);

    std::memset(&profile, 0, sizeof(profile));
    std::strcpy(profile.gender, "male");
    profile.heightCm = -1;
    profile.shoeSize = -1;
    setFirstSuspectProfile(db, &profile);

    ASSERT_EQ(investigation_analyseCase(db, db->cases[0].caseId, &result), CF_SUCCESS);
    EXPECT_EQ(result.suspects[0].score, 0);
    EXPECT_EQ(result.suspects[0].matchedCount, 0);
    EXPECT_EQ(result.suspects[0].checkedCount, 1);
}

TEST_F(AnalysisTest, MultipleMatchingAttributesAccumulate)
{
    AnalysisResult_t result;
    PhysicalProfile_t profile;
    std::string evidenceId;

    prepareCase(db, ATTRIBUTE_GENDER, "male", &evidenceId);
    ASSERT_EQ(evidenceManager_addAttribute(db, evidenceId.c_str(), ATTRIBUTE_HAIR, "black"), CF_SUCCESS);

    std::memset(&profile, 0, sizeof(profile));
    std::strcpy(profile.gender, "male");
    std::strcpy(profile.hairColour, "black");
    profile.heightCm = -1;
    profile.shoeSize = -1;
    setFirstSuspectProfile(db, &profile);

    ASSERT_EQ(investigation_analyseCase(db, db->cases[0].caseId, &result), CF_SUCCESS);
    EXPECT_EQ(result.suspects[0].score, WEIGHT_GENDER + WEIGHT_HAIR);
    EXPECT_EQ(result.suspects[0].matchedCount, 2);
}

TEST_F(AnalysisTest, ScoreIsCappedAtMaximum)
{
    AnalysisResult_t result;
    PhysicalProfile_t profile;
    std::string evidenceId;

    prepareCase(db, ATTRIBUTE_GENDER, "male", &evidenceId);

    /* Attach enough matching attributes to exceed the cap on their own. */
    ASSERT_EQ(evidenceManager_addAttribute(db, evidenceId.c_str(), ATTRIBUTE_HAIR, "black"), CF_SUCCESS);
    ASSERT_EQ(evidenceManager_addAttribute(db, evidenceId.c_str(), ATTRIBUTE_HEIGHT, "170-190"), CF_SUCCESS);
    ASSERT_EQ(evidenceManager_addAttribute(db, evidenceId.c_str(), ATTRIBUTE_SHOE_SIZE, "41-43"), CF_SUCCESS);
    ASSERT_EQ(evidenceManager_addAttribute(db, evidenceId.c_str(), ATTRIBUTE_BLOOD_GROUP, "O+"), CF_SUCCESS);
    ASSERT_EQ(evidenceManager_addAttribute(db, evidenceId.c_str(), ATTRIBUTE_VEHICLE, "MH-12-AB-4471"),
              CF_SUCCESS);

    std::memset(&profile, 0, sizeof(profile));
    std::strcpy(profile.gender, "male");
    std::strcpy(profile.hairColour, "black");
    profile.heightCm = 178;
    std::strcpy(profile.bloodGroup, "O+");
    profile.shoeSize = 42;
    std::strcpy(profile.vehicles[0], "MH-12-AB-4471");
    profile.vehicleCount = 1;
    setFirstSuspectProfile(db, &profile);

    ASSERT_EQ(investigation_analyseCase(db, db->cases[0].caseId, &result), CF_SUCCESS);
    EXPECT_EQ(result.suspects[0].score, MAX_SUSPICION_SCORE);
}

TEST_F(AnalysisTest, CaseWithNoComparableEvidenceReportsInsufficientEvidence)
{
    AnalysisResult_t result;

    prepareCase(db, ATTRIBUTE_COUNT, nullptr, nullptr);

    EXPECT_EQ(investigation_analyseCase(db, db->cases[0].caseId, &result), CF_ERR_INSUFFICIENT_EVIDENCE);
}

TEST_F(AnalysisTest, CaseWithNoSuspectsReportsInsufficientEvidence)
{
    AnalysisResult_t result;
    Case_t caseRecord;

    std::memset(&caseRecord, 0, sizeof(caseRecord));
    std::strcpy(caseRecord.title, "No suspects");
    caseRecord.crimeType = CRIME_TYPE_FRAUD;
    caseRecord.priority = CASE_PRIORITY_LOW;
    caseRecord.status = CASE_STATUS_OPEN;
    std::strcpy(caseRecord.incidentDate, "2026-02-11");
    ASSERT_EQ(caseManager_addCase(db, &caseRecord), CF_SUCCESS);

    EXPECT_EQ(investigation_analyseCase(db, caseRecord.caseId, &result), CF_ERR_INSUFFICIENT_EVIDENCE);
}

TEST_F(AnalysisTest, UnknownCaseReportsNotFound)
{
    AnalysisResult_t result;

    EXPECT_EQ(investigation_analyseCase(db, "CF-2026-9999", &result), CF_ERR_NOT_FOUND);
}

TEST_F(AnalysisTest, NullArgumentsAreRejected)
{
    AnalysisResult_t result;

    EXPECT_EQ(investigation_analyseCase(nullptr, "CF-2026-0001", &result), CF_ERR_NULL_POINTER);
}

/* ------------------------------------------------------------------ */
/* Ranking                                                             */
/* ------------------------------------------------------------------ */

TEST(InvestigationRankTest, SuspectsAreRankedByDescendingScore)
{
    SuspectScore_t scores[3];

    std::memset(scores, 0, sizeof(scores));
    std::strcpy(scores[0].suspectId, "SU-1");
    scores[0].score = 20;
    std::strcpy(scores[1].suspectId, "SU-2");
    scores[1].score = 60;
    std::strcpy(scores[2].suspectId, "SU-3");
    scores[2].score = 40;

    investigation_rankSuspects(scores, 3);

    EXPECT_EQ(std::string(scores[0].suspectId), "SU-2");
    EXPECT_EQ(std::string(scores[1].suspectId), "SU-3");
    EXPECT_EQ(std::string(scores[2].suspectId), "SU-1");
}

TEST(InvestigationRankTest, EqualScoresKeepTheirOriginalOrder)
{
    SuspectScore_t scores[3];

    std::memset(scores, 0, sizeof(scores));
    std::strcpy(scores[0].suspectId, "SU-A");
    scores[0].score = 30;
    std::strcpy(scores[1].suspectId, "SU-B");
    scores[1].score = 30;
    std::strcpy(scores[2].suspectId, "SU-C");
    scores[2].score = 30;

    investigation_rankSuspects(scores, 3);

    EXPECT_EQ(std::string(scores[0].suspectId), "SU-A");
    EXPECT_EQ(std::string(scores[1].suspectId), "SU-B");
    EXPECT_EQ(std::string(scores[2].suspectId), "SU-C");
}

TEST(InvestigationRankTest, SingleElementAndNullArrayAreHandledSafely)
{
    SuspectScore_t scores[1];

    std::memset(scores, 0, sizeof(scores));
    scores[0].score = 15;

    investigation_rankSuspects(scores, 1);
    investigation_rankSuspects(nullptr, 3);

    EXPECT_EQ(scores[0].score, 15);
}

/* ------------------------------------------------------------------ */
/* Progress computation                                                */
/* ------------------------------------------------------------------ */

TEST_F(AnalysisTest, EmptyDatabaseReportsNotFoundForUnknownCase)
{
    int percent = -1;

    EXPECT_EQ(investigation_computeProgress(db, "CF-2026-9999", &percent, nullptr, 0), CF_ERR_NOT_FOUND);
}

TEST_F(AnalysisTest, ProgressReflectsAnalysedEvidenceAndVerifiedStatements)
{
    int percent = -1;
    std::string caseId;
    Case_t caseRecord;
    Evidence_t evidence;
    Witness_t witness;
    Suspect_t suspect;

    std::memset(&caseRecord, 0, sizeof(caseRecord));
    std::strcpy(caseRecord.title, "Progress fixture");
    caseRecord.crimeType = CRIME_TYPE_ASSAULT;
    caseRecord.priority = CASE_PRIORITY_MEDIUM;
    caseRecord.status = CASE_STATUS_OPEN;
    std::strcpy(caseRecord.incidentDate, "2026-02-03");
    ASSERT_EQ(caseManager_addCase(db, &caseRecord), CF_SUCCESS);
    caseId = caseRecord.caseId;

    std::memset(&suspect, 0, sizeof(suspect));
    std::strcpy(suspect.caseId, caseId.c_str());
    std::strcpy(suspect.fullName, "Progress Suspect");
    suspect.age = 40;
    ASSERT_EQ(suspectManager_add(db, &suspect), CF_SUCCESS);

    std::memset(&evidence, 0, sizeof(evidence));
    std::strcpy(evidence.caseId, caseId.c_str());
    evidence.category = EVIDENCE_CATEGORY_BIOLOGICAL;
    std::strcpy(evidence.description, "Progress evidence");
    std::strcpy(evidence.collectionDate, "2026-02-03");
    evidence.status = EVIDENCE_STATUS_ANALYSED;
    ASSERT_EQ(evidenceManager_add(db, &evidence), CF_SUCCESS);

    std::memset(&witness, 0, sizeof(witness));
    std::strcpy(witness.caseId, caseId.c_str());
    std::strcpy(witness.fullName, "Progress Witness");
    std::strcpy(witness.statement, "A statement.");
    std::strcpy(witness.recordedDate, "2026-02-04");
    witness.verified = 1;
    ASSERT_EQ(witnessManager_add(db, &witness), CF_SUCCESS);

    ASSERT_EQ(investigation_computeProgress(db, caseId.c_str(), &percent, nullptr, 0), CF_SUCCESS);

    /* evidence 50/50 + suspects 25 + statements 25/25 = 100 */
    EXPECT_EQ(percent, 100);
}

TEST_F(AnalysisTest, MatchedFactorsRecordTheSuspectValueForTheExplanation)
{
    AnalysisResult_t result;
    PhysicalProfile_t profile;

    prepareCase(db, ATTRIBUTE_HAIR, "black", nullptr);

    std::memset(&profile, 0, sizeof(profile));
    std::strcpy(profile.hairColour, "black");
    profile.heightCm = -1;
    profile.shoeSize = -1;
    setFirstSuspectProfile(db, &profile);

    ASSERT_EQ(investigation_analyseCase(db, db->cases[0].caseId, &result), CF_SUCCESS);
    ASSERT_EQ(result.suspects[0].factorCount, 1);

    /*
     * Regression test. The suspect value was not copied into the recorded
     * factor for the textual attributes, so the explanation printed an empty
     * value beside the evidence value.
     */
    EXPECT_EQ(std::string(result.suspects[0].factors[0].suspectValue), "black");
    EXPECT_EQ(std::string(result.suspects[0].factors[0].evidenceValue), "black");
    EXPECT_EQ(result.suspects[0].factors[0].matched, 1);
}

TEST_F(AnalysisTest, NonMatchingFactorsStillRecordBothValues)
{
    AnalysisResult_t result;
    PhysicalProfile_t profile;

    prepareCase(db, ATTRIBUTE_HAIR, "brown", nullptr);

    std::memset(&profile, 0, sizeof(profile));
    std::strcpy(profile.hairColour, "black");
    profile.heightCm = -1;
    profile.shoeSize = -1;
    setFirstSuspectProfile(db, &profile);

    ASSERT_EQ(investigation_analyseCase(db, db->cases[0].caseId, &result), CF_SUCCESS);
    ASSERT_EQ(result.suspects[0].factorCount, 1);

    EXPECT_EQ(std::string(result.suspects[0].factors[0].suspectValue), "black");
    EXPECT_EQ(std::string(result.suspects[0].factors[0].evidenceValue), "brown");
    EXPECT_EQ(result.suspects[0].factors[0].matched, 0);
    EXPECT_EQ(result.suspects[0].factors[0].weight, 0);
}

TEST_F(AnalysisTest, ProgressForUnknownCaseReportsNotFound)
{
    int percent = -1;

    EXPECT_EQ(investigation_computeProgress(db, "CF-2026-9999", &percent, nullptr, 0), CF_ERR_NOT_FOUND);
}

/* ------------------------------------------------------------------ */
/* Disclaimer                                                          */
/* ------------------------------------------------------------------ */

TEST(InvestigationDisclaimerTest, DisclaimerIsPresentAndExplainsTheLimit)
{
    const char *disclaimer = investigation_getDisclaimer();

    ASSERT_NE(disclaimer, nullptr);
    EXPECT_NE(std::string(disclaimer).find("NOT"), std::string::npos);
    EXPECT_NE(std::string(disclaimer).find("guilt"), std::string::npos);
}

TEST(InvestigationScoreDescriptionTest, ScoreBandsAreDescribed)
{
    EXPECT_STRNE(investigation_describeScore(0), "");
    EXPECT_STRNE(investigation_describeScore(30), "");
    EXPECT_STRNE(investigation_describeScore(60), "");
    EXPECT_STRNE(investigation_describeScore(90), "");
}

} // namespace
