/*
 * casefile.h - Shared data model for CASEFILE.
 *
 * Holds the structures, enumerations, capacity constants and status codes that
 * every module of the application agrees on. Keeping them in one header avoids
 * duplication and guarantees that each module interprets the records the same way.
 *
 * Design rules applied throughout this project:
 *   - All record collections are fixed-capacity arrays; no dynamic allocation.
 *   - Every fallible function returns a CfStatus_t value.
 *   - All string fields are fixed-length character arrays.
 */

#ifndef CASEFILE_H
#define CASEFILE_H

#include <stddef.h>

/* ------------------------------------------------------------------ */
/* Capacity limits                                                     */
/* ------------------------------------------------------------------ */

#define MAX_CASES                 100
#define MAX_SUSPECTS              400
#define MAX_EVIDENCE              400
#define MAX_WITNESSES             300
#define MAX_EVIDENCE_ATTRIBUTES   6
#define MAX_CUSTODY_EVENTS        12
#define MAX_MATCH_FACTORS         12
#define MAX_RANKED_SUSPECTS       100

/* ------------------------------------------------------------------ */
/* String length limits                                                */
/* ------------------------------------------------------------------ */

#define ID_LENGTH                 16
#define TITLE_LENGTH              64
#define NAME_LENGTH               48
#define PHONE_LENGTH              16
#define ADDRESS_LENGTH            80
#define TEXT_LENGTH               128
#define LOCATION_LENGTH           48
#define OFFICER_LENGTH            48
#define HANDLER_LENGTH            24
#define ALIBI_LENGTH              80
#define MOTIVE_LENGTH             80
#define EVIDENCE_DESC_LENGTH      112
#define STATEMENT_LENGTH          192
#define CUSTODY_REMARK_LENGTH     64
#define ATTRIBUTE_VALUE_LENGTH    32
#define VEHICLE_LENGTH            16
#define MAX_VEHICLES              3

/* ------------------------------------------------------------------ */
/* Format constants                                                    */
/* ------------------------------------------------------------------ */

#define DATE_LENGTH               11          /* YYYY-MM-DD plus terminator */
#define FILE_LINE_LENGTH          512
#define FILE_PATH_LENGTH          256

/* ------------------------------------------------------------------ */
/* Scoring constants                                                   */
/* ------------------------------------------------------------------ */

#define WEIGHT_GENDER             15
#define WEIGHT_HEIGHT             20
#define WEIGHT_HAIR               10
#define WEIGHT_SHOE_SIZE          15
#define WEIGHT_BLOOD_GROUP        20
#define WEIGHT_VEHICLE            25
#define MAX_SUSPICION_SCORE       100

/* ------------------------------------------------------------------ */
/* Enumerations                                                        */
/* ------------------------------------------------------------------ */

/** Result of any operation that can fail. */
typedef enum
{
    CF_SUCCESS = 0,
    CF_ERR_NULL_POINTER,
    CF_ERR_INVALID_ARGUMENT,
    CF_ERR_INVALID_FORMAT,
    CF_ERR_INVALID_DATE,
    CF_ERR_INVALID_ENUM,
    CF_ERR_INVALID_LENGTH,
    CF_ERR_EMPTY_INPUT,
    CF_ERR_NOT_FOUND,
    CF_ERR_DUPLICATE_ID,
    CF_ERR_CAPACITY_FULL,
    CF_ERR_FILE_OPEN,
    CF_ERR_FILE_READ,
    CF_ERR_FILE_WRITE,
    CF_ERR_CORRUPT_FILE,
    CF_ERR_INSUFFICIENT_EVIDENCE,
    CF_ERR_INTERNAL
} CfStatus_t;

/** Investigation status of a crime case. */
typedef enum
{
    CASE_STATUS_OPEN = 0,
    CASE_STATUS_UNDER_INVESTIGATION,
    CASE_STATUS_EVIDENCE_ANALYSIS,
    CASE_STATUS_CLOSED,
    CASE_STATUS_ARCHIVED,
    CASE_STATUS_COUNT
} CaseStatus_t;

/** Priority assigned to a case. */
typedef enum
{
    CASE_PRIORITY_LOW = 0,
    CASE_PRIORITY_MEDIUM,
    CASE_PRIORITY_HIGH,
    CASE_PRIORITY_CRITICAL,
    CASE_PRIORITY_COUNT
} CasePriority_t;

/** Category of crime recorded on a case. */
typedef enum
{
    CRIME_TYPE_HOMICIDE = 0,
    CRIME_TYPE_THEFT,
    CRIME_TYPE_ASSAULT,
    CRIME_TYPE_FRAUD,
    CRIME_TYPE_VANDALISM,
    CRIME_TYPE_CYBERCRIME,
    CRIME_TYPE_TRAFFIC_FATALITY,
    CRIME_TYPE_ARSON,
    CRIME_TYPE_OTHER,
    CRIME_TYPE_COUNT
} CrimeType_t;

