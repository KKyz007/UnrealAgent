# UE5 Add Custom MeshDrawPass 实现指南

> **原文标题**：UE5 Add Custom MeshDrawPass
> **作者**：搓手苍蝇（知乎 · UE5渲染编程专栏）
> **原链接**：https://zhuanlan.zhihu.com/p/XXXXX（需补）
> **引擎版本**：UE 5.0.3（5.1 也适用，5.3/5.5 有接口微调）
> **记录日期**：2026-04-22
> **应用案例**：卡通渲染 **Backface OutLine（背面法线外扩描边）** 多 Pass 方案

---

## 零、文章定位

本文解决的核心问题是 **"如何给 UE5 添加一个自定义的 MeshDrawPass"**，从引擎源码层增加一条完整的渲染流水线入口。

**具体案例**：给材质球加一个"是否开启描边"的开关，开启后该物体会在 Lights Pass 之后、Translucent Pass 之前**额外绘制一遍**（Backface 外扩 + 描边色）。

**描边特性**：
- 材质球面板控制 开关 / 颜色 / 粗细
- 支持顶点色控制颜色和粗细（覆盖材质参数）
- 粗细随视距自动调整（远粗近细，带上限）

> 作者原话：**描边本身不重要，重要的是走通新建 Pass 的代码流程**。

---

## 一、整体思路（5 大步骤）

```
① 全局 Pass 枚举       → 在 MeshPassProcessor.h 注册新 Pass 名字
② 自定义 MeshPassProcessor → 负责收集 MeshBatch + 生成 MeshDrawCommand
③ 自定义 Shader 类     → 继承 FMeshMaterialShader（VS + PS）
④ 修改可见性收集       → ComputeDynamicMeshRelevance + MarkRelevant
⑤ 在 FSceneRenderer 添加 Render 入口函数 → 用 RDG 提交 Pass
⑥ ToonOutline.usf      → Shader 文件（法线外扩 + 纯色输出）
```

---

## 二、C++ 部分详解

### 2.1 注册全局 Pass 枚举（必改）

文件：`Engine\Source\Runtime\Renderer\Public\MeshPassProcessor.h`

> 评论区纠正：文中写的 `MeshDrawProcessor.h` 是错的，正确是 **`MeshPassProcessor.h`**

需要修改三处：
1. `EMeshPass` 枚举里加一行 `BackfaceOutLinePass`
2. `GetMeshPassName()` 函数 switch 加一行（RenderDoc 调试名字）
3. 下方的**编译检测 static_assert**同步加上，否则编译不通过

### 2.2 自定义 FMeshPassProcessor 子类

新建两个文件（放在引擎 Renderer 模块里）：
- `ToonOutlinePassRendering.h`
- `ToonOutlinePassRendering.cpp`

**.h 关键声明**：
```cpp
class FToonOutlineMeshPassProcessor : public FMeshPassProcessor
{
public:
    FToonOutlineMeshPassProcessor(
        const FScene* Scene,
        const FSceneView* InViewIfDynamicMeshCommand,
        const FMeshPassProcessorRenderState& InPassDrawRenderState,
        FMeshPassDrawListContext* InDrawListContext);

    // 必须重载：引擎会把 MeshBatch 喂进来，自己判断是否参与本 Pass
    virtual void AddMeshBatch(
        const FMeshBatch& RESTRICT MeshBatch,
        uint64 BatchElementMask,
        const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy,
        int32 StaticMeshId = -1) override final;

private:
    template<bool bPositionOnly, bool bUsesMobileColorValue>
    bool Process(...);

    FMeshPassProcessorRenderState PassDrawRenderState;
};
```

**过滤规则**（在 `AddMeshBatch` 里）：
```cpp
if (Material.GetRenderingThreadShaderMap() && Material.UseToonOutLine())
{
    if (BlendMode == BLEND_Opaque)
    {
        Process<false, false>(...);  // 只处理不透明材质
    }
}
```

**Process 函数核心**：
1. 通过 `MaterialResource.TryGetShaders()` 拿到编译好的 VS/PS
2. 初始化 `FToonOutlinePassShaderElementData`
3. 计算 SortKey
4. 设置深度状态（`CF_GreaterEqual` - UE 用反向深度）
5. 调用 `BuildMeshDrawCommands(...)` —— 这是**生成 Command 的统一入口**

