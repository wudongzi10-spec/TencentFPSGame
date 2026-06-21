// Copyright Epic Games, Inc. All Rights Reserved.


#include "ShooterWeapon.h"
#include "Kismet/KismetMathLibrary.h"
#include "Engine/World.h"
#include "ShooterProjectile.h"
#include "ShooterWeaponHolder.h"
#include "Components/SceneComponent.h"
#include "TimerManager.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimSequenceBase.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Pawn.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

AShooterWeapon::AShooterWeapon()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	bReplicates = true;

	// create the root
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	// create the first person mesh
	FirstPersonMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("First Person Mesh"));
	FirstPersonMesh->SetupAttachment(RootComponent);

	FirstPersonMesh->SetCollisionProfileName(FName("NoCollision"));
	FirstPersonMesh->SetFirstPersonPrimitiveType(EFirstPersonPrimitiveType::FirstPerson);
	FirstPersonMesh->bOnlyOwnerSee = true;

	// create the third person mesh
	ThirdPersonMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Third Person Mesh"));
	ThirdPersonMesh->SetupAttachment(RootComponent);

	ThirdPersonMesh->SetCollisionProfileName(FName("NoCollision"));
	ThirdPersonMesh->SetFirstPersonPrimitiveType(EFirstPersonPrimitiveType::WorldSpaceRepresentation);
	ThirdPersonMesh->bOwnerNoSee = true;

	static ConstructorHelpers::FObjectFinder<UAnimSequenceBase> DefaultReloadAnimation(TEXT("/Game/Characters/Mannequins/Anims/Rifle/MM_Rifle_Reload.MM_Rifle_Reload"));
	if (DefaultReloadAnimation.Succeeded())
	{
		ReloadAnimation = DefaultReloadAnimation.Object;
	}
}

void AShooterWeapon::BeginPlay()
{
	Super::BeginPlay();

	AActor* OwnerActor = GetOwner();
	if (!OwnerActor)
	{
		return;
	}

	// subscribe to the owner's destroyed delegate
	OwnerActor->OnDestroyed.AddDynamic(this, &AShooterWeapon::OnOwnerDestroyed);

	// cast the weapon owner
	WeaponOwner = Cast<IShooterWeaponHolder>(OwnerActor);
	PawnOwner = Cast<APawn>(OwnerActor);

	// fill the first ammo clip
	CurrentBullets = MagazineSize;

	// remember the starting reserve so we can restore it on re-pickup
	InitialReserveBullets = ReserveBullets;

	// attach the meshes to the owner
	if (WeaponOwner)
	{
		WeaponOwner->AttachWeaponMeshes(this);
	}

	StoreMeshDefaults();
}

void AShooterWeapon::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	UpdateReloadVisuals();
}

void AShooterWeapon::EndPlay(EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);

	// clear the refire timer
	GetWorld()->GetTimerManager().ClearTimer(RefireTimer);
	GetWorld()->GetTimerManager().ClearTimer(ReloadTimer);
}

void AShooterWeapon::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AShooterWeapon, CurrentBullets);
	DOREPLIFETIME(AShooterWeapon, ReserveBullets);
	DOREPLIFETIME(AShooterWeapon, bIsReloading);
}

void AShooterWeapon::OnOwnerDestroyed(AActor* DestroyedActor)
{
	// ensure this weapon is destroyed when the owner is destroyed
	Destroy();
}

void AShooterWeapon::OnRep_CurrentBullets()
{
	if (WeaponOwner)
	{
		WeaponOwner->UpdateWeaponHUD(CurrentBullets, MagazineSize);
	}
}

void AShooterWeapon::OnRep_Reloading()
{
	if (bIsReloading)
	{
		ReloadStartTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
		SetActorTickEnabled(true);

		if (WeaponOwner)
		{
			WeaponOwner->PlayReloadAnimation(ReloadMontage, ReloadAnimation);
		}
	}
	else
	{
		ResetReloadVisuals();
		SetActorTickEnabled(false);
	}
}

void AShooterWeapon::ActivateWeapon()
{
	// unhide this weapon
	SetActorHiddenInGame(false);

	// notify the owner
	if (WeaponOwner)
	{
		WeaponOwner->OnWeaponActivated(this);
	}
}

void AShooterWeapon::DeactivateWeapon()
{
	// ensure we're no longer firing this weapon while deactivated
	StopFiring();

	// hide the weapon
	SetActorHiddenInGame(true);

	// notify the owner
	if (WeaponOwner)
	{
		WeaponOwner->OnWeaponDeactivated(this);
	}
}

