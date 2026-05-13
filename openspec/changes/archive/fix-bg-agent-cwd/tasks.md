# Tasks: Fix Background Agent Working Directory

- [x] 1.1 ClaudeMetaReader.h: add `QString cwd` field to ClaudeMeta struct
- [x] 1.2 ClaudeMetaReader.cpp: assign `m.cwd` from session JSON before early return on empty sessionId
- [x] 2.1 main.cpp enrichAgents(): override `a.cwd` and `a.name` from `m.cwd` when non-empty
- [x] 3.1 ProcScanner.cpp kRules: change Claude Code argvExclude from nullptr to " agents"
- [x] 4.1 Build and verify: cmake --build pulse/build, no warnings
