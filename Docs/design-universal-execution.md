# 设计文档：万能执行层

## 从第一性原理出发

### 最终目标

AI 能完成人在 UE Editor 中能完成的一切操作。

### 原子分解

把这个目标拆到不可再分：

1. **AI 有意图** — "把这个 Actor 移到 (100, 200, 0)"
2. **意图变成指令** — 某种 UE 能理解的格式
3. **UE 执行指令** — 在编辑器上下文中运行
4. **结果返回 AI** — AI 知道成功还是失败，拿到返回数据

这四步中，1 是 AI 自身能力，4 是通道问题（已解决）。核心问题在 **2 和 3**：什么格式的指令，能覆盖所有操作？

### 当前方案的问题

当前 15 个 tool，每个是一段 C++ 手写的逻辑：

```
意图 → 选择 tool → 固定参数 → 固定 C++ 逻辑 → 固定返回
```

这是一个**枚举模型** — 预先列出所有可能的操作。UE Editor 有几千个类、几万个方法，枚举不完。

### 关键洞察

UE Editor 的所有操作，本质上都是在调 API。如果 AI 能直接写 API 调用代码并执行，就不需要中间的 tool 翻译层：

```
枚举模型：  意图 → tool schema → C++ handler → UE API
万能模型：  意图 → 代码 → 执行 → UE API
```

中间层从"人类预定义的有限映射"变成"AI 自由生成的代码"，覆盖度从有限变为理论上无限。

---

## 执行语言的选择

### 排除法

| 语言 | 能否热执行 | 覆盖度 | AI 生成质量 | 结论 |
|------|-----------|--------|------------|------|
| C++ | 不能，需要编译 | 100% | 高 | 排除：无法热执行 |
| Blueprint | 不能，需要可视化编辑器 | 高 | 不适合文本 | 排除：不是文本格式 |
| Console Command | 能 | 低，只有已注册的命令 | 高 | 排除：覆盖度不够 |
| **Python** | **能，UE 原生支持** | **~95%** | **高** | **唯一选择** |

### 为什么是 Python

UE 从 4.x 开始内置 `PythonScriptPlugin`，5.x 进一步完善。它：

- 在 Game Thread 上同步执行（线程安全）
- 通过 `unreal` 模块暴露了几乎所有 Editor API
- 支持 `import unreal` 后直接调用 C++ 暴露的类和方法
- 自带 stdout/stderr 捕获机制
- AI 对 Python + UE API 的训练数据充足

关键事实：**UE 的 Python 子系统本身就是为"脚本化编辑器操作"设计的**。我们不是在造新东西，是在接通一条已有的路。

---

## 核心设计

### 最小可行接口

从第一性原理推导，只需要一个接口：

```
execute_python(code: str) → { output: str, error: str | null }
```

输入一段 Python 代码，UE 执行它，返回 stdout 和错误信息。

### 执行模型：有状态 vs 无状态

**无状态**（每次调用独立）：
- 简单，无副作用积累
- 每次都要 import、初始化
- 无法分步构建复杂操作

**有状态**（共享 Python 上下文）：
- AI 可以先定义函数，后续调用复用
- 变量在调用之间保持
- 更接近人类使用 Python 控制台的体验
- 风险：状态污染，需要 reset 机制

**结论：有状态，但提供 reset**。

理由：UE 自身的 Python 控制台就是有状态的。AI 的使用模式是多步交互——先查询、再操作、再验证。有状态更自然。

### 结果捕获

Python 代码的输出有三个来源：

1. **stdout** — `print()` 的输出
2. **返回值** — 表达式的求值结果
3. **异常** — 运行时错误

都需要捕获并返回给 AI：

```python
# C++ 侧伪代码
redirect stdout → buffer
try:
    exec(code, globals)
    # 如果最后一行是表达式，额外 eval 它取返回值
except Exception as e:
    error = str(e)
restore stdout
return { output: buffer, return_value: last_expr_value, error: error }
```

### 线程模型

**必须在 Game Thread 执行**。

UE 的 Editor API 绑定主线程。当前架构中 TCP 接收在非主线程，通过 `FTSTicker` 回到主线程处理。Python 执行也必须走这条路：

```
TCP recv (worker thread)
  → 解析 JSON-RPC
  → FTSTicker callback (game thread)
    → 执行 Python
    → 捕获结果
  → TCP send (worker thread)
```

这与现有 Command 的执行路径完全一致，不需要改通道层。

---

## 安全性分析

### 风险矩阵

