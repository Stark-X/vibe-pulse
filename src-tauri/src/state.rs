use serde::{Deserialize, Serialize};

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
#[serde(rename_all = "lowercase")]
pub enum AgentStatus {
    Running,
    Exited,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
pub struct PermissionRequest {
    pub session_id: String,
    pub tool_name: String,
    #[serde(default)]
    pub description: Option<String>,
    pub input: serde_json::Value,
    #[serde(default)]
    pub created_at_ms: Option<u64>,
    #[serde(default)]
    pub expires_at_ms: Option<u64>,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
pub struct AgentInfo {
    pub name: String,                     // cwd basename only
    pub tool_type: String,
    pub pid: u32,
    pub status: AgentStatus,
    pub cwd: String,
    pub session_id: Option<String>,
    pub session_name: Option<String>,     // from ~/.claude/sessions/<pid>.json "name"
    pub current_step: Option<String>,     // last tool_use from transcript
    pub window_address: Option<String>,
    pub pending_permission: Option<PermissionRequest>,
}

#[allow(dead_code)]
#[derive(Debug, Clone, Deserialize)]
pub struct HyprWorkspace {
    pub id: i32,
    pub name: String,
}

#[allow(dead_code)]
#[derive(Debug, Clone, Deserialize)]
pub struct HyprClient {
    pub address: String,
    pub pid: i64,
    pub class: String,
    pub title: String,
    pub workspace: HyprWorkspace,
}
