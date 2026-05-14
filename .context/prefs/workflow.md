# Development Workflow Rules

> 此文件定义 LLM 开发工作流的强制规则。
> 所有 LLM 工具在执行任务时必须遵守，不可跳过任何步骤。

## Full Flow (MUST follow, no exceptions)

### feat (新功能)
1. 理解需求，分析影响范围
2. 读取现有代码，理解模式
3. 编写实现代码
4. `make build` 验证编译通过
5. 手动验证热重载效果
6. 更新 OpenSpec 文档（若 API 变更）

### fix (缺陷修复)
1. 复现问题，确认症状
2. 读取实际进程/文件状态定位根因
3. 修复代码
4. 验证编译通过
5. 确认修复生效

### refactor (重构)
1. 确保编译通过后再改
2. 小步修改，每步 `cargo check`
3. 不改变外部 IPC 接口

## Project-Specific Rules

- **进程检测**：用 `proc.name()` (comm)，不用 `exe().file_name()`（会读到版本号）
- **线程过滤**：读 `/proc/pid/status` Pid==Tgid 才是主进程
- **sysinfo 缓存**：每次 poll 新建 `System`，不复用（避免旧进程残留）
- **每次 poll 新建 System**：`let mut sys = System::new();`
- **稳定排序**：`.sort_by_key(|a| a.pid)` 防止 UI 跳动

## Context Logging (决策记录)

当你做出以下决策时，MUST 追加到 `.context/current/branches/<当前分支>/session.log`：

1. **方案选择**：选 A 不选 B 时，记录原因
2. **Bug 发现与修复**：根因 + 修复方法 + 教训
3. **API/架构决策**：接口设计选择
4. **放弃的方案**：为什么放弃

追加格式：

## <ISO-8601 时间>
**Decision**: <你选择了什么>
**Alternatives**: <被排除的方案>
**Reason**: <为什么>
**Risk**: <潜在风险>
