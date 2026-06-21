// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Fonts/SlateFontInfo.h"
#include "GameFramework/HUD.h"
#include "ShooterHUD.generated.h"

class AShooterCharacter;
class AShooterGameState;
class AShooterPlayerState;

/**
 * Chinese CF-like combat HUD with compact match, ammo and kill feedback.
 */
UCLASS()
class TENCENTFPSDEMO_API AShooterHUD : public AHUD
{
	GENERATED_BODY()

public:
	AShooterHUD();

	virtual void BeginPlay() override;
	virtual void PostRender() override;
	virtual void DrawHUD() override;

protected:
	UPROPERTY(EditAnywhere, Category="Shooter|HUD")
	float KillNoticeDuration = 2.0f;

	UPROPERTY(EditAnywhere, Category="Shooter|HUD")
	bool bShowHelpTip = true;

	UPROPERTY(EditAnywhere, Category="Shooter|HUD")
	bool bSuppressWorldDebugText = true;

	int32 LastSeenKillSerial = INDEX_NONE;
	float KillNoticeEndTime = 0.0f;
	FString KillNoticeText;
	int32 KillNoticeScore = 0;

	int32 LastSeenWave = 0;
	int32 LastAliveCount = 0;
	float WaveBannerEndTime = 0.0f;
	FString WaveBannerText;
	FLinearColor WaveBannerColor = FLinearColor::White;

	/** Smoothly animated team score shown on the scoreboard (lerps toward the real value) */
	float DisplayedTeamScore = -1.0f;

	/** Smoothly animated health fraction for the health panel */
	float DisplayedHealth = -1.0f;

	UPROPERTY(Transient)
	TObjectPtr<UObject> HUDTextFontObject;

	void UpdateKillNotice(const AShooterGameState* ShooterGameState);
	void UpdateWaveBanner(const AShooterGameState* ShooterGameState, float Now);
	void DrawWaveBanner(float Now);
	void DrawTopScoreboard(const AShooterGameState* ShooterGameState, const AShooterPlayerState* ShooterPlayerState);
	void DrawHelpTip(const AShooterGameState* ShooterGameState);
	void DrawBottomStatus(const AShooterCharacter* ShooterCharacter);
	void DrawAmmoStatus(const AShooterCharacter* ShooterCharacter);
	void DrawLowHealthVignette(const AShooterCharacter* ShooterCharacter);
	void DrawKillFeedback(float Now);
	void DrawKillIcon(float CenterX, float CenterY, float Scale, float Alpha);
	void DrawVictoryPanel(const AShooterGameState* ShooterGameState);
	void DrawCrosshair(const AShooterCharacter* ShooterCharacter);
	void DrawHitMarker(float Now);
	void DrawDamageIndicators(float Now);
	void DrawDamageNumbers(float Now);
	void DrawKillPopup(float Now);
	void DrawMultiKillBanner(float Now);
	void DrawDamageFlash(float Now);
	void DrawWorldMarkers();

	void DrawCnText(const FString& Text, float X, float Y, float Size, const FLinearColor& Color, bool bShadow = true);
	void DrawTextRight(const FString& Text, float RightX, float Y, float Size, const FLinearColor& Color);
	void DrawTextCentered(const FString& Text, float CenterX, float Y, float Size, const FLinearColor& Color, bool bShadow = true);
	void DrawSoftRect(float X, float Y, float Width, float Height, const FLinearColor& Color);
	void DrawOutlineRect(float X, float Y, float Width, float Height, const FLinearColor& Color, float Thickness = 1.0f);
	void DrawBar(float X, float Y, float Width, float Height, float Percent, const FLinearColor& FillColor, const FLinearColor& BackColor);

	/** Dark glass panel used as the base of every HUD widget */
	void DrawPanel(float X, float Y, float Width, float Height);
	/** L-shaped accent brackets at the four corners of a rect */
	void DrawCornerBrackets(float X, float Y, float Width, float Height, const FLinearColor& Color, float Len, float Thick);
	/** Row of pips showing wave progress */
	void DrawWavePips(float X, float Y, int32 CurrentWave, int32 TotalWaves);
	/** Segmented magazine readout (one tick per bullet) */
	void DrawSegmentedBar(float X, float Y, float Width, float Height, int32 Filled, int32 Total, const FLinearColor& OnColor, const FLinearColor& OffColor);

	FSlateFontInfo MakeHUDFont(float Size) const;
	FVector2D MeasureCnText(const FString& Text, float Size) const;

	/** Builds a runtime composite UFont that can render CJK glyphs on the Canvas. */
	void BuildHUDFont();
};