| 风险 | 严重度 | 概率 | 对策 |
|------|--------|------|------|
| 删除文件/资产 | 高 | 中 | 默认只绑定 localhost；编辑器有 undo |
| 死循环卡死编辑器 | 高 | 中 | **执行超时** |
| import os; os.system(...) | 高 | 低 | 文档告知；可选沙箱 |
| 修改项目设置 | 中 | 中 | 编辑器有 undo |
| 大量 stdout 输出 | 低 | 中 | **输出截断** |

### 必须实现的安全措施

1. **执行超时**（默认 30 秒）— 防止死循环冻结编辑器
2. **输出长度限制**（默认 64KB）— 防止内存爆炸
3. **仅 localhost 访问** — 已有

### 可选安全措施（v2）

4. 模块白名单（只允许 `import unreal`，限制 `os`, `subprocess` 等）
5. 操作审计日志
6. 确认机制（破坏性操作弹窗确认）

### 关于沙箱的取舍

完全沙箱化 Python 执行是可以做的（限制 import、限制文件访问），但会大幅削弱能力。第一性原理出发：

> **工具的价值 = 能力 × 安全性**

过度限制能力，工具就没用了。正确的做法是：
- **信任边界画在网络层**（只接受本地连接）
- **破坏可恢复**（UE 的 undo 系统）
- **硬保护只对不可恢复的风险**（超时、输出限制）

---

## 与现有 Tool 的关系

### 1 + N 架构

```
┌─────────────────────────────────────────┐
│              MCP Tool Layer             │
├──────────┬──────────────────────────────┤
│ 快捷 Tool │      execute_python         │
│  (N 个)   │        (1 个)               │
├──────────┼──────────────────────────────┤
│ 固定逻辑  │    AI 生成的任意代码          │
│ 固定返回  │    动态逻辑，动态返回          │
├──────────┴──────────────────────────────┤
│           UE Editor API                 │
└─────────────────────────────────────────┘
```

### 快捷 Tool 保留的理由

从第一性原理看，`execute_python` 一个就够了。但保留快捷 Tool 有工程价值：

| 维度 | execute_python | 快捷 Tool |
|------|---------------|-----------|
| 可靠性 | AI 可能写错代码 | 逻辑固定，不出错 |
| Token 消耗 | 生成代码 + 传输 | 只传参数 |
| 延迟 | Python 解释 + 执行 | 直接 C++ |
| 可审计 | 需要看代码才知道做了什么 | 方法名即语义 |

**结论：高频操作保留快捷 Tool，长尾操作走 execute_python。**

不需要人为划分哪些走快捷、哪些走万能——AI 会自己选择。常用操作有专用 tool 就用专用的，没有就写 Python。

---

## 实现路径

### 第一步：C++ 侧 — UAPythonCommand

新增一个 Command 类，注册 `execute_python` 方法：

- 接收参数：`code`（string）、`timeout_seconds`（float，默认 30）
- 重定向 Python stdout/stderr 到 FString buffer
- 调用 `IPythonScriptPlugin::ExecPythonCommandEx()` 执行代码
- 捕获输出和异常
- 返回 `{ success, output, error }`

需要的 UE 模块依赖：`PythonScriptPlugin`（运行时可选加载）。

### 第二步：MCP Server 侧 — 新增 tool

在 Python MCP Server 中注册 `execute_python` tool，透传 code 参数到 C++ 侧。

### 第三步：.uplugin 依赖

在 `UnrealAgent.uplugin` 中添加对 `PythonScriptPlugin` 的可选依赖（`"Optional": true`），确保插件存在时才启用万能执行。

### 第四步：验证

测试场景：
1. 简单查询 — `print(unreal.EditorLevelLibrary.get_all_level_actors())`
2. 复合操作 — 查找所有 StaticMeshActor 并批量修改材质
3. 错误处理 — 语法错误、运行时异常、超时
4. 有状态验证 — 第一次定义函数，第二次调用

---

## 长期演进

### 万能执行解锁的能力

有了 `execute_python`，以下能力无需任何额外开发：

- 批量资产操作（重命名、移动、修改属性）
- 材质/纹理检查和修改
- 关卡序列化和批量编辑
- 自动化构建流程
- 自定义编辑器工具创建
- 数据表操作
- 动画资产处理
- ...

所有这些只需要 AI 知道对应的 `unreal` 模块 API——不需要我们写任何新 tool。

### 自适应工具演化系统

#### 仿生学启示：神经系统的分层加速

人类神经系统面对的问题和我们完全一样：**如何用有限的资源，处理无限多样的操作？**

大脑的解法不是为每种可能的动作预先布线，而是一套三层架构：

