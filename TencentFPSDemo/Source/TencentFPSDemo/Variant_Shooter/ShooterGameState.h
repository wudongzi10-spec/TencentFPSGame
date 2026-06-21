// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "ShooterGameState.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FShooterMatchStateChangedDelegate);

class APlayerState;

/**
 * Replicated match state for the shooter assignment demo.
 * Stores the cooperative score, wave progress and victory flag.
 */
UCLASS()
class TENCENTFPSDEMO_API AShooterGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	AShooterGameState();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Raised whenever any replicated match value changes. */
	UPROPERTY(BlueprintAssignable, Category="Shooter|Match")
	FShooterMatchStateChangedDelegate OnMatchStateChanged;

	/** Raised when the match enters victory state. */
	UPROPERTY(BlueprintAssignable, Category="Shooter|Match")
	FShooterMatchStateChangedDelegate OnGameWon;

	UFUNCTION(BlueprintPure, Category="Shooter|Match")
	int32 GetTeamScore() const { return TeamScore; }

	UFUNCTION(BlueprintPure, Category="Shooter|Match")
	int32 GetCurrentWave() const { return CurrentWave; }

	UFUNCTION(BlueprintPure, Category="Shooter|Match")
	int32 GetAliveEnemyCount() const { return AliveEnemyCount; }

	UFUNCTION(BlueprintPure, Category="Shooter|Match")
	int32 GetTotalWaves() const { return TotalWaves; }

	UFUNCTION(BlueprintPure, Category="Shooter|Match")
	int32 GetTargetScore() const { return TargetScore; }

	UFUNCTION(BlueprintPure, Category="Shooter|Match")
	bool IsGameWon() const { return bGameWon; }

	UFUNCTION(BlueprintPure, Category="Shooter|Combat")
	int32 GetKillFeedSerial() const { return KillFeedSerial; }

	UFUNCTION(BlueprintPure, Category="Shooter|Combat")
	const FString& GetLastKillMessage() const { return LastKillMessage; }

	UFUNCTION(BlueprintPure, Category="Shooter|Combat")
	int32 GetLastKillScoreAward() const { return LastKillScoreAward; }

	void ConfigureMatch(int32 InTotalWaves, int32 InTargetScore);
	void AddTeamScore(int32 ScoreDelta);
	void RecordKill(APlayerState* KillerState, int32 ScoreAward);
	void SetCurrentWave(int32 InCurrentWave);
	void SetAliveEnemyCount(int32 InAliveEnemyCount);
	void SetGameWon(bool bInGameWon);

protected:
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_MatchStatus, Category="Shooter|Match")
	int32 TeamScore = 0;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_MatchStatus, Category="Shooter|Match")
	int32 CurrentWave = 0;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_MatchStatus, Category="Shooter|Match")
	int32 AliveEnemyCount = 0;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_MatchStatus, Category="Shooter|Match")
	int32 TotalWaves = 3;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_MatchStatus, Category="Shooter|Match")
	int32 TargetScore = 120;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_GameWon, Category="Shooter|Match")
	bool bGameWon = false;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_KillFeed, Category="Shooter|Combat")
	int32 KillFeedSerial = 0;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_KillFeed, Category="Shooter|Combat")
	FString LastKillMessage;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_KillFeed, Category="Shooter|Combat")
	int32 LastKillScoreAward = 0;

	UFUNCTION()
	void OnRep_MatchStatus();

	UFUNCTION()
	void OnRep_GameWon();

	UFUNCTION()
	void OnRep_KillFeed();

	void BroadcastMatchStateChanged();

	UFUNCTION(BlueprintImplementableEvent, Category="Shooter|Match", meta=(DisplayName="On Match State Changed"))
	void BP_OnMatchStateChanged();

	UFUNCTION(BlueprintImplementableEvent, Category="Shooter|Match", meta=(DisplayName="On Game Won"))
	void BP_OnGameWon();
};
