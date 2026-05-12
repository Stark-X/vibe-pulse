#![cfg_attr(not(debug_assertions), windows_subsystem = "windows")]

mod agents;
mod hyprland;
mod state;
mod tmux;

use state::AgentInfo;
use sysinfo::System;
use std::sync::{Arc, Mutex};
use tauri::Emitter;

#[tauri::command]
fn get_agents(state: tauri::State<Arc<Mutex<Vec<AgentInfo>>>>) -> Vec<AgentInfo> {
    state.lock().unwrap_or_else(|e| e.into_inner()).clone()
}

#[tauri::command]
fn focus_window(address: String) -> Result<(), String> {
    hyprland::focus_window(&address)
}


fn main() {
    let agent_state: Arc<Mutex<Vec<AgentInfo>>> = Arc::new(Mutex::new(vec![]));

    tauri::Builder::default()
        .plugin(tauri_plugin_global_shortcut::Builder::new().build())
        .manage(agent_state.clone())
        .setup(move |app| {
            let handle = app.handle().clone();
            let state = agent_state.clone();

            std::thread::spawn(move || {
                let mut last_hash = String::new();

                loop {
                    std::thread::sleep(std::time::Duration::from_millis(2000));

                    // Create fresh System each poll — avoids stale process cache in sysinfo 0.32
                    let mut sys = System::new();

                    let clients = if hyprland::available() {
                        hyprland::get_clients()
                    } else {
                        vec![]
                    };

                    let mut snapshot = agents::scan(&mut sys);

                    for agent in &mut snapshot {
                        agent.window_address =
                            hyprland::find_window_address(agent.pid, &clients);

                        if agent.window_address.is_none() {
                            if let Some(term_pid) =
                                tmux::find_terminal_for_tmux_pane(agent.pid)
                            {
                                agent.window_address =
                                    hyprland::find_window_address(term_pid, &clients);
                            }
                        }
                    }

                    let hash = format!("{:?}", snapshot);
                    if hash != last_hash {
                        last_hash = hash;
                        {
                            let mut guard = state.lock().unwrap_or_else(|e| e.into_inner());
                            *guard = snapshot.clone();
                        }
                        let _ = handle.emit("agents-updated", &snapshot);
                    }
                }
            });

            Ok(())
        })
        .invoke_handler(tauri::generate_handler![get_agents, focus_window])
        .run(tauri::generate_context!())
        .expect("error while running tauri application");
}
