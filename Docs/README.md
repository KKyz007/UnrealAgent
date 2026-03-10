<p align="center">
  <img src="https://img.shields.io/badge/Unreal%20Engine-5.5%2B-313131?style=for-the-badge&logo=unrealengine" alt="UE 5.5+">
  <img src="https://img.shields.io/badge/Python-3.10%2B-3776AB?style=for-the-badge&logo=python&logoColor=white" alt="Python 3.10+">
  <img src="https://img.shields.io/badge/MCP-Protocol-6366F1?style=for-the-badge" alt="MCP">
  <img src="https://img.shields.io/badge/License-MIT-green?style=for-the-badge" alt="MIT">
</p>

# UnrealAgent

> AI Agent control interface for Unreal Editor via [MCP](https://modelcontextprotocol.io/) (Model Context Protocol).

Let AI assistants (**Claude, Cursor, Windsurf, CodeBuddy**, etc.) directly query and control your Unreal Editor — manage actors, edit blueprints, create materials, execute Python, capture screenshots, and much more.

```
┌─────────────────────────────────────────┐
│  AI Client (Claude / Cursor / Windsurf) │
└──────────────────┬──────────────────────┘
                   │  MCP (stdio)
┌──────────────────▼──────────────────────┐
│  Python MCP Server (50 tools)           │
│  pip install -e ./MCPServer             │
└──────────────────┬──────────────────────┘
                   │  TCP :55557 (JSON-RPC 2.0)
┌──────────────────▼──────────────────────┐
│  UE Plugin (C++ Editor Module)          │
│  Auto-starts with Unreal Editor         │
└──────────────────┬──────────────────────┘
                   │
┌──────────────────▼──────────────────────┐
│  Unreal Editor API                      │
└─────────────────────────────────────────┘
```

---

## ⚡ Quick Start

> **Just paste this to your AI assistant:**
>
> *"帮我部署这个项目 https://github.com/ky256/UnrealAgent"*
>
> or in English:
>
> *"Help me deploy this project: https://github.com/ky256/UnrealAgent"*
>
> The AI will read this README and complete the setup automatically.

---

## 📦 Deployment

### Prerequisites

| Requirement | Version | Check |
|---|---|---|
| Unreal Engine | 5.5+ | Editor must be running |
| Python | 3.10+ | `python --version` |
| Package manager | `uv` / `pip` / `pipx` | Any one is fine |

### Step 1 — Install the UE Plugin

Choose **one** method:

```bash
# Option A: Git submodule (recommended for version-controlled projects)
cd <your-ue-project>/Plugins
git submodule add https://github.com/ky256/UnrealAgent.git

# Option B: Direct clone
cd <your-ue-project>/Plugins
git clone https://github.com/ky256/UnrealAgent.git

# Option C: Already a submodule? Just init it
git submodule update --init --recursive
```

Then **restart Unreal Editor**. The plugin auto-enables and starts its TCP server.

> ✅ **Verify:** Output Log should show `LogUnrealAgent: TCP server listening on 127.0.0.1:55557`

### Step 2 — Install the MCP Server

```bash
cd <your-ue-project>/Plugins/UnrealAgent/MCPServer

# Using uv (recommended, faster)
uv pip install -e .

# Or using pip
pip install -e .
```

> ✅ **Verify:** `python -m unreal_agent_mcp --help` should run without errors.

### Step 3 — Configure Your AI Client

Add MCP server configuration. Pick your client:

<details>
<summary><b>Claude Desktop</b></summary>

Edit config file:
- **Windows:** `%APPDATA%\Claude\claude_desktop_config.json`
- **macOS:** `~/Library/Application Support/Claude/claude_desktop_config.json`

```json
{
  "mcpServers": {
    "unreal-agent": {
      "command": "python",
      "args": ["-m", "unreal_agent_mcp"],
      "env": {
        "UNREAL_AGENT_HOST": "127.0.0.1",
        "UNREAL_AGENT_PORT": "55557"
      }
    }
  }
}
```

> If the file already has other MCP servers, merge `"unreal-agent"` into the existing `"mcpServers"` object.

</details>

<details>
<summary><b>Cursor</b></summary>

Edit `.cursor/mcp.json` in your project root (or global `~/.cursor/mcp.json`):

```json
{
  "mcpServers": {
    "unreal-agent": {
      "command": "python",
      "args": ["-m", "unreal_agent_mcp"],
      "env": {
        "UNREAL_AGENT_HOST": "127.0.0.1",
        "UNREAL_AGENT_PORT": "55557"
      }
    }
  }
}
```

</details>

<details>
<summary><b>Windsurf</b></summary>

Edit `~/.codeium/windsurf/mcp_config.json`:

```json
{
  "mcpServers": {
    "unreal-agent": {
      "command": "python",
      "args": ["-m", "unreal_agent_mcp"],
      "env": {
        "UNREAL_AGENT_HOST": "127.0.0.1",
        "UNREAL_AGENT_PORT": "55557"
      }
    }
  }
}
```

</details>

<details>
<summary><b>CodeBuddy</b></summary>

```bash
codebuddy mcp add unreal-agent -- python -m unreal_agent_mcp
```

</details>

<details>
<summary><b>Other MCP Clients</b></summary>

Any client supporting MCP stdio transport:

| Field | Value |
|---|---|
| Command | `python -m unreal_agent_mcp` |
| Transport | stdio |
| Env | `UNREAL_AGENT_HOST=127.0.0.1` `UNREAL_AGENT_PORT=55557` |

</details>

### Step 4 — Verify

1. **Restart** your AI client.
2. Make sure **Unreal Editor** is running.
3. Ask: **"What's my current UE project name?"**
4. The AI should call `get_project_info` and return your project details. ✅

---

## 🔧 Available Tools (50)

### Project & Editor

| Tool | Description |
|------|-------------|
| `get_project_info` | Project name, engine version, modules, plugins |
| `get_editor_state` | Active level, PIE status, selected actors |
| `undo` | Undo last editor operation(s) |
| `redo` | Redo last undone operation(s) |

### World & Actors

| Tool | Description |
|------|-------------|
| `get_world_outliner` | All actors in level with properties |
| `get_current_level` | Level name, path, streaming sub-levels |
| `get_actor_details` | Full actor transform, components, tags |
| `create_actor` | Spawn actor by class with transform |
| `delete_actor` | Remove actor from level |
| `select_actors` | Select/deselect actors in editor |

### Assets

| Tool | Description |
|------|-------------|
| `list_assets` | List assets by path, class filter, recursive |
| `search_assets` | Search assets by name |
| `get_asset_info` | Asset metadata and tags |
| `get_asset_references` | Referencers and dependencies graph |
| `create_asset` | Create new asset by class and path |
| `duplicate_asset` | Duplicate an existing asset |
| `rename_asset` | Rename or move an asset |
| `delete_asset` | Delete asset from project |
| `save_asset` | Save specific asset or all dirty assets |
| `create_folder` | Create content browser folder |

### Blueprints (Visual Scripting)

| Tool | Description |
|------|-------------|
| `get_blueprint_overview` | Blueprint class hierarchy, interfaces, flags |
| `get_blueprint_graph` | Node graph with connections |
| `get_blueprint_variables` | List all variables with types and defaults |
| `get_blueprint_functions` | List all functions and their signatures |
| `add_node` | Add a node to blueprint graph |
| `delete_node` | Remove a node from blueprint graph |
| `connect_pins` | Connect two node pins |
| `disconnect_pin` | Disconnect a pin |
| `add_variable` | Add a new variable to blueprint |
| `add_function` | Add a new function to blueprint |
| `compile_blueprint` | Compile and validate blueprint |

### Materials

| Tool | Description |
|------|-------------|
| `get_material_graph` | Material expression graph structure |
| `create_material_expression` | Add material expression node |
| `delete_material_expression` | Remove material expression |
| `connect_material_property` | Connect expression to material output |
| `connect_material_expressions` | Connect two expression pins |
| `set_expression_value` | Set expression parameter values |
| `recompile_material` | Recompile material shader |
| `layout_material_expressions` | Auto-layout expression nodes |
| `get_material_parameters` | List material parameters |
| `set_material_instance_param` | Set material instance parameter value |
| `set_material_property` | Set material-level properties |

### Viewport & Screenshots

| Tool | Description |
|------|-------------|
| `get_viewport_camera` | Camera position and rotation |
| `move_viewport_camera` | Set camera position/rotation |
| `focus_on_actor` | Focus viewport on specific actor |
| `take_screenshot` | Capture viewport screenshot |
| `get_asset_thumbnail` | Get asset thumbnail image |
| `read_image` | Read image file from disk |

### Context & Logs

| Tool | Description |
|------|-------------|
| `get_open_editors` | Currently open asset editors |
| `get_selected_assets` | Selected assets in content browser |
| `get_browser_path` | Current content browser path |
| `get_message_log` | UE message log entries |
| `get_output_log` | Recent output log lines |
| `get_property` | Get actor property value by path |
| `get_recent_events` | Recent editor events |
| `get_events_since` | Events since a timestamp |

### Python Execution

| Tool | Description |
|------|-------------|
| `execute_python` | Run Python code in UE editor context (stateful, undo-wrapped, timeout-protected) |
| `reset_python_context` | Reset the shared Python execution context |

### Knowledge Base

| Tool | Description |
|------|-------------|
| `query_knowledge` | Query the local knowledge base |
| `save_knowledge` | Save entry to knowledge base |
| `get_knowledge_stats` | Knowledge base statistics |

> 💡 **`execute_python`** is the universal fallback — if no dedicated tool exists, AI can always run arbitrary Python with full `unreal` module access.

---

## ⚙️ Plugin Settings

**Edit → Project Settings → Plugins → UnrealAgent**

| Setting | Default | Description |
|---------|---------|-------------|
| `ServerPort` | `55557` | TCP listen port |
| `bAutoStart` | `true` | Auto-start server on editor launch |
| `BindAddress` | `127.0.0.1` | Bind address (local only by default) |
| `MaxConnections` | `4` | Max concurrent TCP connections |
| `bVerboseLogging` | `false` | Enable detailed logging |

---

## 🔍 Troubleshooting

| Symptom | Fix |
|---------|-----|
| AI says "cannot connect" | Make sure UE Editor is running. Check Output Log for `TCP server listening on 127.0.0.1:55557` |
| AI doesn't see tools | Restart AI client after config change. Validate JSON syntax |
| `python -m unreal_agent_mcp` not found | Run `pip install -e .` in `MCPServer/`. Check correct Python is on PATH |
| Python execution says "PythonScriptPlugin not loaded" | Enable Python Editor Script Plugin: Edit → Plugins → search "Python" → enable → restart |
| Connection timeout | Check firewall isn't blocking port 55557. Try `telnet 127.0.0.1 55557` |

---

## 📁 Project Structure

```
UnrealAgent/
├── UnrealAgent.uplugin            # Plugin descriptor
├── Config/
│   └── DefaultUnrealAgent.ini     # Default settings
├── Source/UnrealAgent/
│   ├── Public/
│   │   ├── Commands/              # 16 command modules (one per tool group)
│   │   ├── Server/                # TCP server + client connection
│   │   ├── Protocol/              # JSON-RPC 2.0 handler
│   │   └── Settings/              # Plugin configuration
│   └── Private/                   # Corresponding .cpp implementations
├── MCPServer/
│   ├── pyproject.toml             # Python package config (pip install -e .)
│   └── src/unreal_agent_mcp/
│       ├── server.py              # FastMCP instance + resources
│       ├── connection.py          # TCP client to UE plugin
│       ├── knowledge_store.py     # Local knowledge base
│       ├── ast_fingerprint.py     # Code structural analysis
│       ├── telemetry.py           # Optional anonymous telemetry
│       └── tools/                 # 15 tool modules (50 tools total)
│           ├── project.py         # Project info & editor state
│           ├── assets.py          # Asset query
│           ├── asset_manage.py    # Asset CRUD operations
│           ├── world.py           # World & level inspection
│           ├── actors.py          # Actor creation & management
│           ├── viewport.py        # Viewport camera control
│           ├── editor.py          # Undo / redo
│           ├── python.py          # Python execution engine
│           ├── context.py         # Editor context & logs
│           ├── properties.py      # Property inspection
│           ├── blueprints.py      # Blueprint visual scripting
│           ├── materials.py       # Material graph editing
│           ├── screenshots.py     # Screenshot & thumbnail capture
│           ├── events.py          # Editor event stream
│           └── knowledge.py       # Knowledge base tools
└── Docs/                          # Design docs & test plans
```

---

## 📊 Telemetry (opt-in)

Disabled by default. To enable anonymous usage telemetry:

```bash
export UNREAL_AGENT_TELEMETRY=1   # Linux/macOS
set UNREAL_AGENT_TELEMETRY=1      # Windows
```

Collects only structural code fingerprints and execution stats — never raw code. See [`telemetry.py`](MCPServer/src/unreal_agent_mcp/telemetry.py) for details.

---

## License

MIT License — see [LICENSE](LICENSE) file.
