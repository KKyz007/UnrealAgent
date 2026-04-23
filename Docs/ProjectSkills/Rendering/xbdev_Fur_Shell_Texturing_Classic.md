# Shell Texturing Fur 经典原理（xbdev.net）

> **原文**：Generating Fur in DirectX or OpenGL Easily
> **原链接**：https://www.xbdev.net/directx3dx/specialX/Fur/
> **作者**：bkenwright@xbdev.net
> **技术栈**：DirectX 9 + HLSL（VS1.1 / PS1.3，2002 年经典教程）
> **记录日期**：2026-04-23
> **地位**：Shell Texturing 毛发技术的**元老级参考文献**，后续 Unity ExtendStandard / UE Shell Fur 都源于此

---

## 一、为什么要读这篇 20 多年前的文章？

| 原因 | 说明 |
|------|------|
| **奠基级算法** | 所有现代 Shell Fur 方案（Unity、UE、Unity HDRP 毛发）的数学基础都在这里 |
| **硬件无关** | 文章跑在 DirectX 9 + PS1.3 上都能工作，说明算法本身**极其轻量** |
| **公式最原始** | 去掉所有引擎封装，直击核心公式 `P = Pos + Normal × FurLength` |
| **可读性好** | 教程风格，有完整 HLSL 源码 + 密度衰减曲线 + 重力模型 |

---

## 二、核心算法（就 3 行代码）

### Vertex Shader 核心

```hlsl
// 1. 沿法线外扩
float3 P = IN.position.xyz + (IN.normal * FurLength);

// 2. 重力让发梢下垂（pow 曲线：只有外层变形大）
float k = pow(Layer, 3);
P = P + vGravity * k;

// 3. 投影到裁剪空间
OUT.HPOS = mul(float4(P, 1.0f), worldViewProj);
```

### Pixel Shader 核心

```hlsl
float4 FurColour = tex2D(TextureSampler, IN.T0);
float4 FinalColour = ambient * FurColour + FurColour * dot(LightDir, Normal);
FinalColour.a = FurColour.a;  // ⚠️ Alpha 至关重要，决定毛发形状
return FinalColour;
```

**就这样**。没有 BRDF、没有 SSS、没有各向异性高光 —— 纯 Shell 叠加。

---

## 三、层数效果实验（作者实测数据）

| 层数 | 视觉效果 | 适用场景 |
|------|---------|---------|
| 2 层 | ❌ 粗糙，毛层明显分离 | 不可用 |
| 6 层 | ⚠️ 略有毛感 | 背景物体 |
| 15 层 | ✅ 可识别毛发 | 中距离物体 |
| **30 层** | ⭐ **推荐值，效果良好** | **主角/近景** |
| 60 层 | ⭐⭐ 精细，适用于长波浪毛 | 特写/电影级 |

> **作者结论**：30 层是**效果与性能平衡点**，不用盲目堆到 60+。

这条验证了之前的方案选择 —— **CatchBigGoose 用 10~14 层完全够用**（因为视角远、玩偶小）。

---

## 四、Alpha 噪点纹理的生成（最关键细节）

### 生成步骤
```cpp
// 1. 初始化纹理为全透明
memset(textureData, 0, width * height * 4);

// 2. 设置固定种子（⭐ 必须，保证每层毛发点对齐）
srand(12345);

// 3. 底层放 1000 个点
for (int i = 0; i < 1000; i++)
{
    int x = rand() % width;
    int y = rand() % height;
    textureData[y * width + x] = 
        {rnd(0.9, 0.9), rnd(0.3, 0.5), rnd(0.3, 0.7), 1.0};
}

// 4. 下一层，重置 seed，只放 900 个点（密度递减）
srand(12345);
for (int i = 0; i < 900; i++) { ... }
```

### 关键洞察：**固定种子 srand()**

