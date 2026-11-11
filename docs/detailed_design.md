# CASEFILE — Detailed Software Design

> **Document:** Phase 2 deliverable (Detailed Design)
> **Status:** Complete. This document describes the system exactly as it was implemented in Phase 3 and verified in
> Phase 4. Section numbers correspond to the official mini-project template sections 6–17; the results of the
> build and the test run are in `docs/mini_project_report.md` §19.

---

## 1. Software architecture

CASEFILE is organised as a **layered, modular console application** in the classic three-tier form, adapted so that
each tier maps onto a small number of clear C modules.

**Layer 1 — Presentation / Interaction (`src/main.c`)**
A menu-driven console interface. `main.c` initialises the application, loads persisted data, presents the
top-level menu, collects raw user input, hands it to the appropriate module, prints the module's result (including
its status message) and, on exit, saves the data and releases resources. It contains **no** business rules, no
record structures and no file handling — it only orchestrates.

**Layer 2 — Validation (`src/validation.c`)**
A reusable, side-effect-free service layer that answers questions about data: *is this string a valid date? is this
identifier well formed? is this enumeration value in range? how do I read a bounded line from stdin?* Every other
module calls into it rather than re-implementing checks, which removes duplicated validation logic and makes the
checks independently unit-testable.

**Layer 3 — Business Logic / Application Services**

| Module | Business area |
| --- | --- |
| `case_manager` | Case register: create, read, update, delete, filter, cascade rules. |
| `suspect_manager` | Suspect register and physical profile. |
| `evidence_manager` | Evidence register, analysis attributes, chain-of-custody log. |
| `witness_manager` | Witness register and statement references. |
| `investigation` | Rule-based scoring, ranking, factor explanation, progress computation. |
| `report_generator` | Composition of human-readable case and evidence reports, and report files. |

**Layer 4 — Data Management / Persistence (`src/storage.c`)**
Converts records to and from a **line-oriented text format** in the `data/` directory, and validates that format on
load. It is the only module that opens a `FILE *`.

**Shared data model (`include/casefile.h`, `src/casefile.c`)**
The structure and enumeration definitions, the capacity constants, the status codes and the cross-cutting identifier
lookup helpers live in one place, so the modules agree on the data representation without depending on each other's
internals.

**Dependency direction (strictly one way)**

```
main  →  manager modules  →  validation
main  →  investigation    →  manager modules (read-only) + validation
main  →  report_generator →  manager modules (read-only) + investigation (read-only)
main  →  storage          →  validation
```

Managers never call `storage`; the application orchestrates save and load. `report_generator` and `investigation`
read through the managers' public lookup functions and never mutate records. This keeps the graph acyclic, makes
every module independently testable, and lets the unit tests build a database in memory without touching disk.

**State handling.** The complete in-memory database is a single `CasefileDb_t` created by
`casefile_createDatabase()` and passed by pointer into every module function. There are **no mutable global
variables**, which is what makes the whole application testable from gTest without any global setup.

> **Implementation note.** The database is about 1.4 MB and the analysis result about 0.7 MB, so both are allocated
> on the heap. An earlier version placed them on the stack and also copied whole tables into local arrays; the
> 8 MB stack was exhausted and the program crashed with a segmentation fault. This was found with a memory-safety
> build, and the fix was to allocate the large objects and to stream the listings directly from the database.

---

## 2. Module design

### Module: Shared data model

| Field | Value |
| --- | --- |
| Source file | `src/casefile.c` |
| Header file | `include/casefile.h` |
| Purpose | Defines the record structures, the enumerations, the capacity constants and the status codes shared by every module, and answers the identifier-uniqueness questions that several modules need to ask. |
| Public functions | `casefile_createDatabase`, `casefile_destroyDatabase`, `casefileCaseIdExists`, `casefileSuspectIdExists`, `casefileEvidenceIdExists`, `casefileWitnessIdExists`, `casefileStatementRefExists` |
| Data used or owned | The `CasefileDb_t` aggregate itself. |
| Dependencies | None. |
| Possible errors | `CF_ERR_NULL_POINTER`, `CF_ERR_INVALID_ARGUMENT` |

### Module: Validation

| Field | Value |
| --- | --- |
| Source file | `src/validation.c` |
| Header file | `include/validation.h` |
| Purpose | Provides every reusable check that the rest of the application relies on. |
| Responsibilities | Bounded line reading; trimming; safe field copying; integer parsing; calendar-correct date checking; identifier, phone, age, height and shoe-size checks; conversion of every enumerated choice from and to its name; the status-to-message table. |
| Public functions | `validationReadLine`, `validationTrim`, `validationCopyField`, `validationIsNotEmpty`, `validationIsPrintable`, `validationCheckLength`, `validationCompareIgnoreCase`, `validationContainsIgnoreCase`, `validationParseInt`, `validationReadInt`, `validationIsValidDate`, `validationIsValidIdentifier`, `validationIsValidPhone`, `validationIsValidAge`, `validationIsValidHeight`, `validationIsValidShoeSize`, `validationParse*` (7 functions), `validation*Name` (7 functions), `validationStatusMessage` |
| Data used or owned | None; the module is stateless apart from standard input. |
| Dependencies | The shared data model. |
| Possible errors | `CF_ERR_NULL_POINTER`, `CF_ERR_INVALID_ARGUMENT`, `CF_ERR_INVALID_FORMAT`, `CF_ERR_EMPTY_INPUT`, `CF_ERR_INVALID_ENUM`, `CF_ERR_FILE_READ` |

### Module: Case manager

| Field | Value |
| --- | --- |
| Source file | `src/case_manager.c` |
| Header file | `include/case_manager.h` |
| Purpose | Owns the lifecycle of the crime case register. |
| Responsibilities | Generate a unique case identifier; register a case; look a case up; list and filter the register; update a case; change the investigation status; delete a case and cascade to its child records. |
| Public functions | `caseManager_addCase`, `caseManager_findById`, `caseManager_getCase`, `caseManager_listAll`, `caseManager_filterByStatus`, `caseManager_filterByCrimeType`, `caseManager_update`, `caseManager_setStatus`, `caseManager_delete`, `caseManager_countByStatus`, `caseManager_validateNewId`, `caseManager_generateId` |
| Data used or owned | The `cases` array and `caseCount` of the database. |
| Dependencies | The shared data model and the validation module. |
| Possible errors | `CF_ERR_NULL_POINTER`, `CF_ERR_INVALID_ARGUMENT`, `CF_ERR_EMPTY_INPUT`, `CF_ERR_INVALID_LENGTH`, `CF_ERR_INVALID_DATE`, `CF_ERR_INVALID_ENUM`, `CF_ERR_NOT_FOUND`, `CF_ERR_DUPLICATE_ID`, `CF_ERR_CAPACITY_FULL` |

