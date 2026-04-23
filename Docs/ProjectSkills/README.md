# Project Skills 知识库

> 跨项目游戏开发经验与技术总结  
> 随 UnrealAgent 插件分发，所有安装此插件的项目都能查阅  
> 首次创建：2026-04-23

---

## 分类索引

### 🎮 CatchBigGoose（抓大鹅三消游戏）

UE5.7 C++ 休闲三消游戏的完整开发经验。

| 文档 | 内容概要 |
|------|---------|
| [CatchBigGoose_Skill.md](./CatchBigGoose/CatchBigGoose_Skill.md) | 项目开发总手册：架构、物理系统、动画、交互、性能优化等 9 大模块 |
| [Fur_Implementation_Plan.md](./CatchBigGoose/Fur_Implementation_Plan.md) | 玩偶毛发效果实施方案（Shell Fur 方案 A/B/C 对比 + 推荐路线） |

---

### 🎨 Rendering（渲染技术通用知识）

UE5 与 Unity 的渲染管线、Shader、材质系统技术笔记。

| 文档 | 内容概要 |
|------|---------|
| [UE5_Shell_Fur_Cloth_Material.md](./Rendering/UE5_Shell_Fur_Cloth_Material.md) | YivanLee 知乎文：UE5 Shell Fur 原理与引擎多 Pass 方案对比 |
| [UE5_Add_Custom_MeshDrawPass.md](./Rendering/UE5_Add_Custom_MeshDrawPass.md) | 搓手苍蝇知乎文：给 UE5 添加自定义 MeshDrawPass 完整代码流程 |
| [UE5_Custom_ShadingModel.md](./Rendering/UE5_Custom_ShadingModel.md) | UE5.6 自定义 Shading Model 的 7 步流程 |
| [Unity_PBR_Fur_Material.md](./Rendering/Unity_PBR_Fur_Material.md) | Unity ExtendStandard 皮毛材质 + 布料 BRDF 实现指南 |
| [xbdev_Fur_Shell_Texturing_Classic.md](./Rendering/xbdev_Fur_Shell_Texturing_Classic.md) | **元老级教程**：DirectX Shell Texturing 毛发核心算法（所有 Shell Fur 方案的理论源头） |
| [Gennyx_Fabric_Fresnel_Ramp_Trick.md](./Rendering/Gennyx_Fabric_Fresnel_Ramp_Trick.md) | Maya/Mental Ray 布料伪造技巧：`Fresnel × Ramp` 驱动法，跨引擎通用 |

---

## 使用方式

### 浏览
直接打开对应的 `.md` 文件阅读。

### AI 辅助
开发中向 AI 提问时，可以引用这些文档：
```
"参考 Docs/ProjectSkills/Rendering/UE5_Shell_Fur_Cloth_Material.md，
给我讲讲如何在当前项目实现毛发效果"
```

### 新增经验
- **项目专属经验** → 在对应项目目录下加 `.md`（如 `CatchBigGoose/XXX.md`）
- **通用技术经验** → 归入合适的主题目录（如 `Rendering/`、`Physics/`、`Networking/`）
- **新主题分类** → 直接建新文件夹

---

## 维护规则

1. **文件命名**：主题清晰、不带 `_Guide` 等冗余后缀
2. **文档结构**：建议统一结构（参考文献、记录日期、项目应用价值）
3. **更新时机**：每次踩坑或总结经验后，随提交一起 push
4. **敏感信息**：不要包含公司机密、API Key、账号密码

---

## 仓库信息

- 仓库：https://github.com/KKyz007/UnrealAgent
- 本目录路径：`Plugins/UnrealAgent/Docs/ProjectSkills/`
- 维护者：KKyz007