**为什么必须固定种子？**
- 每一层的"毛发根"位置**必须相同**
- 否则每层的毛发是随机的 → 视觉上是"闪烁的点阵"而不是"连续的毛"
- **固定 seed 保证同一根毛从根到梢在同一 UV 位置**

### 密度衰减曲线选择

| 曲线 | 效果 |
|------|------|
| Linear | 均匀变稀 |
| **Pow2** | 尖部更稀（推荐） |
| Pow3 | 尖部极稀（长毛用） |
| Sine | 波浪毛 |
| Mixed | 混合风格 |

---

## 五、在 UE Material 中的对应实现

文章用 CPU 生成多层纹理，UE 里用材质更简单：

### UE 等价公式（对应前面方案 A）

```
DirectX 原版：
  seed=12345, layer0: 1000 点
  seed=12345, layer1: 900 点
  ...

UE 等价：
  FurNoise.r 只采样一次
  if (FurNoise.r > ShellIndex²) 保留
```

**效果相同**但**资源消耗更少**（DirectX 版每层都要传一张纹理，UE 版共用一张）。

---

## 六、重力模型对比

### xbdev 原版
```hlsl
float k = pow(Layer, 3);  // Layer 是 0~1，pow3 让外层变形剧烈
P = P + vGravity * k;
```

### Unity ExtendStandard
```hlsl
half3 direction = lerp(v.normal, gravity, FUR_OFFSET);
v.vertex.xyz += direction * _FurLength;
```

### UE 方案 A（我们的路线）
```
FurDir = lerp(Normal, Gravity, ShellIndex² × GravityStrength)
```

**三者都是同一思路**：外层受力大、内层刚硬。只是数学形式不同：
- xbdev：`P + G × Layer³`（加法）
- Unity / UE：`lerp(Normal, G, ...)`（方向插值）

**Unity/UE 的 lerp 版本更符合物理直觉**（方向混合），效果也更柔和。

---

## 七、Inter-Fur Shadowing（毛间阴影）

文章给出一个**双 Pass 技巧**增强立体感：

### 原理
每层毛再画一遍"偏移采样"，用颜色差做阴影：

```hlsl
// VS：第二个 UV 偏移一点
OUT.T0 = IN.texCoordDiffuse * UVScale;
OUT.T1 = IN.texCoordDiffuse * UVScale + Normal.xy * 0.0011;  // 偏移

// PS：两次采样相减得阴影
float4 furColor       = tex2D(sampler, T0);
float4 furColorOffset = tex2D(sampler, T1);
float4 shadow = furColorOffset - furColor;

// 转灰度（YUV Y 分量）
float Y = dot(shadow, float4(0.299, 0.587, 0.114, 0));
return shadow_gray;
```

### 代价
- **绘制次数翻倍**（30 层 → 60 层）
- 效果提升：毛发更立体，有"一根根"感

### 在 UE 方案 A 的适配
不建议直接照抄（成本翻倍），替代方案：
- 用 `LayerIndex < 0.3` 时 `BaseColor *= 0.7` 模拟毛根暗（零成本）
- 或在材质里加 **Fresnel** 项（边缘发亮），类似"毛边反射"

---

## 八、与其他毛发技术对比（文章给出）

| 技术 | 原理 | 优点 | 缺点 | 本项目适用性 |
|------|------|------|------|-------------|
| **Shell Texturing**（本文）| 多层外壳 | 简单、GPU 开销低 | 侧面易穿帮 | ⭐⭐⭐⭐⭐ |
| **Fin Rendering** | 轮廓边缘贴片 | 补侧面穿帮 | 管线复杂 | ⭐⭐（需要时再加）|
| **Shell + Fin 混合** | 两者结合 | 全方位OK | 成本高 | ⭐⭐⭐ |
| **Volumetric Fur** | Ray Marching | 最真实 | 性能爆炸 | ❌ |
| **逐根几何** | 每根建模 | 最精细 | 顶点数爆炸 | ❌ |

