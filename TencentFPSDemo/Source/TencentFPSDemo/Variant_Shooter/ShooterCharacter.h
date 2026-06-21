// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "TencentFPSDemoCharacter.h"
#include "ShooterWeaponHolder.h"
#include "ShooterCharacter.generated.h"

class AShooterWeapon;
class UInputAction;
class UInputComponent;
class UPawnNoiseEmitterComponent;
class UAnimSequenceBase;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FBulletCountUpdatedDelegate, int32, MagazineSize, int32, Bullets);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDamagedDelegate, float, LifePercent);

/**
 *  A player controllable first person shooter character
 *  Manages a weapon inventory through the IShooterWeaponHolder interface
 *  Manages health and death
 */
UCLASS(abstract)
class TENCENTFPSDEMO_API AShooterCharacter : public ATencentFPSDemoCharacter, public IShooterWeaponHolder
{
	GENERATED_BODY()
	
	/** AI Noise emitter component */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	UPawnNoiseEmitterComponent* PawnNoiseEmitter;

protected:

	/** Fire weapon input action */
	UPROPERTY(EditAnywhere, Category ="Input")
	UInputAction* FireAction;

	/** Switch weapon input action */
	UPROPERTY(EditAnywhere, Category ="Input")
	UInputAction* SwitchWeaponAction;

	/** Name of the first person mesh weapon socket */
	UPROPERTY(EditAnywhere, Category ="Weapons")
	FName FirstPersonWeaponSocket = FName("HandGrip_R");

	/** Name of the third person mesh weapon socket */
	UPROPERTY(EditAnywhere, Category ="Weapons")
	FName ThirdPersonWeaponSocket = FName("HandGrip_R");

	/** Max distance to use for aim traces */
	UPROPERTY(EditAnywhere, Category ="Aim", meta = (ClampMin = 0, ClampMax = 100000, Units = "cm"))
	float MaxAimDistance = 10000.0f;

	/** Max HP this character can have (Valorant-style 100 HP + 50 shield) */
	UPROPERTY(EditAnywhere, Category="Health")
	float MaxHP = 150.0f;

	/** Current HP remaining to this character */
	UPROPERTY(ReplicatedUsing=OnRep_CurrentHP)
	float CurrentHP = 0.0f;

	/** Team ID for this character*/
	UPROPERTY(EditAnywhere, Category="Team")
	uint8 TeamByte = 0;

	/** Actor tag to grant this character when it dies */
	UPROPERTY(EditAnywhere, Category="Team")
	FName DeathTag = FName("Dead");

	/** List of weapons picked up by the character */
	UPROPERTY(Replicated)
	TArray<AShooterWeapon*> OwnedWeapons;

	/** Weapon currently equipped and ready to shoot with */
	UPROPERTY(ReplicatedUsing=OnRep_CurrentWeapon)
	TObjectPtr<AShooterWeapon> CurrentWeapon;

	UPROPERTY(EditAnywhere, Category ="Destruction", meta = (ClampMin = 0, ClampMax = 10, Units = "s"))
	float RespawnTime = 5.0f;

	FTimerHandle RespawnTimer;

	/** Collision profile applied to the body mesh while ragdolling on death */
	UPROPERTY(EditAnywhere, Category="Destruction")
	FName RagdollCollisionProfile = FName("Ragdoll");

	/** True once the death ragdoll has been applied, to avoid doing it twice */
	bool bDeathVisualsApplied = false;

	/** Seconds of damage immunity granted right after (re)spawning */
	UPROPERTY(EditAnywhere, Category="Health", meta = (ClampMin = 0, ClampMax = 10, Units = "s"))
	float SpawnProtectionTime = 2.0f;

	/** True while the character is protected from damage after spawning */
	UPROPERTY(Replicated)
	bool bSpawnProtected = false;

	FTimerHandle SpawnProtectionTimer;

	/** Upward view kick per shot, in degrees */
	UPROPERTY(EditAnywhere, Category="Recoil", meta = (ClampMin = 0, ClampMax = 10))
	float RecoilPitchPerShot = 0.65f;

	/** Random horizontal view kick per shot, in degrees */
	UPROPERTY(EditAnywhere, Category="Recoil", meta = (ClampMin = 0, ClampMax = 5))
	float RecoilYawPerShot = 0.22f;

	/** How quickly the view recovers back down after recoil */
	UPROPERTY(EditAnywhere, Category="Recoil", meta = (ClampMin = 1, ClampMax = 30))
	float RecoilRecoverSpeed = 11.0f;

	/** Accumulated recoil still to be recovered (degrees) */
	float PendingRecoilPitch = 0.0f;
	float PendingRecoilYaw = 0.0f;

	/** Client's precise aim point, streamed to the server to bypass low-precision replicated view pitch */
	FVector ServerAimTarget = FVector::ZeroVector;
	bool bHasServerAimTarget = false;

	/** Throttle accumulator for streaming the aim point to the server */
	float AimStreamAccumulator = 0.0f;

public:

	/** Bullet count updated delegate */
	FBulletCountUpdatedDelegate OnBulletCountUpdated;

	/** Damaged delegate */
	FDamagedDelegate OnDamaged;

public:

