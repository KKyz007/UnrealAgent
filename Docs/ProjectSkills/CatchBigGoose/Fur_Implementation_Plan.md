# CatchBigGoose 毛发多 Pass 实现方案（UE 5.7）

> **项目定位**：UE 5.7 休闲三消游戏（抓大鹅）
> **目标**：给 7 只玩偶（兔子/猫/鸡/狐狸/狮子/企鹅/浣熊）实现毛茸茸的 Shell Fur 毛发效果
> **方案基础**：整合以下三篇 Skill 文档
>   1. `Unity_PBR_Fur_Material_Guide.md`（原理 + ExtendStandard 源码）
>   2. `UE5_Shell_Fur_Cloth_Material_Guide.md`（UE 方案选型）
>   3. `UE5_Add_Custom_MeshDrawPass_Guide.md`（改引擎多 Pass 完整骨架）
> 记录日期：2026-04-22

---

## 一、决策结论（先看这个）

### 1.1 三种路线综合评估

| 方案 | 效果 | 性能 | 改引擎 | 开发周期 | **本项目推荐度** |
|------|------|------|--------|----------|------------------|
| **A. Material + ISM 伪 Shell**（WPO 沿法线偏移）| ⭐⭐⭐ | ⭐⭐⭐⭐⭐ | ❌ 不需要 | 2~3 天 | ✅ **强烈推荐** |
| **B. Groom（Alembic 曲线毛发）** | ⭐⭐⭐⭐⭐ | ⭐⭐ | ❌ 不需要 | 5~7 天 | ⚠️ 效果过剩 |
| **C. 改引擎 Custom MeshDrawPass** | ⭐⭐⭐⭐ | ⭐⭐⭐ | ✅ 必须 | 2~3 周 | ❌ 不推荐（成本收益比太差） |

### 1.2 最终选择：**方案 A（WPO 伪 Shell）**

**核心理由**：
1. **三消游戏视角远** — 玩偶在屏幕上占比小（~10%），毛发细节感知度低
2. **物理模拟吃帧** — 60 只物理模拟 + 吸力 + CCD 已经吃掉不少性能预算，不能再花在毛发上
3. **7 种玩偶要统一** — 方案 A 做一个 Material Function 就能复用到全部 7 种，改引擎方案维护成本高
4. **UE 5.7 稳定性** — 改引擎方案在 5.0→5.3→5.5 接口已经变了 3 次，5.7 还会再变
5. **随时可升级** — 将来如果要做特写/宣传片，加 Groom 即可，不冲突

---

## 二、方案 A：Material WPO 伪 Shell 实现（推荐）

### 2.1 原理

**不修改引擎，不循环绘制，只用一个 Material + 10 个 StaticMeshComponent 模拟 Shell：**

```
玩偶 BP
├─ StaticMeshComponent（原本体） → 材质用正常 PBR
├─ ShellComponent_01            → 材质 M_Fur，ShellIndex=0.1
├─ ShellComponent_02            → 材质 M_Fur，ShellIndex=0.2
├─ ...
└─ ShellComponent_10            → 材质 M_Fur，ShellIndex=1.0
```

每个 Shell 组件**共享同一个网格资产**（零内存开销），只是材质参数不同。
Material 的 **World Position Offset** 根据 `ShellIndex × FurLength` 沿法线外推。

### 2.2 步骤分解

#### 步骤 1：创建材质函数 `MF_ShellFur`

输入参数：
| 参数 | 类型 | 说明 |
|------|------|------|
| `ShellIndex` | Scalar | 当前层索引 0~1（由 MPC 或 Instance 传入）|
| `FurLength` | Scalar | 毛发总长度（建议 0.3~0.8cm）|
| `LayerNoise` | Texture2D | 灰度噪波图（毛孔分布）|
| `FurDensity` | Scalar | 毛发密度（控制 UV Tiling，5~20）|
| `GravityDir` | Vector3 | 重力方向（世界空间）|
| `GravityStrength` | Scalar | 重力强度 0~1 |
| `BaseColor` | Vector3 | 毛根色 |
| `TipColor` | Vector3 | 毛尖色 |

