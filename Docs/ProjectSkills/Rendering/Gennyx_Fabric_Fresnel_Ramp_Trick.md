# 布料材质的"伪造"技巧 — Fresnel × Ramp 驱动法

> **原文**：It's a Material World
> **作者**：Gen（Gennyx Blog）
> **原链接**：https://gennyx.blogspot.com/2009/11/its-material-world_11.html
> **发布**：2009-11-11
> **技术栈**：Maya + Mental Ray（离线渲染）
> **记录日期**：2026-04-23
> **价值**：提供了一个**跨引擎通用**的"伪布料"思路，**不依赖 PBR BRDF**

---

## 一、本文讲什么（一句话）

> **用 `Fresnel × Ramp`（视角余弦驱动颜色渐变）在任何引擎里快速伪造缎面 / 天鹅绒 / 丝绸的视觉特征。**

不用写复杂的 Fabric BRDF，不用改 Shading Model，只要拿 Fresnel 作为 Ramp 的 UV，就能让材质呈现"边缘颜色变化"这个**布料最关键的视觉特征**。

---

## 二、核心公式

### Maya 原版（Mental Ray）
```
samplerInfo.facingRatio  →  ramp.uCoord
samplerInfo.facingRatio  →  ramp.vCoord
ramp.outColor            →  mia_material.reflection_color
```

### UE 等价公式
```
Fresnel Node (ExponentIn=2~5)
    ↓
Lerp(ColorCenter, ColorEdge, Fresnel)
    ↓
连到 Base Color / Specular / Emissive（看需求）
```

### Unity 等价公式
```hlsl
float facing = 1.0 - saturate(dot(N, V));        // Fresnel
half4 col = tex2D(_RampTex, float2(facing, 0));  // 采 Ramp
return col;
```

**三者本质相同**：都是**"N·V 算 Fresnel → 查 Ramp 图 → 输出颜色"**。

---

## 三、为什么这个技巧有效？

### 布料的关键视觉特征
| 特征 | 物理原因 | 视觉表现 |
|------|---------|---------|
| **边缘发亮** | 表面纤毛斜射散射 | 轮廓处颜色变化 |
| **非清晰反射** | 纤维微观粗糙 | 只反射光源模糊块，不反环境 |
| **视角依赖性** | 纤维方向各向异性 | 旋转时颜色变化 |

### 这个技巧捕捉了哪些
- ✅ **边缘发亮** → Fresnel 驱动 Ramp 完美实现
- ✅ **视角依赖性** → N·V 变化直接驱动颜色
- ⚠️ **非清晰反射** → 需要配合低 Glossiness / 高 Roughness

**虽然不物理精确，但"看起来对"**，这就是游戏/影视能用的理由。

---

## 四、原文实现细节（Maya 版）

### 缎面 Satin 完整配方
```
1. Base Material: mia_material
2. Diffuse Color: 暗红色
3. Reflectivity: 极高
4. BRDF Min/Max: 都调高（任何视角都高反射）
5. 勾选 "Highlights Only"（不反环境，只反光源）
6. Reflection Glossiness: 0.4（模糊反射 = 缎面柔顺感）

Ramp 配置:
  - 面向相机端: 暗红 → 绿 → 蓝（示例）
  - 偏离相机端: 亮红
  - 避免满饱和（会过曝"radioactive sunflares"）
```

### 天鹅绒 Velvet
- **基础流程同缎面**
- **额外**：用 Fractal 节点接到 Bump 输入（绒毛微观凹凸）
- Ramp 配色更偏暗色系（天鹅绒整体偏暗）

### 丝绸 Silk（评论区提到）
- 同样方法
- Ramp 用更鲜艳的饱和色
- 作者引用："Disco 时代丝绸衬衫"

### 薄纱窗帘 Sheer（评论区提到）
- `thin-walled` mia_material（非 solid）
- 加透明度 + Translucency
- 适合能透光的布料

---

## 五、在 UE 材质中的完整实现

