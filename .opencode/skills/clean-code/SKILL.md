---
name: clean-code
description: Concise, pragmatic coding standards for agents - ship direct solutions, keep code small and clear, avoid over-engineering, and verify changes before handing off.
---

# Clean Code - Pragmatic Standards

Use this skill to write direct, maintainable code with minimal ceremony. Default to shipping working changes fast while keeping quality high.

## Quick Start
- Keep functions <=20 lines and <=3 params; one abstraction level per function.
- Rename for clarity instead of adding comments.
- Prefer guard clauses; avoid deep nesting (max 2 levels).
- Fix related files in the same task; leave no broken imports.
- Ask when requirements are unclear; otherwise build or fix immediately.

## Core Principles
- **SRP:** One responsibility per function/class.
- **DRY:** Extract duplication; reuse instead of retyping.
- **KISS:** Choose the simplest solution that works.
- **YAGNI:** Skip features not requested or used.
- **Boy Scout:** Leave touched code cleaner.

## Naming
- Variables: reveal intent (`userCount`, not `n`).
- Functions: verb + noun (`GetUserById`).
- Booleans: question form (`bIsActive`, `bHasAccess`).
- Constants: SCREAMING_SNAKE (`MAX_RETRY_COUNT`).
- If a name needs a comment, rename it.

## Functions
- Small, single-purpose; avoid mixed abstraction levels.
- Inputs are not mutated unexpectedly; return new values or document mutations.
- Prefer 0–2 args; cap at 3.

## Structure
- Use guard clauses for edge cases; keep flow flat.
- Compose small helpers; colocate related code.
- Replace magic numbers with named constants.

## AI Coding Style
- Feature request -> implement directly.
- Bug report -> fix without tutorial.
- Unknowns -> ask a concise clarifying question, then proceed.

## Anti-Patterns -> Fixes
- Commenting obvious code -> rename/clarify instead.
- One-line helpers or tiny utils files -> inline where used.
- Factories for a couple objects -> construct directly.
- God functions/classes -> split by responsibility.
- Deep nesting -> guard clauses and early returns.

## Before Editing Any File
- What imports this file? Update dependents if signatures change.
- What does this file import? Keep interfaces consistent.
- What tests cover it? Run/adjust them as needed.
- Is it shared? Consider ripple effects.

## Self-Check Before Handoff
- Goal met exactly?
- All impacted files updated?
- Code builds/tests as applicable?
- No obvious errors or missing edge cases?

## Verification Scripts
- Run only the script that matches your agent role.
- Mapping:
  - frontend-specialist: `python ~/.claude/skills/frontend-design/scripts/ux_audit.py .` and `python ~/.claude/skills/frontend-design/scripts/accessibility_checker.py .`
  - backend-specialist: `python ~/.claude/skills/api-patterns/scripts/api_validator.py .`
  - mobile-developer: `python ~/.claude/skills/mobile-design/scripts/mobile_audit.py .`
  - database-architect: `python ~/.claude/skills/database-design/scripts/schema_validator.py .`
  - security-auditor: `python ~/.claude/skills/vulnerability-scanner/scripts/security_scan.py .`
  - seo-specialist: `python ~/.claude/skills/seo-fundamentals/scripts/seo_checker.py .` and `python ~/.claude/skills/geo-fundamentals/scripts/geo_checker.py .`
  - performance-optimizer: `python ~/.claude/skills/performance-profiling/scripts/lighthouse_audit.py <url>`
  - test-engineer: `python ~/.claude/skills/testing-patterns/scripts/test_runner.py .` or `python ~/.claude/skills/webapp-testing/scripts/playwright_runner.py <url>`
  - any agent: `python ~/.claude/skills/lint-and-validate/scripts/lint_runner.py .`, `python ~/.claude/skills/lint-and-validate/scripts/type_coverage.py .`, `python ~/.claude/skills/i18n-localization/scripts/i18n_checker.py .`
- Wrong agent + script = violation.

### Handling Script Output
1) Run the script and capture all output.  
2) Parse errors, warnings, passes.  
3) Report using:
```
## Script Results: <script_name>
### ❌ Errors (X)
- [File:Line] ...
### ⚠️ Warnings (Y)
- [File:Line] ...
### ✅ Passed (Z)
- Check ...
Should I fix the X errors?
```
4) Wait for confirmation before fixing; re-run after fixes.