### 2.3 自定义 Shader 类（VS + PS）

两个类都继承 `FMeshMaterialShader`：

**VS (`FToonOutlineVS`)**：
- `LAYOUT_FIELD(FShaderParameter, OutLineScale)` ← 描边粗细参数
- `ShouldCompilePermutation`：只对 **开启 bUseToonOutLine 的材质** + **LocalVertexFactory / GPUSkinVertexFactoryDefault**（静态/骨骼网格）编译
- `GetShaderBindings`：从 `Material.GetToonOutLineScale()` 拿值绑定到 Shader

**PS (`FToonOutlinePS`)**：
- `LAYOUT_FIELD(FShaderParameter, OutLineColor)`
- 同样 `GetShaderBindings` 里从材质取 `ToonOutLineColor` 绑定

**Shader 路径注册**（.cpp）：
```cpp
IMPLEMENT_MATERIAL_SHADER_TYPE(, FToonOutlineVS, 
    TEXT("/Engine/Private/ToonOutLine.usf"), TEXT("MainVS"), SF_Vertex);
IMPLEMENT_MATERIAL_SHADER_TYPE(, FToonOutlinePS, 
    TEXT("/Engine/Private/ToonOutLine.usf"), TEXT("MainPS"), SF_Pixel);
```

### 2.4 修改可见性收集（Static + Dynamic）

引擎有两个地方添加 MeshDrawCommand：
| 类型 | 位置 | 特点 |
|------|------|------|
| **Static Cache** | `MarkRelevant` 函数 | 只有 `FLocalVertexFactory`（StaticMeshComponent）能 cache |
| **Dynamic Cache** | `ComputeDynamicMeshRelevance` 函数 | 每帧生成，其他 VertexFactory 都走这里 |

两处都要改：告诉 `FPrimitiveViewRelevance` 我们这个 Pass **需要参与收集**。

### 2.5 在 FSceneRenderer 添加 Render 入口

**声明**（`SceneRendering.h` 里 FSceneRenderer 类中）：
```cpp
void RenderToonOutlinePass(FRDGBuilder& GraphBuilder, FRDGTextureRef SceneColorTexture);
```

**实现要点**（放在 `ToonOutlinePassRendering.cpp`）：

```cpp
void FSceneRenderer::RenderToonOutlinePass(FRDGBuilder& GraphBuilder, FRDGTextureRef SceneColorTexture)
{
    for (FViewInfo& View : Views)
    {
        // 1. 分配一个 SimpleMeshDrawCommandPass
        FSimpleMeshDrawCommandPass* SimpleMeshPass = 
            GraphBuilder.AllocObject<FSimpleMeshDrawCommandPass>(View, nullptr);

        // 2. 构造 Processor
        FToonOutlineMeshPassProcessor MeshProcessor(
            Scene, &View, DrawRenderState, 
            SimpleMeshPass->GetDynamicPassMeshDrawListContext());

        // 3. 遍历 Scene->Primitives，手动喂 MeshBatch
        for (每个 PrimitiveSceneInfo) {
            if (bStaticRelevance) → 遍历 StaticMeshes 调 AddMeshBatch
            if (bDynamicRelevance) → 遍历 DynamicMeshElements 调 AddMeshBatch
        }

        // 4. 分配 PassParameters（View UBO + RenderTarget + DepthStencil）
        FToonOutlineMeshPassParameters* PassParameters = 
            GraphBuilder.AllocParameters<FToonOutlineMeshPassParameters>();
        PassParameters->RenderTargets[0] = 
            FRenderTargetBinding(SceneTextures.Color.Target, ELoad);
        PassParameters->RenderTargets.DepthStencil = FDepthStencilBinding(
            SceneTextures.Depth.Target, ENoAction, ELoad, DepthWrite_StencilNop);

        // 5. 生成 RenderingCommands
        SimpleMeshPass->BuildRenderingCommands(
            GraphBuilder, View, Scene->GPUScene, 
            PassParameters->InstanceCullingDrawParams);

        // 6. GraphBuilder.AddPass 提交到 RDG
        GraphBuilder.AddPass(
            RDG_EVENT_NAME("ToonOutlinePass"),
            PassParameters,
            ERDGPassFlags::Raster,
            [SimpleMeshPass, PassParameters](FRHICommandList& RHICmdList) {
                SimpleMeshPass->SubmitDraw(RHICmdList, 
                    PassParameters->InstanceCullingDrawParams);
            });
    }
}
```