### Module: Suspect manager

| Field | Value |
| --- | --- |
| Source file | `src/suspect_manager.c` |
| Header file | `include/suspect_manager.h` |
| Purpose | Owns the suspect register and the physical descriptor profile used for comparison. |
| Responsibilities | Generate a unique suspect identifier; add a suspect to an existing case; list the suspects of a case; search by partial name; update a suspect; delete a suspect; store and extend the physical profile. |
| Public functions | `suspectManager_add`, `suspectManager_findById`, `suspectManager_get`, `suspectManager_listByCase`, `suspectManager_searchByName`, `suspectManager_update`, `suspectManager_delete`, `suspectManager_setPhysicalProfile`, `suspectManager_addVehicle`, `suspectManager_generateId`, `suspectManager_countByCase` |
| Data used or owned | The `suspects` array and `suspectCount` of the database. |
| Dependencies | The shared data model, the validation module and the case manager (to confirm the parent case exists). |
| Possible errors | `CF_ERR_NULL_POINTER`, `CF_ERR_INVALID_ARGUMENT`, `CF_ERR_EMPTY_INPUT`, `CF_ERR_INVALID_LENGTH`, `CF_ERR_INVALID_ENUM`, `CF_ERR_NOT_FOUND`, `CF_ERR_CAPACITY_FULL` |

### Module: Evidence manager

| Field | Value |
| --- | --- |
| Source file | `src/evidence_manager.c` |
| Header file | `include/evidence_manager.h` |
| Purpose | Owns the forensic evidence register, the analysis attributes and the chain-of-custody history. |
| Responsibilities | Generate a unique evidence identifier; register evidence against an existing case; search and filter the register; update a record and its examination status; attach and replace analysis attributes; append and read the chain-of-custody log; delete a record. |
| Public functions | `evidenceManager_add`, `evidenceManager_findById`, `evidenceManager_get`, `evidenceManager_listByCase`, `evidenceManager_searchByDescription`, `evidenceManager_filterByCategory`, `evidenceManager_filterByStatus`, `evidenceManager_update`, `evidenceManager_setStatus`, `evidenceManager_addAttribute`, `evidenceManager_addCustodyEvent`, `evidenceManager_getCustodyHistory`, `evidenceManager_delete`, `evidenceManager_generateId`, `evidenceManager_countByCase`, `evidenceManager_countAnalysed` |
| Data used or owned | The `evidence` array and `evidenceCount` of the database. |
| Dependencies | The shared data model, the validation module and the case manager. |
| Possible errors | `CF_ERR_NULL_POINTER`, `CF_ERR_INVALID_ARGUMENT`, `CF_ERR_EMPTY_INPUT`, `CF_ERR_INVALID_LENGTH`, `CF_ERR_INVALID_DATE`, `CF_ERR_INVALID_ENUM`, `CF_ERR_NOT_FOUND`, `CF_ERR_CAPACITY_FULL` |

### Module: Witness manager

| Field | Value |
| --- | --- |
| Source file | `src/witness_manager.c` |
| Header file | `include/witness_manager.h` |
| Purpose | Owns the witness register and the statement reference identifiers. |
| Responsibilities | Generate a unique witness identifier and a unique statement reference; register a witness against an existing case; list and search witnesses; update a witness; set or clear the verification flag; delete a witness. |
| Public functions | `witnessManager_add`, `witnessManager_findById`, `witnessManager_get`, `witnessManager_listByCase`, `witnessManager_searchByName`, `witnessManager_update`, `witnessManager_setVerified`, `witnessManager_delete`, `witnessManager_generateId`, `witnessManager_generateStatementRef`, `witnessManager_countByCase`, `witnessManager_countVerified` |
| Data used or owned | The `witnesses` array and `witnessCount` of the database. |
| Dependencies | The shared data model, the validation module and the case manager. |
| Possible errors | `CF_ERR_NULL_POINTER`, `CF_ERR_INVALID_ARGUMENT`, `CF_ERR_EMPTY_INPUT`, `CF_ERR_INVALID_LENGTH`, `CF_ERR_INVALID_DATE`, `CF_ERR_INVALID_ENUM`, `CF_ERR_NOT_FOUND`, `CF_ERR_CAPACITY_FULL` |

### Module: Investigation

| Field | Value |
| --- | --- |
| Source file | `src/investigation.c` |
| Header file | `include/investigation.h` |
| Purpose | Compares the recorded evidence attributes with the suspect profiles and turns the comparison into a transparent, explainable score. |
| Responsibilities | Apply each comparison rule in isolation; score every suspect of a case; record every factor that was checked; rank the suspects by descending score; detect insufficient evidence; compute the investigation progress; supply the educational disclaimer. |
| Public functions | `investigation_analyseCase`, `investigation_compareAttribute`, `investigation_weightForKey`, `investigation_rankSuspects`, `investigation_computeProgress`, `investigation_getDisclaimer`, `investigation_describeScore` |
| Data used or owned | The `AnalysisResult_t` produced for one case. |
| Dependencies | The shared data model, the validation module and the three managers (read-only). |
| Possible errors | `CF_ERR_NULL_POINTER`, `CF_ERR_INVALID_ARGUMENT`, `CF_ERR_NOT_FOUND`, `CF_ERR_INSUFFICIENT_EVIDENCE` |

### Module: Report generator

| Field | Value |
| --- | --- |
| Source file | `src/report_generator.c` |
| Header file | `include/report_generator.h` |
| Purpose | Composes the human-readable investigation and evidence reports and writes them to a file. |
| Responsibilities | Build the case summary report; build the evidence summary report; write a report to the `reports/` directory. |
| Public functions | `report_generateCaseSummary`, `report_generateEvidenceSummary`, `report_saveToFile` |
| Data used or owned | None; reports are written into a caller-supplied buffer. |
| Dependencies | The shared data model, the validation module, the managers (read-only) and the investigation module. |
| Possible errors | `CF_ERR_NULL_POINTER`, `CF_ERR_INVALID_ARGUMENT`, `CF_ERR_NOT_FOUND`, `CF_ERR_FILE_WRITE` |

### Module: Storage

