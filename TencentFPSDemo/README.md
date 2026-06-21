# TencentFPSDemo

基于 Unreal Engine 5 官方 First Person 模板扩展的多人 FPS Demo，用于腾讯游戏客户端作业提交。

## 作业功能

- 敌人由服务端按波次生成，并通过 AIController / StateTree / NavMesh 追踪玩家。
- 玩家可以使用模板射击玩法击败敌人。
- 武器支持弹匣、备弹、手动 `R` 键换弹、自动空仓换弹和换弹动画播放。
- 服务端负责伤害、击杀、团队得分、个人得分和胜利判断。
- `GameState` 复制团队分数、当前波次、剩余敌人数、击杀提示和胜利状态。
- `PlayerState` 复制个人击杀数和分数。
- 支持 Listen Server 双人联机演示。
- 原生 HUD 展示规则说明、准星、血量、弹药、换弹进度、击杀确认、波次目标和胜利结算。
- 波次刷怪使用分批生成，并关闭部分高开销渲染特性，降低多窗口 PIE 试玩卡顿。

## 核心类

- `AShooterGameMode`：服务端波次、刷怪、得分、胜利规则。
- `AShooterGameState`：复制团队分数、波次、剩余敌人、击杀事件和胜利状态。
- `AShooterPlayerState`：复制个人击杀数和分数。
- `AShooterCharacter`：玩家生命值、武器持有、开火 Server RPC。
- `AShooterNPC`：敌人生命值、死亡复制、击杀归因。
- `AShooterNPCSpawner`：关卡刷怪点，由 GameMode 统一调度。
- `AShooterWeapon` / `AShooterProjectile`：服务端生成投射物，处理伤害、弹匣、备弹和换弹。
- `AShooterHUD`：原生战斗 HUD 显示规则、准星、生命、弹药、击杀、团队分、波次和胜利提示。

## 多人演示方式

1. 打开 `TencentFPSDemo.uproject`。
2. 使用 `Variant_Shooter/Lvl_Shooter` 地图。
3. 在 Play 设置中选择 `Number of Players = 2`。
4. Net Mode 选择 `Play As Listen Server`。
5. 启动 PIE，录制两个玩家窗口、敌人追踪、击杀得分、波次推进和胜利提示。

## 构建验证

已在本机通过编辑器目标编译：

```powershell
TencentFPSDemoEditor Win64 Development
```

结果：Succeeded。当前仅有 Visual Studio 版本偏好和 UE AIModule 引擎级弃用警告，不影响项目编译。

## 提交材料

- Demo 视频：填写录制后的视频链接。
- PDF 技术文档：可由 `Docs/技术实现说明.md` 导出。
- GitHub 链接：填写仓库地址。