核心逻辑（Material Editor 节点图）：

```
【WorldPositionOffset 分支】
1. VertexNormalWS × FurLength × ShellIndex        → 基础外推
2. lerp(VertexNormalWS, GravityDir, ShellIndex)   → 发根刚硬发尖柔软
3. Output = Vec * FurLength * ShellIndex

【Opacity Mask 分支】（仅 Masked 材质用）
1. Tex = LayerNoise.Sample(UV * FurDensity)
2. Threshold = ShellIndex * ShellIndex            // 抛物线裁剪
3. Mask = Tex.r > Threshold ? 1 : 0
4. Tip Fade = 1 - ShellIndex                      // 尖部渐隐

【BaseColor 分支】
Final = lerp(BaseColor, TipColor, ShellIndex)
```

#### 步骤 2：创建材质 `M_Fur`（Masked + Two Sided）

```
Blend Mode    : Masked
Shading Model : Default Lit
Two Sided     : ✅ True（毛发内外都可见）
Opacity Mask Clip Value : 0.333
Material Domain : Surface
Used with Instanced Static Meshes : ✅
```

材质内部直接引用 `MF_ShellFur`，只暴露 `ShellIndex` 作为 **MaterialParameter** 供 MID 修改。

#### 步骤 3：创建蓝图组件 `BP_FurActorComponent`

新建 ActorComponent 蓝图，职责：
1. 在 `BeginPlay` 时根据配置的 **Shell 层数**（默认 10）动态生成 StaticMeshComponent
2. 每个 Component 共享宿主的 StaticMesh
3. 为每个 Component 创建 MID，设置 `ShellIndex = i / (ShellCount - 1)`
4. 绑定到宿主根组件

**蓝图伪代码**：
```
Event BeginPlay
├─ 获取 Owner 的主 StaticMeshComponent
├─ For i = 0 to ShellCount - 1:
│   ├─ AddComponent: StaticMeshComponent (Transient)
│   ├─ SetStaticMesh(主Mesh)
│   ├─ AttachTo(主SMC, KeepRelativeTransform)
│   ├─ SetCollisionEnabled(NoCollision)    // 关键：壳层不参与碰撞
│   ├─ SetCastShadow(false)                 // 关键：只让本体投影
│   ├─ MID = CreateDynamicMaterialInstance(0, M_Fur)
│   └─ MID.SetScalarParameter("ShellIndex", i / (ShellCount - 1))
```

#### 步骤 4：挂接到玩偶 BP

给 7 个玩偶（SM_Bunny / SM_Cat / SM_Chicken / SM_Fox / SM_Lion / SM_Penguin / SM_Raccoon）的 Blueprint 各添加一个 `BP_FurActorComponent`，调参：

| 玩偶 | FurLength | ShellCount | FurDensity | 备注 |
|------|-----------|------------|------------|------|
| SM_Bunny   | 0.6 | 12 | 15 | 毛长细腻 |
| SM_Cat     | 0.5 | 10 | 12 | 正常 |
| SM_Chicken | 0.3 | 8  | 8  | 羽毛短 |
| SM_Fox     | 0.7 | 12 | 12 | 长毛蓬松 |
| SM_Lion    | 0.8 | 14 | 10 | 鬃毛最长 |
| SM_Penguin | 0.2 | 6  | 20 | 短密细绒 |
| SM_Raccoon | 0.6 | 12 | 12 | 正常 |

#### 步骤 5：LOD 策略（性能保险）

在 `BP_FurActorComponent` 里加 `Tick` 检查相机距离，动态启用/禁用壳层：