| Field | Value |
| --- | --- |
| Source file | `src/storage.c` |
| Header file | `include/storage.h` |
| Purpose | Converts the records to and from the text files, and validates that the files are in the expected format. |
| Responsibilities | Save and load every record type; write and verify the format signature; detect a missing, empty or foreign file; sanitise a value so that it cannot break the record layout; check every read and write. |
| Public functions | `storage_loadAll`, `storage_saveAll`, `storage_validateFormat`, `storage_buildPath`, `storage_fileNameByIndex`, `storage_ensureDataDirectory`, `storage_writeLine`, `storage_sanitizeField` |
| Data used or owned | The five files in the `data/` directory. |
| Dependencies | The shared data model and the validation module. |
| Possible errors | `CF_ERR_NULL_POINTER`, `CF_ERR_INVALID_ARGUMENT`, `CF_ERR_FILE_OPEN`, `CF_ERR_FILE_READ`, `CF_ERR_FILE_WRITE`, `CF_ERR_CORRUPT_FILE`, `CF_ERR_CAPACITY_FULL` |

### Module: Main application

| Field | Value |
| --- | --- |
| Source file | `src/main.c` |
| Header file | Not applicable |
| Purpose | Controls the high-level application flow. |
| Responsibilities | Initialise and release the database; load the stored data; present the hierarchical menu; collect and validate the input; dispatch each choice to the module that owns it; print the returned status and the result; save the data on exit. |
| Public functions | `main` only; every other function is `static`. |
| Data used or owned | The `CasefileDb_t` pointer. |
| Dependencies | Every other module. |
| Possible errors | Handles every status the modules return. |

---

## 3. Data design

### 3.1 Structures

| Structure | Purpose | Approximate size |
| --- | --- | --- |
| `Case_t` | One registered crime case. | 344 bytes |
| `Suspect_t` | One suspect linked to a case, including the physical profile. | 548 bytes |
| `PhysicalProfile_t` | The descriptors compared against evidence attributes. | 200 bytes |
| `Evidence_t` | One evidence item with its attributes and its custody log. | 2 736 bytes |
| `Attribute_t` | One analysis attribute attached to an evidence item. | 36 bytes |
| `CustodyEvent_t` | One chain-of-custody event. | 168 bytes |
| `Witness_t` | One witness linked to a case. | 424 bytes |
| `MatchFactor_t` | One attribute that was checked against a suspect. | 88 bytes |
| `SuspectScore_t` | The score and the factors of one suspect. | 1 100 bytes |
| `AnalysisResult_t` | The scores and factors of every suspect of one case. | 111 316 bytes |
| `CasefileDb_t` | Every record of the application. | 1 396 616 bytes |

The full definitions are in `include/casefile.h`.

### 3.2 Enumerations

| Enumeration | Constants | Purpose |
| --- | --- | --- |
| `CfStatus_t` | 17 values from `CF_SUCCESS` to `CF_ERR_INTERNAL` | The single error vocabulary returned across every module boundary. |
| `CaseStatus_t` | `open`, `under_investigation`, `evidence_analysis`, `closed`, `archived` | The investigation status of a case. |
| `CasePriority_t` | `low`, `medium`, `high`, `critical` | The priority of a case. |
| `CrimeType_t` | `homicide`, `theft`, `assault`, `fraud`, `vandalism`, `cybercrime`, `traffic_fatality`, `arson`, `other` | The category of crime. |
| `SuspectStatus_t` | `under_review`, `person_of_interest`, `primary`, `ruled_out`, `confirmed` | The standing of a suspect. |
| `EvidenceCategory_t` | `biological`, `chemical`, `digital`, `ballistic`, `physical_trace`, `documentary`, `vehicle`, `other` | The forensic category of an item. |
| `EvidenceStatus_t` | `collected`, `in_transit`, `under_analysis`, `analysed`, `reported`, `released` | The examination status of an item. |
| `CustodyAction_t` | `collected`, `transferred`, `sealed`, `analysed`, `released` | The action recorded in a custody event. |
| `AttributeKey_t` | `gender`, `height`, `hair`, `shoe_size`, `blood_group`, `vehicle` | The attributes that can be compared with a suspect profile. |

### 3.3 Constants

| Constant | Value | Purpose |
| --- | --- | --- |
| `MAX_CASES` | 100 | Maximum number of cases. |
| `MAX_SUSPECTS` | 400 | Maximum number of suspects. |
| `MAX_EVIDENCE` | 400 | Maximum number of evidence items. |
| `MAX_WITNESSES` | 300 | Maximum number of witnesses. |
| `MAX_EVIDENCE_ATTRIBUTES` | 6 | Maximum number of attributes per evidence item. |
| `MAX_CUSTODY_EVENTS` | 12 | Maximum number of custody events per item. |
| `MAX_MATCH_FACTORS` | 12 | Maximum number of factors recorded per suspect. |
| `MAX_RANKED_SUSPECTS` | 100 | Maximum number of suspects ranked for one case. |
| `ID_LENGTH` | 16 | Size of every identifier field. |
| `TITLE_LENGTH` | 64 | Size of a case title. |
| `NAME_LENGTH` | 48 | Size of a name field. |
| `HANDLER_LENGTH` | 24 | Size of a custody handler field. |
| `ATTRIBUTE_VALUE_LENGTH` | 32 | Size of an attribute value. |
| `DATE_LENGTH` | 11 | Size of a date field, `YYYY-MM-DD` plus terminator. |
| `WEIGHT_GENDER` … `WEIGHT_VEHICLE` | 15, 20, 10, 15, 20, 25 | The weight of each comparison rule. |
| `MAX_SUSPICION_SCORE` | 100 | The cap applied to a score. |
| `STORAGE_DATA_DIRECTORY` | `data` | The directory holding the data files. |
| `STORAGE_FILE_SIGNATURE` | `CASEFILE-DATA-V1` | The signature written at the top of every data file. |
| `STORAGE_FIELD_SEPARATOR` | `\|` | The field separator of the stored records. |
| `REPORT_OUTPUT_DIRECTORY` | `reports` | The directory holding the generated reports. |

### 3.4 Data dictionary

