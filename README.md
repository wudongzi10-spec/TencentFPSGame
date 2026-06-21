# TencentFPSGame — UE5 多人据点防守 FPS Demo

基于 **Unreal Engine 5.7** 官方 First Person 模板（Shooter 变体）用 **C++** 扩展的多人联机第一人称射击 Demo：玩家进入竞技场，服务端按波次刷新会移动并射击的敌人，玩家击败敌人得分，团队达成目标分或清空全部波次即胜利。支持 Listen Server 多人联机。

> 题目要求的四项均已实现：会移动并攻击玩家的敌人 · 玩家可击败敌人 · 基础得分与胜利 · 多人网络对战。并在此之上补充了大量战斗反馈与联网精度优化。

---

## 功能特性

**核心玩法**
- 波次刷怪：服务端逐波刷新敌人，数量与节奏可配。
- 敌人 AI：AIController + StateTree + AI 感知，追踪并射击玩家；失去视线时主动逼近最近玩家，不站桩发呆。
- 得分与胜利：团队分 / 个人击杀·阵亡，达成目标分或清空波次即胜利，服务端权威判定。
- 多人联机：Listen Server，移动 / 射击 / 敌人 / 分数 / 胜利全部网络同步。

**战斗手感与反馈**
- 无畏契约式部位伤害：头 / 身 / 腿分段（头部约 4 倍，近乎一枪）。
- 命中标记（爆头金色）、世界飘字伤害、连杀播报（双杀 / 三杀 / 暴走）。
- 开火镜头后坐力并自动回正、动态准星、分段式弹匣。
- 受击方向弧 + 屏幕红闪、低血量警示。
- 敌我世界标记：敌人红色血条（被掩体遮挡时不显示，无穿墙）、队友绿色标记。
- 死亡布娃娃各端同步、重生短暂无敌保护。
- 自绘中文战斗 HUD（计分板、生命、弹药、胜利结算、波次播报等）。

---

## 环境要求

- **Unreal Engine 5.7**（通过 Epic Games Launcher 安装）。
- **Windows + Visual Studio 2022/2026**（含「使用 C++ 的游戏开发」工作负载），用于编译 C++。
- 约 1.5 GB 磁盘空间（含 Content）。

---

## 安装与运行

### 1. 获取项目
```bash
git clone https://github.com/wudongzi10-spec/TencentFPSGame.git
```

### 2. 打开工程
UE 工程位于仓库的 `TencentFPSDemo/` 子目录。双击打开：
```
TencentFPSGame/TencentFPSDemo/TencentFPSDemo.uproject
```
首次打开若提示「Missing Modules / 是否重新编译」，点 **Yes** 等待编译完成（也可按下面命令行方式先编译）。

### 3.（可选）命令行编译
```bash
"<UE5.7 安装目录>/Engine/Build/BatchFiles/Build.bat" ^
  TencentFPSDemoEditor Win64 Development ^
  -Project="<仓库路径>/TencentFPSDemo/TencentFPSDemo.uproject" -WaitMutex
```

### 4. 运行
打开编辑器后，等右下角着色器 / 资源编译完成，点工具栏 ▶ **Play** 即可进入游戏。

---

## 多人联机测试

1. 工具栏 ▶ Play 右侧下拉，打开播放设置。
2. **Number of Players** 设为 `2`。
3. **Net Mode** 选 **Play As Listen Server**。
4. （可选）勾 New Editor Window (PIE)，两名玩家各自独立窗口。
5. 点击 Play，出现两个窗口即两名玩家；点击某窗口获得焦点后操作，按 **Shift+F1** 释放鼠标切换窗口。

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
├─ README.md                     # 本文件
├─ 技术实现说明.docx              # 技术实现文档（含 GitHub 链接）
├─ 视频录制脚本.md                # Demo 视频录制脚本
├─ docs/                         # 玩法说明与联机测试等文档
└─ TencentFPSDemo/               # UE5 工程
   ├─ TencentFPSDemo.uproject
   ├─ Config/                    # 项目配置
   ├─ Content/                   # 资源（地图、蓝图、模型、动画等）
   └─ Source/TencentFPSDemo/     # C++ 源码
      └─ Variant_Shooter/        # 射击玩法核心（角色、AI、武器、规则、HUD）
```

---

## 技术实现要点

- **服务端权威**：伤害、得分、胜利均在服务端判定，客户端只负责输入与表现，符合多人架构。
- **战斗反馈系统**：命中 / 受击 / 击杀通过客户端 RPC 通知相关玩家做表现；关键状态用复制属性与 RepNotify 驱动 UI。
- **联网命中精度**：客户端以约 30Hz 流式上报精确瞄准点，服务端据此生成命中，解决远端视角 pitch 低精度导致的弹道偏差；投射物从射手视点方向发射，修复贴脸打空。

更完整的技术说明见仓库内 `技术实现说明.docx`。

---

## 致谢

基于 Unreal Engine 5 官方 First Person 模板（Shooter 变体）扩展实现。