```
Distance < 500cm     → 全部 ShellCount 启用
Distance < 1000cm    → 启用 ShellCount / 2
Distance < 1500cm    → 只启用 3 层
Distance >= 1500cm   → 全部隐藏（只剩本体）
```

因为三消游戏相机距离变化小，这步可以简化为**游戏启动时一次判断**。

---

### 2.3 性能估算

```
单只玩偶 DrawCall：
- 本体：1
- Shell 层（10 层）：10
- 总计：11 DrawCall

场景 60 只玩偶最大：
- 60 × 11 = 660 DrawCall
```

**优化手段**：
- **ISM 合批**：把 7 种玩偶各自用 `InstancedStaticMeshComponent` 管理，Shell 也走 ISM，DrawCall 降到 `11 × 7 = 77`
- **CustomDepth 不影响**：Shell 组件设置 `bRenderCustomDepthPass = false`，不参与鼠标高亮

---

## 三、方案 B：Groom 毛发（备选）

### 3.1 什么时候用
- 宣传片/特写镜头
- 玩偶 Showcase 界面（慢速旋转展示）
- 单独做一个"精装玩偶详情页"

### 3.2 流程

1. Blender / Maya / XGen 里给玩偶 FBX 做 XGen 毛发
2. 导出为 **Alembic (.abc)** 文件
3. UE5 导入 → 生成 Groom Asset
4. 玩偶 BP 里加 `GroomComponent`，绑定 Groom Asset
5. 创建 Groom 专用材质（Hair Shading Model）
6. 开启 Groom 物理（Niagara Solver）

### 3.3 坑点
- **Groom 不支持实例化** — 60 只同屏性能崩溃
- **依赖 Alembic 导出** — 美术工作量大
- **移动端基本不可用**

**结论**：**仅用于 UI 展示页或单只特写**，不用于玩法场景。

---

## 四、方案 C：改引擎 Custom MeshDrawPass（不推荐，仅作技术储备）

### 4.1 什么时候用
- **本项目不用**
- 仅作为未来其他项目的技术储备

### 4.2 完整步骤回顾

详细见 `UE5_Add_Custom_MeshDrawPass_Guide.md`，摘要 6 步：

| 步骤 | 改动点 | 工作量 |
|------|--------|--------|
| 1 | `MeshPassProcessor.h` 注册 `EMeshPass::FurShellPass` | 0.5 天 |
| 2 | 新建 `FFurShellMeshPassProcessor`（AddMeshBatch + Process） | 2 天 |
| 3 | 新建 `FFurShellVS / FFurShellPS`（继承 `FMeshMaterialShader`） | 1 天 |
| 4 | 改 `SceneVisibility.cpp`（ComputeDynamicMeshRelevance + MarkRelevant） | 0.5 天 |
| 5 | `FSceneRenderer::RenderFurShellPass` + RDG AddPass | 3 天 |
| 6 | `FurShell.usf`（Shell 层循环 + 裁剪空间外扩） | 2 天 |
| 7 | **调试 + UE 5.7 接口适配 + FSR/TAA 残影修复** | **5 天** |

**总计**：~ 14 天（经验丰富引擎程序）；新手翻倍。

### 4.3 UE 5.7 特别警告

根据原文评论区：
- **5.3** `View.DynamicMeshEndIndices` 接口已变
- **5.5** Shader 获取/状态设置挪到 `CollectPSOInitializers`
- **5.7** 按 Epic 惯例，RDG / Nanite 接口还会调整

**如果未来真要做**：等 UE 5.7 有稳定的社区示例（如官方样例项目）后再动手。

---

## 五、Shader 核心代码参考（方案 A 用 HLSL 版本）

如果未来想从材质蓝图切换到 **Custom HLSL 节点**（更精细控制），可参考：

