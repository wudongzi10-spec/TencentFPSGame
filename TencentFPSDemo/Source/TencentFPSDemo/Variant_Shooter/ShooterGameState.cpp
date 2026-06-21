// Copyright Epic Games, Inc. All Rights Reserved.

#include "Variant_Shooter/ShooterGameState.h"
#include "GameFramework/PlayerState.h"
#include "Net/UnrealNetwork.h"

AShooterGameState::AShooterGameState()
{
	bReplicates = true;
}

void AShooterGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AShooterGameState, TeamScore);
	DOREPLIFETIME(AShooterGameState, CurrentWave);
	DOREPLIFETIME(AShooterGameState, AliveEnemyCount);
	DOREPLIFETIME(AShooterGameState, TotalWaves);
	DOREPLIFETIME(AShooterGameState, TargetScore);
	DOREPLIFETIME(AShooterGameState, bGameWon);
	DOREPLIFETIME(AShooterGameState, KillFeedSerial);
	DOREPLIFETIME(AShooterGameState, LastKillMessage);
	DOREPLIFETIME(AShooterGameState, LastKillScoreAward);
}

void AShooterGameState::ConfigureMatch(int32 InTotalWaves, int32 InTargetScore)
{
	if (!HasAuthority())
	{
		return;
	}

	TotalWaves = FMath::Max(1, InTotalWaves);
	TargetScore = FMath::Max(1, InTargetScore);
	BroadcastMatchStateChanged();
}

void AShooterGameState::AddTeamScore(int32 ScoreDelta)
{
	if (!HasAuthority() || ScoreDelta == 0)
	{
		return;
	}

	TeamScore = FMath::Max(0, TeamScore + ScoreDelta);
	BroadcastMatchStateChanged();
}

void AShooterGameState::RecordKill(APlayerState* KillerState, int32 ScoreAward)
{
	if (!HasAuthority())
	{
		return;
	}

	const FString KillerName = KillerState ? KillerState->GetPlayerName() : TEXT("玩家");
	LastKillMessage = FString::Printf(TEXT("%s 击败敌人"), *KillerName);
	LastKillScoreAward = FMath::Max(0, ScoreAward);
	++KillFeedSerial;
	BroadcastMatchStateChanged();
}

void AShooterGameState::SetCurrentWave(int32 InCurrentWave)
{
	if (!HasAuthority())
	{
		return;
	}

	CurrentWave = FMath::Clamp(InCurrentWave, 0, TotalWaves);
	BroadcastMatchStateChanged();
}

void AShooterGameState::SetAliveEnemyCount(int32 InAliveEnemyCount)
{
	if (!HasAuthority())
	{
		return;
	}

	AliveEnemyCount = FMath::Max(0, InAliveEnemyCount);
	BroadcastMatchStateChanged();
}

void AShooterGameState::SetGameWon(bool bInGameWon)
{
	if (!HasAuthority() || bGameWon == bInGameWon)
	{
		return;
	}

	bGameWon = bInGameWon;
	BroadcastMatchStateChanged();

	if (bGameWon)
	{
		OnGameWon.Broadcast();
		BP_OnGameWon();
	}
}

void AShooterGameState::OnRep_MatchStatus()
{
	BroadcastMatchStateChanged();
}

void AShooterGameState::OnRep_GameWon()
{
	BroadcastMatchStateChanged();

	if (bGameWon)
	{
		OnGameWon.Broadcast();
		BP_OnGameWon();
	}
}

void AShooterGameState::OnRep_KillFeed()
{
	BroadcastMatchStateChanged();
}

void AShooterGameState::BroadcastMatchStateChanged()
{
	OnMatchStateChanged.Broadcast();
	BP_OnMatchStateChanged();
}
