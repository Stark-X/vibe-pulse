# Specs: Tmux Pane Focus on Agent Row Click

## Functional Requirements

### FR-1: Precise tmux pane focus on click
When `AgentModel::focusAgent(row)` is called for an agent with a valid `tmuxTarget` and `tmuxClientTty`:
1. `HyprlandClient::focusWindow(windowAddress)` MUST execute first (sync, existing behavior)
2. `QProcess::startDetached("tmux", {"switch-client", "-c", tmuxClientTty, "-t", tmuxTarget})` MUST execute after

### FR-2: Graceful degradation
- If `tmuxTarget` is empty OR `tmuxClientTty` is empty → skip switch-client, Hyprland focus still executes
- If `windowAddress` is empty → Hyprland focus is skipped (existing behavior, unchanged)
- `canJump` is ONLY based on `!windowAddress.isEmpty()` — tmux fields do not affect `canJump`

### FR-3: tmux pane location discovery
`TmuxResolver::findPaneInfo(quint32 agentPid)` must return `std::optional<TmuxPaneInfo>` where:
- Returns `std::nullopt` if: agentPid <= 1, no tmux ancestor in PPID chain, tmux commands timeout/fail, no matching pane, no valid client
- Returns populated `TmuxPaneInfo` otherwise

### FR-4: Field persistence in AgentInfo
`AgentInfo` gains two new fields: `tmuxTarget` (QString) and `tmuxClientTty` (QString).
Both MUST be compared in `dataEquals()` so snapshot changes trigger `dataChanged` signals.

### FR-5: enrichAgents() integration
`enrichAgents()` in `main.cpp` MUST call `TmuxResolver::findPaneInfo(a.pid)` instead of `findTerminalPid(a.pid)`.
From the returned struct, extract `terminalPid` for `windowAddress` lookup AND populate `a.tmuxTarget`/`a.tmuxClientTty`.

---

## Data Specifications

### TmuxPaneInfo Struct

```cpp
struct TmuxPaneInfo {
    quint32 terminalPid = 0;    // tmux client process PID (for windowAddress lookup)
    QString sessionId;           // tmux session id, e.g. "$1"
    QString sessionName;         // tmux session name (human readable, NOT used in target)
    int     windowIndex = -1;    // tmux window index (0-based)
    int     paneIndex   = -1;    // tmux pane index (0-based)
    QString clientTty;           // tmux client tty, e.g. "/dev/pts/3"
    QString tmuxTarget;          // composed: sessionId:windowIndex.paneIndex
};
```

### tmuxTarget Format
`QStringLiteral("%1:%2.%3").arg(sessionId).arg(windowIndex).arg(paneIndex)`  
Example: `"$1:2.1"`  
**Rule**: ALWAYS use `sessionId` (e.g., `$1`), NEVER `sessionName`. sessionName may contain spaces/colons.

### tmux Command Formats

**list-panes:**
```
tmux list-panes -a -F "#{pane_pid}\t#{session_id}\t#{session_name}\t#{window_index}\t#{pane_index}"
timeout: 1000ms
```

**list-clients:**
```
tmux list-clients -t <sessionId> -F "#{client_pid}\t#{client_tty}"
timeout: 1000ms
```

---

## Algorithm Specifications

### Pane Matching Algorithm (findPaneInfo)

```
INPUT: agentPid: quint32

1. If agentPid <= 1 → return nullopt

2. Build PPID chain C = [agentPid, ppid(agentPid), ppid(ppid(...)), ...]
   Stop when ppid == 0 or ppid == 1 or depth > 32
   
3. Scan C for any PID whose comm starts with "tmux"
   If NOT found → return nullopt (agent not in tmux)

4. Run: tmux list-panes -a -F "#{pane_pid}\t#{session_id}\t#{session_name}\t#{window_index}\t#{pane_index}"
   If fails (timeout or exit != 0) → return nullopt

5. For each pane row:
   - Parse 5 tab-separated fields; skip malformed rows
   - Check if panePid ∈ C (walk from agentPid up, counting hops)
   - Track best match = pane with SMALLEST hop distance to agentPid
   - Tie-break: first occurrence in list-panes output

6. If no matching pane → return nullopt

7. Run: tmux list-clients -t <bestPane.sessionId> -F "#{client_pid}\t#{client_tty}"
   If fails → return nullopt

8. For each client row:
   - Parse 2 fields; skip if clientPid <= 1 or clientTty is empty
   - Pick FIRST valid client

9. If no valid client → return nullopt

10. Compose tmuxTarget = sessionId:windowIndex.paneIndex
    Set terminalPid = client_pid

11. Return TmuxPaneInfo{terminalPid, sessionId, sessionName, windowIndex, paneIndex, clientTty, tmuxTarget}
```

### Multi-Client Selection Rule
Given `N` clients attached to a session:  
Select the **first** client in `tmux list-clients` output order where `clientPid > 1` AND `clientTty` is non-empty.

---

## PBT Properties (Invariants)

| ID | Invariant | Falsification Strategy |
|----|----------|----------------------|
| P1 | `tmuxTarget` always uses `sessionId` (starts with `$`), never `sessionName` | Generate sessionName with spaces/colons; assert target starts with `$` |
| P2 | `findPaneInfo` returns `nullopt` when no panePid is in the PPID chain | Mock list-panes with unrelated PIDs; assert nullopt returned |
| P3 | Nearest PPID chain ancestor wins on multi-pane match | Insert two panePids at depth 2 and 5; assert depth-2 pane is selected |
| P4 | Multi-client selects first valid (pid>1 and tty non-empty) in output order | Input [invalid, validA, validB]; assert validA selected |
| P5 | `focusWindow` always called before `startDetached switch-client` | Spy call order; switch-client must not precede focusWindow |
| P6 | `dataEquals` is false when tmuxTarget or tmuxClientTty changes | Create two AgentInfo differing only in tmuxTarget; assert !dataEquals |
| P7 | Non-tmux agent behavior is unchanged (only Hyprland focus fires) | Set tmuxTarget="" and tmuxClientTty=""; assert switch-client never called |

---

## Non-Functional Requirements

- **Latency**: `focusAgent()` returns immediately (sync Hyprland call ≤100ms, async switch-client)
- **Thread safety**: all tmux calls happen on the main thread via existing QProcess pattern (known limitation, out of scope)
- **Error visibility**: switch-client failures are silent (fire-and-forget, no UI impact)
- **Backward compatibility**: `findTerminalPid()` may be removed or kept as wrapper; callers in main.cpp are updated

---

## Out of Scope

- QML visual feedback on click
- ExpandedView refactor to use AgentRow component
- Widget auto-hide after jump
- Moving enrichment to background thread
- Pure tmux focus when no Hyprland window exists (`canJump` remains `!windowAddress.isEmpty()`)
