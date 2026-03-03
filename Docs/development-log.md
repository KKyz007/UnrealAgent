# Development Log

## 2026-03-03 — v0.1.0 Initial Implementation

### Phase 1: Plugin Skeleton
- Created `UnrealAgent.uplugin` (Editor type, Category: KuoYu)
- Module entry point with auto-start TCP server and editor menu registration
- `UUASettings` developer settings (port, auto-start, bind address, max connections)

### Phase 2: Communication Layer
- `UATcpServer`: FTcpListener + FTSTicker (~100Hz game thread tick)
- `UAClientConnection`: Non-blocking socket with Content-Length message framing
- `UAJsonRpcHandler`: JSON-RPC 2.0 parser/dispatcher with standard error codes

### Phase 3: Command System
- `UACommandBase`: Abstract base class with method registration and JSON Schema generation
- `UACommandRegistry`: Method-name-to-handler dispatch map with `list_tools` built-in

### Phase 4: Tool Commands (15 tools)
- **Project**: get_project_info, get_editor_state
- **Asset**: list_assets, search_assets, get_asset_info, get_asset_references
- **World**: get_world_outliner, get_current_level, get_actor_details
- **Actor**: create_actor, delete_actor, select_actors
- **Viewport**: get_viewport_camera, move_viewport_camera, focus_on_actor

### Phase 5: Python MCP Server
- FastMCP server with stdio transport
- TCP client with Content-Length framing and auto-reconnect
- MCP Resources: `unreal://project/info`, `unreal://editor/state`
- All 15 tools registered as MCP tools

### Compilation Fixes (UE 5.7 API Changes)
| File | Issue | Fix |
|------|-------|-----|
| UATcpServer.h | FIPv4Endpoint undefined | Added `#include "Interfaces/IPv4/IPv4Endpoint.h"` |
| UAActorCommands.cpp | ANY_PACKAGE removed in UE 5.7 | Replaced with `FindFirstObject<UClass>()` |
| UAActorCommands.cpp | SpawnActor takes references not pointers | Changed `&Location, &Rotation` to `Location, Rotation` |
| UAProjectCommands.cpp | USelection incomplete type | Added `#include "Selection.h"` |
| UAAssetCommands.cpp | GetTagsAndValues() removed in UE 5.7 | Replaced with `EnumerateTags()` + lambda |

### Connection Stability Fix
- **Problem**: Dead TCP connections not cleaned up, causing "max connections reached" rejection
- **Root cause**: `GetConnectionState()` unreliable for detecting dead non-blocking sockets
- **Fix**: Added Recv+Peek probe for dead connection detection; cleanup stale connections before accepting new ones; raised default MaxConnections 4→16

### Additional Compilation Fixes (Dead Connection Detection)
| Issue | Fix |
|-------|-----|
| `ESocketWaitConditions::WaitForReadOrError` doesn't exist | Changed to `ESocketWaitConditions::WaitForRead` |
| `HasPendingData(bool)` parameter type mismatch | Used `GetConnectionState()` for error check |
| Unused variable `bIsReadable` | Removed |

### Full Test Results — 15/15 Passed
All tools tested via CodeBuddy MCP integration against live Aura project (UE 5.7.1).

### Editor Crash Fix — TypedElement Assert
- **Symptom**: `delete_actor` on a selected actor triggers assert: `Element type ID '0' has not been registered!`
- **Root cause**: UE 5.7's USelection holds TypedElement handles; destroying an actor without deselecting leaves stale handles
- **Fix**: Call `GEditor->SelectActor(Actor, false, true)` before `Actor->Destroy()` in `UAActorCommands.cpp`

### Universal Execution Layer — execute_python (Phase 1)

**设计理念**: 从第一性原理出发，与其为每种 UE 操作手写 MCP tool（枚举模型），不如提供一个万能执行接口，让 AI 直接写 Python 代码调用 UE API。详见 `Docs/design-universal-execution.md`。

**架构**: 1+N 模式 — 1 个万能 `execute_python` + N 个高频快捷 Tool 共存。

**C++ 实现** (`UAPythonCommands`):
- 调用 `IPythonScriptPlugin::ExecPythonCommandEx()` 执行任意 Python 代码
- `ExecuteFile` 模式 + `Public` scope → 有状态执行（变量跨调用保持）
- `Unattended` flag → 抑制 UI 弹窗
- `FPythonCommandEx::LogOutput` 按类型分离 stdout/stderr
- 输出截断上限 64KB
- `PythonScriptPlugin` 可选依赖，未启用时优雅降级

**MCP 新增 2 个 tool**（总计 17 个）:
- `execute_python(code)` — 执行 Python 代码，返回 output/error
- `reset_python_context()` — 清空共享 Python 上下文

**执行日志**: 每次 `execute_python` 调用记录到 `Cache/execution_log.jsonl`（JSONL 格式），包含 code、timestamp、success、output、execution_ms，为 Phase 2 模式识别积累数据。

**Bug 修复**:
- `reset_python_context` 返回 `success: false` 但实际生效 — 原因：reset 脚本中 `del _k` 在 for 循环未执行时 `_k` 未定义，触发 NameError。修复：用 `globals()[_ua_k]` 显式删除 + try/except 保护。

#### execute_python 测试结果 — 6/6 通过

| 测试 | 操作 | 结果 |
|------|------|------|
| 基础执行 | `print("Hello")`, `import unreal`, 引擎版本查询, 关卡 Actor 列表 | **通过** |
| 有状态上下文 | 定义变量+函数 → 下一次调用中引用 | **通过** |
| UE API 访问 | `unreal.SystemLibrary`, `EditorActorSubsystem` | **通过** |
| 错误处理 | 语法错误、ZeroDivisionError、NameError — 全部捕获并返回 | **通过** |
| Context Reset | 定义变量 → reset(`success: true`) → 变量不存在(NameError) | **通过** |
| 复杂 UE 操作 | 创建 PointLight → 移动 → 改灯光强度 → 删除，全部通过 Python 完成 | **通过** |

**发现**: UE 5.7 中 `EditorLevelLibrary` 已标记 deprecated，应改用 `unreal.get_editor_subsystem(unreal.EditorActorSubsystem)`。

### Repository Split
- Extracted UnrealAgent from the Aura monorepo into a standalone repository: https://github.com/ky256/UnrealAgent
- Aura now references UnrealAgent as a git submodule at `Plugins/UnrealAgent`
- Python virtual environment (`.venv`) and build artifacts (`Binaries/`, `Intermediate/`) remain git-ignored and local only

---

## Backlog / Future Work
- [x] Universal execution layer — execute_python (Phase 1)
- [ ] 自适应工具演化 — AST 指纹 + 模式识别 + Learned Tool 缓存 (Phase 2)
- [ ] Learned Tool MCP 动态注册 (Phase 3)
- [ ] 固化建议生成器 (Phase 4)
- [ ] Blueprint read/write commands (UABlueprintCommands)
- [ ] Material/Texture inspection tools
- [ ] Screenshot/thumbnail capture
- [ ] PIE (Play In Editor) control (start/stop/pause)
- [ ] Console command execution
- [ ] Asset creation/import tools
- [ ] Undo/redo support
- [ ] Multi-level support
- [ ] Remote connection support (non-localhost)
- [ ] Authentication for remote connections
