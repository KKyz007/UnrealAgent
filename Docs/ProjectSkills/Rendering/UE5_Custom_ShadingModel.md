# UE5 自定义 Shading Model 实现指南

> 参考来源：微信公众号文章（UE5.6 版本）
> 记录日期：2026-04-20
> 适用版本：UE5.5 / 5.6 / 5.7（源码版）

---

## 一、概述

自定义 Shading Model 允许你在 UE5 中实现引擎不内置的光照模型（如卡通渲染、各向异性头发、皮肤SSS等）。
**必须使用源码版 UE5**，需要修改引擎 C++ 代码和 Shader 文件。

---

## 二、修改步骤（共 7 步）

### 步骤 1：添加 Shading Model 枚举

**文件**：`Engine/Source/Runtime/Engine/Classes/Engine/EngineTypes.h`

在 `EMaterialShadingModel` 枚举中添加新值：
```cpp
// 在 MSM_NUM 之前添加
MSM_ToonLit        UMETA(DisplayName = "Toon Lit"),
```

**文件**：`Engine/Source/Runtime/Engine/Public/SceneTypes.h`

同步在 `FMaterialShadingModelField` 相关位置更新。

---

### 步骤 2：修改材质编辑器 UI 显示

**文件**：`Engine/Source/Runtime/Engine/Private/Materials/MaterialAttributeDefinitionMap.cpp`

在 `FMaterialAttributeDefinitionMap::InitializeAttributeMap()` 中添加新 Shading Model 的属性定义：
```cpp
// 注册自定义属性引脚（如 Toon 的 Shadow Threshold）
AddCustomAttribute(
    FGuid(...),
    "ToonThreshold",
    "Toon Shadow Threshold",
    ...
    SF_Float,
    FVector4(0.5f, 0, 0, 0),
    EShaderFrequency::SF_Pixel,
    MP_CustomData0
);
```

使材质编辑器的 Shading Model 下拉菜单中出现新选项，并支持自定义输入引脚。

---

### 步骤 3：定义 Shading Model ID（Shader 侧）

**文件**：`Engine/Shaders/Private/ShadingCommon.ush`

```hlsl
#define SHADINGMODELID_TOON_LIT    14   // 选一个未使用的 ID
```

**文件**：`Engine/Shaders/Private/Definitions.usf`（或宏定义相关文件）

确保 C++ 侧和 Shader 侧的 ID 值一致。

---

### 步骤 4：修改 GBuffer 编码/解码

**文件**：`Engine/Shaders/Private/DeferredShadingCommon.ush`

在 `EncodeGBuffer` 和 `DecodeGBuffer` 中添加对新 Shading Model 的处理：

```hlsl
// 编码：写入 CustomData
if (ShadingModelID == SHADINGMODELID_TOON_LIT)
{
    GBuffer.CustomData.x = ToonThreshold;  // 卡通阴影阈值
    GBuffer.CustomData.y = ToonSoftness;   // 过渡柔和度
}
```

**CustomData 通道规划**（只有 4 个通道 RGBA）：

| 通道 | 用途示例 |
|------|----------|
| CustomData.x | Toon Shadow Threshold |
| CustomData.y | Toon Shadow Softness |
| CustomData.z | 备用 |
| CustomData.w | 备用 |

---

### 步骤 5：实现核心光照计算

**文件**：`Engine/Shaders/Private/ShadingModels.ush`

添加自定义光照函数：

```hlsl
FDirectLighting ToonLitBxDF(FGBufferData GBuffer, half3 N, half3 V, half3 L, float Falloff, float NoL, FAreaLight AreaLight, FShadowTerms Shadow)
{
    FDirectLighting Lighting;

    // 卡通阶梯化光照
    float ToonThreshold = GBuffer.CustomData.x;
    float ToonSoftness  = GBuffer.CustomData.y;

    // 用 smoothstep 实现软边卡通阴影
    float ToonNdotL = smoothstep(ToonThreshold - ToonSoftness, ToonThreshold + ToonSoftness, NoL);

    // Diffuse
    Lighting.Diffuse = AreaLight.FalloffColor * (Falloff * ToonNdotL) * Diffuse_Lambert(GBuffer.DiffuseColor);

    // Specular（可选：卡通高光）
    Lighting.Specular = 0;

    // Transmission
    Lighting.Transmission = 0;

    return Lighting;
}
```

