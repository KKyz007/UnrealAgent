# API Reference

UnrealAgent exposes 15 tools via MCP (Model Context Protocol). All tools communicate through JSON-RPC 2.0 over TCP.

**Connection**: `127.0.0.1:55557` (configurable in Project Settings)
**Protocol**: Content-Length framed JSON-RPC 2.0

---

## Project Tools

### get_project_info

Get detailed information about the current Unreal project.

**Parameters**: None

**Response**:
```json
{
  "project_name": "Aura",
  "engine_version": "5.7.1-48512491+++UE5+Release-5.7",
  "project_dir": "I:/Aura/",
  "modules": ["Aura"],
  "plugins": [
    {"name": "GameplayAbilities", "enabled": true}
  ]
}
```

---

### get_editor_state

Get current editor state including active level, play status, and selection.

**Parameters**: None

**Response**:
```json
{
  "current_level": "Enw",
  "world_path": "/Game/Map/Enw.Enw",
  "is_playing": false,
  "selected_actors": [
    {
      "name": "PointLight",
      "class": "PointLight",
      "location": {"x": 0, "y": 0, "z": 300}
    }
  ],
  "selected_count": 1
}
```

---

## Asset Tools

### list_assets

List assets in the project by path.

**Parameters**:
| Param | Type | Required | Default | Description |
|-------|------|----------|---------|-------------|
| path | string | No | /Game | Asset path to list |
| class_filter | string | No | "" | Filter by class name (e.g., Blueprint, StaticMesh) |
| recursive | boolean | No | true | Search subdirectories |

**Response**:
```json
{
  "assets": [
    {
      "name": "BP_AuraCharacter",
      "path": "/Game/Blueprints/Chracter/Aura/BP_AuraCharacter.BP_AuraCharacter",
      "class": "Blueprint",
      "package": "/Game/Blueprints/Chracter/Aura/BP_AuraCharacter"
    }
  ],
  "count": 1,
  "path": "/Game"
}
```

---

### search_assets

Search for assets by name with optional class filter.

**Parameters**:
| Param | Type | Required | Description |
|-------|------|----------|-------------|
| query | string | Yes | Case-insensitive search string |
| class_filter | string | No | Filter by class name |

**Response**: Same format as `list_assets`.

---

### get_asset_info

Get detailed metadata about a specific asset.

**Parameters**:
| Param | Type | Required | Description |
|-------|------|----------|-------------|
| asset_path | string | Yes | Full asset path (e.g., /Game/Blueprints/BP_Player) |

**Response**:
```json
{
  "name": "BP_AuraCharacter",
  "path": "/Game/Blueprints/Chracter/Aura/BP_AuraCharacter.BP_AuraCharacter",
  "class": "Blueprint",
  "package": "/Game/Blueprints/Chracter/Aura/BP_AuraCharacter",
  "tags": {
    "ParentClass": "/Script/CoreUObject.Class'/Script/Aura.AuraCharacter'",
    "BlueprintType": "BPTYPE_Normal",
    "IsDataOnly": "False"
  }
}
```

---

### get_asset_references

Get the reference graph for an asset (what uses it, what it depends on).

**Parameters**:
| Param | Type | Required | Description |
|-------|------|----------|-------------|
| asset_path | string | Yes | Full asset path to query |

**Response**:
```json
{
  "asset_path": "/Game/Blueprints/Chracter/Aura/BP_AuraCharacter.BP_AuraCharacter",
  "referencers": ["/Game/Blueprints/Game/BP_AuraGameMode"],
  "referencer_count": 1,
  "dependencies": ["/Script/Aura", "/Game/Assets/Characters/Aura/SKM_Aura"],
  "dependency_count": 2
}
```

---

## World Tools

### get_world_outliner

Get all actors in the current level.

**Parameters**:
| Param | Type | Required | Description |
|-------|------|----------|-------------|
| class_filter | string | No | Filter by class name (case-insensitive) |

**Response**:
```json
{
  "actors": [
    {
      "name": "DirectionalLight",
      "class": "DirectionalLight",
      "path": "/Game/Map/Enw.Enw:PersistentLevel.DirectionalLight_1",
      "location": {"x": 0, "y": 0, "z": 0},
      "rotation": {"pitch": -67.09, "yaw": -110.38, "roll": -18.83},
      "scale": {"x": 2.5, "y": 2.5, "z": 2.5},
      "is_hidden": false
    }
  ],
  "count": 1,
  "level": "Enw"
}
```

---

### get_current_level

Get current level information.

**Parameters**: None

**Response**:
```json
{
  "name": "Enw",
  "path": "/Game/Map/Enw.Enw",
  "streaming_levels": [
    {"name": "SubLevel_01", "is_loaded": true, "is_visible": true}
  ]
}
```

---

### get_actor_details

