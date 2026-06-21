// Copyright Epic Games, Inc. All Rights Reserved.


#include "ShooterCharacter.h"
#include "ShooterWeapon.h"
#include "EnhancedInputComponent.h"
#include "Components/InputComponent.h"
#include "Components/PawnNoiseEmitterComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimSequenceBase.h"
#include "Engine/World.h"
#include "Camera/CameraComponent.h"
#include "TimerManager.h"
#include "ShooterGameMode.h"
#include "Variant_Shooter/ShooterPlayerController.h"
#include "Variant_Shooter/ShooterPlayerState.h"
#include "Components/CapsuleComponent.h"
#include "Net/UnrealNetwork.h"
#include "InputCoreTypes.h"

AShooterCharacter::AShooterCharacter()
{
	bReplicates = true;
	SetReplicateMovement(true);

	// tick is needed to smoothly recover the view after recoil
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	// create the noise emitter component
	PawnNoiseEmitter = CreateDefaultSubobject<UPawnNoiseEmitterComponent>(TEXT("Pawn Noise Emitter"));

	// configure movement
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 600.0f, 0.0f);
}

void AShooterCharacter::BeginPlay()
{
	Super::BeginPlay();

	// reset HP to max
	if (HasAuthority())
	{
		CurrentHP = MaxHP;

		// grant brief damage immunity so the player isn't instantly re-killed on spawn
		if (SpawnProtectionTime > 0.0f)
		{
			bSpawnProtected = true;
			GetWorld()->GetTimerManager().SetTimer(SpawnProtectionTimer, this, &AShooterCharacter::EndSpawnProtection, SpawnProtectionTime, false);
		}
	}
	else if (CurrentHP <= 0.0f)
	{
		CurrentHP = MaxHP;
	}

	// update the HUD
	OnDamaged.Broadcast(GetHealthPercent());
}

void AShooterCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// a remote client streams its precise aim point to the server (~30Hz) so server-spawned
	// shots match the crosshair instead of the engine's low-precision replicated view pitch
	if (IsLocallyControlled() && !HasAuthority() && CurrentWeapon && !IsDead())
	{
		AimStreamAccumulator += DeltaSeconds;
		if (AimStreamAccumulator >= 0.033f)
		{
			AimStreamAccumulator = 0.0f;
			ServerUpdateAimTarget(GetWeaponTargetLocation());
		}
	}

	// smoothly pull the view back down to recover from recoil, on the locally controlled client
	if (!IsLocallyControlled() || (PendingRecoilPitch == 0.0f && PendingRecoilYaw == 0.0f))
	{
		return;
	}

	AController* C = GetController();
	if (!C)
	{
		return;
	}

	const float NewPitch = FMath::FInterpTo(PendingRecoilPitch, 0.0f, DeltaSeconds, RecoilRecoverSpeed);
	const float NewYaw = FMath::FInterpTo(PendingRecoilYaw, 0.0f, DeltaSeconds, RecoilRecoverSpeed);

	FRotator ControlRot = C->GetControlRotation();
	ControlRot.Pitch -= (PendingRecoilPitch - NewPitch);
	ControlRot.Yaw -= (PendingRecoilYaw - NewYaw);
	C->SetControlRotation(ControlRot);

	PendingRecoilPitch = NewPitch;
	PendingRecoilYaw = NewYaw;
}

void AShooterCharacter::EndPlay(EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);

	// clear the respawn timer
	GetWorld()->GetTimerManager().ClearTimer(RespawnTimer);
}

void AShooterCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	// base class handles move, aim and jump inputs
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	// Set up action bindings
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		// Firing
		EnhancedInputComponent->BindAction(FireAction, ETriggerEvent::Started, this, &AShooterCharacter::DoStartFiring);
		EnhancedInputComponent->BindAction(FireAction, ETriggerEvent::Completed, this, &AShooterCharacter::DoStopFiring);

		// Switch weapon
		EnhancedInputComponent->BindAction(SwitchWeaponAction, ETriggerEvent::Triggered, this, &AShooterCharacter::DoSwitchWeapon);
	}

	PlayerInputComponent->BindKey(EKeys::R, IE_Pressed, this, &AShooterCharacter::DoReload);

	// bind Q directly so weapon switching works even if the input mapping context doesn't map it
	PlayerInputComponent->BindKey(EKeys::Q, IE_Pressed, this, &AShooterCharacter::DoSwitchWeapon);
}

void AShooterCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AShooterCharacter, CurrentHP);
	DOREPLIFETIME(AShooterCharacter, OwnedWeapons);
	DOREPLIFETIME(AShooterCharacter, CurrentWeapon);
	DOREPLIFETIME(AShooterCharacter, bSpawnProtected);
}