---

### 步骤 6：在光照通道中接入

**文件**：`Engine/Shaders/Private/DeferredLightingCommon.ush`

在 `IntegrateBxDF` 函数的 `switch` 中添加分支：

```hlsl
case SHADINGMODELID_TOON_LIT:
    return ToonLitBxDF(GBuffer, N, V, L, Falloff, NoL, AreaLight, Shadow);
```

**文件**：`Engine/Shaders/Private/BasePassPixelShader.usf`

在 BasePass 的 `SetGBufferForShadingModel` 中添加处理：

```hlsl
if (ShadingModel == SHADINGMODELID_TOON_LIT)
{
    GBuffer.CustomData.x = GetMaterialCustomData0(MaterialParameters);
    GBuffer.CustomData.y = GetMaterialCustomData1(MaterialParameters);
}
```

---

### 步骤 7：Shader 编译宏

**文件**：`Engine/Source/Runtime/Renderer/Private/MaterialShader.cpp`（或相关文件）

确保编译时定义宏：

```cpp
// 在 GetShadingModelString 或类似函数中添加
case MSM_ToonLit: return TEXT("SHADINGMODELID_TOON_LIT");
```

---

## 三、编译与调试

### 3.1 编译
```
1. 重新生成项目文件（GenerateProjectFiles.bat）
2. 编译整个引擎（Development Editor）
3. 首次编译会触发全量 Shader 重编（耗时较长）
```

### 3.2 调试
- **Shader 热重载**：`Ctrl + Shift + .`（修改 .ush/.usf 后无需重启）
- **Buffer Visualization**：Viewport → Buffer Visualization → 检查 Shading Model / CustomData
- **RenderDoc**：抓帧分析 GBuffer 输出

### 3.3 材质编辑器中使用
1. 新建材质
2. Details → Shading Model → 选择 "Toon Lit"
3. 连接自定义引脚（Toon Threshold / Toon Softness）
4. 预览效果

---

## 四、关键注意事项

| 注意点 | 说明 |
|--------|------|
| **必须源码版** | Launcher 版无法修改引擎代码 |
| **GBuffer 通道有限** | CustomData 只有 4 个 float，需合理规划 |
| **Lumen 兼容** | 自定义 SM 可能不被 Lumen GI 正确处理，需额外适配 |
| **Nanite 兼容** | Nanite 的 Programmable Rasterizer 可能需要额外处理 |
| **引擎升级** | 每次升级 UE 版本都需要手动合并自定义修改 |
| **Mobile** | 移动端走前向渲染，需要在 MobileBasePass 中也添加处理 |
| **Shader 编译缓存** | 修改 Shading Model 后首次启动需要等大量 Shader 重编 |

---

## 五、需要修改的文件清单

| 文件 | 改动类型 |
|------|----------|
| `EngineTypes.h` | 添加枚举值 |
| `SceneTypes.h` | 同步枚举 |
| `MaterialAttributeDefinitionMap.cpp` | 注册属性/引脚 |
| `ShadingCommon.ush` | 定义 Shader ID |
| `DeferredShadingCommon.ush` | GBuffer 编解码 |
| `ShadingModels.ush` | 核心光照函数 |
| `DeferredLightingCommon.ush` | switch case 接入 |
| `BasePassPixelShader.usf` | BasePass 写入 |
| `MaterialShader.cpp` | 编译宏/字符串映射 |

---

## 六、UE5.5/5.6 版本变化

相比 UE4/UE5.0~5.3，新版本（5.5+）的主要变化：
- **Substrate（原Strata）材质系统**：如果启用了 Substrate，自定义 SM 的方式有所不同
- **Shader Permutation 优化**：UE5.5+ 对 Shader 变体做了更严格的裁剪
- **Lumen 依赖更深**：默认 GI 方案是 Lumen，自定义 SM 需要确保 Lumen 兼容
- **Nanite 材质支持**：5.5+ 的 Nanite 支持更多材质特性，需注意兼容

**建议**：如果不需要修改引擎，可以用 **Post Process Material + Custom Depth Stencil** 方案近似实现卡通渲染（如本项目中的高亮描边），避免修改引擎源码。