| Data item | C type | Valid range or size | Validation rule | Description |
| --- | --- | --- | --- | --- |
| Case identifier | `char[16]` | `CF-YYYY-NNNN` | Uppercase letters, digits and hyphens; must begin with a letter; not already in use | The unique reference of a case. |
| Case title | `char[64]` | 1–63 characters | Must not be empty; printable | The short name of the case. |
| Crime type | `CrimeType_t` | 0–8 | Must be one of the nine defined values | The category of crime. |
| Priority | `CasePriority_t` | 0–3 | Must be one of the four defined values | How urgently the case is worked. |
| Investigation status | `CaseStatus_t` | 0–4 | Must be one of the five defined values | Where the investigation has reached. |
| Incident date | `char[11]` | `YYYY-MM-DD` | Calendar-correct, including leap years; year 1900–2200 | When the incident happened. |
| Location | `char[48]` | 0–47 characters | Printable | Where the incident happened. |
| Assigned officer | `char[48]` | 0–47 characters | Printable | Who owns the case. |
| Summary | `char[128]` | 0–127 characters | Printable | The narrative summary of the case. |
| Suspect identifier | `char[16]` | `SU-NNNNN` | Generated; must not already be in use | The unique reference of a suspect. |
| Suspect age | `int` | 1–120 | Must be within the range | The age of the suspect. |
| Phone number | `char[16]` | 7–15 digits, optional `+` | Digits only after an optional `+` | How to reach the suspect. |
| Alibi | `char[80]` | 0–79 characters | Printable | The stated alibi of the suspect. |
| Motive | `char[80]` | 0–79 characters | Printable | The possible motive. |
| Suspect status | `SuspectStatus_t` | 0–4 | Must be one of the five defined values | The standing of the suspect. |
| Gender | `char[48]` | 0–47 characters | Printable; may be left unknown | Compared with the `gender` attribute. |
| Height | `int` | 100–250 cm, or −1 | Must be within the range when known | Compared with the `height` band. |
| Hair colour | `char[48]` | 0–47 characters | Printable; may be left unknown | Compared with the `hair` attribute. |
| Shoe size | `int` | 30–52, or −1 | Must be within the range when known | Compared with the `shoe_size` band. |
| Blood group | `char[48]` | 0–47 characters | Printable; may be left unknown | Compared with the `blood_group` attribute. |
| Vehicle registrations | `char[3][16]` | Up to 3 entries | Printable; the list may not exceed the capacity | Compared with the `vehicle` attribute. |
| Evidence identifier | `char[16]` | `EV-NNNNN` | Generated; must not already be in use | The unique reference of an evidence item. |
| Evidence category | `EvidenceCategory_t` | 0–7 | Must be one of the eight defined values | The forensic category of the item. |
| Evidence description | `char[112]` | 1–111 characters | Must not be empty | What the item is. |
| Collection date | `char[11]` | `YYYY-MM-DD` | Calendar-correct | When the item was collected. |
| Collection location | `char[48]` | 0–47 characters | Printable | Where the item was collected. |
| Collected by | `char[48]` | 0–47 characters | Printable | Who collected the item. |
| Examination status | `EvidenceStatus_t` | 0–5 | Must be one of the six defined values | How far the examination has progressed. |
| Analysis attribute | `Attribute_t` | Up to 6 per item | The key must be one of the six comparable keys; the value must not be empty | A recorded characteristic of the item. |
| Custody event | `CustodyEvent_t` | Up to 12 per item | The action must be defined; the date valid; both handlers non-empty | One movement of the item. |
| Witness identifier | `char[16]` | `WI-NNNNN` | Generated; must not already be in use | The unique reference of a witness. |
| Statement reference | `char[16]` | `ST-NNNNN` | Generated; must not already be in use | The reference of the statement. |
| Statement text | `char[192]` | 1–191 characters | Must not be empty | What the witness said. |
| Recorded date | `char[11]` | `YYYY-MM-DD` | Calendar-correct | When the statement was taken. |
| Verified flag | `int` | 0 or 1 | Set to exactly 0 or 1 | Whether the statement has been corroborated. |
| Suspicion score | `int` | 0–100 | The sum of the matched weights, capped | The decision-support indicator. |

---

## 4. Function design

Every fallible function follows the same contract: it returns `CfStatus_t`, it validates its arguments before it
touches any state, and it changes nothing when it returns an error.

| Function | Module | Purpose | Returns |
| --- | --- | --- | --- |
| `casefile_createDatabase` | data model | Create an empty database on the heap. | The database, or `NULL` on allocation failure. |
| `casefile_destroyDatabase` | data model | Release the database. | Nothing. |
| `casefile*IdExists` | data model | Test whether an identifier is already in use. | 1 when it exists, 0 otherwise. |
| `validationReadLine` | validation | Read one bounded line from standard input. | `CF_SUCCESS` or `CF_ERR_FILE_READ`. |
| `validationTrim` | validation | Remove the surrounding whitespace. | Nothing. |
| `validationCopyField` | validation | Copy a value into a fixed-size field safely. | Nothing. |
| `validationParseInt` | validation | Parse a whole string as an integer. | `CF_SUCCESS` or `CF_ERR_INVALID_FORMAT`. |
| `validationReadInt` | validation | Read an integer within a range, re-prompting. | `CF_SUCCESS` or `CF_ERR_INVALID_FORMAT`. |
| `validationIsValidDate` | validation | Test a date for calendar correctness. | 1 when valid, 0 otherwise. |
| `validationIsValidIdentifier` | validation | Test the format of an identifier. | 1 when valid, 0 otherwise. |
| `validationParse*` | validation | Convert an enumerated name to its value. | `CF_SUCCESS`, `CF_ERR_EMPTY_INPUT` or `CF_ERR_INVALID_ENUM`. |
| `validationStatusMessage` | validation | Describe a status code. | A message that is never empty. |
| `caseManager_addCase` | case manager | Register a case with a generated identifier. | `CF_SUCCESS` or a specific error. |
| `caseManager_findById` | case manager | Find the index of a case. | The index, or −1 when not found. |
| `caseManager_getCase` | case manager | Copy a case out of the database. | `CF_SUCCESS` or `CF_ERR_NOT_FOUND`. |
| `caseManager_filterBy*` | case manager | Collect the cases matching a value. | `CF_SUCCESS` or `CF_ERR_INVALID_ENUM`. |
| `caseManager_update` | case manager | Change the editable fields of a case. | `CF_SUCCESS` or a specific error. |
| `caseManager_setStatus` | case manager | Change the investigation status. | `CF_SUCCESS`, `CF_ERR_INVALID_ENUM` or `CF_ERR_NOT_FOUND`. |
| `caseManager_delete` | case manager | Delete a case and its child records. | `CF_SUCCESS` or `CF_ERR_NOT_FOUND`. |
| `suspectManager_add` | suspect manager | Add a suspect to an existing case. | `CF_SUCCESS` or a specific error. |
| `suspectManager_searchByName` | suspect manager | Find the suspects whose name contains the text. | `CF_SUCCESS` with the count. |
| `suspectManager_setPhysicalProfile` | suspect manager | Store the descriptors used for comparison. | `CF_SUCCESS` or a specific error. |
| `suspectManager_addVehicle` | suspect manager | Add a known vehicle registration. | `CF_SUCCESS` or `CF_ERR_CAPACITY_FULL`. |
| `evidenceManager_add` | evidence manager | Register evidence against an existing case. | `CF_SUCCESS` or a specific error. |
| `evidenceManager_addAttribute` | evidence manager | Attach or replace an analysis attribute. | `CF_SUCCESS` or a specific error. |
| `evidenceManager_addCustodyEvent` | evidence manager | Append a custody event to the log. | `CF_SUCCESS` or a specific error. |
| `evidenceManager_getCustodyHistory` | evidence manager | Read the custody log of an item. | `CF_SUCCESS` with the count. |
| `evidenceManager_update` | evidence manager | Change the editable fields, keeping the attributes and the custody log. | `CF_SUCCESS` or a specific error. |
| `witnessManager_add` | witness manager | Register a witness with a generated statement reference. | `CF_SUCCESS` or a specific error. |
| `witnessManager_setVerified` | witness manager | Set or clear the verification flag. | `CF_SUCCESS` or `CF_ERR_NOT_FOUND`. |
| `investigation_compareAttribute` | investigation | Apply one comparison rule in isolation. | The weight of a match, or 0. |
| `investigation_analyseCase` | investigation | Score and rank the suspects of a case. | `CF_SUCCESS`, `CF_ERR_NOT_FOUND` or `CF_ERR_INSUFFICIENT_EVIDENCE`. |
| `investigation_rankSuspects` | investigation | Sort the scores by descending score, stably. | Nothing. |
| `investigation_computeProgress` | investigation | Derive the progress of a case from the real record counts. | `CF_SUCCESS` or `CF_ERR_NOT_FOUND`. |
| `report_generateCaseSummary` | report generator | Build the investigation summary. | `CF_SUCCESS` or a specific error. |
| `report_generateEvidenceSummary` | report generator | Build the evidence summary. | `CF_SUCCESS` or a specific error. |
| `report_saveToFile` | report generator | Write a report to the `reports/` directory. | `CF_SUCCESS` or `CF_ERR_FILE_WRITE`. |
| `storage_saveAll` | storage | Write every record to the data files. | `CF_SUCCESS` or a file error. |
| `storage_loadAll` | storage | Read every record from the data files. | `CF_SUCCESS`, `CF_ERR_CORRUPT_FILE` or a file error. |
| `storage_validateFormat` | storage | Check that a file carries the expected signature. | `CF_SUCCESS`, `CF_ERR_FILE_OPEN` or `CF_ERR_CORRUPT_FILE`. |

