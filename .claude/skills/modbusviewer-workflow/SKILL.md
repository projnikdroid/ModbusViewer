---
name: modbusviewer-workflow
description: Session-start and finish-milestone checklist for the ModbusViewer project. Use at the very start of any ModbusViewer session (including a bare "continue"), and again once a milestone's implementation is complete, self-reviewed, and built/tested, to close it out consistently.
---

This is a checklist/procedure, not project knowledge — project knowledge lives in
`PROGRESS.md`, `docs/*.md`, and `CLAUDE.md`. Read those for the actual state and
rationale; this skill only says what order to do things in and what to touch.

## Session start

Run this at the beginning of a session, or when the user says "continue".

1. Read `PROGRESS.md` in full.
2. Check "Current status" for an explicit "first task next session" flag or an
   unresolved question left for the user — handle that before anything else,
   don't silently skip ahead to the milestone table.
3. Identify the active/next milestone from the Milestones table.
4. Load only the `docs/*.md` file(s) relevant to that milestone's topic (see the
   table's topic mapping in `CLAUDE.md`) — don't blind-load all of them, and
   don't load `docs/history.md` unless you specifically need the *why* behind a
   past decision.
5. State the plan for the session in one or two sentences before starting.

## Finish milestone

Run after a milestone's implementation is complete and self-reviewed (see
`CLAUDE.md`'s "Self-review before building/testing").

1. Build (`cmake --build build`) and run the full suite (`ctest
   --output-on-failure`) — confirm 100% pass, not just "no errors printed."
2. If the milestone has a manual/GUI verification step, prompt the user for it
   and wait for their confirmation before marking it done.
3. Update `PROGRESS.md`:
   - Milestone table row.
   - "Current status": move the *previous* most-recent milestone's full
     narrative into `docs/history.md` (oldest-first, matching that file's
     existing order), and write this milestone's narrative in its place — what
     was built, key decisions, deviations from plan. Keep only the newest
     milestone's full detail in "Current status"; that's what keeps it lean.
   - "Known rough edges" if anything new applies.
4. Update the relevant `docs/*.md` topic file only if the implementation
   diverged from what it already describes — most milestones won't need this.
5. Mark the TaskUpdate task complete, move the next one to in_progress.
6. Report to the user: what changed, test counts, and what's next — concise,
   concrete numbers, no padding.
