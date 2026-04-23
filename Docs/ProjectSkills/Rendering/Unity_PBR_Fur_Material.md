# Unity PBR 皮毛材质实现指南

> 原文：Unity 的 PBR 扩展 — 皮毛材质（作者：YOung，知乎）
> 参考：王者荣耀"峡谷第一美"妲己尾巴毛发制作分享
> **完整源码仓库**：https://github.com/chenyong2github/ExtendStandard
> 记录日期：2026-04-20

---

## 🔗 配套开源项目：ExtendStandard

**仓库地址**：https://github.com/chenyong2github/ExtendStandard
**作者**：chenyong2github (ChenYong)
**Stars**：434+ ｜ **Forks**：124+
**语言**：HLSL (75.5%) + ShaderLab (24.5%)

### 项目定位
扩展 Unity 内置 Standard PBR Shader，在保留物理真实性的基础上加入自定义渲染特性：
- **Unity 的标准 PBR** = `Standard` Shader
- **UE4/5 的对应** = `Default Lit` Shading Model
- 标准 PBR 统一资产规范，但无法覆盖所有项目的差异化风格需求
- 本项目提供可复用的扩展框架（含皮毛材质、布料/天鹅绒 BRDF 等）

### 仓库结构
```
ExtendStandard/
├── Assets/              # Unity 资源（Shader 源码在此）
├── ProjectSettings/     # Unity 项目设置
├── Screenshots/         # 效果截图
└── README.md
```

### 配套文章
- 扩展框架：https://zhuanlan.zhihu.com/p/50822664（扩展方法论）
- 皮毛材质：https://zhuanlan.zhihu.com/p/57897827（本文主题）

---

## 一、核心技术：Shell 层渲染


皮毛渲染使用**层渲染（Shell Rendering）** 技术：
- 模型表面沿法线方向挤出多层"壳"
- 每个 Shell 层作为一个 Pass（通常 10~20 层）
- 通过 Noise/Layer 贴图做 alpha 剔除，模拟毛发形状

**适用引擎**：Unity / Unreal / 自研引擎均可实现，原理通用。

---

## 二、实现要点

### 2.1 Shell 层顶点偏移

在顶点着色器中沿法线挤出：
```hlsl
v.vertex.xyz += v.normal * _FurLength * FUR_OFFSET;
```
- `_FurLength`：毛发总长度（全局参数）
- `FUR_OFFSET`：当前层相对位置 = 当前层索引 / 总层数（0~1）

**20 层示例**：FUR_OFFSET 依次为 0、0.05、0.1、...、0.95、1

---

### 2.2 Alpha 剔除（毛发形状）

**三角形（根粗尖细）**：
```hlsl
alpha = step(FUR_OFFSET, alpha);
```

**抛物线形状（更自然）**：
```hlsl
alpha = step(FUR_OFFSET * FUR_OFFSET, alpha);
```
非线性裁剪让毛发形状更像抛物线。

**尖部透明度递减**：
```hlsl
color.a = 1 - FUR_OFFSET;
```

---

### 2.3 密集毛发控制

用灰度图作为 Alpha 输入：
```hlsl
fixed alpha = tex2D(_LayerTex, TRANSFORM_TEX(i.texcoord.xy, _LayerTex)).r;
color.a *= alpha;
```
通过调整 `_LayerTex` 的 **Tiling** 控制毛发密集程度：
- Tiling=1：稀疏
- Tiling=5：密集
- Tiling=20：非常密集

---

### 2.4 外力影响（重力/风）

**基础版**：
```hlsl
half3 direction = _Gravity * _GravityStrength + v.normal * (1 - _GravityStrength);
v.vertex.xyz += direction * _FurLength;
```

**进阶版**（发根刚硬，发尖柔软）：
```hlsl
half3 direction = lerp(v.normal, direction, FUR_OFFSET);
```
- `FUR_OFFSET = 0`（发根）：完全沿法线，不受力
- `FUR_OFFSET = 1`（发尖）：完全受重力影响

---

## 三、PBR 扩展：布料/天鹅绒模型

### 3.1 经验参数

| 材质 | Metallic | Roughness | 说明 |
|------|----------|-----------|------|
| 棉布 | 0 | 高 | 漫反射为主，有细绒毛 |
| 亚麻 | 0 | 中高 | 粗糙 |
| 丝绸 | 中等 | 低 | 类金属反射，多种高光 |
| **天鹅绒** | **1** | **1** | 表面粗糙，看起来很黑 |

---

### 3.2 Inverted Gaussian 分布