```
┌─────────────────────────────────────────────────────────────────┐
│  大脑皮层（前额叶）                                                │
│  · 处理新问题，灵活但慢（数百毫秒）                                  │
│  · 需要有意识的注意力                                              │
│  · 资源消耗大                                                     │
├─────────────────────────────────────────────────────────────────┤
│  小脑 / 基底神经节                                                 │
│  · 习得的技能，快且自动化（数十毫秒）                                 │
│  · 通过反复练习从皮层"下沉"而来                                     │
│  · 资源消耗中等                                                   │
├─────────────────────────────────────────────────────────────────┤
│  脊髓反射弧                                                       │
│  · 硬编码响应，最快（数毫秒）                                       │
│  · 不经过大脑，直接 stimulus → response                            │
│  · 资源消耗极小                                                   │
└─────────────────────────────────────────────────────────────────┘
```

关键机制：**髓鞘化（Myelination）**。当同一条神经通路被反复激活时，胶质细胞会在轴突外包裹髓鞘，使信号传导速度从 ~2m/s 提升到 ~120m/s，提升 **60 倍**。这不是新建一条路，而是把已有的慢路变快。

这个架构直接映射到我们的系统：

```
大脑皮层       →  execute_python     灵活，能处理任何操作，但慢
小脑           →  缓存的 Python 片段  习得的模式，中等速度
脊髓反射弧     →  快捷 C++ Tool       硬编码，最快，不经过 Python
髓鞘化过程     →  工具固化流水线       高频慢路径自动变成快路径
```

#### 免疫系统的另一层启示

神经系统解决"如何加速"，免疫系统解决"如何记忆"：

- **先天免疫**（Innate Immunity）— 通用防御，响应任何入侵。类比 `execute_python`：什么都能处理，但没有针对性优化。
- **适应性免疫**（Adaptive Immunity）— 首次遇到抗原时响应慢，但会产生**记忆细胞**。再次遇到同一抗原时，响应速度快几个数量级。类比：首次执行某操作用 Python，记录模式后生成快捷 Tool，下次直接调用。
- **关键区别**：免疫系统不需要预先知道所有可能的病原体。它遇到新威胁时用通用机制应对，然后把成功的应对方案**记忆**下来。我们的系统也不需要预先知道所有可能的 UE 操作。

#### 映射到系统架构

```
                    ┌─────────────────────────────┐
                    │        AI (大脑皮层)          │
                    │   理解意图，生成代码或选择工具    │
                    └──────────┬──────────────────┘
                               │
                    ┌──────────▼──────────────────┐
                    │       工具路由层 (丘脑)        │
                    │   有快捷 Tool？→ 直接调用      │
                    │   有缓存片段？→ 加载执行       │
                    │   都没有？   → execute_python  │
                    └──────────┬──────────────────┘
                               │
              ┌────────────────┼────────────────┐
              ▼                ▼                ▼
    ┌─────────────┐  ┌──────────────┐  ┌──────────────┐
    │  脊髓反射弧   │  │   小脑回路    │  │   大脑皮层    │
    │  快捷 C++ Tool│  │ 缓存 Python  │  │ execute_python│
    │  ~1ms        │  │  ~10ms       │  │  ~100ms      │
    │  硬编码逻辑   │  │ 预编译片段   │  │  动态代码     │
    └─────────────┘  └──────────────┘  └──────────────┘
              │                │                │
              └────────────────┼────────────────┘
                               ▼
                    ┌──────────────────────┐
                    │     UE Editor API     │
                    └──────────────────────┘

              ─────── 髓鞘化（固化）方向 ────────→
              execute_python → 缓存片段 → 快捷 Tool
```

#### 详细设计：三层执行引擎

**第一层：反射层（Reflex Layer）— 快捷 C++ Tool**

```
特征：硬编码、最快、最可靠、不可变
类比：膝跳反射，不经过大脑
生命周期：编译时确定，随插件发布
```

这是现有的 15 个 tool。特点：
- 逻辑写死在 C++ 中，编译时确定
- 零解释开销，直接调 UE API
- Schema 明确，AI 调用不出错
- 只覆盖最基础的高频操作

入选标准：
- 几乎每个 AI 会话都会用到
- 逻辑稳定，不需要灵活变化
- 错误后果严重（如 delete_actor 需要确保先 deselect）

**第二层：习得层（Learned Layer）— 缓存的 Python 片段**

```
特征：从使用中习得、中等速度、可演化
类比：骑自行车，曾经需要思考，现在自动化
生命周期：运行时动态积累，持久化到磁盘
```

这是新增的中间层。当 `execute_python` 执行成功后，系统记录：