---

## 5. Input-validation plan

| Input | Source | Validation rule | Invalid example | Expected error behaviour |
| --- | --- | --- | --- | --- |
| Menu selection | User | An integer within the range of the menu | `99`, `abc`, empty | `! Value 99 is out of range...` then the menu is shown again |
| Case identifier | User | Uppercase letters, digits and hyphens; begins with a letter; at most 15 characters | `cf-2026-1`, `1CF`, empty | `! 'cf-2026-1' is not a valid identifier...` |
| Title | User | Not empty; at most 63 characters | empty | `! A value is required. Input cannot be left empty.` |
| Incident date | User | `YYYY-MM-DD`, calendar-correct including leap years | `2026-02-30`, `2026-13-01`, `2026/01/14` | `! The date is not valid. Use the YYYY-MM-DD format.` |
| Crime type | User | One of the nine defined names, ignoring case | `piracy` | `! The value entered is not one of the permitted choices.` |
| Priority | User | One of the four defined names | `urgent` | The same message as above |
| Investigation status | User | One of the five defined names | `solved` | The same message as above |
| Age | User | An integer from 1 to 120 | `0`, `500`, `thirty` | `! Value 0 is out of range...` or `! 'thirty' is not a valid whole number.` |
| Phone | User | 7 to 15 digits after an optional `+` | `12345`, `98200ABCDE` | Rejected with the enum/format message |
| Height | User | 100 to 250 cm, or left empty | `20`, `tall` | `! 'tall' is not a valid whole number.` |
| Shoe size | User | 30 to 52, or left empty | `5` | `! Value 5 is out of range...` |
| Evidence category | User | One of the eight defined names | `unknown` | `! The value entered is not one of the permitted choices.` |
| Attribute key | User | One of the six comparable keys | `favourite_colour` | The same message as above |
| Attribute value | User | Not empty; at most 31 characters | empty | `! A value is required...` |
| Custody action | User | One of the five defined names | `lost` | The enum message |
| Custody handlers | User | Both not empty | empty | `! A value is required...` |
| Statement text | User | Not empty; at most 191 characters | empty | `! A value is required...` |
| Confirmation | User | `y`, `yes`, `n` or `no`, ignoring case | `maybe` | Treated as "no"; the deletion is cancelled |
| A record that does not exist | User | The identifier must be in the register | `CF-2026-9999` | `! No matching record was found.` |
| A duplicate identifier | Generated | The sequence never produces a value already in use | — | `! That identifier is already in use.` |
| A full record table | Internal | The count must be below the capacity | — | `! The maximum number of records has been reached.` |
| A null pointer argument | Internal | Every pointer argument is checked | — | `CF_ERR_NULL_POINTER` |
| A file with a foreign signature | File | The first line must be `CASEFILE-DATA-V1` | `SOMETHING-ELSE-V9` | `! The data file contains content in an unrecognised format.` |
| An empty file | File | The signature line must be present | empty | The same corrupt-file message |
| A missing file | File | Not an error; the application starts empty | — | `No stored data was found. Starting with an empty case file.` |
| A file that cannot be opened | File | `fopen` must succeed | unreadable permissions | `! The data file could not be opened.` |

**Common strategy.** Every value read from the user passes through `validationReadLine`, which reads into a bounded
buffer, discards any over-long remainder and strips the newline. It is then trimmed and passed to a specific check.
No unsafe input function is used anywhere: `gets` is never called and `scanf` is never used for text.

---

## 6. File-handling plan

### 6.1 Files

| File | Purpose | Format | Created by | Read by | Updated by |
| --- | --- | --- | --- | --- | --- |
| `data/cases.txt` | The case register | Text, `\|` separated | `storage_saveAll` | `storage_loadAll` | `storage_saveAll` |
| `data/suspects.txt` | The suspect register | Text, `\|` separated | `storage_saveAll` | `storage_loadAll` | `storage_saveAll` |
| `data/evidence.txt` | The evidence register with its attributes | Text, `\|` separated | `storage_saveAll` | `storage_loadAll` | `storage_saveAll` |
| `data/custody.txt` | The chain-of-custody log of every item | Text, `\|` separated | `storage_saveAll` | `storage_loadAll` | `storage_saveAll` |
| `data/witnesses.txt` | The witness register | Text, `\|` separated | `storage_saveAll` | `storage_loadAll` | `storage_saveAll` |
| `reports/<case>_<kind>.txt` | A generated report | Text | `report_saveToFile` | Read by the user | `report_saveToFile` |