/** Standing of a suspect within a case. */
typedef enum
{
    SUSPECT_STATUS_UNDER_REVIEW = 0,
    SUSPECT_STATUS_PERSON_OF_INTEREST,
    SUSPECT_STATUS_PRIMARY,
    SUSPECT_STATUS_RULED_OUT,
    SUSPECT_STATUS_CONFIRMED,
    SUSPECT_STATUS_COUNT
} SuspectStatus_t;

/** Forensic category of an evidence item. */
typedef enum
{
    EVIDENCE_CATEGORY_BIOLOGICAL = 0,
    EVIDENCE_CATEGORY_CHEMICAL,
    EVIDENCE_CATEGORY_DIGITAL,
    EVIDENCE_CATEGORY_BALLISTIC,
    EVIDENCE_CATEGORY_PHYSICAL_TRACE,
    EVIDENCE_CATEGORY_DOCUMENTARY,
    EVIDENCE_CATEGORY_VEHICLE,
    EVIDENCE_CATEGORY_OTHER,
    EVIDENCE_CATEGORY_COUNT
} EvidenceCategory_t;

/** Examination status of an evidence item. */
typedef enum
{
    EVIDENCE_STATUS_COLLECTED = 0,
    EVIDENCE_STATUS_IN_TRANSIT,
    EVIDENCE_STATUS_UNDER_ANALYSIS,
    EVIDENCE_STATUS_ANALYSED,
    EVIDENCE_STATUS_REPORTED,
    EVIDENCE_STATUS_RELEASED,
    EVIDENCE_STATUS_COUNT
} EvidenceStatus_t;

/** Action recorded in a chain-of-custody event. */
typedef enum
{
    CUSTODY_ACTION_COLLECTED = 0,
    CUSTODY_ACTION_TRANSFERRED,
    CUSTODY_ACTION_SEALED,
    CUSTODY_ACTION_ANALYSED,
    CUSTODY_ACTION_RELEASED,
    CUSTODY_ACTION_COUNT
} CustodyAction_t;

/** Attribute key that can be attached to an evidence item and compared with a suspect. */
typedef enum
{
    ATTRIBUTE_GENDER = 0,
    ATTRIBUTE_HEIGHT,
    ATTRIBUTE_HAIR,
    ATTRIBUTE_SHOE_SIZE,
    ATTRIBUTE_BLOOD_GROUP,
    ATTRIBUTE_VEHICLE,
    ATTRIBUTE_COUNT
} AttributeKey_t;

/* ------------------------------------------------------------------ */
/* Structures                                                          */
/* ------------------------------------------------------------------ */

/** A registered crime case. */
typedef struct
{
    char caseId[ID_LENGTH];
    char title[TITLE_LENGTH];
    CrimeType_t crimeType;
    CasePriority_t priority;
    CaseStatus_t status;
    char incidentDate[DATE_LENGTH];
    char location[LOCATION_LENGTH];
    char assignedOfficer[OFFICER_LENGTH];
    char summary[TEXT_LENGTH];
} Case_t;

/** Physical descriptors of a suspect, used for evidence comparison. */
typedef struct
{
    char gender[NAME_LENGTH];
    int heightCm;                                  /* -1 when unknown */
    char hairColour[NAME_LENGTH];
    int shoeSize;                                  /* -1 when unknown */
    char bloodGroup[NAME_LENGTH];
    char vehicles[MAX_VEHICLES][VEHICLE_LENGTH];
    int vehicleCount;
} PhysicalProfile_t;

/** A suspect linked to a case. */
typedef struct
{
    char suspectId[ID_LENGTH];
    char caseId[ID_LENGTH];
    char fullName[NAME_LENGTH];
    int age;
    char phone[PHONE_LENGTH];
    char address[ADDRESS_LENGTH];
    char alibi[ALIBI_LENGTH];
    char motive[MOTIVE_LENGTH];
    SuspectStatus_t status;
    PhysicalProfile_t physical;
} Suspect_t;

/** A single analysis attribute attached to an evidence item. */
typedef struct
{
    AttributeKey_t key;
    char value[ATTRIBUTE_VALUE_LENGTH];
} Attribute_t;

/** One chain-of-custody event. */
typedef struct
{
    CustodyAction_t action;
    char timestamp[DATE_LENGTH];
    char fromHandler[HANDLER_LENGTH];
    char toHandler[HANDLER_LENGTH];
    char location[LOCATION_LENGTH];
    char remarks[CUSTODY_REMARK_LENGTH];
} CustodyEvent_t;