void AShooterWeapon::StartFiring()
{
	if (bIsReloading)
	{
		return;
	}

	// raise the firing flag
	bIsFiring = true;

	// check how much time has passed since we last shot
	// this may be under the refire rate if the weapon shoots slow enough and the player is spamming the trigger
	const float TimeSinceLastShot = GetWorld()->GetTimeSeconds() - TimeOfLastShot;

	if (TimeSinceLastShot > RefireRate)
	{
		// fire the weapon right away
		Fire();

	} else {

		// if we're full auto, schedule the next shot
		if (bFullAuto)
		{
			const float RemainingCooldown = FMath::Max(0.0f, RefireRate - TimeSinceLastShot);
			GetWorld()->GetTimerManager().SetTimer(RefireTimer, this, &AShooterWeapon::Fire, RemainingCooldown, false);
		}

	}
}

void AShooterWeapon::StopFiring()
{
	// lower the firing flag
	bIsFiring = false;

	// clear the refire timer
	GetWorld()->GetTimerManager().ClearTimer(RefireTimer);
}

void AShooterWeapon::Fire()
{
	if (!HasAuthority())
	{
		return;
	}

	// ensure the player still wants to fire. They may have let go of the trigger
	if (!bIsFiring)
	{
		return;
	}
	
	if (!WeaponOwner || !PawnOwner)
	{
		return;
	}

	if (!CanFire())
	{
		StartReload();
		return;
	}

	// fire a projectile at the target
	FireProjectile(WeaponOwner->GetWeaponTargetLocation());

	// update the time of our last shot
	TimeOfLastShot = GetWorld()->GetTimeSeconds();

	// make noise so the AI perception system can hear us
	MakeNoise(ShotLoudness, PawnOwner, PawnOwner->GetActorLocation(), ShotNoiseRange, ShotNoiseTag);

	if (!bIsFiring || bIsReloading)
	{
		return;
	}

	// are we full auto?
	if (bFullAuto)
	{
		// schedule the next shot
		GetWorld()->GetTimerManager().SetTimer(RefireTimer, this, &AShooterWeapon::Fire, RefireRate, false);
	} else {

		// for semi-auto weapons, schedule the cooldown notification
		GetWorld()->GetTimerManager().SetTimer(RefireTimer, this, &AShooterWeapon::FireCooldownExpired, RefireRate, false);

	}
}

void AShooterWeapon::FireCooldownExpired()
{
	// notify the owner
	if (WeaponOwner)
	{
		WeaponOwner->OnSemiWeaponRefire();
	}
}

void AShooterWeapon::FireProjectile(const FVector& TargetLocation)
{
	if (!HasAuthority())
	{
		return;
	}

	// get the projectile transform
	FTransform ProjectileTransform = CalculateProjectileSpawnTransform(TargetLocation);
	
	// spawn the projectile
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnParams.TransformScaleMethod = ESpawnActorScaleMethod::OverrideRootScale;
	SpawnParams.Owner = GetOwner();
	SpawnParams.Instigator = PawnOwner;

	GetWorld()->SpawnActor<AShooterProjectile>(ProjectileClass, ProjectileTransform, SpawnParams);

	// play the firing montage
	if (WeaponOwner)
	{
		WeaponOwner->PlayFiringMontage(FiringMontage);
	}

	// add recoil
	if (WeaponOwner)
	{
		WeaponOwner->AddWeaponRecoil(FiringRecoil);
	}

	// consume bullets
	--CurrentBullets;

	// update the weapon HUD
	if (WeaponOwner)
	{
		WeaponOwner->UpdateWeaponHUD(CurrentBullets, MagazineSize);
	}

	if (CurrentBullets <= 0)
	{
		StartReload();
	}
}

void AShooterWeapon::StartReload()
{
	if (!HasAuthority() || bIsReloading || CurrentBullets >= MagazineSize || ReserveBullets <= 0)
	{
		return;
	}

	StopFiring();

	bIsReloading = true;
	ReloadStartTime = GetWorld()->GetTimeSeconds();
	SetActorTickEnabled(true);

	if (WeaponOwner)
	{
		WeaponOwner->PlayReloadAnimation(ReloadMontage, ReloadAnimation);
	}

	GetWorld()->GetTimerManager().SetTimer(ReloadTimer, this, &AShooterWeapon::FinishReload, ReloadDuration, false);
}

void AShooterWeapon::FinishReload()
{
	if (!HasAuthority())
	{
		return;
	}

	const int32 MissingBullets = FMath::Max(0, MagazineSize - CurrentBullets);
	const int32 BulletsToLoad = FMath::Min(MissingBullets, ReserveBullets);

	CurrentBullets += BulletsToLoad;
	ReserveBullets -= BulletsToLoad;
	bIsReloading = false;
	ResetReloadVisuals();
	SetActorTickEnabled(false);

	if (WeaponOwner)
	{
		WeaponOwner->UpdateWeaponHUD(CurrentBullets, MagazineSize);
	}
}

bool AShooterWeapon::CanFire() const
{
	return CurrentBullets > 0 && !bIsReloading;
}