### 6.2 Record format

Every file begins with the signature line `CASEFILE-DATA-V1`. Each following line is one record whose fields are
separated by `|`.

```
cases.txt      CF-2026-0001|Warehouse Break-in|1|2|0|2026-01-14|Sector 9|Insp. Kadam|Summary
suspects.txt   SU-00001|CF-2026-0001|Ravi Deshmukh|34|9820041155|Addr|Alibi|Motive|0|male|178|black|42|O+|vehicles
evidence.txt   EV-00001|CF-2026-0001|4|Fibre strand|2026-01-14|Sector 9|C. Naik|0|1|2=black;4=170-185
custody.txt    EV-00001|1|2026-01-15|C. Naik|Evidence Store|Locker L-14|Remarks
witnesses.txt  WI-00001|CF-2026-0001|ST-00001|Meera Kulkarni|9820041177|Statement text|2026-01-16|1
```

A field that would contain the separator or a newline has it replaced with a space by `storage_sanitizeField` before
the record is written, so a record can never span more than one line.

### 6.3 File operations

| Operation | Function | C library function | Failure handling |
| --- | --- | --- | --- |
| Create or open for writing | `openForWrite` | `fopen(path, "w")` | `CF_ERR_FILE_OPEN`; the signature line is checked with `fputs`. |
| Read | `openForRead` | `fopen(path, "r")` | A missing file sets the "missing" flag and is not an error; a foreign signature gives `CF_ERR_CORRUPT_FILE`. |
| Read a record | `loadCases` and the other loaders | `fgets` | The line is checked; a record with too few fields gives `CF_ERR_CORRUPT_FILE`. |
| Write a record | `saveCases` and the other savers | `fprintf` / `fputs` | The return value is checked; a failure gives `CF_ERR_FILE_WRITE`. |
| Update | `storage_saveAll` | `fopen(path, "w")` rewrites the whole file | The whole file is rewritten, so the file is never left half updated. |
| Close | every writer | `fclose` | The return value of `fclose` is checked in `report_saveToFile`. |

Every `fopen`, `fgets`, `fprintf`, `fputs` and `fclose` call in the project has its return value checked.

### 6.4 File error conditions

| Error condition | Detection method | Expected behaviour |
| --- | --- | --- |
| File does not exist | `fopen` returns `NULL` | The application starts with an empty case file and says so. |
| File cannot be opened | `fopen` returns `NULL` for another reason | `CF_ERR_FILE_OPEN`; the user is told which file could not be read. |
| File is empty | `fgets` returns `NULL` on the first line | `CF_ERR_CORRUPT_FILE`; the file is not loaded. |
| File contains invalid data | The signature does not match, or a record has too few fields, or a date or enumeration value is out of range | `CF_ERR_CORRUPT_FILE`; the file is not loaded and the application continues. |
| Read or write fails | The return value of `fgets`, `fputs` or `fclose` | `CF_ERR_FILE_READ` or `CF_ERR_FILE_WRITE`; the application reports the failure and does not claim success. |

---

## 7. Error-handling strategy

### 7.1 Status representation

```c
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
```

### 7.2 Error catalogue

| Error ID | Error condition | Detection location | Returned status | User-facing behaviour |
| --- | --- | --- | --- | --- |
| ERR-01 | A required pointer argument is `NULL` | The first line of the function | `CF_ERR_NULL_POINTER` | `Internal error: a required pointer was NULL.` |
| ERR-02 | An argument is outside its permitted range | The validation of the argument | `CF_ERR_INVALID_ARGUMENT` | `Invalid argument supplied to the function.` |
| ERR-03 | A string is not in the expected format | `validationParseInt`, `validationIsValidIdentifier` | `CF_ERR_INVALID_FORMAT` | `The value entered is not in the expected format.` |
| ERR-04 | A date is impossible | `validationIsValidDate` | `CF_ERR_INVALID_DATE` | `The date is not valid. Use the YYYY-MM-DD format.` |
| ERR-05 | A value is not one of the permitted choices | `validationParse*` | `CF_ERR_INVALID_ENUM` | `The value entered is not one of the permitted choices.` |
| ERR-06 | A string is too long for its field | `validationCheckLength` | `CF_ERR_INVALID_LENGTH` | `The text entered is too long.` |
| ERR-07 | A required value was left empty | `validationIsNotEmpty` | `CF_ERR_EMPTY_INPUT` | `A value is required. Input cannot be left empty.` |
| ERR-08 | The record does not exist | The `findById` of the owning module | `CF_ERR_NOT_FOUND` | `No matching record was found.` |
| ERR-09 | The identifier is already in use | The `*IdExists` helpers | `CF_ERR_DUPLICATE_ID` | `That identifier is already in use.` |
| ERR-10 | The record table is full | The capacity check before an insertion | `CF_ERR_CAPACITY_FULL` | `The maximum number of records has been reached.` |
| ERR-11 | A file cannot be opened | `fopen` | `CF_ERR_FILE_OPEN` | `The data file could not be opened.` |
| ERR-12 | A file cannot be read | `fgets` | `CF_ERR_FILE_READ` | `The data file could not be read.` |
| ERR-13 | A file cannot be written | `fputs`, `fprintf`, `fclose` | `CF_ERR_FILE_WRITE` | `The data file could not be written.` |
| ERR-14 | A file is not in the expected format | The signature check and the record checks | `CF_ERR_CORRUPT_FILE` | `The data file contains content in an unrecognised format.` |
| ERR-15 | The evidence cannot support a score | `investigation_analyseCase` | `CF_ERR_INSUFFICIENT_EVIDENCE` | `Insufficient evidence: the recorded evidence cannot support a score.` |

### 7.3 Recovery strategy

- **Invalid input never terminates the program.** The message is printed and the prompt repeats. The integer reader
  gives up after five attempts and falls back to the smallest permitted value, so the application always continues.
- **A failed load is survivable.** A missing or corrupt file produces a warning and an empty but fully usable
  session; the user can still inspect the file status and rebuild the register.
- **A failed save is reported loudly.** The exit path prints the error and states explicitly that the changes were
  **not** saved, and the process returns a failure exit code.
- **A failed allocation is reported.** `casefile_createDatabase()` returns `NULL` and the application stops with a
  clear message rather than dereferencing it.
