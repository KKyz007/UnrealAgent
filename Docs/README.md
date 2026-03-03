# UnrealAgent

AI Agent control interface for Unreal Editor via MCP (Model Context Protocol).

## Overview

UnrealAgent is an Unreal Engine editor plugin that allows AI agents (Claude, Cursor, CodeBuddy, etc.) to query and control the UE Editor through the MCP protocol.

### Architecture

```
AI Client (CodeBuddy/Claude/Cursor)
    │ stdio
    ▼
Python MCP Server (FastMCP)
    │ TCP :55557
    ▼
UE Plugin (C++ Editor Module)
    │ JSON-RPC 2.0
    ▼
Unreal Editor API
```

### Available Tools (15)

| Group | Tool | Description |
|-------|------|-------------|
| Project | `get_project_info` | Project name, engine version, modules, plugins |
| Project | `get_editor_state` | Active level, PIE status, selected actors |
| Asset | `list_assets` | List assets by path, class filter, recursive |
| Asset | `search_assets` | Search assets by name |
| Asset | `get_asset_info` | Asset metadata and tags |
| Asset | `get_asset_references` | Referencers and dependencies graph |
| World | `get_world_outliner` | All actors in level with properties |
| World | `get_current_level` | Level name, path, streaming sub-levels |
| World | `get_actor_details` | Full actor transform, components, tags |
| Actor | `create_actor` | Spawn actor by class with transform |
| Actor | `delete_actor` | Remove actor from level |
| Actor | `select_actors` | Select/deselect actors in editor |
| Viewport | `get_viewport_camera` | Camera position and rotation |
| Viewport | `move_viewport_camera` | Set camera position/rotation |
| Viewport | `focus_on_actor` | Focus viewport on specific actor |

## Quick Start

### Prerequisites

- Unreal Engine 5.7+
- Python 3.10+

### Setup

1. Place the `UnrealAgent` folder in your project's `Plugins/` directory
2. Open the project in UE Editor — the plugin loads automatically
3. Install the Python MCP Server:

```bash
cd Plugins/UnrealAgent/MCPServer
python -m venv .venv
.venv/Scripts/pip install -e .    # Windows
# .venv/bin/pip install -e .      # macOS/Linux
```

4. Add MCP configuration to your project root `.mcp.json`:

```json
{
  "mcpServers": {
    "unreal-agent": {
      "type": "stdio",
      "command": "<absolute-path-to>/MCPServer/.venv/Scripts/python.exe",
      "args": ["-m", "unreal_agent_mcp"],
      "env": {
        "UNREAL_AGENT_HOST": "127.0.0.1",
        "UNREAL_AGENT_PORT": "55557"
      }
    }
  }
}
```

5. Restart your AI client — the tools will be available immediately.

### Plugin Settings

Edit → Project Settings → Plugins → UnrealAgent

| Setting | Default | Description |
|---------|---------|-------------|
| ServerPort | 55557 | TCP listen port |
| bAutoStart | true | Auto-start on editor launch |
| BindAddress | 127.0.0.1 | Bind address (local only) |
| MaxConnections | 16 | Max concurrent TCP connections |
| bVerboseLogging | false | Enable detailed logging |

## Project Structure

```
UnrealAgent/
├── UnrealAgent.uplugin
├── Config/
│   └── DefaultUnrealAgent.ini
├── Source/UnrealAgent/
│   ├── UnrealAgent.Build.cs
│   ├── Public/
│   │   ├── UnrealAgent.h
│   │   ├── Settings/UASettings.h
│   │   ├── Server/UATcpServer.h, UAClientConnection.h
│   │   ├── Protocol/UAProtocolTypes.h, UAJsonRpcHandler.h
│   │   └── Commands/UACommandBase.h, UACommandRegistry.h,
│   │       UAProjectCommands.h, UAAssetCommands.h,
│   │       UAWorldCommands.h, UAActorCommands.h,
│   │       UAViewportCommands.h
│   └── Private/
│       └── (corresponding .cpp files)
├── MCPServer/
│   ├── pyproject.toml
│   └── src/unreal_agent_mcp/
│       ├── server.py, connection.py
│       └── tools/ (project, assets, world, actors, viewport)
└── Docs/
    ├── development-log.md
    └── api-reference.md
```

## License

Copyright KuoYu. All Rights Reserved.
