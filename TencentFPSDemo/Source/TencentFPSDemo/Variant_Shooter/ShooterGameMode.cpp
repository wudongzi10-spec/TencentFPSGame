// Copyright Epic Games, Inc. All Rights Reserved.

#include "Variant_Shooter/ShooterGameMode.h"
#include "Variant_Shooter/AI/ShooterNPC.h"
#include "Variant_Shooter/AI/ShooterNPCSpawner.h"
#include "Variant_Shooter/ShooterGameState.h"
#include "Variant_Shooter/ShooterPlayerState.h"
#include "Variant_Shooter/ShooterPlayerController.h"
#include "Variant_Shooter/UI/ShooterHUD.h"
#include "ShooterUI.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

AShooterGameMode::AShooterGameMode()
{
	GameStateClass = AShooterGameState::StaticClass();
	PlayerStateClass = AShooterPlayerState::StaticClass();
	HUDClass = AShooterHUD::StaticClass();

	EnemiesPerWave = { 3, 4, 5 };
}

void AShooterGameMode::BeginPlay()
{
	Super::BeginPlay();

	if (bUseDemoOptimizedWaveProfile)
	{
		EnemiesPerWave = { 3, 4, 5 };
		TargetTeamScore = 120;
		EnemySpawnInterval = 0.55f;
	}

	CacheSpawnPoints();

	if (AShooterGameState* ShooterGameState = GetGameState<AShooterGameState>())
	{
		ShooterGameState->ConfigureMatch(EnemiesPerWave.Num(), TargetTeamScore);
		ShooterGameState->SetCurrentWave(0);
		ShooterGameState->SetAliveEnemyCount(0);
	}

	if (bUseLegacyShooterUI && ShooterUIClass)
	{
		if (APlayerController* FirstPC = UGameplayStatics::GetPlayerController(GetWorld(), 0))
		{
			ShooterUI = CreateWidget<UShooterUI>(FirstPC, ShooterUIClass);
			if (ShooterUI)
			{
				ShooterUI->AddToViewport(0);
			}
		}
	}

	GetWorld()->GetTimerManager().SetTimer(WaveTimer, this, &AShooterGameMode::StartNextWave, InitialWaveDelay, false);
}

void AShooterGameMode::EndPlay(EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);

	GetWorld()->GetTimerManager().ClearTimer(WaveTimer);
	GetWorld()->GetTimerManager().ClearTimer(WaveSpawnTimer);
}

void AShooterGameMode::IncrementTeamScore(uint8 TeamByte)
{
	// Backward-compatible hook for older template code. Enemy team deaths award objective score.
	if (TeamByte == 1)
	{
		if (AShooterGameState* ShooterGameState = GetGameState<AShooterGameState>())
		{
			ShooterGameState->AddTeamScore(ScorePerEnemy);
		}
	}
}

void AShooterGameMode::RegisterSpawner(AShooterNPCSpawner* Spawner)
{
	if (IsValid(Spawner))
	{
		SpawnPoints.AddUnique(Spawner);
	}
}

void AShooterGameMode::HandleEnemySpawned(AShooterNPC* Enemy)
{
	if (!IsValid(Enemy))
	{
		return;
	}

	++AliveEnemyCount;
	UpdateReplicatedMatchState();
}

void AShooterGameMode::HandleEnemyKilled(AController* KillerController, AShooterNPC* Enemy, bool bHeadshot)
{
	const int32 Award = ScorePerEnemy + (bHeadshot ? HeadshotBonus : 0);

	if (AShooterGameState* ShooterGameState = GetGameState<AShooterGameState>())
	{
		ShooterGameState->AddTeamScore(Award);
		ShooterGameState->RecordKill(KillerController ? KillerController->PlayerState : nullptr, Award);
	}

	if (KillerController)
	{
		if (AShooterPlayerState* KillerState = KillerController->GetPlayerState<AShooterPlayerState>())
		{
			KillerState->AddKill(Award);
		}

		// drive the killer's animated score popup
		if (AShooterPlayerController* KillerPC = Cast<AShooterPlayerController>(KillerController))
		{
			KillerPC->ClientKillScored(Award, bHeadshot);
		}
	}

	AliveEnemyCount = FMath::Max(0, AliveEnemyCount - 1);
	UpdateReplicatedMatchState();
	CheckVictoryOrAdvanceWave();
}

