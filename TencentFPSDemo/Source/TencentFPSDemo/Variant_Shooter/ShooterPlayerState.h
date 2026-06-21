// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "ShooterPlayerState.generated.h"

/**
 * Replicated per-player statistics for the shooter assignment demo.
 */
UCLASS()
class TENCENTFPSDEMO_API AShooterPlayerState : public APlayerState
{
	GENERATED_BODY()

public:
	AShooterPlayerState();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintPure, Category="Shooter|Score")
	int32 GetKills() const { return Kills; }

	UFUNCTION(BlueprintPure, Category="Shooter|Score")
	int32 GetDeaths() const { return Deaths; }

	void AddKill(int32 ScoreAward);

	void AddDeath();

protected:
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_Kills, Category="Shooter|Score")
	int32 Kills = 0;

	UPROPERTY(BlueprintReadOnly, Replicated, Category="Shooter|Score")
	int32 Deaths = 0;

	UFUNCTION()
	void OnRep_Kills();

	void BroadcastKillsChanged();

	UFUNCTION(BlueprintImplementableEvent, Category="Shooter|Score", meta=(DisplayName="On Kills Changed"))
	void BP_OnKillsChanged();
};
