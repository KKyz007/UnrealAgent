# UE5 Shell Fur & Cloth Material 实现指南

> **原文**：虚幻5渲染编程(材质篇)[第二卷: Shell Fur Cloth Material]
> **作者**：YivanLee（知乎）
> **原链接**：https://zhuanlan.zhihu.com/p/395723380
> 记录日期：2026-04-22

---

## 一、核心结论

**Shell Fur（壳体毛发）是短毛类材质的最佳方案**，适合：
- 毛巾
- 短皮毛
- 绒毛（层数加多即可）

**其他尝试失败的方案**：
- Parallax Occlusion（视差遮蔽）— 效果差
- Ray Marching（光线步进）— 效果差

**结论**：要让毛发真正"突出来"，只能用 **Shell Fur**。

---

## 二、UE vs Unity 的核心差异

| 引擎 | 多 Pass 实现难度 |
|------|------------------|
| **Unity** | ⭐ 简单：ShaderLab 里多写几个 `Pass{}` 即可，或用多个 SubMaterial |
| **UE** | ⭐⭐⭐⭐⭐ 困难：需要**修改引擎源码**，对 UE 架构熟悉且有代码功力 |

> 在 UE 中实现多 Pass 渲染的**难度是 Unity 的 100 倍**，这就不是一般 TA 或图形程序能做到的了。

---

## 三、Shell Fur 的原理

### 3.1 核心思路
```
把模型重复渲染 N 层 → 每层贴图往内收缩一点 → 产生立体感
```

### 3.2 正确做法
❌ **错误**：真的把模型复制 N 份拖到场景里（产生大量冗余数据）
✅ **正确**：**画多次模型**，只需要添加多个 `MeshDrawCommand` 即可

### 3.3 毛发扭曲
在 Vertex Shader 中对**向外挤出的模型顶点**做扭曲即可，非常方便：
```hlsl
// 示例：风力扭曲发尖
VertexOffset = Normal * FurLength + WindDir * ShellOffset;
```

### 3.4 毛发形状控制
作者使用**曲线**来控制毛发的上下端（根部粗、尖部细的渐变）。

---

## 四、修改 UE 引擎添加多 Pass（核心步骤）

### 步骤 1：在 Material 里增加 SubMaterial 数组
用来存放每一层 Shell 的材质引用。

### 步骤 2：在 BasePass 之后增加一个单独的 Pass
这个新 Pass 负责循环调用 SubMaterial 做多次绘制。

### 步骤 3：增加自定义的 MeshDrawCommandProcessor
**参考实现**：可以参考引擎中 `SkyPass` 的处理方式。

核心逻辑：
```cpp
// 伪代码：循环创建多个 MeshDrawCommand
for (int i = 0; i < SubMaterials.Num(); ++i)
{
    FMeshDrawCommand Cmd;
    Cmd.MaterialRenderProxy = SubMaterials[i];
    Cmd.ShellIndex = i;  // 传递层索引给 VS
    OutDrawList.Add(Cmd);
}
```

**效果**：添加了多少个 SubMaterial，这个模型就会被绘制多少次。

### 步骤 4：在 ComputeDynamicMeshRelevance 里打开 PassMask
让渲染系统知道这个新 Pass 需要被执行。

### 步骤 5：重载自己的 AddBatchMesh
把自定义 Pass 接入到渲染管线里。

---

## 五、UMaterial / FMaterial 架构补充

**关键概念**（作者额外记录）：

| 类 | 作用 |
|----|------|
| `UMaterial` | 蓝图资产层，一个 UMaterial 对应一个 `.uasset` |
| `FMaterial` | 运行时渲染数据，一个 UMaterial 会对应**多个 FMaterial** |
| `FMaterialResource` | FMaterial 的子类 |

**为什么一个 UMaterial 有多个 FMaterial？**
因为需要按 **平台（PC/iOS/Android）** × **质量等级（Low/Med/High）** 存储不同的 Shader 和资源。

---

## 六、不改引擎的替代方案

如果**不想修改引擎源码**，可以用插件方式实现：

| 方案 | 适用对象 |
|------|----------|
| **基于插件的 StaticMesh 多 Pass 绘制** | 静态网格（推荐） |
| **基于插件的 SkeletalMesh 多 Pass 绘制** | 骨骼网格（角色） |
| **塞模型法** | 直接在场景放多份网格，冗余但简单 |

参考 bluerose 的知乎专栏文章。

---

## 七、参考文献

1. [Generating Fur in DirectX or OpenGL Easily](https://developer.nvidia.com/gpugems/gpugems/part-iii-materials/chapter-23-hair-animation-and-rendering-nalu-demo)
2. Fur Rendering 通用算法
3. [gim.studio - An Introduction to Shell Based Fur Technique](https://gim.studio/an-introduction-to-shell-based-fur-technique/)
4. bluerose：基于插件的 StaticMesh 多 Pass 绘制方案
5. bluerose：基于插件的 SkeletalMesh 多 Pass 绘制方案
6. [作者另一篇：虚幻4渲染编程(Shader篇)第十六卷 Multi-BasePass in UE4](https://zhuanlan.zhihu.com/p/139934922)

---

## 八、与 Unity Shell Fur 的对比（结合之前的 Skill）

| 维度 | Unity（YOung） | UE5（YivanLee） |
|------|----------------|------------------|
| **多 Pass 实现** | ShaderLab 多个 `Pass{}` | 修改引擎添加自定义 Pass |
| **难度** | ⭐ | ⭐⭐⭐⭐⭐ |
| **是否改引擎** | 不需要 | **必须改源码** |
| **扩展方式** | 直接改 Shader 文件 | C++ + HLSL，需要熟悉 Renderer 模块 |
| **适合对象** | TA / 图形程序 | 引擎程序 / 资深 TA |
| **配套源码** | [ExtendStandard](https://github.com/chenyong2github/ExtendStandard) | 无（作者未公开源码） |

---

## 九、在本项目（CatchBigGoose）中的参考价值

如果未来要给玩偶加毛发效果（如兔子、狐狸、浣熊的毛茸茸外观）：

### 推荐路线（从简到难）

1. **UE5 Groom 系统**（最简单）
   - Alembic 导入曲线毛发
   - 原生支持，无需改引擎
   - 适合角色特写

2. **Niagara 粒子毛发**（中等）
   - 用粒子模拟毛发
   - 可控性好

3. **ISM + Material Shell**（中等，不改引擎）
   - 每层 Shell 作为 InstancedStaticMesh
   - Material 中 `WorldPositionOffset` 沿法线偏移
   - 性能中等

4. **修改引擎添加多 Pass**（最强，但工作量极大）
   - 本文方案
   - 仅在需要极致效果时考虑

**本项目建议**：用 **Material 中伪造 Shell**（WorldPositionOffset + ISM）就够了，无需修改引擎。
