# Requirements Document - Rotating Roster Tool for Elderly Home Care Staff

## 1. Purpose

This document captures the requirements currently known for a planning tool that can generate and validate a **base roster** for care staff in an elderly home.

The tool is intended to support the regular fixed base roster only. Absences, holidays, sickness, and other replacements are handled outside this tool by a separate staffing process.

---

## 2. Scope

### In scope
- Create a base weekly roster for the care staff.
- Model weekday and weekend shift coverage.
- Enforce nurse coverage requirements.
- Enforce the alternating weekend pattern.
- Enforce compensatory rest linked to weekend work.
- Support a fixed weekly template where **employees rotate through rows each week**.
- Treat all included employees as full-time and always available in the base model.
- Optionally generate multiple valid candidate base rosters for review.

### Out of scope
- Part-time staff handling.
- Sick leave.
- Holidays / vacation.
- Ad hoc replacements.
- The separate backup/replacement pool.
- Overtime administration.
- Direct editing of the approved base roster by regular staff.

---

## 3. Staff in scope

There are 14 total employees at the workplace, but only 10 are relevant for this planning tool.

### Care staff included in the roster
- 6 nurses
- 4 nursing assistants

### Staff excluded for now
- 4 helpers / logistical workers

These helpers are currently out of scope because they do not provide direct care to residents.

---

## 4. Core planning assumptions

For the current design, the following assumptions apply:

1. All 10 care staff are assumed to be available.
2. All 10 care staff are assumed to work full time.
3. The roster is the **ideal base situation** without sickness, extra rest days, or leave.
4. Any absences or staffing gaps caused by sickness, holidays, or part-time contracts are handled outside this tool by a separate replacement pool.
5. The schedule should match the exact required work hours in the base roster. Extra time beyond that is overtime, which is tracked separately and is not relevant to the base roster design.
6. The practical display of actual working rosters is currently done in TOBA, with the base roster serving as the foundation.

---

## 5. Nurse coverage requirement

A key functional rule is:

- **There must always be a nurse present in the residential care center.**

For roster design, this currently translates to:
- At minimum, the roster must ensure nurse presence every day between **07:00 and 19:00**.

### Operational design preference
The roster should avoid inefficient patterns where too many nurses are scheduled on late shifts while too few nurses are present in the morning.

Example concern raised from the current roster:
- some days currently have **2 nurses on late shift and 1 nurse on day shift**,
- which is considered costly and operationally unnecessary,
- while mornings may then have too little nurse coverage.

---

## 6. Role rules

### Equal shift eligibility
- There is **no difference** in the base roster between nurses and nursing assistants regarding which shift types they may work.
- Everyone should have the same types of shifts in the base roster.
- The base roster should be fair, for example so that everyone has comparable numbers of early and late shifts.

### Important exception
Even though the shift structure is the same for everyone, the roster must still satisfy the requirement that a nurse is present where needed.

---

## 7. Weekday staffing requirements

A fully covered weekday requires 7 care staff.

| Shift code | Shift name      | Start | End   | Break | Paid hours | People needed |
|-----------|-----------------|-------|-------|-------|------------|---------------|
| EA        | Early shift A   | 07:00 | 15:15 | 45 min unpaid | 7h30 | 3 |
| EB        | Early shift B   | 06:45 | 15:00 | 45 min unpaid | 7h30 | 1 |
| D         | Day shift       | 10:45 | 19:00 | 45 min unpaid | 7h30 | 1 |
| L1        | Late shift 1    | 13:00 | 21:00 | 30 min unpaid | 7h30 | 1 |
| L2        | Late shift 2    | 13:30 | 21:30 | 30 min unpaid | 7h30 | 1 |

### Weekday total
- 3 x EA
- 1 x EB
- 1 x D
- 1 x L1
- 1 x L2
- **Total: 7 people per weekday**

---

## 8. Weekend staffing requirements

A fully covered weekend day requires 5 care staff.

| Shift code | Shift name        | Part 1         | Part 2         | Break | Paid hours | People needed |
|-----------|-------------------|----------------|----------------|-------|------------|---------------|
| WE-EA     | Early shift A     | 06:45-15:15    | -              | 30 min unpaid | 8h00 | 1 |
| WE-EB     | Early shift B     | 07:00-15:30    | -              | 30 min unpaid | 8h00 | 1 |
| WE-SD     | Split day shift   | 07:45-12:45    | 16:00-19:00    | no break | 8h00* | 1 |
| WE-SL     | Split late shift  | 07:45-12:00    | 17:00-20:45    | no break | 8h00 | 1 |
| WE-L      | Late shift        | 13:00-21:30    | -              | 30 min unpaid | 8h00 | 1 |