float AShooterCharacter::TakeDamage(float Damage, struct FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	if (!HasAuthority())
	{
		return 0.0f;
	}

	// ignore if already dead
	if (CurrentHP <= 0.0f)
	{
		return 0.0f;
	}

	// ignore damage while protected right after spawning
	if (bSpawnProtected)
	{
		return 0.0f;
	}

	// Reduce HP
	CurrentHP = FMath::Clamp(CurrentHP - Damage, 0.0f, MaxHP);

	// tell our own controller where the damage came from so the HUD can show a directional indicator
	if (AShooterPlayerController* ShooterPC = Cast<AShooterPlayerController>(GetController()))
	{
		const AActor* Source = DamageCauser ? DamageCauser : (EventInstigator ? EventInstigator->GetPawn() : nullptr);
		if (Source)
		{
			ShooterPC->ClientTookDamage(Source->GetActorLocation());
		}
	}

	// Have we depleted HP?
	if (CurrentHP <= 0.0f)
	{
		Die();
	}

	// update the HUD
	OnDamaged.Broadcast(GetHealthPercent());

	return Damage;
}

void AShooterCharacter::DoAim(float Yaw, float Pitch)
{
	// only route inputs if the character is not dead
	if (!IsDead())
	{
		Super::DoAim(Yaw, Pitch);
	}
}

void AShooterCharacter::DoMove(float Right, float Forward)
{
	// only route inputs if the character is not dead
	if (!IsDead())
	{
		Super::DoMove(Right, Forward);
	}
}

void AShooterCharacter::DoJumpStart()
{
	// only route inputs if the character is not dead
	if (!IsDead())
	{
		Super::DoJumpStart();
	}
}

void AShooterCharacter::DoJumpEnd()
{
	// only route inputs if the character is not dead
	if (!IsDead())
	{
		Super::DoJumpEnd();
	}
}

void AShooterCharacter::DoStartFiring()
{
	// fire the current weapon
	if (CurrentWeapon && !IsDead())
	{
		if (HasAuthority())
		{
			CurrentWeapon->StartFiring();
		}
		else
		{
			ServerStartFiring();
		}
	}
}

void AShooterCharacter::DoStopFiring()
{
	// stop firing the current weapon
	if (CurrentWeapon && !IsDead())
	{
		if (HasAuthority())
		{
			CurrentWeapon->StopFiring();
		}
		else
		{
			ServerStopFiring();
		}
	}
}

void AShooterCharacter::DoSwitchWeapon()
{
	if (!HasAuthority())
	{
		ServerSwitchWeapon();
		return;
	}

	// ensure we have at least two weapons two switch between
	if (OwnedWeapons.Num() > 1 && !IsDead())
	{
		// deactivate the old weapon
		CurrentWeapon->DeactivateWeapon();

		// find the index of the current weapon in the owned list
		int32 WeaponIndex = OwnedWeapons.Find(CurrentWeapon);

		// is this the last weapon?
		if (WeaponIndex == OwnedWeapons.Num() - 1)
		{
			// loop back to the beginning of the array
			WeaponIndex = 0;
		}
		else {
			// select the next weapon index
			++WeaponIndex;
		}

		// set the new weapon as current
		CurrentWeapon = OwnedWeapons[WeaponIndex];

		// activate the new weapon
		CurrentWeapon->ActivateWeapon();
	}
}

void AShooterCharacter::DoReload()
{
	if (CurrentWeapon && !IsDead())
	{
		if (HasAuthority())
		{
			CurrentWeapon->StartReload();
		}
		else
		{
			ServerReload();
		}
	}
}

float AShooterCharacter::GetHealthPercent() const
{
	return MaxHP > 0.0f ? FMath::Clamp(CurrentHP / MaxHP, 0.0f, 1.0f) : 0.0f;
}

void AShooterCharacter::AttachWeaponMeshes(AShooterWeapon* Weapon)
{
	const FAttachmentTransformRules AttachmentRule(EAttachmentRule::SnapToTarget, false);

	// attach the weapon actor
	Weapon->AttachToActor(this, AttachmentRule);

	// attach the weapon meshes
	Weapon->GetFirstPersonMesh()->AttachToComponent(GetFirstPersonMesh(), AttachmentRule, FirstPersonWeaponSocket);
	Weapon->GetThirdPersonMesh()->AttachToComponent(GetMesh(), AttachmentRule, ThirdPersonWeaponSocket);
	
}

void AShooterCharacter::PlayFiringMontage(UAnimMontage* Montage)
{
	if (!Montage)
	{
		return;
	}

	if (UAnimInstance* FirstPersonAnim = GetFirstPersonMesh()->GetAnimInstance())
	{
		FirstPersonAnim->Montage_Play(Montage);
	}

	if (UAnimInstance* ThirdPersonAnim = GetMesh()->GetAnimInstance())
	{
		ThirdPersonAnim->Montage_Play(Montage);
	}
}