void AShooterWeapon::RefillAmmo()
{
	if (!HasAuthority())
	{
		return;
	}

	// restore magazine and reserve to their starting amounts
	CurrentBullets = MagazineSize;
	ReserveBullets = FMath::Max(ReserveBullets, InitialReserveBullets);

	// update the weapon HUD
	if (WeaponOwner)
	{
		WeaponOwner->UpdateWeaponHUD(CurrentBullets, MagazineSize);
	}
}

void AShooterWeapon::StoreMeshDefaults()
{
	if (bHasStoredMeshDefaults)
	{
		return;
	}

	if (FirstPersonMesh)
	{
		FirstPersonMeshDefaultTransform = FirstPersonMesh->GetRelativeTransform();
	}

	if (ThirdPersonMesh)
	{
		ThirdPersonMeshDefaultTransform = ThirdPersonMesh->GetRelativeTransform();
	}

	bHasStoredMeshDefaults = true;
}

void AShooterWeapon::UpdateReloadVisuals()
{
	if (!bIsReloading || !GetWorld())
	{
		return;
	}

	StoreMeshDefaults();

	const float Progress = GetReloadProgress();
	const float Dip = FMath::Sin(Progress * UE_PI);
	const float Snap = FMath::Sin(FMath::Clamp(Progress * 2.4f, 0.0f, 1.0f) * UE_PI);

	if (FirstPersonMesh)
	{
		const FVector Offset(-9.0f * Dip, 4.0f * Dip, -13.0f * Dip);
		const FRotator Rotate(-15.0f * Dip, 7.0f * Dip, -12.0f * Snap);
		const FRotator TargetRotation = (FirstPersonMeshDefaultTransform.GetRotation().Rotator() + Rotate).GetNormalized();
		FirstPersonMesh->SetRelativeLocationAndRotation(FirstPersonMeshDefaultTransform.GetLocation() + Offset, TargetRotation);
	}

	if (ThirdPersonMesh)
	{
		const FVector Offset(-3.0f * Dip, 0.0f, -4.0f * Dip);
		const FRotator Rotate(-8.0f * Dip, 0.0f, -8.0f * Snap);
		const FRotator TargetRotation = (ThirdPersonMeshDefaultTransform.GetRotation().Rotator() + Rotate).GetNormalized();
		ThirdPersonMesh->SetRelativeLocationAndRotation(ThirdPersonMeshDefaultTransform.GetLocation() + Offset, TargetRotation);
	}
}

void AShooterWeapon::ResetReloadVisuals()
{
	if (!bHasStoredMeshDefaults)
	{
		return;
	}

	if (FirstPersonMesh)
	{
		FirstPersonMesh->SetRelativeTransform(FirstPersonMeshDefaultTransform);
	}

	if (ThirdPersonMesh)
	{
		ThirdPersonMesh->SetRelativeTransform(ThirdPersonMeshDefaultTransform);
	}
}

FTransform AShooterWeapon::CalculateProjectileSpawnTransform(const FVector& TargetLocation) const
{
	// find the muzzle location
	const FVector MuzzleLoc = FirstPersonMesh->GetSocketLocation(MuzzleSocketName);

	// aim from a stable source behind the muzzle (the shooter's view point) so point-blank shots,
	// where the muzzle has already passed the target, don't fire backwards or sideways
	const FVector AimSource = PawnOwner ? PawnOwner->GetPawnViewLocation() : MuzzleLoc;
	const FVector VariedTarget = TargetLocation + (UKismetMathLibrary::RandomUnitVector() * AimVariance);
	FVector AimDir = (VariedTarget - AimSource).GetSafeNormal();
	if (AimDir.IsNearlyZero())
	{
		AimDir = PawnOwner ? PawnOwner->GetBaseAimRotation().Vector() : FirstPersonMesh->GetForwardVector();
	}

	// spawn just ahead of the muzzle, oriented along the view-to-target direction
	const FVector SpawnLoc = MuzzleLoc + AimDir * MuzzleOffset;
	const FRotator AimRot = AimDir.Rotation();

	// return the built transform
	return FTransform(AimRot, SpawnLoc, FVector::OneVector);
}

const TSubclassOf<UAnimInstance>& AShooterWeapon::GetFirstPersonAnimInstanceClass() const
{
	return FirstPersonAnimInstanceClass;
}

const TSubclassOf<UAnimInstance>& AShooterWeapon::GetThirdPersonAnimInstanceClass() const
{
	return ThirdPersonAnimInstanceClass;
}

float AShooterWeapon::GetReloadProgress() const
{
	if (!bIsReloading || ReloadDuration <= 0.0f || !GetWorld())
	{
		return bIsReloading ? 0.0f : 1.0f;
	}

	return FMath::Clamp((GetWorld()->GetTimeSeconds() - ReloadStartTime) / ReloadDuration, 0.0f, 1.0f);
}
