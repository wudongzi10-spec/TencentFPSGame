// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ShooterWeaponHolder.h"
#include "Animation/AnimInstance.h"
#include "ShooterWeapon.generated.h"

class IShooterWeaponHolder;
class AShooterProjectile;
class USkeletalMeshComponent;
class UAnimMontage;
class UAnimInstance;
class UAnimSequenceBase;

/**
 *  Base class for a simple first person shooter weapon
 *  Provides both first person and third person perspective meshes
 *  Handles ammo and firing logic
 *  Interacts with the weapon owner through the ShooterWeaponHolder interface
 */
UCLASS(abstract)
class TENCENTFPSDEMO_API AShooterWeapon : public AActor
{
	GENERATED_BODY()
	
	/** First person perspective mesh */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	USkeletalMeshComponent* FirstPersonMesh;

	/** Third person perspective mesh */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	USkeletalMeshComponent* ThirdPersonMesh;

protected:

	/** Cast pointer to the weapon owner */
	IShooterWeaponHolder* WeaponOwner;

	/** Type of projectiles this weapon will shoot */
	UPROPERTY(EditAnywhere, Category="Ammo")
	TSubclassOf<AShooterProjectile> ProjectileClass;

	/** Number of bullets in a magazine */
	UPROPERTY(EditAnywhere, Category="Ammo", meta = (ClampMin = 0, ClampMax = 100))
	int32 MagazineSize = 10;

	/** Number of bullets in the current magazine */
	UPROPERTY(ReplicatedUsing=OnRep_CurrentBullets)
	int32 CurrentBullets = 0;

	/** Spare bullets carried for manual reloads */
	UPROPERTY(EditAnywhere, ReplicatedUsing=OnRep_CurrentBullets, Category="Ammo", meta = (ClampMin = 0, ClampMax = 999))
	int32 ReserveBullets = 90;

	/** Reserve ammo this weapon started with, used to restore ammo on re-pickup */
	int32 InitialReserveBullets = 90;

	/** True while this weapon is going through a reload */
	UPROPERTY(ReplicatedUsing=OnRep_Reloading)
	bool bIsReloading = false;
	
	/** Animation montage to play when firing this weapon */
	UPROPERTY(EditAnywhere, Category="Animation")
	UAnimMontage* FiringMontage;

	/** Animation montage to play when reloading this weapon */
	UPROPERTY(EditAnywhere, Category="Animation")
	UAnimMontage* ReloadMontage;

	/** Fallback animation sequence to play as a dynamic montage when no reload montage is set */
	UPROPERTY(EditAnywhere, Category="Animation")
	UAnimSequenceBase* ReloadAnimation;

	/** Animation slot the reload sequence plays on. Must match a slot routed in the arms AnimBlueprint. */
	UPROPERTY(EditAnywhere, Category="Animation")
	FName ReloadSlotName = FName("DefaultSlot");

	/** Time needed to complete a reload */
	UPROPERTY(EditAnywhere, Category="Ammo", meta = (ClampMin = 0.1, ClampMax = 10, Units = "s"))
	float ReloadDuration = 1.65f;

	/** AnimInstance class to set for the first person character mesh when this weapon is active */
	UPROPERTY(EditAnywhere, Category="Animation")
	TSubclassOf<UAnimInstance> FirstPersonAnimInstanceClass;

	/** AnimInstance class to set for the third person character mesh when this weapon is active */
	UPROPERTY(EditAnywhere, Category="Animation")
	TSubclassOf<UAnimInstance> ThirdPersonAnimInstanceClass;

	/** Cone half-angle for variance while aiming */
	UPROPERTY(EditAnywhere, Category="Aim", meta = (ClampMin = 0, ClampMax = 90, Units = "Degrees"))
	float AimVariance = 0.0f;

	/** Amount of firing recoil to apply to the owner */
	UPROPERTY(EditAnywhere, Category="Aim", meta = (ClampMin = 0, ClampMax = 100))
	float FiringRecoil = 0.0f;

	/** Name of the first person muzzle socket where projectiles will spawn */
	UPROPERTY(EditAnywhere, Category="Aim")
	FName MuzzleSocketName;

	/** Distance ahead of the muzzle that bullets will spawn at */
	UPROPERTY(EditAnywhere, Category="Aim", meta = (ClampMin = 0, ClampMax = 1000, Units = "cm"))
	float MuzzleOffset = 10.0f;

	/** If true, this weapon will automatically fire at the refire rate */
	UPROPERTY(EditAnywhere, Category="Refire")
	bool bFullAuto = false;

	/** Time between shots for this weapon. Affects both full auto and semi auto modes */
	UPROPERTY(EditAnywhere, Category="Refire", meta = (ClampMin = 0, ClampMax = 5, Units = "s"))
	float RefireRate = 0.5f;

