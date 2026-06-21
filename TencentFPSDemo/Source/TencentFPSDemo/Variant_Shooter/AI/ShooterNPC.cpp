// Copyright Epic Games, Inc. All Rights Reserved.


#include "Variant_Shooter/AI/ShooterNPC.h"
#include "ShooterWeapon.h"
#include "Components/SkeletalMeshComponent.h"
#include "Camera/CameraComponent.h"
#include "Kismet/KismetMathLibrary.h"
#include "Engine/World.h"
#include "ShooterGameMode.h"
#include "ShooterPlayerController.h"
#include "Engine/DamageEvents.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "TimerManager.h"
#include "Net/UnrealNetwork.h"

AShooterNPC::AShooterNPC()
{
	bReplicates = true;
	SetReplicateMovement(true);
}

void AShooterNPC::BeginPlay()
{
	Super::BeginPlay();

	if (!HasAuthority())
	{
		return;
	}

	// spawn the weapon
	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = this;
	SpawnParams.Instigator = this;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	Weapon = GetWorld()->SpawnActor<AShooterWeapon>(WeaponClass, GetActorTransform(), SpawnParams);
}

void AShooterNPC::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);

	// clear the death timer
	GetWorld()->GetTimerManager().ClearTimer(DeathTimer);
}

void AShooterNPC::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AShooterNPC, CurrentHP);
	DOREPLIFETIME(AShooterNPC, Weapon);
	DOREPLIFETIME(AShooterNPC, bIsDead);
}

float AShooterNPC::TakeDamage(float Damage, struct FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	if (!HasAuthority())
	{
		return 0.0f;
	}

	// ignore if already dead
	if (bIsDead)
	{
		return 0.0f;
	}

	// which body zone did this shot land in? Point damage carries the impact location
	bool bHeadshot = false;
	float ZoneMultiplier = 1.0f;
	FVector HitLocation = GetActorLocation() + FVector(0.0f, 0.0f, 40.0f);
	if (DamageEvent.IsOfType(FPointDamageEvent::ClassID))
	{
		const FPointDamageEvent& PointEvent = static_cast<const FPointDamageEvent&>(DamageEvent);
		HitLocation = PointEvent.HitInfo.ImpactPoint;
		ZoneMultiplier = GetHitZoneMultiplier(HitLocation, bHeadshot);
	}

	const float FinalDamage = Damage * ZoneMultiplier;

	// Reduce HP
	CurrentHP = FMath::Max(0.0f, CurrentHP - FinalDamage);

	// Have we depleted HP?
	const bool bLethal = CurrentHP <= 0.0f;
	if (bLethal)
	{
		Die(EventInstigator, bHeadshot);
	}

	// notify the shooter so their HUD can show a hit/kill marker and floating damage number
	if (AShooterPlayerController* ShooterPC = Cast<AShooterPlayerController>(EventInstigator))
	{
		ShooterPC->ClientHitConfirmed(bLethal, bHeadshot, HitLocation, FinalDamage);
	}

	return FinalDamage;
}

void AShooterNPC::AttachWeaponMeshes(AShooterWeapon* WeaponToAttach)
{
	const FAttachmentTransformRules AttachmentRule(EAttachmentRule::SnapToTarget, false);

	// attach the weapon actor
	WeaponToAttach->AttachToActor(this, AttachmentRule);

	// attach the weapon meshes
	WeaponToAttach->GetFirstPersonMesh()->AttachToComponent(GetFirstPersonMesh(), AttachmentRule, FirstPersonWeaponSocket);
	WeaponToAttach->GetThirdPersonMesh()->AttachToComponent(GetMesh(), AttachmentRule, ThirdPersonWeaponSocket);
}

void AShooterNPC::PlayFiringMontage(UAnimMontage* Montage)
{
	// unused
}

void AShooterNPC::PlayReloadAnimation(UAnimMontage* Montage, UAnimSequenceBase* Animation)
{
	// unused
}

void AShooterNPC::AddWeaponRecoil(float Recoil)
{
	// unused
}

void AShooterNPC::UpdateWeaponHUD(int32 CurrentAmmo, int32 MagazineSize)
{
	// unused
}