**调用位置**：在 `Render()` 函数中 **LightingPass 之后、TranslucentPass 之前** 插入：
```cpp
RenderToonOutlinePass(GraphBuilder, SceneColorTexture);
```

---

## 三、Shader 部分（ToonOutline.usf）

路径：`Engine/Shaders/Private/ToonOutline.usf`

```hlsl
#include "Common.ush"
#include "/Engine/Generated/Material.ush"
#include "/Engine/Generated/VertexFactory.ush"

struct FSimpleMeshPassVSToPS
{
    FVertexFactoryInterpolantsVSToPS FactoryInterpolants;
    float4 Position : SV_POSITION;
};

float OutLineScale;    // from cpp (ShaderBindings)
float3 OutLineColor;

#if VERTEXSHADER
void MainVS(FVertexFactoryInput Input, out FSimpleMeshPassVSToPS Output)
{
    ResolvedView = ResolveView();
    FVertexFactoryIntermediates VFIntermediates = GetVertexFactoryIntermediates(Input);
    
    float4 WorldPos = VertexFactoryGetWorldPosition(Input, VFIntermediates);
    float3 WorldNormal = VertexFactoryGetWorldNormal(Input, VFIntermediates);
    float3x3 TangentToLocal = VertexFactoryGetTangentToLocal(Input, VFIntermediates);
    FMaterialVertexParameters VertexParameters = 
        GetMaterialVertexParameters(Input, VFIntermediates, WorldPos.xyz, TangentToLocal);
    WorldPos.xyz += GetMaterialWorldPositionOffset(VertexParameters);
    
    float4 RasterizedWorldPosition = 
        VertexFactoryGetRasterizedWorldPosition(Input, VFIntermediates, WorldPos);
    Output.FactoryInterpolants = 
        VertexFactoryGetInterpolantsVSToPS(Input, VFIntermediates, VertexParameters);
    Output.Position = mul(RasterizedWorldPosition, ResolvedView.TranslatedWorldToClip);
    
    // 裁剪空间做法线外扩（非世界空间）
    float2 ExtentDir = normalize(mul(float4(WorldNormal, 1.0f), 
                                      ResolvedView.TranslatedWorldToClip).xy);
    float Scale = clamp(0.0f, 0.5f, Output.Position.w * OutLineScale * 0.1f);
    Output.Position.xy += ExtentDir * Scale;
}
#endif

void MainPS(FSimpleMeshPassVSToPS Input, out float4 OutColor : SV_Target0)
{
    OutColor = float4(OutLineColor, 1.0);
}
```

**要点**：
- 描边外扩**在裁剪空间做**（而不是世界空间），好处是粗细随距离自动缩放（因为 `Position.w` 就是深度）
- `clamp(0, 0.5, Position.w * OutLineScale * 0.1)` 控制上限，避免近处过粗
- PS 直接输出 `OutLineColor`，纯色描边

---

## 四、重要概念速览

### 4.1 MeshDrawCommand vs MeshPassProcessor

| 概念 | 作用 |
|------|------|
| **FMeshDrawCommand** | 一次 DrawCall 的全部资源打包（VB/IB/Shader/UBO/State） |
| **FMeshPassProcessor** | 负责**收集资源 → 调用 BuildMeshDrawCommands 生成 Command** |
| `View.ParallelMeshDrawCommandPasses` | 生成好的 Command 存放处 |

### 4.2 BuildMeshDrawCommands 的作用

这是 `FMeshPassProcessor` 的公有方法，**所有 Pass 的 Command 生成函数**。开发者只管:
- 把 Batchmesh、渲染状态、PassFeature 塞进去
- 它自己生成 Command 并 push 到 View 上

### 4.3 Static / Dynamic 差异

> DynamicMesh 每帧生成。目前只有 `FLocalVertexFactory`（即 UStaticComponent）可以 Cached，其它 VertexFactory 需要依赖 View 设置 ShaderBinding。

