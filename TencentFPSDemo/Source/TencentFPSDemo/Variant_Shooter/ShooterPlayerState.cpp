// Copyright Epic Games, Inc. All Rights Reserved.

#include "Variant_Shooter/ShooterPlayerState.h"
#include "Net/UnrealNetwork.h"

AShooterPlayerState::AShooterPlayerState()
{
	bReplicates = true;
}

void AShooterPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AShooterPlayerState, Kills);
	DOREPLIFETIME(AShooterPlayerState, Deaths);
}

void AShooterPlayerState::AddKill(int32 ScoreAward)
{
	if (!HasAuthority())
	{
		return;
	}

	++Kills;
	SetScore(GetScore() + ScoreAward);
	BroadcastKillsChanged();
}

void AShooterPlayerState::AddDeath()
{
	if (!HasAuthority())
	{
		return;
	}

	++Deaths;
}

void AShooterPlayerState::OnRep_Kills()
{
	BroadcastKillsChanged();
}

void AShooterPlayerState::BroadcastKillsChanged()
{
	BP_OnKillsChanged();
}