### Weekend total
- 1 x WE-EA
- 1 x WE-EB
- 1 x WE-SD
- 1 x WE-SL
- 1 x WE-L
- **Total: 5 people per weekend day**

---

## 9. Rest-time and sequence rules

The following rules must be respected:

### Minimum rest time
- There must be at least **12 hours of rest** between two shifts.

### Forbidden shift transition
- A **late shift followed by an early shift** is not allowed because it breaks the 12-hour rest rule.

### Allowed examples
- Day shift to late shift is allowed.
- Early shift to late shift is allowed.

### Consecutive working days
- Employees may **not work more than 4 days in a row**.

### Roster pattern preference
The base roster should prefer **blocks of similar successive shifts** instead of irregular daily switching.

Preferred style example:
- 4 days late - OFF - 4 days early

Not preferred:
- 1 day early, 1 day late, 1 day day shift, then back to early

This is both an organizational preference and a quality requirement for an attractive and understandable base roster.

---

## 10. Weekend rotation rule

Every colleague must:
- work one weekend
- then be off the next weekend
- then work the next weekend again

So the intended pattern is:
- **one weekend on, one weekend off**

This alternating weekend rule is a core structural constraint of the roster design.

---

## 11. Weekend compensatory rest (ZAR / ZOR)

The earlier simplified description of “one compensation day” is refined as follows:

- Each colleague has **one compensatory rest day each week**.
- This is either:
  - **ZAR** = zaterdagsrust, which falls **before** the worked weekend
  - **ZOR** = zondagsrust, which falls **after** the worked weekend

### Practical implication
- Compensation days do not need to be tied to one specific weekday.
- They may be placed where staffing allows.
- However, they must still respect:
  - the maximum of 4 consecutive workdays,
  - the weekly rest structure,
  - and the alternating weekend system.

This means the roster must model **weekly compensatory rest systematically**, not just generic OFF days.

---

## 12. Fixed-template rotating-row model

The roster is **not** intended to be redesigned from scratch every week.

Instead, the system should use:
1. **One fixed base week template**
2. **Ten rows** in that base week
3. A rule that the **names rotate through the rows** each new week

### Meaning of a row
A row represents a full weekly pattern for one position in the base roster.

Each row contains, for each day from Monday to Sunday:
- a shift code, or
- OFF / compensatory rest marker

Example structure only:

| Row | Mon | Tue | Wed | Thu | Fri | Sat   | Sun   |
|-----|-----|-----|-----|-----|-----|-------|-------|
| 1   | EA  | EA  | ZAR | D   | L1  | WE-EA | WE-EB |

### Weekly rotation of names
The weekly template stays fixed, but the names move one row each week in a circular rotation.

Example:

#### Week 1
| Row | Person |
|-----|--------|
| 1   | Ruben  |
| 2   | B      |
| 3   | C      |
| 4   | D      |
| 5   | E      |
| 6   | F      |
| 7   | G      |
| 8   | H      |
| 9   | I      |
| 10  | J      |

#### Week 2
| Row | Person |
|-----|--------|
| 1   | B      |
| 2   | C      |
| 3   | D      |
| 4   | E      |
| 5   | F      |
| 6   | G      |
| 7   | H      |
| 8   | I      |
| 9   | J      |
| 10  | Ruben  |

So if Ruben is in row 1 in week 1, he moves to row 10 in week 2 and inherits that row's weekly pattern.

---

## 13. Weekend structure in the base table

A major simplification is that the weekend pattern can already be fixed at row level.

Because:
- there are 10 rows,
- exactly 5 people are needed per weekend day,
- and the names rotate by one row each week,

it is possible to enforce the alternating weekend rule structurally.

### Proposed row-level weekend pattern
- one row works the weekend
- next row is off the weekend
- one row works the weekend
- next row is off the weekend
- and so on

Example:

| Row | Weekend status |
|-----|----------------|
| 1   | Work |
| 2   | Off  |
| 3   | Work |
| 4   | Off  |
| 5   | Work |
| 6   | Off  |
| 7   | Work |
| 8   | Off  |
| 9   | Work |
| 10  | Off  |

This immediately produces:
- 5 rows with weekend work
- 5 rows with weekend off

Because names rotate by one row every week, each employee automatically alternates between:
- a row with weekend work
- then a row with weekend off
- then a row with weekend work again

This appears to satisfy the alternating-weekend requirement by construction.

---

## 14. Functional objectives of the tool

The planning tool should be able to:

