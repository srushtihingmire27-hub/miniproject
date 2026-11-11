# CASEFILE — Detailed Software Design

> **Document:** Phase 2 deliverable (Detailed Design)
> **Status:** **Skeleton — awaiting Phase 1 approval.** The design decisions that were fixed during Phase 1
> (module boundaries, data model outline, status-code approach, file-format strategy, scoring rules) are recorded
> below as *agreed decisions*. Everything marked *To be completed in Phase 2* still has to be written out in full
> before any code is written.
>
> No source code has been written yet. No test has been written or executed yet.

---

## 0. Agreed design decisions (carried over from Phase 1)

These were fixed during the proposal phase and are treated as settled unless the student objects.

| Decision | Value | Rationale |
| --- | --- | --- |
| Layering | Presentation → Business logic → Validation; Persistence called only by `main` | Keeps `main.c` thin and the dependency graph acyclic |
| Shared state | Single `CasefileDb_t` passed by pointer; no mutable globals | Makes every module independently unit-testable |
| Storage | Line-oriented delimiter-separated text files under `data/`, with a format-signature header line | Human-inspectable, portable, no external dependencies |
| Dynamic memory | Not used for record storage; fixed-capacity arrays with explicit capacity checks | Avoids allocation failure paths; makes "capacity reached" a testable condition |
| Error model | One `CfStatus_t` enum returned across every module boundary, mapped to messages | Consistent error handling across modules |
| Input reading | `fgets` with explicit buffer bounds, manual integer parsing | No unsafe input functions; rejects trailing garbage like `12abc` |
| Dates | ISO `YYYY-MM-DD`, calendar-correct including leap years | Unambiguous, sortable, validatable |
| Scoring | Weighted attribute matching, weights as named constants, score capped at `MAX_SUSPICION_SCORE`, deterministic | Transparent, explainable, no randomness |
| Insufficient evidence | Detected and reported explicitly; never represented as a score of zero | Avoids a misleading result |
| gTest | System install preferred; Makefile auto-detects and fails with a clear message when absent | Environment currently has no gTest and no package manager |

---

## 1. Software architecture

*To be completed in Phase 2.* Will expand the proposal's §5.1 into the six layers named in the guidelines
(interface, business logic, validation, data management, file storage, reporting), with the dependency rules and the
rationale for each layer.

---

## 2. Module design

*To be completed in Phase 2.* One subsection per module following the official structure: source file, header file,
purpose, responsibilities, public functions, data used or owned, dependencies, possible errors.

Nine implementation modules plus `main.c`:

`main` · `validation` · `case_manager` · `suspect_manager` · `evidence_manager` · `witness_manager` ·
`investigation` · `report_generator` · `storage`

---

## 3. Data design

*To be completed in Phase 2.* Full structure definitions with member tables, enumeration definitions with constant
tables, the constants table, and the complete data dictionary.

Planned types (from proposal §2.3, §3.3, §5.5):

- Structures: `Case_t`, `Suspect_t`, `Evidence_t`, `CustodyEvent_t`, `Witness_t`, `Attribute_t`, `MatchFactor_t`,
  `SuspectScore_t`, `CasefileDb_t`
- Enumerations: `CaseStatus_t`, `CasePriority_t`, `CrimeType_t`, `SuspectStatus_t`, `EvidenceCategory_t`,
  `EvidenceStatus_t`, `CustodyAction_t`, `AttributeKey_t`, `CfStatus_t`
- Constants: capacity limits, string-length limits, ID patterns, date format, file names, score weights, maximum score

---

## 4. Function design

*To be completed in Phase 2.* Every important function documented with prototype, purpose, parameter table, return
type, return-value table, preconditions, postconditions, errors handled, related requirements and planned unit
tests.

---

## 5. Input-validation plan

*To be completed in Phase 2.* Rules for integers, floats (none expected), text fields, identifiers, enumerated
choices, empty input, duplicate records, min/max values, maximum string lengths, unsupported menu choices and
invalid file data.

---

## 6. File-handling plan

*To be completed in Phase 2.* Files, purpose, text format, data stored, creation/read/update points, and error
handling for every `fopen`/`fgets`/`fprintf`/`fclose` call.

---

## 7. Error-handling strategy

*To be completed in Phase 2.* `CfStatus_t` definition, error catalogue with detection location and user-facing
behaviour, and the recovery strategy.

---

## 8. Application flowcharts

*To be completed in Phase 2.* The main flow is already in the proposal §5.3. Phase 2 adds the case-registration,
evidence-registration-and-examination, suspect-matching and report-generation flowcharts.

---

## 9. Unit-test plan

*To be completed in Phase 2.* Convert the provisional traceability table (proposal §4.3) into the official test-case
table with positive, negative and boundary cases across at least three functions, plus test data, setup and cleanup.

---

## 10. Makefile design

*To be completed in Phase 2.* Targets `all`, `run`, `test`, `clean`; compiler variables; `-Wall -Wextra -Wpedantic`;
include paths; gTest detection strategy; output paths.

---

## 11. Coding standards

*To be completed in Phase 2.* Naming conventions table, formatting rules, function rules, prohibited practices.

---

## 12. Repository structure and Git plan

*To be completed in Phase 2.* Final file list and the commit milestone plan already drafted in the master report
§17.2.
