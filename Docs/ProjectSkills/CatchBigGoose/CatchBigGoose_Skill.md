# CatchBigGoose 三消游戏开发 Skill 手册

> 项目：抓大鹅（CatchBigGoose）— UE5.7 C++ 三消休闲游戏
> 更新日期：2026-04-22

---

## 零、变更记录

### 2026-04-22
- **UnrealAgent 插件远程仓库切换**
  - 原 `origin`：`https://github.com/ky256/UnrealAgent.git` → 改名为 `upstream`（保留用于同步官方更新）
  - 新 `origin`：`https://github.com/KKyz007/UnrealAgent.git`（用户 fork 仓库）
  - 本地 `main` 分支已重新追踪 `origin/main`
  - 当前 HEAD：`bb37ebc chore: add uv.lock for reproducible Python dependencies`
  - 后续工作流：本地改动 → `push origin main`；同步官方更新 → `fetch upstream && merge upstream/main`

---

## 一、架构设计

### 1.1 核心架构模式
- **GameInstanceSubsystem** 做全局管理（LevelSubsystem、EconomySubsystem 等），生命周期跟随 GameInstance
- **场景配置 Actor**（BP_GameConfig）放在关卡中，集中控制所有玩法参数，设计师拖进场景就能调参
- **DeveloperSettings**（ProjectSettings）作为 GameConfig 缺失时的 fallback 默认值
- **DataTable** 存关卡配置，代码中无 DataTable 时自动生成 fallback 行

### 1.2 物品生命周期状态机
```
Idle → Selected → MovingToSlot → InSlot → PendingMatch → Matched → Destroyed
```
- **PendingMatch** 是关键中间态：标记后不参与匹配计数，防止快速点击导致 4 消
- **Destroyed** 后延迟 1 秒再 `Destroy()`，减少帧内大量销毁的性能尖峰

---

## 二、物理系统

### 2.1 棉花玩偶手感参数
```cpp
Restitution = 0.05f;    // 极低弹性（几乎不弹）
Friction = 0.8f;         // 高摩擦
StaticFriction = 0.9f;
Density = 0.5f;          // 低密度（轻飘飘）
LinearDamping = 2.0f;    // 高阻尼 = 运动快速衰减
AngularDamping = 3.0f;
```

### 2.2 CCD（连续碰撞检测）
- 构造函数设 `BodyInstance.bUseCCD = true`
- **坑**：碰撞配置可能覆盖 CCD 设置，需要在 `SetSimulatePhysics(true)` 前再设一次
- 地板/墙壁是静态碰撞体，不需要开 CCD

### 2.3 生成位置碰撞检测
- 从底部随机 XY 生成，Box 碰撞检测避免重叠
- 冲突时往上挪 + 随机微调 XY
- **X/Y/Z 碰撞间距独立可调**（SpawnCheckScaleX/Y、SpawnZStepScale），通过 GameConfig 暴露

### 2.4 吸力系统（Attractor）
- 独立 Actor（BP_Attractor），场景中放置定义吸力位置
- 支持三种衰减模式：恒定、线性、平方反比
- 有死区半径防止到中心后抖动
- ObjectUnit 在 Idle+物理模拟+有 Attractor 时才开 Tick 施力，否则关闭 Tick

### 2.5 SPH 物理的教训
- 尝试过用 SPH（光滑粒子流体力学）替代 PhysX 驱动物品物理
- **结果**：效果差 + GPU 占用极高，不适合固体物品堆叠场景
- **结论**：SPH 适合流体，固体堆叠用 PhysX 就够了

---

## 三、槽位系统

### 3.1 同类归类入槽
- `FindInsertIndex()`：找最后一个同类型物品后面插入
- 插入后先 `RearrangeSlotPositions()` 重排已有物品，再让新物品飞向目标

### 3.2 三消检测时序
```
TryAddObject → RearrangeSlotPositions → AnimateToSlot → 延迟 MatchCheckDelay → CheckAndPerformMatch
```
- **MatchCheckDelay**（默认 0.4s）等飞行动画完成再检测
- **PostMatchRearrangeDelay**（默认 0.3s）消除动画结束后再重排

### 3.3 消除位置修正（重要坑）
**问题**：消除动画播放时物品位置不对
**原因**：`PerformMatch` 中直接调 `OnMatched()`，但物品可能还在旧位置
**解决**：`PerformMatch` 中先遍历所有活跃物品按槽位顺序更新位置，再标记 PendingMatch 再调 OnMatched
```cpp
// 先更新位置到正确槽位
for (遍历 SlotObjects，跳过 PendingMatch/Matched/Destroyed)
{
    Obj->SetActorLocation(GetSlotWorldLocation(SlotIdx) + Obj->SlotPositionOffset);
    ++SlotIdx;
}
// 然后才标记消除
```

### 3.4 OnMatched 中不要跳位置
**坑**：之前 `OnMatched()` 里有 `SetActorLocation(FlyTargetLocation)` 会把刚设好的正确位置覆盖掉
**解决**：移除该逻辑，直接用当前位置作为消除动画起点

### 3.5 消除动画
- 改为**原地缩放消失**（不移动位置），避免位置跳变
- 动画完成后 `SetActorHiddenInGame(true)` 先隐藏，延迟 1 秒再 `Destroy()`

---

## 四、输入检测

### 4.1 Line Trace 配置
- `bTraceComplex = true`：精确到三角面，支持异形模型
- 用 `ECC_Visibility` 通道
- **坑**：模型必须有碰撞体且响应 Visibility 通道