1. Represent the 10-row fixed weekly template.
2. Represent all weekday and weekend shifts.
3. Represent unpaid breaks and paid hours per shift.
4. Assign row patterns across Monday-Sunday.
5. Ensure weekday and weekend staffing demand is covered.
6. Ensure at least one nurse is present where required.
7. Respect the 12-hour minimum rest rule.
8. Prevent late-to-early transitions.
9. Prevent more than 4 consecutive working days.
10. Model weekly compensatory rest using ZAR / ZOR logic.
11. Keep the weekly paid hours exact for the base roster pattern.
12. Rotate employees through the rows week by week.
13. Optionally explore multiple valid base-roster candidates.
14. Produce a roster that is stable, attractive, and understandable for real-world use.

---

## 15. Suggested data model

### Employee
- employee_id
- name
- role: nurse or nursing_assistant
- active_in_rotation: yes/no
- optional exemptions / protected days outside care work

### Shift definition
- shift_code
- shift_name
- day_type: weekday or weekend
- start time
- end time
- optional second start/end for split shifts
- break duration
- break paid/unpaid flag
- paid_hours

### Row template
- row_id
- weekend_status: work/off
- Monday assignment
- Tuesday assignment
- Wednesday assignment
- Thursday assignment
- Friday assignment
- Saturday assignment
- Sunday assignment
- weekly_total_paid_hours
- ZAR position if applicable
- ZOR position if applicable

### Rotation settings
- number_of_rows = 10
- rotation_step = 1 row per week
- circular_rotation = true

---

## 16. Constraints currently known

### Hard constraints
- Exactly 10 care staff are in scope.
- 6 of them are nurses.
- 4 of them are nursing assistants.
- All included workers can work all shift types.
- Weekday demand must match the defined weekday shift structure.
- Weekend demand must match the defined weekend shift structure.
- There must be nurse coverage where operationally required.
- Employees alternate weekends: one on, one off.
- There must be at least 12 hours between two shifts.
- Late-to-early transitions are forbidden.
- Employees may not work more than 4 consecutive days.
- Weekly compensatory rest must be represented via ZAR / ZOR logic.
- The schedule is based on a fixed weekly template with row rotation.
- The base roster should reflect the exact intended hours, not tolerated deviations.

### Strong design preferences
- Prefer consecutive blocks of similar shifts over highly mixed day-to-day sequences.
- Distribute early and late shifts fairly across everyone.
- Avoid nurse-heavy late patterns that weaken morning nurse coverage.
- Avoid split shifts where possible because reducing split shifts makes the work more attractive and may help recruit new staff.
- The base roster should feel stimulating and inviting, not merely legally valid.

---

## 17. Optional non-base considerations

Some nurses or nursing assistants may have recurring exemptions during working time, for example reference-role duties outside direct care on certain days each month.

Current interpretation:
- the system should be able to take such exemptions into account in some way,
- but they do **not** need to be built into the fixed base roster itself.

---

## 18. Outputs expected from the tool

The exact output format should support practical use.

### Preferred practical outcome
- A fixed base roster that can be printed for colleagues.
- The actual operational roster can then be produced in TOBA using this base roster as the starting point.

### Candidate output views

#### Base roster template
A table with 10 rows and 7 days showing which shift code each row works.

#### Weekly person view
A derived view showing, for a given week number, which person is assigned to which row and therefore which shifts they work that week.

#### Validation output
A set of checks confirming whether the roster satisfies:
- weekday coverage
- weekend coverage
- nurse coverage
- alternating weekend logic
- ZAR / ZOR compensation logic
- 12-hour rest rule
- consecutive-day rule
- paid-hour totals

#### Candidate comparison
If multiple candidate base rosters are generated, the tool should help compare them, especially on:
- number of split shifts
- fairness of shift distribution
- attractiveness / usability
- nurse distribution quality

---

## 19. Governance and editability

The base roster is intended to be fixed.

- Staff should not be able to modify it.
- Even the head of service cannot simply change it informally.
- Approval must come from the management / board level.

This means the tool is primarily a **design and evaluation tool** for a stable base roster, not an everyday self-service roster editor.

---

## 20. Remaining point to confirm

Nothing 
---

## 21. Summary

This project is a **base rotating-roster problem** for an elderly home.

The tool should create a weekly template for 10 care staff:
- 6 nurses
- 4 nursing assistants

The template must cover:
- 7 care staff per weekday
- 5 care staff per weekend day

It must also respect:
- nurse presence requirements with strong morning coverage
- alternating weekends
- compensatory weekly rest via ZAR / ZOR
- a fixed 10-row weekly template
- weekly circular rotation of names through those rows
- exact paid hours per shift
- at least 12 hours of rest between shifts
- no late-to-early transitions
- no more than 4 consecutive working days
- a preference for coherent blocks of similar shifts
- a desire to reduce split shifts where possible

The resulting base roster should be legally sound, operationally useful, fair, printable, and attractive enough to support staff retention and recruitment.