	/** Constructor */
	AShooterCharacter();

protected:

	/** Gameplay initialization */
	virtual void BeginPlay() override;

	/** Per-frame update, used to recover the view after recoil */
	virtual void Tick(float DeltaSeconds) override;

	/** Gameplay cleanup */
	virtual void EndPlay(EEndPlayReason::Type EndPlayReason) override;

	/** Set up input action bindings */
	virtual void SetupPlayerInputComponent(UInputComponent* InputComponent) override;

public:

	/** Replicates combat state */
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Handle incoming damage */
	virtual float TakeDamage(float Damage, struct FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;

public:

	/** Handles aim inputs from either controls or UI interfaces */
	virtual void DoAim(float Yaw, float Pitch) override;

	/** Handles move inputs from either controls or UI interfaces */
	virtual void DoMove(float Right, float Forward)  override;

	/** Handles jump start inputs from either controls or UI interfaces */
	virtual void DoJumpStart()  override;

	/** Handles jump end inputs from either controls or UI interfaces */
	virtual void DoJumpEnd()  override;

	/** Handles start firing input */
	UFUNCTION(BlueprintCallable, Category="Input")
	void DoStartFiring();

	/** Handles stop firing input */
	UFUNCTION(BlueprintCallable, Category="Input")
	void DoStopFiring();

	/** Handles switch weapon input */
	UFUNCTION(BlueprintCallable, Category="Input")
	void DoSwitchWeapon();

	/** Handles reload input */
	UFUNCTION(BlueprintCallable, Category="Input")
	void DoReload();

	/** Returns the current health percentage */
	UFUNCTION(BlueprintPure, Category="Health")
	float GetHealthPercent() const;

public:

	//~Begin IShooterWeaponHolder interface

	/** Attaches a weapon's meshes to the owner */
	virtual void AttachWeaponMeshes(AShooterWeapon* Weapon) override;

	/** Plays the firing montage for the weapon */
	virtual void PlayFiringMontage(UAnimMontage* Montage) override;

	/** Plays the reload animation for the weapon */
	virtual void PlayReloadAnimation(UAnimMontage* Montage, UAnimSequenceBase* Animation) override;

	/** Applies weapon recoil to the owner */
	virtual void AddWeaponRecoil(float Recoil) override;

	/** Updates the weapon's HUD with the current ammo count */
	virtual void UpdateWeaponHUD(int32 CurrentAmmo, int32 MagazineSize) override;

	/** Calculates and returns the aim location for the weapon */
	virtual FVector GetWeaponTargetLocation() override;

	/** Gives a weapon of this class to the owner */
	virtual void AddWeaponClass(const TSubclassOf<AShooterWeapon>& WeaponClass) override;

	/** Activates the passed weapon */
	virtual void OnWeaponActivated(AShooterWeapon* Weapon) override;

	/** Deactivates the passed weapon */
	virtual void OnWeaponDeactivated(AShooterWeapon* Weapon) override;

	/** Notifies the owner that the weapon cooldown has expired and it's ready to shoot again */
	virtual void OnSemiWeaponRefire() override;

	//~End IShooterWeaponHolder interface

protected:

	/** Returns true if the character already owns a weapon of the given class */
	AShooterWeapon* FindWeaponOfType(TSubclassOf<AShooterWeapon> WeaponClass) const;

	/** Called when this character's HP is depleted */
	void Die();

	/** Ragdolls the body so the death reads correctly on every client */
	void ApplyDeathVisuals();

	/** Clears post-spawn damage immunity */
	void EndSpawnProtection();

	UFUNCTION()
	void OnRep_CurrentHP();

	UFUNCTION()
	void OnRep_CurrentWeapon();

	UFUNCTION(Server, Reliable)
	void ServerStartFiring();

	UFUNCTION(Server, Reliable)
	void ServerStopFiring();

	UFUNCTION(Server, Reliable)
	void ServerSwitchWeapon();

	UFUNCTION(Server, Reliable)
	void ServerReload();

	UFUNCTION(Server, Reliable)
	void ServerAddWeaponClass(TSubclassOf<AShooterWeapon> WeaponClass);

	/** Applies a per-shot view-kick on the owning client (called from the server when the weapon fires) */
	UFUNCTION(Client, Unreliable)
	void ClientAddRecoil();

	/** Client streams its exact aim point to the server so server-spawned shots match the crosshair */
	UFUNCTION(Server, Unreliable)
	void ServerUpdateAimTarget(FVector AimTarget);

	/** Called to allow Blueprint code to react to this character's death */
	UFUNCTION(BlueprintImplementableEvent, Category="Shooter", meta = (DisplayName = "On Death"))
	void BP_OnDeath();

	/** Called from the respawn timer to destroy this character and force the PC to respawn */
	void OnRespawn();

public:

	/** Returns true if the character is dead */
	bool IsDead() const;

	/** Returns true while the character has post-spawn damage immunity */
	bool IsSpawnProtected() const { return bSpawnProtected; }

	/** Returns the currently equipped weapon */
	AShooterWeapon* GetCurrentWeapon() const { return CurrentWeapon; }

	/** Returns the player team. Used to filter friendly fire. */
	virtual uint8 GetTeamByte() const override { return TeamByte; }
};