```json
{
  "pattern_id": "batch_rename_actors",
  "intent_signature": "rename multiple actors matching pattern",
  "code_template": "import unreal\nfor actor in unreal.EditorLevelLibrary.get_all_level_actors():\n    if '{pattern}' in actor.get_actor_label():\n        actor.set_actor_label(actor.get_actor_label().replace('{pattern}', '{replacement}'))",
  "parameters": ["pattern", "replacement"],
  "call_count": 0,
  "last_used": null,
  "avg_execution_ms": null,
  "error_rate": 0.0
}
```

存储位置：`{Plugin}/Cache/learned_tools.json`

AI 下次遇到类似意图时，不需要从零写代码，系统提供已验证的模板。这等价于神经系统中小脑对运动模式的记忆——不需要前额叶每次重新规划。

**第三层：认知层（Cognitive Layer）— execute_python**

```
特征：万能、最慢、最灵活、可能出错
类比：第一次做某件事，需要全神贯注思考
生命周期：每次调用即时生成，执行后丢弃（或进入习得层）
```

AI 自由编写 Python 代码。处理所有未被前两层覆盖的操作。

#### 详细设计：髓鞘化流水线（Myelination Pipeline）

神经系统的髓鞘化不是瞬间发生的，而是一个渐进过程。我们的工具固化也是：

```
阶段 0：首次执行
  AI 写 Python → execute_python 执行 → 返回结果
  系统记录：代码、参数、耗时、成功/失败

阶段 1：模式识别（自动）
  同一类操作被执行 N 次（默认 N=3）
  系统提取共性：
    - 哪些部分是固定的（模板）
    - 哪些部分是变化的（参数）
    - 输入输出的 schema

阶段 2：片段缓存（自动）
  生成 learned tool 记录
  下次遇到类似意图时，AI 可以直接引用模板
  等价于：神经通路开始包裹髓鞘

阶段 3：固化建议（半自动）
  当某个 learned tool 的调用次数超过阈值（默认 20 次）
  且错误率低于阈值（默认 5%）
  系统向开发者建议：将此模式固化为 C++ 快捷 Tool
  生成建议包括：方法名、参数 schema、实现伪代码

阶段 4：正式固化（人工）
  开发者审核建议，编写 C++ 实现
  新 Tool 加入反射层，随下一版本发布
  等价于：反射弧正式形成
```

**每个阶段的触发条件和数据流：**

```
execute_python 调用
       │
       ▼
  ┌──────────┐    记录执行数据
  │ 执行日志  │◄────────────────────────────────────┐
  └────┬─────┘                                      │
       │ 累计 ≥3 次相似调用                            │
       ▼                                            │
  ┌──────────┐    提取模板+参数                       │
  │ 模式识别  │                                      │
  └────┬─────┘                                      │
       │ 模式稳定                                    │
       ▼                                            │
  ┌──────────┐    可直接复用                          │
  │ 片段缓存  │────────────────────────────────┐     │
  └────┬─────┘                                │     │
       │ 调用 ≥20 次 & 错误率 <5%              │     │
       ▼                                     ▼     │
  ┌──────────┐                          下次调用时   │
  │ 固化建议  │──→ 开发者审核 ──→ C++ Tool  优先匹配  │
  └──────────┘                                      │
                                                    │
  每次 execute_python 调用都会 ─────────────────────┘
```

#### 详细设计：模式识别算法

模式识别是髓鞘化流水线的核心。如何判断两次 `execute_python` 调用是"同一类操作"？

**方法：代码结构指纹**

```python
# 调用 1：
import unreal
for actor in unreal.EditorLevelLibrary.get_all_level_actors():
    if 'Tree' in actor.get_actor_label():
        actor.set_actor_label(actor.get_actor_label().replace('Tree', 'Oak'))

# 调用 2：
import unreal
for actor in unreal.EditorLevelLibrary.get_all_level_actors():
    if 'Rock' in actor.get_actor_label():
        actor.set_actor_label(actor.get_actor_label().replace('Rock', 'Boulder'))
```

提取结构指纹（将字面量替换为占位符）：

```python
import unreal
for actor in unreal.EditorLevelLibrary.get_all_level_actors():
    if {STR_0} in actor.get_actor_label():
        actor.set_actor_label(actor.get_actor_label().replace({STR_0}, {STR_1}))
```

两次调用的结构指纹相同 → 判定为同一模式 → 提取参数化模板。

**指纹生成规则：**

1. 解析 Python AST
2. 将所有字面量（str, int, float）替换为类型占位符
3. 保留控制流结构、API 调用链、变量关系
4. 对 AST 做规范化序列化，生成 hash

**相似度判定：**