### 节点图（连 Default Lit 主材质）

```
[Fresnel 节点]
 ├─ ExponentIn = 3.0
 ├─ BaseReflectFractionIn = 0.04
 └─ Normal = VertexNormalWS
          ↓
     [Fresnel 标量 0~1]
          ↓
     [Lerp]
  A: CenterColor (暗红 0.3, 0.1, 0.1)
  B: EdgeColor   (亮红 0.9, 0.2, 0.2)
          ↓
     → Base Color

[再来一个 Fresnel]
 ├─ ExponentIn = 5.0（更窄的边缘）
          ↓
     [乘以常量 0.5]
          ↓
     → Emissive Color（边缘发亮，非物理但好看）

其他设置：
 - Metallic: 0
 - Roughness: 0.6~0.8
 - Specular: 0.3
```

### UE HLSL 版本（Custom 节点）
```hlsl
// 输入：Normal (float3), CameraVector (float3)
// 输出：float3 Color

float NdotV = saturate(dot(Normal, CameraVector));
float Fresnel = pow(1.0 - NdotV, 3.0);

float3 CenterColor = float3(0.3, 0.1, 0.1);
float3 EdgeColor   = float3(0.9, 0.2, 0.2);

return lerp(CenterColor, EdgeColor, Fresnel);
```

---

## 六、可推广的用途（超越布料）

原文作者金句：
> **"同样的方法可用于模拟天鹅绒、卡通着色、电子显微镜图像等各种效果。只需要在插入 ramp 的位置上发挥创意即可！"**

### 推广场景
| 效果 | Ramp 内容 | 连接位置 |
|------|----------|---------|
| **布料边缘散射** | 暗→亮颜色渐变 | Base Color |
| **卡通两档色** | 硬边阶跃色 | Base Color |
| **X 光骷髅** | 蓝→白 | Emissive |
| **霓虹灯边缘** | 黑→荧光色 | Emissive |
| **宝石变色** | 多段彩虹 | Base Color |
| **电子显微镜** | 绿色阶梯 | Base Color |
| **老电影胶片感** | 棕褐色温度 | Base Color |

**核心心法**：**把 N·V 当成参数，用它查 Ramp。**

---

## 七、和本项目毛发方案的关系

### 本项目现有方案（Shell Fur）
- ✅ 解决"毛茸茸"的**几何轮廓**（多层外壳）
- ⚠️ 但**表面质感**还是 PBR Default Lit（可能显得"塑料感"）

### 这篇文章可以补什么？
**可以在 Shell Fur 材质里加一层 Fresnel × Ramp**：
```
BaseColor = lerp(RootColor, TipColor, ShellIndex)       ← 原毛色渐变
          + Fresnel × FabricEdgeColor × 0.3              ← 新增：边缘布料感
```

**效果**：
- 玩偶正面看 → 正常毛色（BaseColor）
- 玩偶边缘 → 微微泛光的布料感
- 整体**从"塑料玩偶"变成"毛绒玩具"**

**成本**：
- 加一个 Fresnel 节点（几乎零性能开销）
- Material Function `MF_ShellFur` 里多加 1 个输出/输入

---

## 八、与其他布料方案对比

| 方案 | 物理正确性 | 性能 | 实现难度 | 本项目适用 |
|------|----------|------|---------|-----------|
| **Fresnel × Ramp（本文）** | ❌ 伪造 | ⭐⭐⭐⭐⭐ | ⭐（1 个节点）| ✅ **最佳** |
| **Inverted Gaussian（Order:1886）** | ⚠️ 经验 | ⭐⭐⭐⭐ | ⭐⭐⭐ | ⚠️ 要 Custom Shader |
| **Charlie BRDF（物理布料）** | ✅ 正确 | ⭐⭐⭐ | ⭐⭐⭐⭐ | ⚠️ UE5 自带 Cloth SM 支持 |
| **Anisotropic Highlight** | ⚠️ 半物理 | ⭐⭐⭐⭐ | ⭐⭐ | 丝绸用 |
| **Subsurface Scattering** | ✅ 正确 | ⭐⭐ | ⭐⭐⭐⭐⭐ | 厚布料（不适合本项目）|