	/** Game time of last shot fired, used to enforce refire rate on semi auto */
	float TimeOfLastShot = 0.0f;

	/** If true, the weapon is currently firing */
	bool bIsFiring = false;

	/** Timer to handle full auto refiring */
	FTimerHandle RefireTimer;

	/** Timer to finish an active reload */
	FTimerHandle ReloadTimer;

	/** Server game time when the current reload started */
	float ReloadStartTime = 0.0f;

	/** First-person mesh transform before reload offsets are applied */
	FTransform FirstPersonMeshDefaultTransform;

	/** Third-person mesh transform before reload offsets are applied */
	FTransform ThirdPersonMeshDefaultTransform;

	/** True once the default mesh transforms have been captured after attachment */
	bool bHasStoredMeshDefaults = false;

	/** Cast pawn pointer to the owner for AI perception system interactions */
	TObjectPtr<APawn> PawnOwner;

	/** Loudness of the shot for AI perception system interactions */
	UPROPERTY(EditAnywhere, Category="Perception", meta = (ClampMin = 0, ClampMax = 100))
	float ShotLoudness = 1.0f;

	/** Max range of shot AI perception noise */
	UPROPERTY(EditAnywhere, Category="Perception", meta = (ClampMin = 0, ClampMax = 100000, Units = "cm"))
	float ShotNoiseRange = 3000.0f;

	/** Tag to apply to noise generated by shooting this weapon */
	UPROPERTY(EditAnywhere, Category="Perception")
	FName ShotNoiseTag = FName("Shot");

public:	

	/** Constructor */
	AShooterWeapon();

	virtual void Tick(float DeltaSeconds) override;

protected:
	
	/** Gameplay initialization */
	virtual void BeginPlay() override;

	/** Gameplay Cleanup */
	virtual void EndPlay(EEndPlayReason::Type EndPlayReason) override;

	/** Replicates ammo state */
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:

	/** Called when the weapon's owner is destroyed */
	UFUNCTION()
	void OnOwnerDestroyed(AActor* DestroyedActor);

	UFUNCTION()
	void OnRep_CurrentBullets();

	UFUNCTION()
	void OnRep_Reloading();

public:

	/** Activates this weapon and gets it ready to fire */
	void ActivateWeapon();

	/** Deactivates this weapon */
	void DeactivateWeapon();

	/** Start firing this weapon */
	void StartFiring();

	/** Stop firing this weapon */
	void StopFiring();

	/** Starts a manual reload if the weapon needs one */
	void StartReload();

	/** Completes the active reload */
	void FinishReload();

	/** Refills the magazine and reserve ammo back to their starting amounts (used on weapon re-pickup) */
	void RefillAmmo();

protected:

	/** Fire the weapon */
	virtual void Fire();

	/** Returns true when the weapon has ammo in the magazine */
	bool CanFire() const;

	/** Stores the attached weapon mesh transforms used by procedural reload feedback */
	void StoreMeshDefaults();

	/** Applies a subtle weapon dip/tilt while reloading so reload is visible even without a montage */
	void UpdateReloadVisuals();

	/** Restores weapon meshes after reload feedback finishes */
	void ResetReloadVisuals();

	/** Called when the refire rate time has passed while shooting semi auto weapons */
	void FireCooldownExpired();

	/** Fire a projectile towards the target location */
	virtual void FireProjectile(const FVector& TargetLocation);

	/** Calculates the spawn transform for projectiles shot by this weapon */
	FTransform CalculateProjectileSpawnTransform(const FVector& TargetLocation) const;

public:

	/** Returns the first person mesh */
	UFUNCTION(BlueprintPure, Category="Weapon")
	USkeletalMeshComponent* GetFirstPersonMesh() const { return FirstPersonMesh; };

	/** Returns the third person mesh */
	UFUNCTION(BlueprintPure, Category="Weapon")
	USkeletalMeshComponent* GetThirdPersonMesh() const { return ThirdPersonMesh; };

	/** Returns the first person anim instance class */
	const TSubclassOf<UAnimInstance>& GetFirstPersonAnimInstanceClass() const;

	/** Returns the third person anim instance class */
	const TSubclassOf<UAnimInstance>& GetThirdPersonAnimInstanceClass() const;

	/** Returns the magazine size */
	int32 GetMagazineSize() const { return MagazineSize; };

	/** Returns the current bullet count */
	int32 GetBulletCount() const { return CurrentBullets; }

	/** Returns the carried reserve ammo */
	int32 GetReserveBulletCount() const { return ReserveBullets; }

	/** Returns true while a reload is active */
	bool IsReloading() const { return bIsReloading; }

	/** Returns the animation slot the reload sequence plays on */
	FName GetReloadSlotName() const { return ReloadSlotName; }

	/** Returns reload progress in the range [0,1] */
	float GetReloadProgress() const;
};
