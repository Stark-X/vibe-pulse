# Tasks: Agent Permission Inline Actions

## Phase 1: Backend Fixes (gemini skeleton needs corrections)

- [x] 1.0 state.rs — PermissionRequest struct and session_id field added to AgentInfo (gemini)
- [x] 1.0 agents.rs — claude_metadata returns session_id, check_pending_permissions added (gemini)
- [x] 1.0 main.rs — respond_permission Tauri command added (gemini)
- [x] 1.1 state.rs — fix PermissionRequest: description→Option<String>, input→serde_json::Value, add serde(default) for optional fields
- [x] 1.2 main.rs — respond_permission: add sessionId format validation before path construction
- [x] 1.3 agents.rs — check_pending_permissions: add stale cleanup (mtime > 45s → delete and skip)
- [x] 1.4 cargo check passes zero errors after all fixes

## Phase 2: Hook Infrastructure (not yet implemented)

- [x] 2.1 main.rs setup — create ~/.claude/pulse/{pending,response,auto-approve} dirs (mode 0700)
- [x] 2.2 main.rs setup — write permission-hook.sh to ~/.claude/pulse/ (idempotent), chmod 0700
- [x] 2.3 main.rs setup — inject PermissionRequest hook into ~/.claude/settings.json (idempotent, preserve existing hooks, tmp+rename)
- [x] 2.4 main.rs setup — clean_stale_permissions on startup (delete pending/response > 45s old)

## Phase 3: Frontend Fixes

- [x] 3.0 main.js — buildPermissionRow and respondPermission skeleton (gemini)
- [x] 3.1 main.js — fix input display: input is serde_json::Value object, not a string needing JSON.parse
- [x] 3.2 main.js — replace browser prompt() with inline <input> + confirm button for Deny+Reason flow
- [x] 3.3 main.js — respondPermission: call resizeWindow() after response to collapse row
- [x] 3.4 styles.css — add perm-row, perm-head, perm-tool, perm-desc, perm-input CSS classes
- [x] 3.5 styles.css — add btn.approve-all style (peach/orange color)

## Phase 4: Verification

- [ ] 4.1 Trigger claude permission request → Pulse shows expanded row within 2s
- [ ] 4.2 Approve → Claude proceeds, row collapses
- [ ] 4.3 Deny → Claude gets block, row collapses
- [ ] 4.4 Approve All → auto-approve file created, next same tool auto-approved
- [ ] 4.5 Deny+ → inline input, feedback sent to Claude
- [ ] 4.6 30s timeout → row disappears, Claude falls back to terminal
- [ ] 4.7 settings.json injection is idempotent (run twice → single hook entry)
