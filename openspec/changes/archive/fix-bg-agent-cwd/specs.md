# Specs: Fix Background Agent Working Directory

## Functional Requirements

### FR-1: Background Agent Shows Correct Working Directory
- **Given**: Claude Code is started with `claude --bg` in DIR_A
- **When**: The agent process changes its working directory (chdir) after startup
- **Then**: Pulse widget MUST display DIR_A (from session JSON), not the process's current cwd

### FR-2: `claude agents` Command Excluded from Widget
- **Given**: User runs `claude agents` (any subcommand starting with `agents`) in DIR_B
- **When**: ProcScanner scans /proc
- **Then**: This process MUST NOT appear as an agent in the widget

### FR-3: Normal Agent Behavior Unaffected
- **Given**: Claude Code is started normally (without `--bg`) in DIR_C
- **When**: Session JSON is NOT found (e.g., agent just started)
- **Then**: Widget falls back to procfs cwd (existing behavior, no regression)

### FR-4: Name Field Consistency
- **Given**: FR-1 applies (session cwd overrides procfs cwd)
- **Then**: `a.name` (basename displayed in widget title) MUST equal `QFileInfo(m.cwd).fileName()`

## Invariants (PBT)

### INV-1: Session cwd supersedes procfs cwd
```
∀ agent a where a.toolType == "Claude Code":
  session_json_exists(a.pid) ∧ session.cwd ≠ "" → a.cwd == session.cwd
```
Falsification: start `claude --bg` in DIR_A, verify widget shows DIR_A after agent changes cwd internally.

### INV-2: Exclusion idempotency
```
∀ process p where comm(p) == "claude" ∧ cmdline(p).contains(" agents"):
  p ∉ scanAll()
```
Falsification: run `claude agents` while widget is open, verify no transient entry appears.

### INV-3: name = basename(cwd)
```
∀ agent a: a.name == QFileInfo(a.cwd).fileName()
```
Must hold after enrichment for both bg and non-bg agents.

## Non-Requirements

- NO change to QML/UI layer
- NO change to AgentInfo struct (cwd field already exists)
- NO change to Codex/OpenCode handling
- NO environ fallback (deferred)
- NO change to dedup logic
