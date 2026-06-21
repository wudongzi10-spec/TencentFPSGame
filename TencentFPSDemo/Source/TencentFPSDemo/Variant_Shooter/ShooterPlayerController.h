// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "ShooterPlayerController.generated.h"

class UInputMappingContext;
class AShooterCharacter;
class UShooterBulletCounterUI;

/**
 *  Simple PlayerController for a first person shooter game
 *  Manages input mappings
 *  Respawns the player pawn when it's destroyed
 */
UCLASS(abstract, config="Game")
class TENCENTFPSDEMO_API AShooterPlayerController : public APlayerController
{
	GENERATED_BODY()
	
protected:

	/** Input mapping contexts for this player */
	UPROPERTY(EditAnywhere, Category="Input|Input Mappings")
	TArray<UInputMappingContext*> DefaultMappingContexts;

	/** Input Mapping Contexts */
	UPROPERTY(EditAnywhere, Category="Input|Input Mappings")
	TArray<UInputMappingContext*> MobileExcludedMappingContexts;

	/** Mobile controls widget to spawn */
	UPROPERTY(EditAnywhere, Category="Input|Touch Controls")
	TSubclassOf<UUserWidget> MobileControlsWidgetClass;

	/** Pointer to the mobile controls widget */
	UPROPERTY()
	TObjectPtr<UUserWidget> MobileControlsWidget;

	/** If true, the player will use UMG touch controls even if not playing on mobile platforms */
	UPROPERTY(EditAnywhere, Config, Category = "Input|Touch Controls")
	bool bForceTouchControls = false;

	/** Character class to respawn when the possessed pawn is destroyed */
	UPROPERTY(EditAnywhere, Category="Shooter|Respawn")
	TSubclassOf<AShooterCharacter> CharacterClass;

	/** Type of bullet counter UI widget to spawn */
	UPROPERTY(EditAnywhere, Category="Shooter|UI")
	TSubclassOf<UShooterBulletCounterUI> BulletCounterUIClass;

	/** Keep disabled because AShooterHUD now owns the combat interface. */
	UPROPERTY(EditAnywhere, Category="Shooter|UI")
	bool bUseLegacyBulletCounterUI = false;

	/** Tag to grant the possessed pawn to flag it as the player */
	UPROPERTY(EditAnywhere, Category="Shooter|Player")
	FName PlayerPawnTag = FName("Player");

	/** Pointer to the bullet counter UI widget */
	UPROPERTY()
	TObjectPtr<UShooterBulletCounterUI> BulletCounterUI;

protected:

	/** Gameplay Initialization */
	virtual void BeginPlay() override;

	/** Initialize input bindings */
	virtual void SetupInputComponent() override;

	/** Pawn initialization */
	virtual void OnPossess(APawn* InPawn) override;

	/** Called if the possessed pawn is destroyed */
	UFUNCTION()
	void OnPawnDestroyed(AActor* DestroyedActor);

	/** Called when the bullet count on the possessed pawn is updated */
	UFUNCTION()
	void OnBulletCountUpdated(int32 MagazineSize, int32 Bullets);

	/** Called when the possessed pawn is damaged */
	UFUNCTION()
	void OnPawnDamaged(float LifePercent);

	/** Returns true if the player should use UMG touch controls */
	bool ShouldUseTouchControls() const;

public:

	/** A single "you were hit from here" marker for the directional damage indicator */
	struct FDamageIndicator
	{
		FVector WorldLocation = FVector::ZeroVector;
		float Time = 0.0f;
	};

	/** A floating world-space damage number spawned at a hit location */
	struct FDamageNumber
	{
		FVector WorldLocation = FVector::ZeroVector;
		float Amount = 0.0f;
		float Time = 0.0f;
		bool bHeadshot = false;
	};

	/** Server tells the owning client that one of its shots landed on an enemy */
	UFUNCTION(Client, Unreliable)
	void ClientHitConfirmed(bool bLethal, bool bHeadshot, FVector HitLocation, float DamageAmount);

	/** Server tells the owning client it took damage, passing the damage source location */
	UFUNCTION(Client, Unreliable)
	void ClientTookDamage(FVector FromLocation);

	/** Server tells the owning client it scored a kill, with the score awarded and whether it was a headshot */
	UFUNCTION(Client, Unreliable)
	void ClientKillScored(int32 ScoreAward, bool bHeadshot);

	/** Game time of the most recent confirmed hit, used by the HUD hit marker */
	float GetHitMarkerTime() const { return HitMarkerTime; }

	/** True if the most recent confirmed hit was a kill */
	bool WasHitMarkerLethal() const { return bHitMarkerLethal; }

	/** True if the most recent confirmed hit was a headshot */
	bool WasHitMarkerHeadshot() const { return bHitMarkerHeadshot; }

	/** Active directional damage indicators for the HUD */
	const TArray<FDamageIndicator>& GetDamageIndicators() const { return DamageIndicators; }

	/** Active floating damage numbers for the HUD */
	const TArray<FDamageNumber>& GetDamageNumbers() const { return DamageNumbers; }

	/** Game time of the most recent damage taken, used by the HUD damage flash */
	float GetDamageFlashTime() const { return DamageFlashTime; }

	/** Game time of the most recent scored kill, used by the HUD score popup */
	float GetKillPopupTime() const { return KillPopupTime; }

	/** Score awarded by the most recent kill */
	int32 GetKillPopupScore() const { return KillPopupScore; }

	/** Whether the most recent scored kill was a headshot */
	bool WasKillPopupHeadshot() const { return bKillPopupHeadshot; }

	/** Number of kills in the current multi-kill streak (>=2 means double kill etc.) */
	int32 GetMultiKillCount() const { return MultiKillCount; }

	/** Game time the current multi-kill streak last updated */
	float GetMultiKillTime() const { return MultiKillTime; }

protected:

	/** Game time of the most recent confirmed hit */
	float HitMarkerTime = -10.0f;

	/** Whether the most recent confirmed hit killed the target */
	bool bHitMarkerLethal = false;

	/** Whether the most recent confirmed hit was a headshot */
	bool bHitMarkerHeadshot = false;

	/** Recent directional damage sources, pruned by the HUD as they expire */
	TArray<FDamageIndicator> DamageIndicators;

	/** Recent floating damage numbers, pruned as they expire */
	TArray<FDamageNumber> DamageNumbers;

	/** Game time of the most recent damage taken */
	float DamageFlashTime = -10.0f;

	/** Game time of the most recent scored kill */
	float KillPopupTime = -10.0f;

	/** Score awarded by the most recent kill */
	int32 KillPopupScore = 0;

	/** Whether the most recent scored kill was a headshot */
	bool bKillPopupHeadshot = false;

	/** Game time of the previous kill, used to chain consecutive kills into a streak */
	float LastKillTime = -10.0f;

	/** Number of kills in the current consecutive streak */
	int32 MultiKillCount = 0;

	/** Game time the multi-kill count last changed */
	float MultiKillTime = -10.0f;

	/** Max seconds between kills for them to chain into the same multi-kill streak */
	float MultiKillWindow = 4.0f;
};
