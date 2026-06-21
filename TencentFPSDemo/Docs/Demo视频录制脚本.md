# Demo 视频录制脚本

建议时长：2 到 4 分钟。

## 录制流程

1. 项目开场

打开 UE5 编辑器和 `Variant_Shooter/Lvl_Shooter`，说明项目基于官方 First Person 模板扩展。

旁白参考：这是一个基于 UE5 First Person 模板实现的多人 FPS Demo，包含敌人 AI、玩家战斗、得分胜利机制和多人同步。

2. 多人启动

在 Play 设置中选择 2 个玩家窗口，Net Mode 选择 Listen Server，然后启动游戏。

旁白参考：这里使用 Listen Server 模式启动两个玩家窗口，演示多人联机。

3. 玩家基础操作

分别展示两个窗口中的玩家移动、跳跃和射击。

4. 敌人 AI

展示敌人从出生点刷新、发现玩家、追踪玩家并攻击。

旁白参考：敌人由服务端生成，AI 在服务端运行，客户端通过复制看到相同的敌人位置和状态。

5. 击杀与得分

展示玩家击败敌人，HUD 中个人击杀数、个人分、团队分和剩余敌人数变化。

旁白参考：伤害和击杀结算由服务端负责，然后通过 PlayerState 和 GameState 同步到所有客户端。

6. 波次与胜利

展示下一波敌人刷新，最终达到团队目标分或清空全部波次，出现 Victory 提示。

7. 技术展示

快速展示关键类：

- `AShooterGameMode`
- `AShooterGameState`
- `AShooterPlayerState`
- `AShooterCharacter`
- `AShooterNPC`
- `AShooterNPCSpawner`
- `AShooterWeapon`
- `AShooterProjectile`

## 必须录到的画面

- 两个玩家窗口。
- 敌人移动追踪玩家。
- 敌人攻击玩家，玩家生命值下降。
- 玩家击败敌人。
- 分数和剩余敌人数变化。
- 胜利提示。
- GitHub 仓库或 README。

## 录制建议

- 如果视频时间紧，可以临时把 `TargetTeamScore` 调低，快速触发 Victory。
- New Editor Window PIE 比嵌入视口更适合录屏。
- 保持 HUD 可见，方便评审看到同步数据。