---

## 九、作者 **Shell Texturing 的固有缺陷（侧面穿帮）**

**问题**：
```
  正面看：  ░░░░░  ← 毛发茂密（所有层叠加）
  侧面看：  ═════  ← 只有薄薄几层（壳的边缘）
```

**原因**：壳是均匀外扩的**模型副本**，从侧面看就是几条线叠加。

**解决方案**（依难度排序）：
1. **加层数**（30→60→100）— 最蠢，性能崩溃
2. **提高 FurDensity**（Noise 贴图 Tiling × 2）— 免费，首选
3. **Fin Pass**（在剪影边缘贴额外毛片）— 要改渲染管线
4. **换成 Groom（曲线毛）** — UE 专属终极方案

**本项目现状**：三消游戏视角**总是俯视**，侧面穿帮基本看不到，**不需要修**。

---

## 十、作者的扩展思路（IDEA 章节）

启发性的点子：
1. **用模型顶点色控制毛发区域**（把要长毛的地方涂某种颜色）
2. **不同层赋予不同颜色** → 渐变毛色（根深尖浅）
3. **接入骨骼动画** → 角色动起来毛发跟着变形
4. **风/重力动态效果** → 草地随风摆动

这些点子在现代引擎里都已经**标配实现**了（顶点色蒙版、Color Ramp 节点、SkeletalMesh 自动适配、风效节点）。

---

## 十一、和项目已有 Skill 的关系

```
xbdev（本文）        Unity ExtendStandard        UE Shell Fur（YivanLee）
    ↓                      ↓                          ↓
 算法源头（2002）        Unity 落地（2018）         UE 落地（2022）
    |                      |                          |
    └──── 核心公式相同 ────┴──── 都是 Shell 层法线外扩 ─┘
                                      ↓
                            UE 方案 A（本项目）
                            组件化多实例 + Material WPO
```

**这篇文章是整个技术谱系的根**。后面所有文章都是它的"工程实现版本"。

---

## 十二、对本项目的启示

### 可以借鉴的
1. ✅ **固定种子生成毛孔图**（如果将来做手绘 Noise 贴图，必须用固定种子）
2. ✅ **30 层是效果-性能临界点**（本项目 10~14 层够了）
3. ✅ **pow 曲线做重力**（比线性插值更真实，可以替换 lerp 版本试试）
4. ✅ **Alpha 是一切**（再次强调 Masked / Opacity Mask 的核心地位）

### 不要照抄的
1. ❌ 每层一张纹理（太费显存，共用一张 + ShellIndex 数学运算即可）
2. ❌ Inter-Fur Shadowing 双 Pass（成本翻倍，UE 用材质 Fresnel 替代）
3. ❌ CPU 生成纹理（现在都是 GPU 程序化 Noise，或者美术手绘）

---

## 十三、参考链接集合

- 原文：https://www.xbdev.net/directx3dx/specialX/Fur/
- 可下载的 Demo（xbdev 提供）：
  - 基础 Fur 演示（11KB）
  - 多纹理层 Demo（15KB）
  - X File 模型毛发（59KB）
  - 甜甜圈毛发（50KB）
  - Inter-Fur Shadow（16KB）
  - 力/重力 Demo（50KB）
- 相关 Skill：
  - `Rendering/Unity_PBR_Fur_Material.md`（Unity 工程化版）
  - `Rendering/UE5_Shell_Fur_Cloth_Material.md`（UE 理论版）
  - `CatchBigGoose/Fur_Implementation_Plan.md`（项目实施方案）

---

## 十四、一句话总结

> **Shell Texturing 的全部秘密就是一行代码：`P = Pos + Normal × FurLength`。**
> 过去 24 年所有毛发技术都是在这基础上加调料（重力曲线、密度衰减、颜色渐变、阴影增强、BRDF 优化等）。
> 理解这一行，就理解了所有 Shell Fur 方案的本质。