- **A destructive action is confirmed.** A deletion always asks for confirmation and reports what will be removed
  first, including the number of child records that will go with it.

---

## 8. Coding standards

### 8.1 Naming conventions

| Item | Convention | Example |
| --- | --- | --- |
| Source files | `lower_snake_case.c` | `case_manager.c` |
| Header files | `lower_snake_case.h` | `case_manager.h` |
| Functions | `lower_snake_case`, `<module>_<verb><Object>` | `caseManager_addCase` |
| Local variables | `lowerCamelCase` | `caseCount` |
| Structures | `PascalCase` with a `_t` suffix | `Case_t` |
| Enumerations | `PascalCase` with a `_t` suffix | `CaseStatus_t` |
| Enumeration constants | `UPPER_SNAKE_CASE` | `CASE_STATUS_CLOSED` |
| Constants and macros | `UPPER_SNAKE_CASE` | `MAX_CASES` |
| File-scope functions | `static` where the function is private to its module | `static void cascadeDeleteChildren(...)` |

### 8.2 Formatting rules

- Indentation: four spaces, no tabs.
- Brace style: opening brace on its own line for functions and control blocks.
- Maximum line length: 110 characters.
- Compiler warning flags: `-Wall -Wextra -Wpedantic` with `-std=c11`.
- Comment style: `/* ... */` blocks above a declaration; a `//`-style comment is used only inside a function body
  where a single line of explanation is enough.
- One statement per line; spaces around every operator.

### 8.3 Function rules

- One clear responsibility per function; a function that needs the word "and" in its description is doing too much.
- Every pointer argument is checked before it is dereferenced.
- Every relevant input is validated before any state is changed.
- Every function returns a meaningful value: `CfStatus_t` when it can fail, a value or a `void` with a clear
  contract otherwise.
- A function that returns an error changes nothing.
- Functions are small enough to be understood and tested in isolation.

### 8.4 Prohibited practices

- Putting the complete application in `main.c`.
- Ignoring a compiler warning or a return value.
- Using unexplained magic numbers.
- Using mutable global variables.
- Duplicating logic that another module already provides.
- Keeping unused variables, functions or commented-out code.
- Using unsafe input functions (`gets`, unbounded `scanf`).
- Hard-coding machine-specific absolute paths.
- Copying code without understanding it.

---

## 9. Application flow

### 9.1 Main application flow

```
        ┌──────────────────────┐
        │   Application start   │
        └──────────┬───────────┘
                   │
        ┌──────────▼───────────┐
        │ Create the database   │   casefile_createDatabase()
        └──────────┬───────────┘
                   │ NULL? ─────────────► [!] Not enough memory ─► Exit failure
                   │
        ┌──────────▼───────────┐
        │  storage_loadAll()   │   read data/*.txt
        └──────────┬───────────┘
                   │
     ┌─────────────┼─────────────┐
     │             │             │
  Success      Missing        Corrupt
     │             │             │
     │             │      [!] Format message
     │             │      "Starting with an empty case file"
     │             │             │
     └─────────────┴─────────────┘
                   │
        ┌──────────▼───────────┐
        │   Display main menu  │◄──────────────┐
        └──────────┬───────────┘               │
                   │                           │
        ┌──────────▼───────────┐               │
        │  Read the choice     │               │
        └──────────┬───────────┘               │
                   │                           │
        ┌──────────▼───────────┐   invalid     │
        │  Is the choice valid?├──────────────►│ [!] Message
        └──────────┬───────────┘               │
                   │ valid                     │
        ┌──────────▼───────────┐               │
        │  Call the module     │               │
        └──────────┬───────────┘               │
                   │                           │
        ┌──────────▼───────────┐               │
        │  Read the status     │               │
        └──────────┬───────────┘               │
                   │                           │
        ┌──────────▼───────────┐               │
        │ Print result or [!]  │───────────────┘
        │ error message        │
        └──────────┬───────────┘
                   │ choice = 0?
                   │ yes
        ┌──────────▼───────────┐
        │ storage_saveAll()    │
        └──────────┬───────────┘
                   │
        ┌──────────▼───────────┐
        │ Release the database │   casefile_destroyDatabase()
        └──────────┬───────────┘
                   │
        ┌──────────▼───────────┐
        │   Application exit   │
        └──────────────────────┘
```

### 9.2 Operation flow: case registration

```
  Select "Register new case"
        │
        ├─ Read the title ────────────── empty? ─► [!] and return
        ├─ Read the crime type ───────── unknown? ─► [!] and return
        ├─ Read the priority ─────────── unknown? ─► [!] and return
        ├─ Read the incident date ────── invalid? ─► [!] and return
        ├─ Read the location, the officer and the summary
        │
        ▼
  caseManager_addCase()
        │
        ├─ capacity reached? ─────────────────► CF_ERR_CAPACITY_FULL
        ├─ validate the title, the date and the enumerations
        ├─ generate the next unused CF-YYYY-NNNN from the incident year
        └─ store the record
        │
        ▼
  Print "[OK] Case registered." and the generated identifier
```

### 9.3 Operation flow: evidence registration and examination

```
  Select "Register evidence"
        │
        ├─ Read the case identifier ── unknown case? ─► [!] and return
        ├─ Read the category ────────── unknown? ──────► [!] and return
        ├─ Read the description ─────── empty? ───────► [!] and return
        ├─ Read the collection date ─── invalid? ─────► [!] and return
        ├─ Read the location, the collector and the status
        │
        ▼
  evidenceManager_addCase() → generates EV-NNNNN
        │
        ▼
  "Attach analysis attribute"  →  evidenceManager_addAttribute()
        │   key must be one of the six comparable keys
        │   a key already present is replaced, not duplicated
        │   the list is bounded by MAX_EVIDENCE_ATTRIBUTES
        │
        ▼
  "Record chain-of-custody event"  →  evidenceManager_addCustodyEvent()
        │   action, date, both handlers and the remarks are validated
        │   the event is appended, so the log stays in order
        │   the log is bounded by MAX_CUSTODY_EVENTS
        │
        ▼
  "Update examination status"  →  evidenceManager_setStatus()
            collected → in_transit → under_analysis → analysed → reported → released
```

### 9.4 Operation flow: suspect matching

