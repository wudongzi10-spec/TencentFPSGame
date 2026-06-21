// Copyright Epic Games, Inc. All Rights Reserved.


#include "Variant_Shooter/ShooterPlayerController.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "InputMappingContext.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerStart.h"
#include "ShooterCharacter.h"
#include "ShooterBulletCounterUI.h"
#include "TencentFPSDemo.h"
#include "Widgets/Input/SVirtualJoystick.h"

void AShooterPlayerController::BeginPlay()
{
	Super::BeginPlay();

	// only spawn touch controls on local player controllers
	if (IsLocalPlayerController())
	{
		if (ShouldUseTouchControls())
		{
			// spawn the mobile controls widget
			MobileControlsWidget = CreateWidget<UUserWidget>(this, MobileControlsWidgetClass);

			if (MobileControlsWidget)
			{
				// add the controls to the player screen
				MobileControlsWidget->AddToPlayerScreen(0);

			} else {

				UE_LOG(LogTencentFPSDemo, Error, TEXT("Could not spawn mobile controls widget."));

			}
		}

		if (bUseLegacyBulletCounterUI && BulletCounterUIClass)
		{
			BulletCounterUI = CreateWidget<UShooterBulletCounterUI>(this, BulletCounterUIClass);
			if (BulletCounterUI)
			{
				BulletCounterUI->AddToPlayerScreen(0);
			}
		}
		
	}
}

void AShooterPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	// only add IMCs for local player controllers
	if (IsLocalPlayerController())
	{
		// add the input mapping contexts
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
		{
			for (UInputMappingContext* CurrentContext : DefaultMappingContexts)
			{
				Subsystem->AddMappingContext(CurrentContext, 0);
			}

			// only add these IMCs if we're not using mobile touch input
			if (!ShouldUseTouchControls())
			{
				for (UInputMappingContext* CurrentContext : MobileExcludedMappingContexts)
				{
					Subsystem->AddMappingContext(CurrentContext, 0);
				}
			}
		}
	}
}

void AShooterPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	// subscribe to the pawn's OnDestroyed delegate
	InPawn->OnDestroyed.AddDynamic(this, &AShooterPlayerController::OnPawnDestroyed);

	// is this a shooter character?
	if (AShooterCharacter* ShooterCharacter = Cast<AShooterCharacter>(InPawn))
	{
		// add the player tag
		ShooterCharacter->Tags.Add(PlayerPawnTag);

		// subscribe to the pawn's delegates
		ShooterCharacter->OnBulletCountUpdated.AddDynamic(this, &AShooterPlayerController::OnBulletCountUpdated);
		ShooterCharacter->OnDamaged.AddDynamic(this, &AShooterPlayerController::OnPawnDamaged);

		// force update the life bar
		ShooterCharacter->OnDamaged.Broadcast(1.0f);
	}
}

void AShooterPlayerController::OnPawnDestroyed(AActor* DestroyedActor)
{
	// reset the bullet counter HUD
	if (IsValid(BulletCounterUI))
	{
		BulletCounterUI->BP_UpdateBulletCounter(0, 0);
	}

	// find the player start
	TArray<AActor*> ActorList;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), APlayerStart::StaticClass(), ActorList);

	if (ActorList.Num() > 0)
	{
		// select a random player start
		AActor* RandomPlayerStart = ActorList[FMath::RandRange(0, ActorList.Num() - 1)];

		// spawn a character at the player start
		const FTransform SpawnTransform = RandomPlayerStart->GetActorTransform();

		if (AShooterCharacter* RespawnedCharacter = GetWorld()->SpawnActor<AShooterCharacter>(CharacterClass, SpawnTransform))
		{
			// possess the character
			Possess(RespawnedCharacter);
		}
	}
}

void AShooterPlayerController::OnBulletCountUpdated(int32 MagazineSize, int32 Bullets)
{
	// update the UI
	if (BulletCounterUI)
	{
		BulletCounterUI->BP_UpdateBulletCounter(MagazineSize, Bullets);
	}
}

void AShooterPlayerController::OnPawnDamaged(float LifePercent)
{
	if (IsValid(BulletCounterUI))
	{
		BulletCounterUI->BP_Damaged(LifePercent);
	}
}

bool AShooterPlayerController::ShouldUseTouchControls() const
{
	// are we on a mobile platform? Should we force touch?
	return SVirtualJoystick::ShouldDisplayTouchInterface() || bForceTouchControls;
}

void AShooterPlayerController::ClientHitConfirmed_Implementation(bool bLethal, bool bHeadshot, FVector HitLocation, float DamageAmount)
{
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;

	HitMarkerTime = Now;
	bHitMarkerLethal = bLethal;
	bHitMarkerHeadshot = bHeadshot;

	// spawn a floating damage number at the impact point
	if (DamageAmount > 0.0f)
	{
		FDamageNumber Number;
		Number.WorldLocation = HitLocation;
		Number.Amount = DamageAmount;
		Number.Time = Now;
		Number.bHeadshot = bHeadshot;
		DamageNumbers.Add(Number);

		const int32 MaxNumbers = 12;
		if (DamageNumbers.Num() > MaxNumbers)
		{
			DamageNumbers.RemoveAt(0, DamageNumbers.Num() - MaxNumbers);
		}
	}
}

void AShooterPlayerController::ClientKillScored_Implementation(int32 ScoreAward, bool bHeadshot)
{
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;

	KillPopupTime = Now;
	KillPopupScore = ScoreAward;
	bKillPopupHeadshot = bHeadshot;

	// chain consecutive kills into an escalating streak; a long gap resets it.
	// The streak only ever climbs (2->3->4...) within the window, never drops back.
	if (Now - LastKillTime <= MultiKillWindow)
	{
		++MultiKillCount;
	}
	else
	{
		MultiKillCount = 1;
	}
	LastKillTime = Now;
	MultiKillTime = Now;
}

void AShooterPlayerController::ClientTookDamage_Implementation(FVector FromLocation)
{
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;

	DamageFlashTime = Now;

	FDamageIndicator Indicator;
	Indicator.WorldLocation = FromLocation;
	Indicator.Time = Now;
	DamageIndicators.Add(Indicator);

	// keep only the most recent handful so the HUD stays readable
	const int32 MaxIndicators = 6;
	if (DamageIndicators.Num() > MaxIndicators)
	{
		DamageIndicators.RemoveAt(0, DamageIndicators.Num() - MaxIndicators);
	}
}