```hlsl
// 方案 A：在 Custom 节点中替代 Material 节点图
// 输入：ShellIndex (float), FurLength (float), VertexNormal (float3), GravityDir (float3)

// 1. 发根 → 发尖方向插值（发根刚硬，发尖受重力）
float3 FurDir = lerp(VertexNormal, GravityDir, ShellIndex * ShellIndex);

// 2. 沿插值方向外推
float3 Offset = FurDir * FurLength * ShellIndex;

// 3. Opacity Mask（抛物线裁剪）
float NoiseVal = Texture2DSample(LayerNoise, UV * FurDensity).r;
float Threshold = ShellIndex * ShellIndex;
float Mask = step(Threshold, NoiseVal);

// 4. 尖部透明度递减
float Alpha = (1.0 - ShellIndex) * Mask;

// 输出：WorldPositionOffset = Offset; OpacityMask = Alpha;
```

**对比 Unity ExtendStandard 源码**（Unity_PBR_Fur_Material_Guide 中）：
```hlsl
// Unity 版本（参考）
v.vertex.xyz += v.normal * _FurLength * FUR_OFFSET;
alpha = step(FUR_OFFSET * FUR_OFFSET, alpha);
color.a = 1 - FUR_OFFSET;
```

逻辑完全一致，只是 UE 在材质节点或 Custom 里写。

---

## 六、落地清单（Todo List）

如果决定做方案 A，可以分 3 个阶段：

### 阶段 1：Demo 验证（0.5 天）
- [ ] 创建 `MF_ShellFur` 材质函数
- [ ] 创建 `M_Fur` 材质
- [ ] 先拿 SM_Bunny 一只手动加 10 个 SMC 测试，调出效果

### 阶段 2：组件化（1 天）
- [ ] 创建 `BP_FurActorComponent`
- [ ] 动态创建 Shell 组件 + MID 设置 ShellIndex
- [ ] 把 7 只玩偶 BP 都挂上组件
- [ ] 按 2.2 表格调参

### 阶段 3：性能优化（1 天）
- [ ] LayerNoise 贴图制作（512×512 灰度）
- [ ] 关闭 Shell 的碰撞、CustomDepth、阴影
- [ ] 根据 Profiler 调整 ShellCount（8~12 之间）
- [ ] 必要时接入 ISM 合批

### 阶段 4（可选）：极致效果
- [ ] 给 Fabric Scatter 布料散射色做个后期参数
- [ ] 玩偶被选中时触发 "fur ruffle"（Shell 随机扰动）
- [ ] 消除特效里 Shell 炸开 → 粒子化

---

## 七、参考资料全集

| # | 文档 | 来源 |
|---|------|------|
| 1 | `Unity_PBR_Fur_Material_Guide.md` | 本项目 Skill 文件夹 |
| 2 | `UE5_Shell_Fur_Cloth_Material_Guide.md` | 本项目 Skill 文件夹 |
| 3 | `UE5_Add_Custom_MeshDrawPass_Guide.md` | 本项目 Skill 文件夹 |
| 4 | `UE5_Custom_ShadingModel_Guide.md` | 本项目 Skill 文件夹 |
| 5 | ExtendStandard 源码 | https://github.com/chenyong2github/ExtendStandard |
| 6 | NVIDIA GPU Gems - Chapter 23 Hair Animation | https://developer.nvidia.com/gpugems |
| 7 | gim.studio - Shell Based Fur | https://gim.studio/an-introduction-to-shell-based-fur-technique/ |
| 8 | YivanLee - UE5 Shell Fur Cloth Material | 知乎 |
| 9 | 搓手苍蝇 - UE5 Add Custom MeshDrawPass | 知乎 |

---

## 八、最终建议

> **做方案 A**。  
> 2~3 天内能看到效果，不动引擎，不影响现有性能优化成果（MaxPhysicsSpeed / CustomDepth / BoxHalfSizeLimit 都不受影响）。
> 
> **如果后面觉得毛发效果不够震撼**，再投资做 Groom 版本的"玩偶图鉴"特写页。
> 
> **改引擎方案留给未来**，等你做一款真正的动物特写游戏再说。