void AShooterGameMode::StartNextWave()
{
	if (AShooterGameState* ShooterGameState = GetGameState<AShooterGameState>())
	{
		if (ShooterGameState->IsGameWon())
		{
			return;
		}
	}

	++CurrentWaveIndex;

	if (!EnemiesPerWave.IsValidIndex(CurrentWaveIndex))
	{
		CheckVictoryOrAdvanceWave();
		return;
	}

	CacheSpawnPoints();

	const int32 EnemyCount = FMath::Max(0, EnemiesPerWave[CurrentWaveIndex]);
	AliveEnemyCount = 0;
	PendingWaveEnemies = EnemyCount;
	NextSpawnIndex = 0;
	UpdateReplicatedMatchState();

	if (PendingWaveEnemies > 0)
	{
		if (SpawnPoints.Num() == 0)
		{
			UE_LOG(LogTemp, Warning, TEXT("ShooterGameMode could not spawn wave enemies because no ShooterNPCSpawner exists in the level."));
			PendingWaveEnemies = 0;
			CheckVictoryOrAdvanceWave();
			return;
		}

		SpawnNextWaveEnemy();
		GetWorld()->GetTimerManager().SetTimer(WaveSpawnTimer, this, &AShooterGameMode::SpawnNextWaveEnemy, EnemySpawnInterval, true);
		return;
	}

	if (AliveEnemyCount == 0)
	{
		CheckVictoryOrAdvanceWave();
	}
}

void AShooterGameMode::CacheSpawnPoints()
{
	TArray<AActor*> FoundSpawners;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AShooterNPCSpawner::StaticClass(), FoundSpawners);

	for (AActor* FoundActor : FoundSpawners)
	{
		if (AShooterNPCSpawner* Spawner = Cast<AShooterNPCSpawner>(FoundActor))
		{
			RegisterSpawner(Spawner);
		}
	}
}

void AShooterGameMode::SpawnNextWaveEnemy()
{
	if (PendingWaveEnemies <= 0)
	{
		GetWorld()->GetTimerManager().ClearTimer(WaveSpawnTimer);
		CheckVictoryOrAdvanceWave();
		return;
	}

	if (SpawnPoints.Num() == 0)
	{
		PendingWaveEnemies = 0;
		GetWorld()->GetTimerManager().ClearTimer(WaveSpawnTimer);
		CheckVictoryOrAdvanceWave();
		return;
	}

	const int32 SpawnPointIndex = NextSpawnIndex % SpawnPoints.Num();
	++NextSpawnIndex;
	--PendingWaveEnemies;

	if (AShooterNPCSpawner* SpawnPoint = SpawnPoints[SpawnPointIndex])
	{
		if (AShooterNPC* Enemy = SpawnPoint->SpawnNPCForWave())
		{
			HandleEnemySpawned(Enemy);
		}
	}

	if (PendingWaveEnemies <= 0)
	{
		GetWorld()->GetTimerManager().ClearTimer(WaveSpawnTimer);
		if (AliveEnemyCount == 0)
		{
			CheckVictoryOrAdvanceWave();
		}
	}
}

void AShooterGameMode::UpdateReplicatedMatchState()
{
	if (AShooterGameState* ShooterGameState = GetGameState<AShooterGameState>())
	{
		ShooterGameState->SetCurrentWave(CurrentWaveIndex + 1);
		ShooterGameState->SetAliveEnemyCount(AliveEnemyCount);
	}
}

void AShooterGameMode::CheckVictoryOrAdvanceWave()
{
	AShooterGameState* ShooterGameState = GetGameState<AShooterGameState>();
	if (!ShooterGameState || ShooterGameState->IsGameWon())
	{
		return;
	}

	if (ShooterGameState->GetTeamScore() >= TargetTeamScore || HasWonByClearingWaves())
	{
		ShooterGameState->SetGameWon(true);
		return;
	}

	if (PendingWaveEnemies == 0 && AliveEnemyCount == 0 && EnemiesPerWave.IsValidIndex(CurrentWaveIndex + 1))
	{
		GetWorld()->GetTimerManager().SetTimer(WaveTimer, this, &AShooterGameMode::StartNextWave, NextWaveDelay, false);
	}
}

bool AShooterGameMode::HasWonByClearingWaves() const
{
	return PendingWaveEnemies == 0 && AliveEnemyCount == 0 && CurrentWaveIndex >= EnemiesPerWave.Num() - 1;
}
