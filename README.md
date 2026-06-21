# TencentFPSGame — UE5 多人据点防守 FPS

一个基于 **Unreal Engine 5.7** 官方 First Person 模板（Shooter 变体）、以 **C++** 扩展实现的多人联机第一人称射击项目。玩家进入竞技场，服务端按波次刷新会移动并射击的敌人；玩家击败敌人累计团队分数，达成目标分或清空全部波次即取得胜利。支持 Listen Server 多人联机，所有战斗判定在服务端进行，客户端只负责输入与表现。

---

## 目录

- [功能特性](#功能特性)
- [技术栈](#技术栈)
- [架构与网络模型](#架构与网络模型)
- [战斗系统](#战斗系统)
- [主要 C++ 类](#主要-c-类)
- [环境要求](#环境要求)
- [安装与运行](#安装与运行)
- [多人联机](#多人联机)
- [操作说明](#操作说明)
- [项目结构](#项目结构)

---

## 功能特性

**核心玩法**
- **波次刷怪**：服务端逐波刷新敌人，每波数量与节奏可在 GameMode 配置。
- **敌人 AI**：AIController + StateTree + AI 感知，追踪并射击玩家；失去视线时主动逼近最近玩家并转身面向，不站桩发呆。
- **得分与胜利**：团队分、个人击杀 / 阵亡分别统计；达成目标团队分或清空全部波次即胜利，由服务端权威判定并同步给所有客户端。
- **多人联机**：Listen Server，移动 / 射击 / 敌人 / 分数 / 胜利全部网络同步。

**战斗手感与反馈**
- **部位伤害**：按命中高度区分头 / 身 / 腿（头部约 4 倍，近乎一枪），数值参考主流竞技射击。
- **命中反馈**：命中标记（爆头金色）、世界飘字伤害、连杀播报（双杀 / 三杀 / 暴走）。
- **武器手感**：开火镜头后坐力并自动回正、动态准星、分段式弹匣、手动 / 空仓换弹与换弹动画。
- **生存反馈**：受击方向弧 + 屏幕红闪、低血量边缘警示、死亡布娃娃各端同步、重生短暂无敌保护。
- **敌我识别**：敌人红色血条标记（被掩体遮挡时不显示，杜绝穿墙透视）、队友绿色标记。
- **自绘 HUD**：中文战斗 HUD，含计分板、生命、弹药、命中飘字、连杀横幅、波次播报与胜利结算。

---

## 技术栈

| 项 | 内容 |
| --- | --- |
| 引擎 | Unreal Engine 5.7 |
| 语言 | C++ |
| 输入 | Enhanced Input |
| AI | AIController + StateTree + AI Perception + NavMesh |
| UI | AHUD + Canvas 自绘（运行时合成 CJK 字体渲染中文） |
| 网络 | Listen Server，属性复制 + RepNotify + RPC |

---

## 架构与网络模型

项目严格遵循**服务端权威（server-authoritative）**：伤害、击杀、得分、胜利全部在服务端判定，客户端仅负责采集输入与呈现表现，避免客户端篡改战斗结果。

- **移动**：使用 Character Movement 默认网络复制。
- **射击**：客户端通过 Server RPC 请求开火，服务端生成投射物并做命中判定。
- **战斗反馈**：服务端把命中 / 受击 / 击杀通过 Client RPC 通知相关玩家，由其本地 HUD 表现（命中标记、飘字、连杀、镜头后坐力等）。
- **比赛状态**：团队分、波次、存活数、胜利标志等通过 `GameState` 复制，并用 RepNotify 驱动 UI 刷新。
- **联网命中精度**：针对引擎远端视角 pitch 低精度复制导致的远距弹道偏差，客户端以约 30Hz 流式上报精确瞄准点，服务端据此生成命中；投射物从射手视点方向发射，修复贴脸时枪口越过目标导致的打空。

---

## 战斗系统

- **武器与弹药**：弹匣 + 备弹，手动 `R` 换弹、自动空仓换弹，分段式弹匣直观显示剩余子弹；重复拾取已有武器会补满弹药并切换。
- **部位伤害**：投射物以点伤害携带命中位置，目标按命中高度判定头 / 身 / 腿并应用对应倍率；同阵营免伤，避免误伤队友或敌人互殴刷分。
- **死亡与重生**：角色死亡触发布娃娃并在各端同步，数秒后在出生点重生，并获得短暂无敌保护。
- **敌人行为**：感知到玩家则追踪并射击；失去视线时持续向最近玩家逼近，保证战斗节奏。

---

## 主要 C++ 类

| 类 | 职责 |
| --- | --- |
| `AShooterCharacter` | 玩家角色：血量、死亡布娃娃、重生与无敌保护、开火 Server RPC、武器栏、镜头后坐力、客户端精确瞄准上报 |
| `AShooterNPC` / `AShooterAIController` | 敌人与 AI：StateTree 感知 / 追踪 / 射击，无视线时主动逼近；头 / 身 / 腿分段伤害判定 |
| `AShooterWeapon` / `AShooterProjectile` | 武器与投射物：服务端开火、命中即停并隐藏、点伤害携带命中位置、同阵营免伤 |
| `AShooterNPCSpawner` | 关卡刷怪点，由 GameMode 统一调度 |
| `AShooterGameMode` | 服务端权威规则：波次刷新、击杀计分（含爆头加成）、胜利判定 |
| `AShooterGameState` | 复制比赛状态：团队分、波次、存活数、胜利标志、击杀提示 |
| `AShooterPlayerState` | 复制个人统计：击杀数、阵亡数、个人分 |
| `AShooterPlayerController` | 命中 / 受击 / 击杀的 Client RPC、连杀统计、重生与输入 |
| `AShooterHUD` | Canvas 战斗 HUD：计分板、生命 / 弹药、命中标记、飘字伤害、连杀横幅、敌我标记、方向受击提示等 |

---

## 环境要求

- **Unreal Engine 5.7**（通过 Epic Games Launcher 安装）。
- **Windows + Visual Studio 2022/2026**，需勾选「使用 C++ 的游戏开发」工作负载（用于编译 C++）。
- 约 1.5 GB 磁盘空间（含 Content）。

---

## 安装与运行

**1. 克隆项目**
```bash
git clone https://github.com/wudongzi10-spec/TencentFPSGame.git
```

**2. 打开工程**　UE 工程位于仓库的 `TencentFPSDemo/` 子目录，双击打开：
```
TencentFPSGame/TencentFPSDemo/TencentFPSDemo.uproject
```
首次打开若提示 *Missing Modules / 是否重新编译*，点 **Yes** 等待编译完成。

**3.（可选）命令行编译**
```bash
"<UE5.7 安装目录>/Engine/Build/BatchFiles/Build.bat" \
  TencentFPSDemoEditor Win64 Development \
  -Project="<仓库路径>/TencentFPSDemo/TencentFPSDemo.uproject" -WaitMutex
```

**4. 运行**　进入编辑器后，等右下角着色器 / 资源编译完成，点工具栏 ▶ **Play** 即可游玩。默认地图为 `Variant_Shooter/Lvl_Shooter`。

---

## 多人联机

1. 工具栏 ▶ Play 右侧下拉，打开播放设置。
2. **Number of Players** 设为 `2`。
3. **Net Mode** 选 **Play As Listen Server**。
4. （可选）勾选 New Editor Window (PIE)，两名玩家各自独立窗口。
5. 点击 Play 出现两个窗口；点击某窗口获得焦点后操作，按 **Shift + F1** 释放鼠标切换窗口。

---

## 操作说明

| 操作 | 按键 |
| --- | --- |
| 移动 | W A S D |
| 瞄准 | 鼠标 |
| 跳跃 | 空格 |
| 射击 | 鼠标左键 |
| 换弹 | R |
| 切枪 | Q |

---

## 项目结构

```
TencentFPSGame/
├─ README.md
└─ TencentFPSDemo/                 # UE5 工程
   ├─ TencentFPSDemo.uproject
   ├─ Config/                      # 项目配置
   ├─ Content/                     # 资源（地图、蓝图、模型、动画等）
   └─ Source/TencentFPSDemo/
      └─ Variant_Shooter/          # 射击玩法核心：角色 / AI / 武器 / 规则 / HUD
```

---

基于 Unreal Engine 5 官方 First Person 模板（Shooter 变体）扩展实现。