Get detailed properties of a specific actor.

**Parameters**:
| Param | Type | Required | Description |
|-------|------|----------|-------------|
| actor_name | string | Yes | Actor label or internal name |

**Response**:
```json
{
  "name": "DirectionalLight",
  "internal_name": "DirectionalLight_1",
  "class": "DirectionalLight",
  "path": "/Game/Map/Enw.Enw:PersistentLevel.DirectionalLight_1",
  "location": {"x": 0, "y": 0, "z": 0},
  "rotation": {"pitch": -67.09, "yaw": -110.38, "roll": -18.83},
  "scale": {"x": 2.5, "y": 2.5, "z": 2.5},
  "components": [
    {"name": "LightComponent0", "class": "DirectionalLightComponent"},
    {"name": "ArrowComponent0", "class": "ArrowComponent"}
  ],
  "is_hidden": false,
  "is_editable": true,
  "tags": []
}
```

---

## Actor Tools

### create_actor

Spawn a new actor in the current level.

**Parameters**:
| Param | Type | Required | Default | Description |
|-------|------|----------|---------|-------------|
| class_name | string | Yes | — | Actor class (StaticMeshActor, PointLight, DirectionalLight, SpotLight, CameraActor) |
| label | string | No | "" | Display label |
| location_x | number | No | 0 | World X position |
| location_y | number | No | 0 | World Y position |
| location_z | number | No | 0 | World Z position |
| rotation_pitch | number | No | 0 | Pitch in degrees |
| rotation_yaw | number | No | 0 | Yaw in degrees |
| rotation_roll | number | No | 0 | Roll in degrees |
| scale_x | number | No | 1 | X scale |
| scale_y | number | No | 1 | Y scale |
| scale_z | number | No | 1 | Z scale |

**Response**:
```json
{
  "name": "MCP_TestLight",
  "class": "PointLight",
  "location": {"x": 500, "y": 0, "z": 300},
  "success": true
}
```

---

### delete_actor

Remove an actor from the current level.

**Parameters**:
| Param | Type | Required | Description |
|-------|------|----------|-------------|
| actor_name | string | Yes | Actor label or internal name |

**Response**:
```json
{
  "deleted_actor": "MCP_TestLight",
  "class": "PointLight",
  "success": true
}
```

---

### select_actors

Select specific actors in the editor. Pass empty array to clear selection.

**Parameters**:
| Param | Type | Required | Description |
|-------|------|----------|-------------|
| actor_names | string[] | Yes | Array of actor names to select |

**Response**:
```json
{
  "selected": ["MCP_TestLight", "DirectionalLight"],
  "selected_count": 2,
  "not_found": []
}
```

---

## Viewport Tools

### get_viewport_camera

Get current editor viewport camera position and rotation.

**Parameters**: None

**Response**:
```json
{
  "location": {"x": -266.67, "y": -16.99, "z": 754.51},
  "rotation": {"pitch": -55, "yaw": 0, "roll": 0}
}
```

---

### move_viewport_camera

Move the editor viewport camera. Only provided values are changed.

**Parameters**:
| Param | Type | Required | Description |
|-------|------|----------|-------------|
| location_x | number | No | Camera X position |
| location_y | number | No | Camera Y position |
| location_z | number | No | Camera Z position |
| rotation_pitch | number | No | Pitch in degrees |
| rotation_yaw | number | No | Yaw in degrees |
| rotation_roll | number | No | Roll in degrees |

**Response**:
```json
{
  "location": {"x": 500, "y": 0, "z": 500},
  "rotation": {"pitch": -30, "yaw": 0, "roll": 0},
  "success": true
}
```

---

### focus_on_actor

Focus the editor viewport camera on a specific actor.

**Parameters**:
| Param | Type | Required | Description |
|-------|------|----------|-------------|
| actor_name | string | Yes | Actor label or name to focus on |

**Response**:
```json
{
  "focused_actor": "MCP_TestLight",
  "success": true
}
```

---

## Error Codes

Standard JSON-RPC 2.0 error codes:

| Code | Name | Description |
|------|------|-------------|
| -32700 | Parse Error | Invalid JSON |
| -32600 | Invalid Request | Missing jsonrpc/method fields |
| -32601 | Method Not Found | Unknown method name |
| -32602 | Invalid Params | Missing or invalid parameters |
| -32603 | Internal Error | Server internal error |
| -32000 | Server Error | Generic server-side error |
| -32001 | Execution Error | Command execution failed |

## Wire Protocol

Messages use LSP-style Content-Length framing:

```
Content-Length: 82\r\n
\r\n
{"jsonrpc":"2.0","method":"get_project_info","params":{},"id":1}
```

Response:
```
Content-Length: 150\r\n
\r\n
{"jsonrpc":"2.0","id":1,"result":{"project_name":"Aura",...}}
```