- 指纹完全匹配 → 同一模式
- 指纹 diff ≤ 2 个节点 → 高度相似，人工确认后合并
- 指纹 diff > 2 → 不同模式

#### 详细设计：执行日志 Schema

```json
{
  "execution_log": {
    "id": "uuid",
    "timestamp": "ISO8601",
    "code": "原始 Python 代码",
    "code_fingerprint": "AST 结构 hash",
    "parameters_extracted": {
      "STR_0": "Tree",
      "STR_1": "Oak"
    },
    "result": {
      "success": true,
      "output": "stdout 内容",
      "error": null,
      "execution_ms": 45
    },
    "context": {
      "session_id": "会话标识",
      "preceding_tool_calls": ["get_world_outliner"],
      "user_intent": "从 AI 的 tool call description 中提取"
    }
  }
}
```

存储位置：`{Plugin}/Cache/execution_log.jsonl`（追加写入，JSONL 格式）

#### 详细设计：Learned Tool 注册

当模式被识别后，系统自动生成一个 learned tool 定义：

```json
{
  "tool_name": "batch_rename_actors_by_label",
  "description": "Rename actors whose label contains a pattern",
  "parameters": {
    "search_pattern": { "type": "string", "description": "Label substring to match" },
    "replacement": { "type": "string", "description": "Replacement string" }
  },
  "code_template": "import unreal\nresults = []\nfor actor in unreal.EditorLevelLibrary.get_all_level_actors():\n    if '{search_pattern}' in actor.get_actor_label():\n        old = actor.get_actor_label()\n        actor.set_actor_label(old.replace('{search_pattern}', '{replacement}'))\n        results.append(f'{old} -> {actor.get_actor_label()}')\nprint('\\n'.join(results) if results else 'No actors matched')",
  "stats": {
    "created_from_executions": ["uuid1", "uuid2", "uuid3"],
    "total_calls": 3,
    "success_rate": 1.0,
    "avg_execution_ms": 42,
    "last_used": "ISO8601"
  },
  "status": "learned"
}
```

Learned tool 通过 MCP 对外暴露时，有两种策略：

**策略 A：透明代理** — Learned tool 不作为独立 MCP tool 暴露。AI 仍然调用 `execute_python`，但 MCP Server 侧拦截意图，自动匹配 learned tool 并填充模板。AI 无感知。

**策略 B：显式注册** — Learned tool 作为新的 MCP tool 动态注册。AI 在 tool list 中直接看到它，像调用快捷 Tool 一样使用。

**结论：策略 B 更好。** 原因：
- AI 能看到 tool 的 schema 和 description，调用更准确
- 减少 token 消耗（不需要生成代码）
- 审计更清晰（调了哪个 tool 一目了然）
- 符合 MCP 协议的 tool 动态注册能力

#### 演化方向总结

```
                         时间轴 →
                         
  工具数量  │ ■ ■ ■ ■ ■ ■ ■ ■ ■ ■ ■ ■ ■    Learned Tools (持续增长)
            │ ■ ■ ■ ■ ■ ■ ■ ■ ■ ■ ■ ■ ■
            │ ■ ■ ■ ■ ■ ■ ■ ■ ■
            │ ■ ■ ■ ■ ■
            │ ■ ■ ■
            │ ■                              
            │───────────────────────────────
            │ ● ● ● ● ● ● ● ● ● ●          C++ Tools (缓慢增长)
            │ ● ● ● ● ● ●
            │ ● ● ● ● ●
            │ ● ● ● ● ●                     ← 初始 15 个
            └───────────────────────────────→
              v0.1    v0.2    v0.3    v1.0

  execute_python 的调用占比会随时间下降：
  v0.1: 80% python / 20% tools
  v0.3: 40% python / 60% tools
  v1.0: 10% python / 90% tools  ← 大部分操作已有快捷路径

  但 execute_python 永远存在，作为处理长尾/新场景的兜底。
  如同大脑皮层永远不会消失——你总会遇到需要"思考"的新问题。
```

#### 实现分期

**Phase 1（当前版本）：** 只实现 execute_python + 执行日志记录。不做模式识别。先积累数据。

**Phase 2：** 实现 AST 指纹 + 模式识别 + learned tool 缓存。系统能自动从日志中提取模式。

**Phase 3：** 实现 learned tool 的 MCP 动态注册。AI 能直接调用 learned tool。

**Phase 4：** 实现固化建议生成器。系统向开发者推荐哪些 learned tool 值得固化为 C++ 实现。

Phase 1 是前提。没有真实的执行数据，后续的模式识别都是空中楼阁。先把万能执行做出来，让 AI 用起来，数据自然会来。
