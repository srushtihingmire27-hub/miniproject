# CASEFILE — Forensic Evidence Management and Crime Investigation System

> **Domain:** Defence / Law Enforcement — Forensic Laboratory Management and Crime Investigation
> **Language:** C (ISO C11) | **Tests:** Google Test | **Build:** Make
> **Status:** Phase 1 (Project Proposal) complete. Implementation has **not** started yet.

CASEFILE is a modular, menu-driven command-line application that helps investigators maintain fictional crime
investigation records: cases, suspects, witnesses, forensic evidence, chain-of-custody history, a transparent
rule-based evidence-to-suspect comparison, and generated investigation reports.

> **The suspicion score produced by this application is an educational, rule-based decision-support indicator for
> prioritising investigative effort. It is not a forensic conclusion, not a probability, and not evidence of guilt.
> All case data in this project is fictional and is used for teaching purposes only.**

---

## Status

| Phase | Deliverable | Status |
| --- | --- | --- |
| 1 | Project Proposal (`docs/project_proposal.md`) | **Complete** |
| 2 | Detailed Software Design (`docs/detailed_design.md`) | Pending approval of Phase 1 |
| 3 | Implementation (modular C source) | Not started |
| 4 | Testing and debugging (Google Test) | Not started |
| 5 | Documentation, Git/GitHub publication, demonstration | Not started |

The master document that follows the official mini-project template section by section is
[`docs/mini_project_report.md`](docs/mini_project_report.md).

---

## Problem statement

> In forensic laboratory and crime-investigation units, investigating officers, forensic examiners and records clerks
> currently face manual, paper-and-spreadsheet case management in which case identifiers, evidence registers,
> chain-of-custody remarks, witness statements and suspect profiles are maintained in disconnected registers. The
> current method results in duplicate and inconsistent identifiers, unqueryable custody history, unsystematic
> comparison between recorded evidence attributes and suspect descriptors, and manually re-typed investigation
> reports. CASEFILE provides a single validated case file with enforced unique identifiers, a structured
> chain-of-custody history, systematic rule-based evidence-to-suspect comparison that ranks investigative leads and
> explains its reasoning, and generated investigation and evidence reports.

---

## Planned features

- **Case management** — register cases with auto-generated unique IDs, list, search, filter by status/crime type,
  update details and investigation status, delete with confirmation and cascade to child records.
- **Suspect management** — multiple suspects per case with physical descriptors (gender, height, hair colour, shoe
  size, blood group, known vehicles), alibi, motive, status; search by ID and partial name; update and delete.
- **Forensic evidence management** — evidence with unique IDs, category enumeration, description, collection date,
  location and collector, examination status, analysis attributes, and a structured chain-of-custody history.
- **Witness management** — witnesses linked to a case with unique statement reference IDs, verification flag,
  search, update and delete.
- **Evidence analysis** — a documented, deterministic, rule-based matching engine producing a 0–100 suspicion score
  per suspect, a ranked suspect list, an explanation of contributing and non-contributing attributes, and an explicit
  *insufficient evidence* outcome when the evidence cannot support a score.
- **Investigation reports** — case summary and evidence summary reports on screen and optionally saved to a file.
- **Persistence** — all records saved to `data/*.txt`, reloaded on start-up, with format validation and graceful
  handling of missing, empty or corrupted files.
- **Robust validation and consistent status codes** across every module.

---

## Architecture

Layered, with a strict one-way dependency direction and no mutable global state.

```
main.c (presentation)
   │
   ├──▶ case_manager / suspect_manager / evidence_manager / witness_manager  (business logic)
   │              │
   │              └──▶ validation  (shared validation services)
   │
   ├──▶ investigation      (rule-based scoring, ranking, progress)
   ├──▶ report_generator   (case & evidence reports)
   └──▶ storage            (file persistence & format validation) ──▶ validation
```

- `include/casefile.h` holds the shared structures, enumerations, capacity constants and the `CfStatus_t` status codes.
- `main.c` contains only initialisation, menu workflow, dispatch, result printing, save and cleanup — no business logic.
- The complete in-memory database is a single `CasefileDb_t` passed by pointer into every module function, which is
  what makes the whole application unit-testable without global setup.

The full architecture diagram, the main application workflow, and the module-by-module breakdown are in
[`docs/project_proposal.md`](docs/project_proposal.md) §5 and in the master report §5.

---

## Repository structure

```
CASEFILE/
|
|-- src/                 # C implementation (main.c + 8 modules)
|-- include/             # Public headers, one per module
|-- tests/               # Google Test suites (C++ test files)
|-- data/                # Runtime persistence files and sample data
|-- docs/                # Proposal, detailed design, master report
|-- Makefile             # all / run / test / clean
|-- README.md
|-- .gitignore
`-- LICENSE
```

---

## Prerequisites

| Requirement | Notes |
| --- | --- |
| C compiler with C11 support | `clang` on macOS, `gcc` on Linux |
| C++ compiler | Required only to build the Google Test suites |
| `make` | Standard GNU make |
| Google Test | Required only for `make test`; the Makefile auto-detects a system installation and reports clearly if it is absent |

---

## Build, run and test

```bash
make all     # compile the application into build/casefile
make run     # build if needed, then start the application
make test    # build and execute the Google Test suites
make clean   # remove all generated files
```

Full instructions, sample input and sample output will be completed in Phase 5 alongside the verified implementation.

---

## C concepts demonstrated

Structures · Enumerations · Functions with documented parameters and return values · Fixed-capacity arrays ·
Character strings · Pointers · Named constants · File handling (`fopen`/`fgets`/`fprintf`/`fclose`) ·
Input validation with bounded `fgets` · Consistent status codes · Sorting and filtering · Meaningful calculation
(rule-based scoring and progress computation) · Report generation · Modular multi-file design with header guards.

Dynamic memory allocation is deliberately avoided for record storage: fixed, documented capacities keep the data
model simple and the "capacity reached" condition testable.

---

## Coding standards

Four-space indentation · one brace style throughout · uppercase names for constants · header guards on every
header · one clear responsibility per function · all return values checked · no mutable global variables · no
duplicated logic · meaningful comments (purpose and non-obvious logic, not restating the code) · no unsafe input
functions · no machine-specific absolute paths · portable between macOS and Linux.

---

## Documentation

| Document | Purpose |
| --- | --- |
| [`docs/project_proposal.md`](docs/project_proposal.md) | Phase 1 deliverable — scenario, problem, solution, scope, requirements, architecture, modules, planned C concepts |
| [`docs/mini_project_report.md`](docs/mini_project_report.md) | Master report following the official template's 24 sections in order |
| `docs/detailed_design.md` | Phase 2 — module design, data design, function design, validation, file handling, error handling, test plan |

---

## License

MIT — see [`LICENSE`](LICENSE).

## GitHub repository

*Pending — the repository will be created and published in Phase 4.*