### 4.2 按下检测 + 松开选取
```
左键按下 → 开始 Tick 检测 → 命中物品 Stencil=1（高亮）
左键按住 → 每帧更新高亮（滑动切换）
左键松开 → 再做一次最终检测 → 选取物品 → Stencil=0
```
- **坑**：松开时 `CurrentHighlightedObject` 可能因 Tick 时序被清空
- **解决**：`OnClickReleased` 中再做一次 `TraceForObject`，优先用射线结果

### 4.3 Enhanced Input 事件
- `Started`：按下瞬间
- `Triggered`：按住每帧（需要 IA 配置正确的 Trigger）
- `Completed`：松开瞬间
- **坑**：`Completed` 不一定 100% 触发，用 `Started` + Tick + `Completed` 更稳

### 4.4 Custom Depth Stencil 高亮
- `RenderCustomDepth` 和 `StencilValue` 要**同步开关**
- 默认关闭，只有高亮物品才开启（性能最优）
- **坑**：`UpdateVisualState()` 中如果有 `SetRenderCustomDepth(false)` 会把所有物品的 CustomDepth 关掉

---

## 五、动画系统

### 5.1 全 Tick 驱动
- 不用 Timeline/Sequencer，所有动画在 `Tick()` 中用 Alpha 插值
- 没有动画在播放时 `SetActorTickEnabled(false)` 节省性能

### 5.2 生长动画
- 从 10% 缩放 → 100%，ease-out 立方曲线
- 生长期间物理关闭，完成后开启
- 逐个延迟触发（`TotalGrowTime / N` 间隔）

### 5.3 飞行动画
- 位置：ease-out 立方插值 + 正弦弧线
- 旋转：同步插值到槽位目标旋转
- 缩放：同步插值到 `SlotScaleMultiplier`（每种物品可独立配置）

### 5.4 Squash & Stretch
- 碰撞回调 `OnHit` 触发
- 前 30% 压扁，后 70% 弹回 + 过冲
- `SquashMinImpactSpeed` 防止微小碰撞触发

---

## 六、配置系统

### 6.1 FCBGObjectTypeConfig 结构体
每种物品可独立配置：
| 参数 | 说明 |
|------|------|
| Mesh | 模型 |
| ScaleMultiplier | 场景中缩放倍率 |
| SlotScaleMultiplier | 槽位缩放倍率 |
| SlotPositionOffset | 槽位位置偏移 |
| ClickSound | 点击音效 |

### 6.2 参数优先级
```
DataTable > 场景 GameConfig Actor > ProjectSettings (DeveloperSettings)
```

### 6.3 SlotAnchor
- 场景中放 7 个 BP_SlotAnchor 实例（SlotIndex 0~6）
- SlotSystem BeginPlay 自动收集位置和旋转
- 无 Anchor 时用默认等距布局

---

## 七、性能优化

### 7.1 Tick 最小化原则
| 组件 | 策略 |
|------|------|
| ObjectUnit | 无动画+无吸力时关闭 Tick |
| PlayerController | 鼠标不按下时不检测（Tick 只在按下期间跑） |
| SlotSystem | 完全无 Tick |
| GameConfig | 完全无 Tick |

### 7.2 Custom Depth 按需开启
- 只有当前高亮的 1 个物品开启 `RenderCustomDepth`
- 其余全部关闭，避免 84 个物品全部渲染 Custom Depth Pass

### 7.3 延迟销毁
- 消除动画完成后先隐藏（`SetActorHiddenInGame`），延迟 1 秒再 `Destroy()`
- 避免同帧大量 Destroy 导致卡顿

---

## 八、命名规范

### 8.1 资产命名前缀
| 前缀 | 类型 |
|------|------|
| SM_ | Static Mesh |
| MI_ | Material Instance |
| M_ | Material |
| T_ | Texture |
| SFX_ | Sound Effect |
| MUS_ | Music |
| NS_ | Niagara System |
| BP_ | Blueprint |
| IMC_ | Input Mapping Context |
| IA_ | Input Action |

### 8.2 C++ 命名
- 项目前缀：CBG（CatchBigGoose）
- UE5 标准：PascalCase、bool 加 b 前缀
- 类前缀：A(Actor)、U(Object)、F(Struct)、E(Enum)

### 8.3 目录结构
```
Content/
  Audio/SFX/Animals/    ← 动物叫声
  Audio/Music/          ← BGM
  Meshes/风格化潮玩/    ← SM_Bunny, SM_Cat 等
  VFX/Niagara/          ← 特效
  Input/                ← IMC_Default, IA_Click
  Maps/                 ← L_StylizedToy 等
```

---

## 九、常见坑总结

| 坑 | 原因 | 解决 |
|----|------|------|
| 点击穿透 | `ETriggerEvent::Triggered` 每帧触发 | 用 `Started` 只触发一次 |
| 消除丢物品 | PerformMatch 嵌套 Timer | 立即标记 PendingMatch + 立即 OnMatched |
| 消除位置跳变 | OnMatched 跳到 FlyTargetLocation | 移除跳转，用 PreMatch 更新的当前位置 |
| 飞行旋转不对 | 没传目标旋转 | AnimateToSlot 接收 SlotAnchor 旋转 |
| Custom Depth 不生效 | UpdateVisualState 关了 RenderCustomDepth | 移除该行，保持 CustomDepth 由 PlayerController 控制 |
| SPH 性能差 | CPU 端 SPH 不适合固体堆叠 | 回退到 PhysX |
| 物品生成重叠 | 碰撞检测间距太小 | 暴露 SpawnCheckScaleX/Y 到 GameConfig |
| Squash 不触发 | ComplexAsSimple 碰撞不产生 OnHit | 需要 Auto Convex Collision |
| 模型碰撞偏 | Pivot 不在中心 | Blender 批量 XY 居中 + Z 底部对齐 |