/** A forensic evidence item linked to a case. */
typedef struct
{
    char evidenceId[ID_LENGTH];
    char caseId[ID_LENGTH];
    EvidenceCategory_t category;
    char description[EVIDENCE_DESC_LENGTH];
    char collectionDate[DATE_LENGTH];
    char collectionLocation[LOCATION_LENGTH];
    char collectedBy[NAME_LENGTH];
    EvidenceStatus_t status;
    Attribute_t attributes[MAX_EVIDENCE_ATTRIBUTES];
    int attributeCount;
    CustodyEvent_t custody[MAX_CUSTODY_EVENTS];
    int custodyCount;
} Evidence_t;

/** A witness linked to a case. */
typedef struct
{
    char witnessId[ID_LENGTH];
    char caseId[ID_LENGTH];
    char statementRef[ID_LENGTH];
    char fullName[NAME_LENGTH];
    char phone[PHONE_LENGTH];
    char statement[STATEMENT_LENGTH];
    char recordedDate[DATE_LENGTH];
    int verified;
} Witness_t;

/** A single factor that contributed to (or was checked against) a suspect's score. */
typedef struct
{
    char evidenceId[ID_LENGTH];
    AttributeKey_t key;
    char evidenceValue[ATTRIBUTE_VALUE_LENGTH];
    char suspectValue[ATTRIBUTE_VALUE_LENGTH];
    int weight;
    int matched;
} MatchFactor_t;

/** Scored result for one suspect within one case. */
typedef struct
{
    char suspectId[ID_LENGTH];
    char suspectName[NAME_LENGTH];
    int score;
    int matchedCount;
    int checkedCount;
    MatchFactor_t factors[MAX_MATCH_FACTORS];
    int factorCount;
} SuspectScore_t;

/** Outcome of analysing a case against its suspects. */
typedef struct
{
    int evaluated;
    int comparableEvidence;
    SuspectScore_t suspects[MAX_RANKED_SUSPECTS];
    int suspectCount;
} AnalysisResult_t;

/** The complete in-memory database. */
typedef struct
{
    Case_t cases[MAX_CASES];
    size_t caseCount;
    Suspect_t suspects[MAX_SUSPECTS];
    size_t suspectCount;
    Evidence_t evidence[MAX_EVIDENCE];
    size_t evidenceCount;
    Witness_t witnesses[MAX_WITNESSES];
    size_t witnessCount;
} CasefileDb_t;

/* ------------------------------------------------------------------ */
/* Shared lookup helpers (implemented in casefile.c)                   */
/* ------------------------------------------------------------------ */

/*
 * The helpers below answer questions that several modules need to ask about
 * the database as a whole: does this identifier already exist? Each module
 * keeps its own record-specific lookup function, so these helpers cover only
 * the cross-cutting uniqueness checks and no lookup logic is duplicated.
 */

/**
 * Determine whether a case identifier already exists in the database.
 *
 * @param db     Database to search (must not be NULL).
 * @param caseId Identifier to test (must not be NULL).
 * @return 1 when the identifier exists, 0 otherwise.
 */
int casefileCaseIdExists(const CasefileDb_t *db, const char *caseId);

/**
 * Determine whether a suspect identifier already exists in the database.
 *
 * @param db        Database to search (must not be NULL).
 * @param suspectId Identifier to test (must not be NULL).
 * @return 1 when the identifier exists, 0 otherwise.
 */
int casefileSuspectIdExists(const CasefileDb_t *db, const char *suspectId);

/**
 * Determine whether an evidence identifier already exists in the database.
 *
 * @param db         Database to search (must not be NULL).
 * @param evidenceId Identifier to test (must not be NULL).
 * @return 1 when the identifier exists, 0 otherwise.
 */
int casefileEvidenceIdExists(const CasefileDb_t *db, const char *evidenceId);

/**
 * Determine whether a witness identifier already exists in the database.
 *
 * @param db        Database to search (must not be NULL).
 * @param witnessId Identifier to test (must not be NULL).
 * @return 1 when the identifier exists, 0 otherwise.
 */
int casefileWitnessIdExists(const CasefileDb_t *db, const char *witnessId);

/**
 * Determine whether a statement reference identifier is already in use.
 *
 * @param db           Database to search (must not be NULL).
 * @param statementRef Statement reference to test (must not be NULL).
 * @return 1 when the reference exists, 0 otherwise.
 */
int casefileStatementRefExists(const CasefileDb_t *db, const char *statementRef);

/**
 * Create an empty database.
 *
 * The database holds every record of the application and is large enough that
 * it cannot safely live on the stack, so it is allocated on the heap. The
 * caller owns the returned database and must release it with
 * casefile_destroyDatabase().
 *
 * @return Pointer to a new empty database, or NULL when the allocation fails.
 */
CasefileDb_t *casefile_createDatabase(void);

/**
 * Release a database created by casefile_createDatabase().
 *
 * @param db Database to release; passing NULL is safe and does nothing.
 */
void casefile_destroyDatabase(CasefileDb_t *db);

#endif /* CASEFILE_H */