void AShooterCharacter::PlayReloadAnimation(UAnimMontage* Montage, UAnimSequenceBase* Animation)
{
	// play the reload sequence on the slot configured by the active weapon so it shows on the arms
	const FName ReloadSlot = CurrentWeapon ? CurrentWeapon->GetReloadSlotName() : FName("DefaultSlot");

	auto PlayOnAnimInstance = [Montage, Animation, ReloadSlot](UAnimInstance* AnimInstance)
	{
		if (!AnimInstance)
		{
			return;
		}

		if (Montage)
		{
			AnimInstance->Montage_Play(Montage);
			return;
		}

		if (Animation)
		{
			AnimInstance->PlaySlotAnimationAsDynamicMontage(Animation, ReloadSlot);
		}
	};

	PlayOnAnimInstance(GetFirstPersonMesh()->GetAnimInstance());
	PlayOnAnimInstance(GetMesh()->GetAnimInstance());
}

void AShooterCharacter::AddWeaponRecoil(float Recoil)
{
	// trigger the procedural view-kick on the owning client (firing is server-authoritative)
	ClientAddRecoil();
}

void AShooterCharacter::ClientAddRecoil_Implementation()
{
	if (!IsLocallyControlled() || IsDead())
	{
		return;
	}

	AController* C = GetController();
	if (!C)
	{
		return;
	}

	// kick the view up (and a touch sideways), then Tick recovers it back down
	const float KickPitch = RecoilPitchPerShot;
	const float KickYaw = FMath::RandRange(-RecoilYawPerShot, RecoilYawPerShot);

	FRotator ControlRot = C->GetControlRotation();
	ControlRot.Pitch += KickPitch;
	ControlRot.Yaw += KickYaw;
	C->SetControlRotation(ControlRot);

	PendingRecoilPitch += KickPitch;
	PendingRecoilYaw += KickYaw;
}

void AShooterCharacter::UpdateWeaponHUD(int32 CurrentAmmo, int32 MagazineSize)
{
	OnBulletCountUpdated.Broadcast(MagazineSize, CurrentAmmo);
}

FVector AShooterCharacter::GetWeaponTargetLocation()
{
	// on the server, a remote client's shots use the precise aim point it streamed to us,
	// avoiding the inaccuracy of the engine's compressed replicated view pitch
	if (HasAuthority() && !IsLocallyControlled() && bHasServerAimTarget)
	{
		return ServerAimTarget;
	}

	// trace ahead from the camera viewpoint
	FHitResult OutHit;

	const FVector Start = GetFirstPersonCameraComponent()->GetComponentLocation();
	const FVector End = Start + (GetFirstPersonCameraComponent()->GetForwardVector() * MaxAimDistance);

	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(this);

	GetWorld()->LineTraceSingleByChannel(OutHit, Start, End, ECC_Visibility, QueryParams);

	// return either the impact point or the trace end
	return OutHit.bBlockingHit ? OutHit.ImpactPoint : OutHit.TraceEnd;
}

void AShooterCharacter::ServerUpdateAimTarget_Implementation(FVector AimTarget)
{
	ServerAimTarget = AimTarget;
	bHasServerAimTarget = true;
}

void AShooterCharacter::AddWeaponClass(const TSubclassOf<AShooterWeapon>& WeaponClass)
{
	if (!HasAuthority())
	{
		ServerAddWeaponClass(WeaponClass);
		return;
	}

	// do we already own this weapon?
	AShooterWeapon* OwnedWeapon = FindWeaponOfType(WeaponClass);

	if (OwnedWeapon)
	{
		// re-picking an owned weapon resupplies its ammo and equips it
		OwnedWeapon->RefillAmmo();

		if (CurrentWeapon != OwnedWeapon)
		{
			if (CurrentWeapon)
			{
				CurrentWeapon->DeactivateWeapon();
			}

			CurrentWeapon = OwnedWeapon;
			CurrentWeapon->ActivateWeapon();
		}

		return;
	}

	// spawn the new weapon
	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = this;
	SpawnParams.Instigator = this;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnParams.TransformScaleMethod = ESpawnActorScaleMethod::MultiplyWithRoot;

	AShooterWeapon* AddedWeapon = GetWorld()->SpawnActor<AShooterWeapon>(WeaponClass, GetActorTransform(), SpawnParams);

	if (AddedWeapon)
	{
		// add the weapon to the owned list
		OwnedWeapons.Add(AddedWeapon);

		// if we have an existing weapon, deactivate it
		if (CurrentWeapon)
		{
			CurrentWeapon->DeactivateWeapon();
		}

		// switch to the new weapon
		CurrentWeapon = AddedWeapon;
		CurrentWeapon->ActivateWeapon();
	}
}