### 4.4 UE5 RDG 风格

相比 UE4 到处散落的 RHI 调用，**UE5 用 `FRDGBuilder::AddPass(...)` 统一管理**：
- 输入：PassParameters（RT / DepthStencil / UBO）
- Lambda 里写 RHI 命令
- RDG 自动管理资源依赖和生命周期

---

## 五、评论区重要补充（踩坑点）

| 坑点 | 说明 |
|------|------|
| **文件名拼写** | 正确是 `MeshPassProcessor.h`，原文写成 `MeshDrawProcessor.h` |
| **OutLine vs Outline** | 命名推荐 `Outline`（英文惯例），文中大量用 `OutLine` |
| **加 Pass 必须编译引擎** | 改源码的，肯定要编译（评论区有人问） |
| **UE 5.3 API 变化** | `View.DynamicMeshEndIndices` 接口变了，需参考其它 Pass 写法 |
| **UE 5.5 API 变化** | Shader 获取 / 混合状态 / 深度模板设置都挪到 `CollectPSOInitializers` 函数里 |
| **FSR 残影问题** | 开启后有黑色描边残影（TAA / TSR 时间反走样问题），**作者自己也没解决**；有人改用 Overlay Material 方式规避 |
| **RWTexture3D 绑定** | 可以参考 CustomDepthStencil Pass 或后处理的写法 |

---

## 六、与前一篇 Shell Fur 的关联

搓手苍蝇这篇 + YivanLee 那篇 **刚好形成完整方案**：

| 文章 | 解决什么 | 关系 |
|------|----------|------|
| YivanLee《Shell Fur Cloth Material》 | **为什么要加 Pass**（毛发需要多层 Shell） | 理论层 |
| 搓手苍蝇《Add Custom MeshDrawPass》 | **怎么加 Pass**（完整代码骨架） | 实现层 |

**把描边逻辑换成循环 N 次 + 每层沿法线外扩不同距离**，就是 Shell Fur 的实现。

---

## 七、在本项目（CatchBigGoose）的参考价值

### 7.1 本项目**不需要**改引擎的场景
- 玩偶高亮 → **CustomDepthStencil + 后处理材质**（已做，见 Skill）
- 描边 → **PostProcess 边缘检测**或 **OverlayMaterial**（不改引擎）
- 简单毛发 → **Groom 系统** / **WPO + ISM 伪造 Shell**

### 7.2 本项目**可以考虑**改引擎的场景
- 极致风格化效果（NPR 描边 + 毛发 + 卡通渲染堆叠）
- 需要精确控制渲染顺序的 Debug 可视化 Pass
- 做引擎级插件给多个项目复用

### 7.3 技术储备建议
本文代码骨架可作为**通用 MeshDrawPass 模板**，日后如需实现：
- 自定义描边 / Xray 穿透效果
- Shell Fur 毛发
- 特殊材质分类渲染
- Debug 可视化（法线/UV 等）

都可以沿用这个架构，只改 Shader 和 Process 过滤规则即可。

---

## 八、Reference（原文参考）

1. 虚幻4渲染编程(Shader篇)【第十三卷：定制自己的MeshDrawPass】—— https://zhuanlan.zhihu.com/p/66545369
2. 【UE4.26.0】定制一个自己的MeshPass（移动端踩坑）—— https://zhuanlan.zhihu.com/p/342681912
3. 虚幻5渲染编程(材质篇)[第六卷: Material Shading Model ID数据的传递过程] —— https://zhuanlan.zhihu.com/p/462719812
4. 虚幻五渲染编程(Graphic篇)【第三卷：Create UniformBuffer in RDG System】—— https://zhuanlan.zhihu.com/p/472290623
5. RenderDependencyGraph 学习笔记 —— BlueRose's Blog
6. 剖析虚幻渲染体系 —— 0向往0 cnblogs

---

**记录结论**：
> 如果不是核心需求（特殊美术风格 / 引擎级功能），**普通游戏不推荐走这条路**。
> CatchBigGoose 这种休闲三消玩法，用后处理和蓝图材质完全够用，改引擎成本收益比太差。
> 这篇文章的价值在于 **"将来有需要时，知道怎么下手"**。