**问题**：标准 GGX 分布在 NdotH=1 时高光最强（灯光、视线、法线同向），但天鹅绒的高光恰恰在**边缘**（NdotH 接近 0 时）。

**解决**：使用反向 Gaussian 分布（The Order:1886 使用的方案）：

```hlsl
inline float FabricD (float NdotH, float roughness)
{
    return 0.96 * pow(1 - NdotH, 2) + 0.057;
}
```

用 `FabricD` 替换 `UnityStandardBRDF.cginc` 中 `BRDF1_Unity_PBS` 的 `GGXTerm`。

**效果**：高光从中心转移到边缘，完美模拟天鹅绒特性。

---

### 3.3 Fabric Scatter（布料散射）

模拟布料纤维边缘绒毛和多重高光：

```hlsl
inline half FabricScatterFresnelLerp(half nv, half scale)
{
    half t0 = Pow4(1 - nv);           // 4 次方 Fresnel（比标准 5 次方范围更大）
    half t1 = 0.4 * (1 - nv);          // 叠加更大范围的可调 Fresnel
    return (t1 - t0) * scale + t0;
}
```

**最终 BRDF 返回**：
```hlsl
half3 color = diffColor * (gi.diffuse + light.color * diffuseTerm)
            + specularTerm * light.color * FresnelTerm(specColor, lh)
            + _FabricScatterColor * (nl * 0.5 + 0.5) * FabricScatterFresnelLerp(nv, _FabricScatterScale);
```

**关键改动**：
1. Fresnel 从 `Pow5` 改为 `Pow4`，扩大范围
2. 叠加 `_FabricScatterColor`（可调布料散射色）
3. **去掉间接光反射**，让金属感的布料（如丝绸）不那么 metallic

---

## 四、完整流程总结

```
1. 复制模型网格 N 次（N = Shell 层数，通常 10~20）
2. 每层 Pass 中：
   ├─ 沿法线偏移顶点（长度 = _FurLength × FUR_OFFSET）
   ├─ 施加重力/风力偏移（lerp 控制根部和尖部的响应）
   ├─ 采样 Layer 贴图获取 alpha（控制毛发密度）
   ├─ 用 step 做 alpha 剔除（形成抛物线形状）
   ├─ 尖部透明度递减（1 - FUR_OFFSET）
   └─ 光照计算：
      ├─ FabricD 替换 GGXTerm（Inverted Gaussian）
      └─ FabricScatter 叠加 Fresnel 散射色
3. 按从里到外的顺序渲染所有层
```

---

## 五、UE 实现参考

Unity 的 Shell 毛发在 UE 中也能实现，但 UE 的 Pass 系统不同：

### 方式一：Material + Instance Static Mesh
- 每个 Shell 层作为一个 StaticMesh Instance
- 用 Material 的 `World Position Offset` 沿法线偏移
- 在 Material 中传入 `LayerIndex` 参数控制每层

### 方式二：Niagara 粒子毛发
- 用 Niagara 发射粒子模拟毛发
- 每个粒子 = 一根毛发

### 方式三：Groom（UE 官方毛发系统）
- UE5 原生支持 Alembic 毛发导入
- 用 Groom Asset + Groom Component
- 内置光照、物理、自阴影

**推荐**：游戏需求用 Shell 方案（性能好），电影级需求用 Groom（效果好）。

---

## 六、性能优化建议

| 场景 | 建议层数 | 说明 |
|------|---------|------|
| 移动端 | 5~8 层 | 性能敏感 |
| PC 主机 | 10~15 层 | 平衡 |
| 角色特写 | 20~30 层 | 电影级效果 |
| 背景 NPC | 3~5 层 | 远距离不需要高精度 |

**LOD 策略**：远距离减少 Shell 层数，近距离增加。

---

## 七、关键参数对照

| 参数 | 作用 | 典型值 |
|------|------|--------|
| `_FurLength` | 毛发总长度 | 0.01~0.1 |
| `_LayerCount` | Shell 层数 | 10~20 |
| `_LayerTex Tiling` | 毛发密度 | 5~20 |
| `_Gravity` | 重力方向 | (0, -1, 0) |
| `_GravityStrength` | 外力强度 | 0.3~0.7 |
| `_FabricScatterColor` | 散射色 | 暖色调 |
| `_FabricScatterScale` | 散射强度 | 0.5~2.0 |

---

## 参考文献

1. Generating Fur in DirectX or OpenGL Easily
2. gFur Help - Introduction
3. Custom Fabric Shader for Unreal Engine 4（SlideShare）
4. PBS Fabric Shading Notes - SelfShadow
5. CiteSeer: Interactive Fur Rendering