void AShooterCharacter::OnWeaponActivated(AShooterWeapon* Weapon)
{
	// update the bullet counter
	OnBulletCountUpdated.Broadcast(Weapon->GetMagazineSize(), Weapon->GetBulletCount());

	// set the character mesh AnimInstances
	GetFirstPersonMesh()->SetAnimInstanceClass(Weapon->GetFirstPersonAnimInstanceClass());
	GetMesh()->SetAnimInstanceClass(Weapon->GetThirdPersonAnimInstanceClass());
}

void AShooterCharacter::OnWeaponDeactivated(AShooterWeapon* Weapon)
{
	// unused
}

void AShooterCharacter::OnSemiWeaponRefire()
{
	// unused
}

AShooterWeapon* AShooterCharacter::FindWeaponOfType(TSubclassOf<AShooterWeapon> WeaponClass) const
{
	// check each owned weapon
	for (AShooterWeapon* Weapon : OwnedWeapons)
	{
		if (Weapon->IsA(WeaponClass))
		{
			return Weapon;
		}
	}

	// weapon not found
	return nullptr;

}

void AShooterCharacter::Die()
{
	// deactivate the weapon
	if (IsValid(CurrentWeapon))
	{
		CurrentWeapon->DeactivateWeapon();
	}

	// increment the team score
	if (AShooterGameMode* GM = Cast<AShooterGameMode>(GetWorld()->GetAuthGameMode()))
	{
		GM->IncrementTeamScore(TeamByte);
	}

	// grant the death tag to the character
	Tags.Add(DeathTag);

	// record the death on the player's stats
	if (AShooterPlayerState* ShooterPS = GetPlayerState<AShooterPlayerState>())
	{
		ShooterPS->AddDeath();
	}

	// ragdoll on the server (clients ragdoll from OnRep_CurrentHP) so the body looks dead everywhere
	ApplyDeathVisuals();

	// disable controls
	DisableInput(nullptr);

	// reset the bullet counter UI
	OnBulletCountUpdated.Broadcast(0, 0);

	// call the BP handler
	BP_OnDeath();

	// schedule character respawn
	GetWorld()->GetTimerManager().SetTimer(RespawnTimer, this, &AShooterCharacter::OnRespawn, RespawnTime, false);
}

void AShooterCharacter::ApplyDeathVisuals()
{
	if (bDeathVisualsApplied)
	{
		return;
	}
	bDeathVisualsApplied = true;

	// stop movement and disable capsule collision
	GetCharacterMovement()->StopMovementImmediately();
	GetCharacterMovement()->DisableMovement();
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// ragdoll the third person body mesh (the one other players see)
	USkeletalMeshComponent* Body = GetMesh();
	Body->SetCollisionProfileName(RagdollCollisionProfile);
	Body->SetSimulatePhysics(true);
	Body->SetAllBodiesSimulatePhysics(true);
	Body->WakeAllRigidBodies();
	Body->SetPhysicsBlendWeight(1.0f);
}

void AShooterCharacter::EndSpawnProtection()
{
	bSpawnProtected = false;
}

void AShooterCharacter::OnRespawn()
{
	// destroy the character to force the PC to respawn
	Destroy();
}

bool AShooterCharacter::IsDead() const
{
	// the character is dead if their current HP drops to zero
	return CurrentHP <= 0.0f;
}

void AShooterCharacter::OnRep_CurrentHP()
{
	OnDamaged.Broadcast(GetHealthPercent());

	if (IsDead())
	{
		OnBulletCountUpdated.Broadcast(0, 0);

		// ragdoll on clients so the dead body doesn't appear standing
		ApplyDeathVisuals();
	}
}

void AShooterCharacter::OnRep_CurrentWeapon()
{
	if (CurrentWeapon)
	{
		OnWeaponActivated(CurrentWeapon);
	}
}

void AShooterCharacter::ServerStartFiring_Implementation()
{
	if (CurrentWeapon && !IsDead())
	{
		CurrentWeapon->StartFiring();
	}
}

void AShooterCharacter::ServerStopFiring_Implementation()
{
	if (CurrentWeapon)
	{
		CurrentWeapon->StopFiring();
	}
}

void AShooterCharacter::ServerSwitchWeapon_Implementation()
{
	DoSwitchWeapon();
}

void AShooterCharacter::ServerReload_Implementation()
{
	DoReload();
}

void AShooterCharacter::ServerAddWeaponClass_Implementation(TSubclassOf<AShooterWeapon> WeaponClass)
{
	AddWeaponClass(WeaponClass);
}
