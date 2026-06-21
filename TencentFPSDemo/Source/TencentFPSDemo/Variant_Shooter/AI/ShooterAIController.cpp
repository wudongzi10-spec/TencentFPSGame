// Copyright Epic Games, Inc. All Rights Reserved.


#include "Variant_Shooter/AI/ShooterAIController.h"
#include "ShooterNPC.h"
#include "Variant_Shooter/ShooterCharacter.h"
#include "Components/StateTreeAIComponent.h"
#include "Perception/AIPerceptionComponent.h"
#include "Navigation/PathFollowingComponent.h"
#include "AI/Navigation/PathFollowingAgentInterface.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "Engine/World.h"

AShooterAIController::AShooterAIController()
{
	// create the StateTree component
	StateTreeAI = CreateDefaultSubobject<UStateTreeAIComponent>(TEXT("StateTreeAI"));
	StateTreeAI->SetStartLogicAutomatically(false);

	// create the AI perception component. It will be configured in BP
	AIPerception = CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("AIPerception"));

	// subscribe to the AI perception delegates
	AIPerception->OnTargetPerceptionUpdated.AddDynamic(this, &AShooterAIController::OnPerceptionUpdated);
	AIPerception->OnTargetPerceptionForgotten.AddDynamic(this, &AShooterAIController::OnPerceptionForgotten);
}

void AShooterAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	// ensure we're possessing an NPC
	if (AShooterNPC* NPC = Cast<AShooterNPC>(InPawn))
	{
		// add the team tag to the pawn
		NPC->Tags.Add(TeamTag);

		// subscribe to the pawn's OnDeath delegate
		NPC->OnPawnDeath.AddDynamic(this, &AShooterAIController::OnPawnDeath);

		// start AI logic
		StateTreeAI->StartLogic();

		// keep advancing toward the nearest player even without line of sight, so enemies never idle
		GetWorld()->GetTimerManager().SetTimer(SeekTimer, this, &AShooterAIController::SeekNearestPlayer, SeekInterval, true, 0.5f);
	}
}

void AShooterAIController::SeekNearestPlayer()
{
	// perception already has a target; let the StateTree handle the engagement
	if (IsValid(TargetEnemy))
	{
		return;
	}

	APawn* NPCPawn = GetPawn();
	if (!NPCPawn)
	{
		return;
	}

	if (const AShooterNPC* NPC = Cast<AShooterNPC>(NPCPawn))
	{
		if (NPC->IsDead())
		{
			return;
		}
	}

	// find the nearest living player and advance toward them
	TArray<AActor*> Players;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AShooterCharacter::StaticClass(), Players);

	AShooterCharacter* Nearest = nullptr;
	float NearestDistSq = TNumericLimits<float>::Max();
	const FVector NPCLocation = NPCPawn->GetActorLocation();

	for (AActor* Actor : Players)
	{
		AShooterCharacter* Candidate = Cast<AShooterCharacter>(Actor);
		if (!Candidate || Candidate->IsDead())
		{
			continue;
		}

		const float DistSq = FVector::DistSquared(NPCLocation, Candidate->GetActorLocation());
		if (DistSq < NearestDistSq)
		{
			NearestDistSq = DistSq;
			Nearest = Candidate;
		}
	}

	if (Nearest)
	{
		// always face the nearest player so the NPC tracks the threat even when the path is blocked
		// (e.g. the player is on high ground the navmesh can't reach), instead of standing inert
		SetFocus(Nearest, EAIFocusPriority::Gameplay);
		MoveToActor(Nearest, SeekAcceptanceRadius);
	}
	else
	{
		ClearFocus(EAIFocusPriority::Gameplay);
	}
}

void AShooterAIController::OnPawnDeath()
{
	// stop seeking
	GetWorld()->GetTimerManager().ClearTimer(SeekTimer);

	// stop movement
	GetPathFollowingComponent()->AbortMove(*this, FPathFollowingResultFlags::UserAbort);

	// stop StateTree logic
	StateTreeAI->StopLogic(FString(""));

	// unpossess the pawn
	UnPossess();

	// destroy this controller
	Destroy();
}

void AShooterAIController::SetCurrentTarget(AActor* Target)
{
	TargetEnemy = Target;
}

void AShooterAIController::ClearCurrentTarget()
{
	TargetEnemy = nullptr;
}

void AShooterAIController::OnPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus)
{
	// pass the data to the StateTree delegate hook
	OnShooterPerceptionUpdated.ExecuteIfBound(Actor, Stimulus);
}

void AShooterAIController::OnPerceptionForgotten(AActor* Actor)
{
	// pass the data to the StateTree delegate hook
	OnShooterPerceptionForgotten.ExecuteIfBound(Actor);
}
