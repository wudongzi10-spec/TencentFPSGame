// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "TimerManager.h"
#include "ShooterGameMode.generated.h"

class AController;
class AShooterNPC;
class AShooterNPCSpawner;
class UShooterUI;

/**
 *  Simple GameMode for a first person shooter game.
 *  Runs server-authoritative waves, scoring and victory checks.
 */
UCLASS(abstract)
class TENCENTFPSDEMO_API AShooterGameMode : public AGameModeBase
{
	GENERATED_BODY()
	
protected:

	/** Type of UI widget to spawn */
	UPROPERTY(EditAnywhere, Category="Shooter")
	TSubclassOf<UShooterUI> ShooterUIClass;

	/** Keep disabled because AShooterHUD now owns the combat interface. */
	UPROPERTY(EditAnywhere, Category="Shooter")
	bool bUseLegacyShooterUI = false;

	/** Pointer to the UI widget */
	TObjectPtr<UShooterUI> ShooterUI;

	/** Number of enemies to spawn in each wave */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Shooter|Waves")
	TArray<int32> EnemiesPerWave;

	/** Applies a smoother wave profile for multi-window demo recording. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Shooter|Waves")
	bool bUseDemoOptimizedWaveProfile = true;

	/** Team score required to win even if waves are still active */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Shooter|Rules", meta=(ClampMin=1))
	int32 TargetTeamScore = 120;

	/** Score awarded to the player and team for every enemy defeated */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Shooter|Rules", meta=(ClampMin=1))
	int32 ScorePerEnemy = 10;

	/** Extra score awarded on top of ScorePerEnemy when the killing blow is a headshot */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Shooter|Rules", meta=(ClampMin=0))
	int32 HeadshotBonus = 5;

	/** Delay before the first wave starts */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Shooter|Waves", meta=(ClampMin=0, Units="s"))
	float InitialWaveDelay = 2.0f;

	/** Delay before the next wave starts after all enemies are defeated */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Shooter|Waves", meta=(ClampMin=0, Units="s"))
	float NextWaveDelay = 4.0f;

	/** Interval between individual enemy spawns inside one wave */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Shooter|Waves", meta=(ClampMin=0.05, Units="s"))
	float EnemySpawnInterval = 0.55f;

	/** Registered spawn points found in the level */
	UPROPERTY()
	TArray<TObjectPtr<AShooterNPCSpawner>> SpawnPoints;

	/** Zero-based index of the active wave */
	int32 CurrentWaveIndex = INDEX_NONE;

	/** Server-side count of active enemies */
	int32 AliveEnemyCount = 0;

	/** Enemies remaining to spawn in the active wave */
	int32 PendingWaveEnemies = 0;

	/** Running index used to distribute the active wave across spawn points */
	int32 NextSpawnIndex = 0;

	FTimerHandle WaveTimer;
	FTimerHandle WaveSpawnTimer;

protected:

	/** Gameplay initialization */
	virtual void BeginPlay() override;

	/** Gameplay cleanup */
	virtual void EndPlay(EEndPlayReason::Type EndPlayReason) override;

public:
	AShooterGameMode();

	/** Increases the score for the given team */
	void IncrementTeamScore(uint8 TeamByte);

	/** Registers a level spawn point for wave spawning */
	void RegisterSpawner(AShooterNPCSpawner* Spawner);

	/** Called when an enemy has been spawned */
	void HandleEnemySpawned(AShooterNPC* Enemy);

	/** Called when an enemy has been defeated */
	void HandleEnemyKilled(AController* KillerController, AShooterNPC* Enemy, bool bHeadshot);

	UFUNCTION(BlueprintCallable, Category="Shooter|Waves")
	void StartNextWave();

	UFUNCTION(BlueprintPure, Category="Shooter|Rules")
	int32 GetScorePerEnemy() const { return ScorePerEnemy; }

protected:
	void CacheSpawnPoints();
	void SpawnNextWaveEnemy();
	void UpdateReplicatedMatchState();
	void CheckVictoryOrAdvanceWave();
	bool HasWonByClearingWaves() const;
};