**本项目选 Fresnel × Ramp** 的理由：
- 零性能开销
- 1 个材质节点
- 视觉提升明显
- 不需要改引擎 / 不需要 Custom Shader

---

## 九、UE5 的官方对应方案（作为对比）

UE5 自带一个 **"Cloth" Shading Model**（Fabric），用法：
```
Material Details Panel:
  Shading Model: Cloth

多出两个输入：
  - Fuzz Color（绒毛色，类似 Ramp Edge）
  - Cloth Mask（布料强度）
```

**本文方案 vs UE5 Cloth**：
| 对比点 | 本文方案 | UE5 Cloth SM |
|-------|---------|------------|
| 性能 | 更轻 | 稍重（多一个 BRDF 分支）|
| 可控性 | 完全自由（Ramp 随便配）| 受限（只能调 FuzzColor）|
| 物理性 | 纯伪造 | 基于物理 |
| Deferred 支持 | ✅ | ✅ |
| Forward/Mobile | ✅ | ⚠️ 部分平台不支持 |

**结论**：
- 本项目追求**可控 + 风格化** → 用 Fresnel × Ramp 伪造
- 如果追求**统一的 PBR 管线** → 用 UE5 Cloth SM
- **两种也可以混用**（Cloth SM + 额外 Fresnel Emissive）

---

## 十、代码文物（原文 2009 年的思路今天还适用）

**时间检验**：
- 文章写于 **2009 年**（17 年前！Mental Ray 时代）
- 但 `Fresnel × Ramp` 这个技巧 **至今还在用**：
  - 原神卡通渲染的"边缘光"
  - Unreal 的 Stylized 渲染教程
  - 皮克斯的短片《Piper》海鸟羽毛

**启示**：**渲染的"视觉本质"比"数学物理"更重要**。找到视觉特征，用最简单的公式驱动它。

---

## 十一、一句话总结

> **布料最关键的视觉特征是"边缘发亮"，而边缘发亮的本质是"Fresnel"。**
> 所以只要用 Fresnel 驱动一个 Ramp / Lerp，**在任何引擎都能零成本伪造出布料质感**。
> 这个 2009 年的 Maya 小技巧，放到 UE5 材质里就是**一个 Fresnel 节点 + 一个 Lerp**，2 分钟搞定。

---

## 十二、对 CatchBigGoose 的具体建议

### 可以加到玩偶材质里
在 `MF_ShellFur` 材质函数里新增一组输入：
```
FabricEdgeColor : Vector3（边缘布料色）
FabricExponent  : Scalar（Fresnel 指数，默认 3）
FabricStrength  : Scalar（混合强度，默认 0.3）
```

输出 `BaseColor` 前加上：
```
FinalBaseColor = lerp(RootColor, TipColor, ShellIndex)
               + pow(1 - NdotV, FabricExponent) × FabricEdgeColor × FabricStrength
```

### 效果
- 玩偶**从"毛绒"升级到"毛绒玩具"**（毛 + 布料质感）
- 成本：0（就多一个 Fresnel 节点）
- 7 种玩偶可各调 FabricEdgeColor 强化个性

---

## 十三、参考资料

- 原文：https://gennyx.blogspot.com/2009/11/its-material-world_11.html
- 相关 Skill：
  - `Unity_PBR_Fur_Material.md`（Fabric BRDF 数学版）
  - `xbdev_Fur_Shell_Texturing_Classic.md`（Shell 技术源头）
  - `UE5_Shell_Fur_Cloth_Material.md`（UE Shell + Cloth 双主题）
- 扩展阅读：
  - UE5 Cloth Shading Model 官方文档
  - 《The Order: 1886》SIGGRAPH 2013 Fabric BRDF
  - 《Rendering the World of Far Cry 4》织物技巧