```
  Select "Analyse a case"
        │
        ▼
  investigation_analyseCase(db, caseId, result)
        │
        ├─ Find the case ───────────────── not found? ─► CF_ERR_NOT_FOUND
        ├─ Collect the suspects of the case
        ├─ Count the evidence items that carry a comparable attribute
        │
        ├─ no suspects?  ───────────┐
        ├─ no comparable evidence? ─┴─► CF_ERR_INSUFFICIENT_EVIDENCE
        │                               "[!] Insufficient evidence ..." and no score
        ▼
  For each suspect, for each comparable attribute:
        │
        ├─ compare the attribute with the suspect profile
        │     gender / hair / blood group → exact match, ignoring case
        │     height / shoe size          → the suspect value must be inside the band
        │     vehicle                     → the registration must be in the suspect list
        │
        ├─ matched?  add the weight to the score and record the factor
        └─ not matched?  record the factor with a weight of zero
        │
        ▼
  Cap the score at MAX_SUSPICION_SCORE
        │
        ▼
  investigation_rankSuspects()  →  sort by descending score, stable
        │
        ▼
  Print the ranking, every contributing and non-contributing factor,
  and the educational disclaimer
```

### 9.5 Operation flow: report generation

```
  Select "Investigation summary report" or "Evidence summary report"
        │
        ▼
  Read the case identifier ───────── unknown case? ─► [!] and return
        │
        ▼
  report_generateCaseSummary() / report_generateEvidenceSummary()
        │
        ├─ copy the case identity and the status
        ├─ copy the suspect ranking with its factors
        ├─ copy the evidence register and its custody history
        ├─ copy the witness statements
        └─ append the disclaimer
        │      every write is bounds checked, so a short buffer truncates
        │      rather than overflowing
        ▼
  Print the report on screen
        │
        ▼
  "Save this report to a file? (y/n)"
        │
        ├─ yes ─► report_saveToFile()
        │            mkdir reports/ if needed
        │            fopen for writing ── fails? ─► [!] file error
        │            fputs the report ──── fails? ─► [!] file error
        │            fclose ─────────────── fails? ─► [!] file error
        │            print the path
        └─ no  ─► return to the menu
```

---

## 10. Unit-test plan

### 10.1 Functions selected for testing

| Function | Module | Reason for testing |
| --- | --- | --- |
| `validation*` | validation | The foundation every other module relies on; a defect here corrupts the whole application. |
| `caseManager_*`, `suspectManager_*`, `evidenceManager_*`, `witnessManager_*` | the managers | The record lifecycle, the identifier rules, the cascade and the capacity limits. |
| `investigation_compareAttribute`, `investigation_analyseCase`, `investigation_rankSuspects` | investigation | The rule-based comparison, the band boundaries, the score cap and the insufficient-evidence decision. |
| `storage_validateFormat`, `storage_writeLine`, `storage_sanitizeField` | storage | The corruption detection and the checked writes. |
| `report_generateCaseSummary`, `report_generateEvidenceSummary` | report generator | The report content and the buffer safety. |

### 10.2 Test cases

The executed suite contains 164 cases across 32 suites; the full list with the expected result of each is printed by
the test runner and summarised in `docs/mini_project_report.md` §19.2. The categories are:

| Category | Examples |
| --- | --- |
| Positive | A valid date is accepted; a case is stored with a generated identifier; a matching attribute raises the score; a custody event is appended in order. |
| Negative | An impossible date, a malformed identifier, an unknown enumeration value, a trailing-garbage integer, an unknown case, a null pointer, a foreign file signature. |
| Boundary | `2024-02-29` is accepted and `2026-02-29` is rejected; a 15-character value fits a 16-byte field and a 16-character value does not; a height of exactly 175 and of exactly 185 falls inside the band and 186 does not; a record table at its capacity is refused. |

### 10.3 Test data and setup

- The database is created by `casefile_createDatabase()` in the fixture `SetUp` and released in `TearDown`, so each
  test starts from an empty, known state and no test depends on the order of the others.
- The line-reading tests write their input to a temporary file under `/tmp/opencode_casefile_test/`, redirect standard
  input onto it with `dup2`, and restore the original stream afterwards; the file is removed in `TearDown`.
- The file-format tests write their fixtures to the same temporary directory and remove them afterwards.
- The tests never touch the interactive menu and never read from the real `data/` directory.

### 10.4 Expected test command

```
make test
```

---

## 11. Makefile design

### 11.1 Required targets

| Target | Purpose | Expected output |
| --- | --- | --- |
| `all` | Build the application | `build/casefile` |
| `run` | Build if needed, then start the application | The interactive menu |
| `test` | Build and execute the unit tests | The Google Test summary |
| `clean` | Remove the generated files | A confirmation line |

Two further targets support the build: `gtest-check` reports a helpful message when Google Test cannot be found, and
`help` prints the list of targets.

### 11.2 Build configuration

| Item | Planned value |
| --- | --- |
| C compiler | `cc` |
| C++ compiler | `c++` |
| C compiler flags | `-std=c11 -Wall -Wextra -Wpedantic -Iinclude -O2 -MMD -MP` |
| C++ compiler flags | `-std=c++17 -Wall -Wextra -Iinclude -O2 -MMD -MP` |
| Include paths | `-Iinclude` |
| Google Test discovery | `GTEST_ROOT` if set, otherwise `/opt/homebrew/opt/googletest`, `/usr/local/opt/googletest`, `/opt/homebrew`, `/usr/local`, `/usr`, `~/.local`, `~/local`, `~/opt` |
| Object files | `build/*.o`, with the dependency files `build/*.d` included by the Makefile |
| Application executable | `build/casefile` |
| Test executable | `build/casefile_test` |
| Link order | The test binary links the module objects but **not** `main.o`, because Google Test supplies its own `main()`. |

---

## 12. Repository structure and Git plan

The final structure is shown in `README.md`. The commit history is a series of meaningful milestones:

| Milestone | Commit message |
| --- | --- |
| Repository creation | `Create initial project structure` |
| Data design | `Define data model, enumerations and status codes` |
| Validation | `Implement input validation module` |
| Case management | `Implement case register module` |
| Suspects and witnesses | `Implement suspect and witness register modules` |
| Evidence | `Implement evidence register and chain of custody` |
| Analysis | `Implement rule-based suspicion scoring and ranking` |
| Reporting | `Implement investigation and evidence report generation` |
| File handling | `Implement file persistence and format validation` |
| Application | `Implement menu-driven application entry point` |
| Build | `Add Makefile with build, run, test and clean targets` |
| Testing | `Add Google Test unit tests` |
| Refinement | `Improve start-up message and Google Test discovery` |
| Presentation | `Align table columns with truncated display fields` |
| Documentation | `Document build, run and test instructions` |

The `.gitignore` excludes the `build/` directory, the object files, the generated executables, the runtime data
files, the `reports/` directory, the logs, the core dumps and the editor and operating-system noise.