FVector AShooterNPC::GetWeaponTargetLocation()
{
	// start aiming from the camera location
	const FVector AimSource = GetFirstPersonCameraComponent()->GetComponentLocation();

	FVector AimDir, AimTarget = FVector::ZeroVector;

	// do we have an aim target?
	if (CurrentAimTarget)
	{
		// target the actor location
		AimTarget = CurrentAimTarget->GetActorLocation();

		// apply a vertical offset to target head/feet
		AimTarget.Z += FMath::RandRange(MinAimOffsetZ, MaxAimOffsetZ);

		// get the aim direction and apply randomness in a cone
		AimDir = (AimTarget - AimSource).GetSafeNormal();
		AimDir = UKismetMathLibrary::RandomUnitVectorInConeInDegrees(AimDir, AimVarianceHalfAngle);

		
	} else {

		// no aim target, so just use the camera facing
		AimDir = UKismetMathLibrary::RandomUnitVectorInConeInDegrees(GetFirstPersonCameraComponent()->GetForwardVector(), AimVarianceHalfAngle);

	}

	// calculate the unobstructed aim target location
	AimTarget = AimSource + (AimDir * AimRange);

	// run a visibility trace to see if there's obstructions
	FHitResult OutHit;

	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(this);

	GetWorld()->LineTraceSingleByChannel(OutHit, AimSource, AimTarget, ECC_Visibility, QueryParams);

	// return either the impact point or the trace end
	return OutHit.bBlockingHit ? OutHit.ImpactPoint : OutHit.TraceEnd;
}

void AShooterNPC::AddWeaponClass(const TSubclassOf<AShooterWeapon>& InWeaponClass)
{
	// unused
}

void AShooterNPC::OnWeaponActivated(AShooterWeapon* InWeapon)
{
	// unused
}

void AShooterNPC::OnWeaponDeactivated(AShooterWeapon* InWeapon)
{
	// unused
}

void AShooterNPC::OnSemiWeaponRefire()
{
	// are we still shooting?
	if (bIsShooting && Weapon)
	{
		// fire the weapon
		Weapon->StartFiring();
	}
}

float AShooterNPC::GetHitZoneMultiplier(const FVector& ImpactPoint, bool& bOutHeadshot) const
{
	bOutHeadshot = false;

	const float HalfHeight = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	if (HalfHeight <= 0.0f)
	{
		return 1.0f;
	}

	// impact height relative to the capsule center, as a fraction of the half-height
	const float Frac = (ImpactPoint.Z - GetActorLocation().Z) / HalfHeight;

	if (Frac >= HeadshotHeightFraction)
	{
		bOutHeadshot = true;
		return HeadshotDamageMultiplier;
	}

	if (Frac <= -LegHeightFraction)
	{
		return LegDamageMultiplier;
	}

	return 1.0f;
}

void AShooterNPC::Die(AController* KillerController, bool bHeadshot)
{
	// ignore if already dead
	if (bIsDead)
	{
		return;
	}

	// raise the dead flag
	bIsDead = true;

	ApplyDeathState();

	// call the delegate
	OnPawnDeath.Broadcast();

	if (AShooterGameMode* GM = Cast<AShooterGameMode>(GetWorld()->GetAuthGameMode()))
	{
		GM->HandleEnemyKilled(KillerController, this, bHeadshot);
	}

	// schedule actor destruction
	GetWorld()->GetTimerManager().SetTimer(DeathTimer, this, &AShooterNPC::DeferredDestruction, DeferredDestructionTime, false);
}

void AShooterNPC::OnRep_CurrentHP()
{
	if (CurrentHP <= 0.0f && bIsDead)
	{
		ApplyDeathState();
	}
}

void AShooterNPC::OnRep_IsDead()
{
	if (bIsDead)
	{
		ApplyDeathState();
	}
}

void AShooterNPC::ApplyDeathState()
{
	StopShooting();

	// grant the death tag to the character
	Tags.AddUnique(DeathTag);

	// disable capsule collision
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// stop movement
	GetCharacterMovement()->StopMovementImmediately();
	GetCharacterMovement()->StopActiveMovement();

	// enable ragdoll physics on the third person mesh
	if (bUseRagdollOnDeath)
	{
		GetMesh()->SetCollisionProfileName(RagdollCollisionProfile);
		GetMesh()->SetSimulatePhysics(true);
		GetMesh()->SetPhysicsBlendWeight(1.0f);
	}
	else
	{
		GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		GetMesh()->SetSimulatePhysics(false);
	}
}

void AShooterNPC::DeferredDestruction()
{
	Destroy();
}

void AShooterNPC::StartShooting(AActor* ActorToShoot)
{
	if (bIsDead || !Weapon)
	{
		return;
	}

	// save the aim target
	CurrentAimTarget = ActorToShoot;

	// raise the flag
	bIsShooting = true;

	// signal the weapon
	Weapon->StartFiring();
}

void AShooterNPC::StopShooting()
{
	// lower the flag
	bIsShooting = false;

	// signal the weapon
	if (Weapon)
	{
		Weapon->StopFiring();
	}
}
