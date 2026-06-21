// Copyright Epic Games, Inc. All Rights Reserved.

#include "Variant_Shooter/UI/ShooterHUD.h"
#include "Variant_Shooter/ShooterCharacter.h"
#include "Variant_Shooter/ShooterPlayerController.h"
#include "Variant_Shooter/ShooterGameState.h"
#include "Variant_Shooter/ShooterPlayerState.h"
#include "Variant_Shooter/AI/ShooterNPC.h"
#include "Variant_Shooter/Weapons/ShooterWeapon.h"
#include "Components/CapsuleComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Kismet/GameplayStatics.h"
#include "Framework/Application/SlateApplication.h"
#include "Fonts/FontMeasure.h"
#include "CanvasItem.h"
#include "Engine/Canvas.h"
#include "Engine/Font.h"
#include "Engine/FontFace.h"
#include "Fonts/CompositeFont.h"
#include "GameFramework/PlayerController.h"
#include "Styling/CoreStyle.h"

namespace ShooterHUDPalette
{
	static const FLinearColor Panel(0.035f, 0.045f, 0.055f, 0.72f);   // dark glass panel
	static const FLinearColor Accent(1.00f, 0.60f, 0.12f, 1.0f);      // amber highlight
	static const FLinearColor TextCol(0.96f, 0.98f, 1.00f, 1.0f);     // primary text
	static const FLinearColor Dim(0.60f, 0.69f, 0.78f, 1.0f);         // secondary text
	static const FLinearColor HealthCol(0.22f, 0.86f, 0.46f, 1.0f);   // health green
	static const FLinearColor DangerCol(0.97f, 0.27f, 0.18f, 1.0f);   // alert red
	static const FLinearColor BarBack(0.14f, 0.16f, 0.19f, 0.85f);    // empty bar
	static const FLinearColor Win(0.34f, 0.96f, 0.56f, 1.0f);         // victory green
	static const FLinearColor Hairline(1.0f, 1.0f, 1.0f, 0.06f);      // subtle panel edge
}

using namespace ShooterHUDPalette;

AShooterHUD::AShooterHUD()
{
}

void AShooterHUD::BeginPlay()
{
	Super::BeginPlay();

	BuildHUDFont();
}

void AShooterHUD::BuildHUDFont()
{
	// DroidSansFallback ships as a UFontFace, which cannot be drawn on the Canvas directly.
	// Wrap it in a runtime composite UFont so FSlateFontInfo can render Chinese (and ASCII) glyphs.
	UFontFace* FontFace = LoadObject<UFontFace>(nullptr, TEXT("/Engine/EngineFonts/Faces/DroidSansFallback.DroidSansFallback"));
	if (!FontFace)
	{
		return;
	}

	UFont* Font = NewObject<UFont>(this, TEXT("ShooterHUDFont"));
	Font->FontCacheType = EFontCacheType::Runtime;

	FTypefaceEntry& TypefaceEntry = Font->GetMutableInternalCompositeFont().DefaultTypeface.Fonts.AddDefaulted_GetRef();
	TypefaceEntry.Name = FName(TEXT("Regular"));
	TypefaceEntry.Font = FFontData(FontFace);

	HUDTextFontObject = Font;
}

void AShooterHUD::PostRender()
{
	if (bSuppressWorldDebugText)
	{
		DebugTextList.Reset();
	}

	Super::PostRender();
}

void AShooterHUD::DrawHUD()
{
	Super::DrawHUD();

	if (!Canvas)
	{
		return;
	}

	APlayerController* PC = GetOwningPlayerController();
	const AShooterCharacter* ShooterCharacter = PC ? Cast<AShooterCharacter>(PC->GetPawn()) : nullptr;
	const AShooterPlayerState* ShooterPlayerState = PC ? PC->GetPlayerState<AShooterPlayerState>() : nullptr;
	const AShooterGameState* ShooterGameState = GetWorld() ? GetWorld()->GetGameState<AShooterGameState>() : nullptr;
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;

	UpdateKillNotice(ShooterGameState);
	UpdateWaveBanner(ShooterGameState, Now);

	DrawWorldMarkers();
	DrawDamageNumbers(Now);
	DrawTopScoreboard(ShooterGameState, ShooterPlayerState);
	DrawHelpTip(ShooterGameState);
	DrawCrosshair(ShooterCharacter);
	DrawHitMarker(Now);
	DrawDamageIndicators(Now);
	DrawKillPopup(Now);
	DrawMultiKillBanner(Now);
	DrawKillFeedback(Now);
	DrawBottomStatus(ShooterCharacter);
	DrawAmmoStatus(ShooterCharacter);
	DrawDamageFlash(Now);
	DrawLowHealthVignette(ShooterCharacter);
	DrawWaveBanner(Now);
	DrawVictoryPanel(ShooterGameState);
}

void AShooterHUD::DrawLowHealthVignette(const AShooterCharacter* ShooterCharacter)
{
	// only warn when the local player is alive and below the danger threshold
	if (!ShooterCharacter)
	{
		return;
	}

	const float Health = ShooterCharacter->GetHealthPercent();
	const float Threshold = 0.35f;
	if (Health <= 0.0f || Health >= Threshold)
	{
		return;
	}

	// stronger and faster pulse the closer the player is to death
	const float Danger = 1.0f - (Health / Threshold);
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	const float Pulse = 0.55f + 0.45f * FMath::Sin(Now * (4.0f + Danger * 6.0f));
	const float Alpha = FMath::Clamp(Danger * 0.5f * Pulse, 0.0f, 0.5f);

	const float W = Canvas->ClipX;
	const float H = Canvas->ClipY;
	const float Band = FMath::Clamp(H * 0.16f, 60.0f, 180.0f);
	const FLinearColor Warn(0.85f, 0.05f, 0.04f, Alpha);

	// draw red bands hugging the screen edges
	DrawRect(Warn, 0.0f, 0.0f, W, Band);
	DrawRect(Warn, 0.0f, H - Band, W, Band);
	DrawRect(Warn, 0.0f, 0.0f, Band * 0.6f, H);
	DrawRect(Warn, W - Band * 0.6f, 0.0f, Band * 0.6f, H);
}

void AShooterHUD::UpdateWaveBanner(const AShooterGameState* ShooterGameState, float Now)
{
	if (!ShooterGameState)
	{
		return;
	}

	const int32 Wave = ShooterGameState->GetCurrentWave();
	const int32 Alive = ShooterGameState->GetAliveEnemyCount();
	const bool bWon = ShooterGameState->IsGameWon();

	// a new wave has begun
	if (Wave > 0 && Wave != LastSeenWave)
	{
		LastSeenWave = Wave;
		WaveBannerText = FString::Printf(TEXT("第 %d 波 来袭"), Wave);
		WaveBannerColor = FLinearColor(1.0f, 0.66f, 0.16f, 1.0f);
		WaveBannerEndTime = Now + 2.6f;
	}
	// the current wave has just been cleared (enemies dropped to zero) and the match isn't over
	else if (!bWon && Wave > 0 && Alive == 0 && LastAliveCount > 0)
	{
		WaveBannerText = FString::Printf(TEXT("第 %d 波 已清空"), Wave);
		WaveBannerColor = FLinearColor(0.34f, 0.96f, 0.56f, 1.0f);
		WaveBannerEndTime = Now + 2.4f;
	}

	LastAliveCount = Alive;
}

void AShooterHUD::DrawWaveBanner(float Now)
{
	if (Now > WaveBannerEndTime)
	{
		return;
	}

	if (WaveBannerText.IsEmpty())
	{
		return;
	}

	// punchy pop-in, hold, then fade out over the last 0.6s (duration-independent)
	const float Remaining = WaveBannerEndTime - Now;
	const float Alpha = FMath::Clamp(Remaining / 0.6f, 0.0f, 1.0f);

	const float CX = Canvas->ClipX * 0.5f;
	const float Y = Canvas->ClipY * 0.20f;
	const float Size = 34.0f;

	const FLinearColor Color(WaveBannerColor.R, WaveBannerColor.G, WaveBannerColor.B, Alpha);
	const FVector2D TextSize = MeasureCnText(WaveBannerText, Size);
	DrawTextCentered(WaveBannerText, CX, Y, Size, Color);

	// decorative lines flanking the title (no longer crossing through it)
	const float MidY = Y + TextSize.Y * 0.5f;
	const float Inner = TextSize.X * 0.5f + 22.0f;
	const FLinearColor Rule(WaveBannerColor.R, WaveBannerColor.G, WaveBannerColor.B, 0.8f * Alpha);
	DrawLine(CX - Inner - 64.0f, MidY, CX - Inner, MidY, Rule, 2.0f);
	DrawLine(CX + Inner, MidY, CX + Inner + 64.0f, MidY, Rule, 2.0f);
}

void AShooterHUD::UpdateKillNotice(const AShooterGameState* ShooterGameState)
{
	if (!ShooterGameState)
	{
		return;
	}

	const int32 KillSerial = ShooterGameState->GetKillFeedSerial();
	if (KillSerial > 0 && KillSerial != LastSeenKillSerial && !ShooterGameState->GetLastKillMessage().IsEmpty())
	{
		LastSeenKillSerial = KillSerial;
		KillNoticeText = ShooterGameState->GetLastKillMessage();
		KillNoticeScore = ShooterGameState->GetLastKillScoreAward();
		KillNoticeEndTime = GetWorld() ? GetWorld()->GetTimeSeconds() + KillNoticeDuration : 0.0f;
	}
}

void AShooterHUD::DrawTopScoreboard(const AShooterGameState* ShooterGameState, const AShooterPlayerState* ShooterPlayerState)
{
	const float W = Canvas->ClipX;
	const float PanelW = FMath::Clamp(W * 0.32f, 320.0f, 460.0f);
	const float X = (W - PanelW) * 0.5f;
	const float Y = 14.0f;
	const float H = 62.0f;
	const float CX = X + PanelW * 0.5f;

	const int32 TeamScore = ShooterGameState ? ShooterGameState->GetTeamScore() : 0;
	const int32 TargetScore = ShooterGameState ? ShooterGameState->GetTargetScore() : 120;
	const int32 Wave = ShooterGameState ? ShooterGameState->GetCurrentWave() : 0;
	const int32 TotalWaves = ShooterGameState ? ShooterGameState->GetTotalWaves() : 0;
	const int32 Alive = ShooterGameState ? ShooterGameState->GetAliveEnemyCount() : 0;
	const int32 Kills = ShooterPlayerState ? ShooterPlayerState->GetKills() : 0;
	const int32 Deaths = ShooterPlayerState ? ShooterPlayerState->GetDeaths() : 0;

	DrawPanel(X, Y, PanelW, H);
	DrawRect(Accent, X, Y, PanelW, 2.0f);
	DrawCornerBrackets(X, Y, PanelW, H, Accent, 14.0f, 2.0f);

	// center: title + big team score (animated count-up)
	if (DisplayedTeamScore < 0.0f)
	{
		DisplayedTeamScore = static_cast<float>(TeamScore);
	}
	const float DeltaTime = GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.0f;
	const float CountSpeed = FMath::Max(40.0f, FMath::Abs(TeamScore - DisplayedTeamScore) * 5.0f);
	DisplayedTeamScore = FMath::FInterpConstantTo(DisplayedTeamScore, static_cast<float>(TeamScore), DeltaTime, CountSpeed);
	const int32 ShownScore = FMath::RoundToInt(DisplayedTeamScore);

	DrawTextCentered(TEXT("佣兵突袭"), CX, Y + 8.0f, 12.0f, Accent);
	DrawTextCentered(FString::Printf(TEXT("%d / %d"), ShownScore, TargetScore), CX, Y + 21.0f, 26.0f, TextCol);

	// left: wave pips + wave/alive
	DrawWavePips(X + 16.0f, Y + 16.0f, Wave, TotalWaves);
	DrawCnText(FString::Printf(TEXT("第 %d/%d 波  存活 %d"), Wave, TotalWaves, Alive), X + 16.0f, Y + 34.0f, 11.0f, Dim);

	// right: kills + deaths
	DrawTextRight(FString::Printf(TEXT("击杀 %d"), Kills), X + PanelW - 16.0f, Y + 14.0f, 13.0f, Accent);
	DrawTextRight(FString::Printf(TEXT("阵亡 %d"), Deaths), X + PanelW - 16.0f, Y + 35.0f, 12.0f, Dim);

	// full-width progress bar along the bottom edge
	const float Progress = TargetScore > 0 ? static_cast<float>(TeamScore) / static_cast<float>(TargetScore) : 0.0f;
	DrawBar(X, Y + H - 3.0f, PanelW, 3.0f, Progress, Accent, BarBack);
}

void AShooterHUD::DrawHelpTip(const AShooterGameState* ShooterGameState)
{
	if (!bShowHelpTip)
	{
		return;
	}

	// objective line just under the top scoreboard
	DrawTextCentered(TEXT("目标：清空所有波次或达到团队目标分"), Canvas->ClipX * 0.5f, 78.0f, 12.0f, Dim);

	// slim control hint pill at the bottom center
	const FString Controls = TEXT("WASD 移动    左键 射击    R 换弹    Q 切枪");
	const float TextW = MeasureCnText(Controls, 12.0f).X;
	const float PadX = 18.0f;
	const float PillW = TextW + PadX * 2.0f;
	const float PillH = 26.0f;
	const float PillX = (Canvas->ClipX - PillW) * 0.5f;
	const float PillY = Canvas->ClipY - 34.0f;

	DrawPanel(PillX, PillY, PillW, PillH);
	DrawCnText(Controls, PillX + PadX, PillY + 6.0f, 12.0f, Dim);
}

void AShooterHUD::DrawBottomStatus(const AShooterCharacter* ShooterCharacter)
{
	const float Health = ShooterCharacter ? ShooterCharacter->GetHealthPercent() : 0.0f;

	// smoothly animate the bar/number toward the real value (snap up on heal, ease down on damage)
	if (DisplayedHealth < 0.0f)
	{
		DisplayedHealth = Health;
	}
	const float DeltaTime = GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.0f;
	DisplayedHealth = FMath::FInterpTo(DisplayedHealth, Health, DeltaTime, 7.0f);
	const float Shown = DisplayedHealth;

	const float X = 30.0f;
	const float W = FMath::Clamp(Canvas->ClipX * 0.19f, 230.0f, 300.0f);
	const float H = 54.0f;
	const float Y = Canvas->ClipY - 64.0f - H;

	// low health pushes the readout from green toward red
	const FLinearColor BarColor = FMath::Lerp(DangerCol, ShooterHUDPalette::HealthCol, FMath::Clamp(Shown * 1.6f, 0.0f, 1.0f));

	DrawPanel(X, Y, W, H);
	DrawRect(BarColor, X, Y, 3.0f, H);
	DrawCornerBrackets(X, Y, W, H, BarColor, 11.0f, 2.0f);

	// medical cross icon
	const float IcX = X + 22.0f;
	const float IcY = Y + 18.0f;
	DrawRect(BarColor, IcX - 2.0f, IcY - 7.0f, 4.0f, 14.0f);
	DrawRect(BarColor, IcX - 7.0f, IcY - 2.0f, 14.0f, 4.0f);

	DrawCnText(TEXT("生命"), X + 38.0f, Y + 10.0f, 13.0f, Dim);
	DrawTextRight(FString::FromInt(FMath::RoundToInt(Shown * 100.0f)), X + W - 16.0f, Y + 6.0f, 24.0f, TextCol);
	DrawBar(X + 16.0f, Y + H - 14.0f, W - 32.0f, 7.0f, Shown, BarColor, BarBack);

	// post-spawn damage immunity indicator
	if (ShooterCharacter && ShooterCharacter->IsSpawnProtected())
	{
		const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
		const float Pulse = 0.6f + 0.4f * FMath::Sin(Now * 6.0f);
		DrawCnText(TEXT("重生保护"), X + 38.0f, Y - 18.0f, 13.0f, FLinearColor(0.45f, 0.85f, 1.0f, Pulse));
	}
}

void AShooterHUD::DrawAmmoStatus(const AShooterCharacter* ShooterCharacter)
{
	const AShooterWeapon* Weapon = ShooterCharacter ? ShooterCharacter->GetCurrentWeapon() : nullptr;
	const int32 Bullets = Weapon ? Weapon->GetBulletCount() : 0;
	const int32 Reserve = Weapon ? Weapon->GetReserveBulletCount() : 0;
	const int32 Magazine = Weapon ? Weapon->GetMagazineSize() : 0;
	const bool bReloading = Weapon && Weapon->IsReloading();
	const float AmmoPercent = Magazine > 0 ? static_cast<float>(Bullets) / static_cast<float>(Magazine) : 0.0f;

	const float W = FMath::Clamp(Canvas->ClipX * 0.22f, 250.0f, 340.0f);
	const float H = 54.0f;
	const float X = Canvas->ClipX - W - 30.0f;
	const float Y = Canvas->ClipY - 64.0f - H;
	const FLinearColor AmmoColor = bReloading ? DangerCol : Accent;

	DrawPanel(X, Y, W, H);
	DrawRect(AmmoColor, X + W - 3.0f, Y, 3.0f, H);
	DrawCornerBrackets(X, Y, W, H, AmmoColor, 11.0f, 2.0f);

	DrawCnText(TEXT("弹药"), X + 14.0f, Y + 10.0f, 13.0f, Dim);

	// big current bullets, smaller reserve, right-aligned
	const FString CurStr = FString::Printf(TEXT("%d"), Bullets);
	const FString ResStr = FString::Printf(TEXT("/ %d"), Reserve);
	const float RightX = X + W - 16.0f;
	const float ResW = MeasureCnText(ResStr, 14.0f).X;
	DrawCnText(ResStr, RightX - ResW, Y + 17.0f, 14.0f, Dim);
	DrawCnText(CurStr, RightX - ResW - 6.0f - MeasureCnText(CurStr, 26.0f).X, Y + 6.0f, 26.0f, bReloading ? DangerCol : TextCol);

	// segmented magazine, or a sweeping bar while reloading
	if (bReloading)
	{
		DrawBar(X + 14.0f, Y + H - 13.0f, W - 28.0f, 7.0f, Weapon->GetReloadProgress(), DangerCol, BarBack);
		DrawCnText(TEXT("换弹中…"), X + 14.0f, Y + 28.0f, 12.0f, DangerCol);
	}
	else
	{
		DrawSegmentedBar(X + 14.0f, Y + H - 13.0f, W - 28.0f, 7.0f, Bullets, Magazine, Accent, BarBack);
	}
}

void AShooterHUD::DrawKillFeedback(float Now)
{
	if (Now > KillNoticeEndTime || KillNoticeText.IsEmpty())
	{
		return;
	}

	const float Alpha = FMath::Clamp((KillNoticeEndTime - Now) / KillNoticeDuration, 0.0f, 1.0f);

	// the local player's own kill is celebrated by the centered score popup,
	// so the kill feed only keeps a compact top-right log entry
	const float FeedW = 252.0f;
	const float FeedH = 34.0f;
	const float FeedX = Canvas->ClipX - FeedW - 28.0f;
	const float FeedY = 118.0f;
	if (FeedX <= 24.0f)
	{
		return;
	}

	DrawSoftRect(FeedX, FeedY, FeedW, FeedH, FLinearColor(0.02f, 0.02f, 0.025f, 0.55f * Alpha));
	DrawRect(FLinearColor(0.95f, 0.18f, 0.1f, 0.9f * Alpha), FeedX, FeedY, 3.0f, FeedH);
	DrawKillIcon(FeedX + 22.0f, FeedY + FeedH * 0.5f, 0.7f, Alpha);
	DrawCnText(KillNoticeText, FeedX + 40.0f, FeedY + 9.0f, 13.0f, FLinearColor(1.0f, 0.92f, 0.82f, Alpha));
}

void AShooterHUD::DrawKillIcon(float CenterX, float CenterY, float Scale, float Alpha)
{
	const FLinearColor Red(1.0f, 0.16f, 0.1f, Alpha);
	const FLinearColor Gold(1.0f, 0.76f, 0.2f, Alpha);
	const float S = FMath::Max(0.1f, Scale);

	DrawLine(CenterX - 13.0f * S, CenterY, CenterX - 4.0f * S, CenterY, Gold, 2.0f * S);
	DrawLine(CenterX + 4.0f * S, CenterY, CenterX + 13.0f * S, CenterY, Gold, 2.0f * S);
	DrawLine(CenterX, CenterY - 13.0f * S, CenterX, CenterY - 4.0f * S, Gold, 2.0f * S);
	DrawLine(CenterX, CenterY + 4.0f * S, CenterX, CenterY + 13.0f * S, Gold, 2.0f * S);
	DrawRect(Red, CenterX - 5.0f * S, CenterY - 5.0f * S, 10.0f * S, 10.0f * S);
	DrawOutlineRect(CenterX - 8.0f * S, CenterY - 8.0f * S, 16.0f * S, 16.0f * S, Gold, 1.5f * S);
}

void AShooterHUD::DrawVictoryPanel(const AShooterGameState* ShooterGameState)
{
	if (!ShooterGameState || !ShooterGameState->IsGameWon())
	{
		return;
	}

	const float SW = Canvas->ClipX;
	const float SH = Canvas->ClipY;

	// dim the whole screen to focus on the result
	DrawRect(FLinearColor(0.0f, 0.0f, 0.0f, 0.58f), 0.0f, 0.0f, SW, SH);

	const float CX = SW * 0.5f;
	const float BandH = 150.0f;
	const float BandY = SH * 0.30f;

	// full-width victory band with green accent rules
	DrawRect(FLinearColor(0.03f, 0.05f, 0.04f, 0.86f), 0.0f, BandY, SW, BandH);
	DrawRect(Win, 0.0f, BandY, SW, 2.5f);
	DrawRect(Win, 0.0f, BandY + BandH - 2.5f, SW, 2.5f);

	// centered emblem brackets framing the title
	const float FrameW = FMath::Min(SW - 120.0f, 560.0f);
	DrawCornerBrackets(CX - FrameW * 0.5f, BandY + 14.0f, FrameW, BandH - 28.0f, Win, 22.0f, 2.5f);

	DrawTextCentered(TEXT("任 务 完 成"), CX, BandY + 26.0f, 40.0f, Win);
	DrawTextCentered(TEXT("全 队 目 标 达 成"), CX, BandY + 86.0f, 15.0f, TextCol);
	DrawTextCentered(FString::Printf(TEXT("团队分数 %d / %d"), ShooterGameState->GetTeamScore(), ShooterGameState->GetTargetScore()), CX, BandY + 112.0f, 15.0f, Accent);
}

void AShooterHUD::DrawCrosshair(const AShooterCharacter* ShooterCharacter)
{
	const float X = Canvas->ClipX * 0.5f;
	const float Y = Canvas->ClipY * 0.5f;
	const FLinearColor Line(0.95f, 0.98f, 1.0f, 0.9f);
	const FLinearColor Shadow(0.0f, 0.0f, 0.0f, 0.55f);

	// dynamic spread: the reticle opens up while moving and tightens when still
	float Spread = 0.0f;
	if (ShooterCharacter)
	{
		const float Speed = ShooterCharacter->GetVelocity().Size2D();
		Spread = FMath::GetMappedRangeValueClamped(FVector2D(80.0f, 600.0f), FVector2D(0.0f, 9.0f), Speed);
	}

	// brief outward bloom right after landing a hit, for tactile confirmation
	if (const AShooterPlayerController* ShooterPC = Cast<AShooterPlayerController>(GetOwningPlayerController()))
	{
		const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
		const float HitAge = Now - ShooterPC->GetHitMarkerTime();
		if (HitAge >= 0.0f && HitAge < 0.18f)
		{
			Spread += 6.0f * (1.0f - HitAge / 0.18f);
		}
	}

	// soft shadow under each stroke for readability over bright scenery, then the bright stroke
	auto Stroke = [&](float x0, float y0, float x1, float y1)
	{
		DrawLine(x0 + 1.0f, y0 + 1.0f, x1 + 1.0f, y1 + 1.0f, Shadow, 2.0f);
		DrawLine(x0, y0, x1, y1, Line, 1.6f);
	};

	const float Gap = 5.0f + Spread;
	const float Len = 10.0f;
	Stroke(X - Gap - Len, Y, X - Gap, Y);
	Stroke(X + Gap, Y, X + Gap + Len, Y);
	Stroke(X, Y - Gap - Len, X, Y - Gap);
	Stroke(X, Y + Gap, X, Y + Gap + Len);

	// center dot
	DrawRect(Shadow, X - 1.5f, Y - 1.5f, 3.0f, 3.0f);
	DrawRect(Accent, X - 1.0f, Y - 1.0f, 2.0f, 2.0f);
}

void AShooterHUD::DrawWorldMarkers()
{
	UWorld* World = GetWorld();
	APlayerController* PC = GetOwningPlayerController();
	if (!World || !PC || !Canvas)
	{
		return;
	}

	const APawn* LocalPawn = PC->GetPawn();
	const FVector ViewLoc = PC->PlayerCameraManager ? PC->PlayerCameraManager->GetCameraLocation()
		: (LocalPawn ? LocalPawn->GetActorLocation() : FVector::ZeroVector);
	const FLinearColor Outline(0.0f, 0.0f, 0.0f, 0.6f);

	// returns false when world geometry blocks the line from the player's view to the target
	auto IsVisible = [&](const AActor* Target, const FVector& TargetCenter) -> bool
	{
		FHitResult Hit;
		FCollisionQueryParams Params;
		if (LocalPawn)
		{
			Params.AddIgnoredActor(LocalPawn);
		}

		const bool bBlocked = World->LineTraceSingleByChannel(Hit, ViewLoc, TargetCenter, ECC_Visibility, Params);
		if (!bBlocked || Hit.GetActor() == Target)
		{
			return true;
		}

		// allow a small buffer so hitting the target's own capsule surface still counts as visible
		return Hit.Distance >= FVector::Dist(ViewLoc, TargetCenter) - 80.0f;
	};

	// enemies: red health bar, but only when not occluded by the map (no wallhack)
	TArray<AActor*> Enemies;
	UGameplayStatics::GetAllActorsOfClass(World, AShooterNPC::StaticClass(), Enemies);
	const FLinearColor EnemyColor(0.98f, 0.22f, 0.16f, 0.95f);
	for (AActor* Actor : Enemies)
	{
		const AShooterNPC* NPC = Cast<AShooterNPC>(Actor);
		if (!NPC || NPC->IsDead())
		{
			continue;
		}

		const float HalfH = NPC->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
		const FVector Center = NPC->GetActorLocation() + FVector(0.0f, 0.0f, 40.0f);
		if (!IsVisible(NPC, Center))
		{
			continue;
		}

		const FVector Screen = Project(NPC->GetActorLocation() + FVector(0.0f, 0.0f, HalfH + 22.0f), false);
		if (Screen.Z <= 0.0f)
		{
			continue;
		}

		const float BW = 40.0f;
		const float BH = 5.0f;
		DrawRect(Outline, Screen.X - BW * 0.5f - 1.0f, Screen.Y - 1.0f, BW + 2.0f, BH + 2.0f);
		DrawBar(Screen.X - BW * 0.5f, Screen.Y, BW, BH, NPC->GetHealthPercent(), EnemyColor, FLinearColor(0.06f, 0.07f, 0.08f, 0.9f));
	}

	// teammates: green downward triangle (always visible for friendly awareness)
	TArray<AActor*> Players;
	UGameplayStatics::GetAllActorsOfClass(World, AShooterCharacter::StaticClass(), Players);
	const FLinearColor AllyColor(0.30f, 0.95f, 0.52f, 0.92f);
	for (AActor* Actor : Players)
	{
		const AShooterCharacter* Mate = Cast<AShooterCharacter>(Actor);
		if (!Mate || Mate == LocalPawn || Mate->IsDead())
		{
			continue;
		}

		const float HalfH = Mate->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
		const FVector Screen = Project(Mate->GetActorLocation() + FVector(0.0f, 0.0f, HalfH + 28.0f), false);
		if (Screen.Z <= 0.0f)
		{
			continue;
		}

		const float SX = Screen.X;
		const float SY = Screen.Y;
		const float TriW = 11.0f;
		const float ApexY = SY - 2.0f;
		const float TopY = ApexY - 12.0f;

		DrawLine(SX - TriW - 1.0f, TopY - 1.0f, SX, ApexY + 1.0f, Outline, 3.0f);
		DrawLine(SX + TriW + 1.0f, TopY - 1.0f, SX, ApexY + 1.0f, Outline, 3.0f);
		DrawLine(SX - TriW - 1.0f, TopY - 1.0f, SX + TriW + 1.0f, TopY - 1.0f, Outline, 3.0f);
		for (float t = 0.0f; t <= 1.0f; t += 0.12f)
		{
			const float SpanY = FMath::Lerp(TopY, ApexY, t);
			const float SpanHalf = TriW * (1.0f - t);
			DrawLine(SX - SpanHalf, SpanY, SX + SpanHalf, SpanY, AllyColor, 2.0f);
		}
	}
}

void AShooterHUD::DrawHitMarker(float Now)
{
	const AShooterPlayerController* ShooterPC = Cast<AShooterPlayerController>(GetOwningPlayerController());
	if (!ShooterPC)
	{
		return;
	}

	const float Age = Now - ShooterPC->GetHitMarkerTime();
	const float Life = 0.32f;
	if (Age < 0.0f || Age > Life)
	{
		return;
	}

	const float T = Age / Life;
	const float Alpha = 1.0f - T;
	const bool bLethal = ShooterPC->WasHitMarkerLethal();
	const bool bHeadshot = ShooterPC->WasHitMarkerHeadshot();

	// white = body hit, red = kill, gold = headshot
	FLinearColor Color = bLethal ? FLinearColor(1.0f, 0.22f, 0.16f, Alpha) : FLinearColor(1.0f, 1.0f, 1.0f, Alpha);
	if (bHeadshot)
	{
		Color = FLinearColor(1.0f, 0.78f, 0.16f, Alpha);
	}

	const float X = Canvas->ClipX * 0.5f;
	const float Y = Canvas->ClipY * 0.5f;
	// the marks punch outward as they fade for a satisfying confirmation
	const float In = 4.0f + 4.0f * T;
	const float Out = In + (bLethal || bHeadshot ? 11.0f : 8.0f);
	const float Thick = bLethal || bHeadshot ? 2.4f : 1.8f;

	auto Tick = [&](float dx0, float dy0, float dx1, float dy1)
	{
		DrawLine(X + dx0 + 1.0f, Y + dy0 + 1.0f, X + dx1 + 1.0f, Y + dy1 + 1.0f, FLinearColor(0.0f, 0.0f, 0.0f, 0.5f * Alpha), Thick);
		DrawLine(X + dx0, Y + dy0, X + dx1, Y + dy1, Color, Thick);
	};

	Tick(-Out, -Out, -In, -In);
	Tick(Out, -Out, In, -In);
	Tick(-Out, Out, -In, In);
	Tick(Out, Out, In, In);

	// headshots add vertical/horizontal ticks for an unmistakable confirmation
	if (bHeadshot)
	{
		Tick(0.0f, -Out - 2.0f, 0.0f, -In - 2.0f);
		Tick(0.0f, In + 2.0f, 0.0f, Out + 2.0f);
	}
}

void AShooterHUD::DrawKillPopup(float Now)
{
	const AShooterPlayerController* ShooterPC = Cast<AShooterPlayerController>(GetOwningPlayerController());
	if (!ShooterPC)
	{
		return;
	}

	const float Age = Now - ShooterPC->GetKillPopupTime();
	const float Life = 1.1f;
	if (Age < 0.0f || Age > Life)
	{
		return;
	}

	const float T = Age / Life;
	// quick pop-in scale, then a gentle rise and fade-out
	const float PopIn = FMath::Clamp(Age / 0.12f, 0.0f, 1.0f);
	const float Scale = FMath::InterpEaseOut(0.5f, 1.0f, PopIn, 2.0f);
	const float Alpha = 1.0f - FMath::Clamp((T - 0.4f) / 0.6f, 0.0f, 1.0f);
	const float Rise = 26.0f * T;

	const bool bHeadshot = ShooterPC->WasKillPopupHeadshot();
	const int32 Score = ShooterPC->GetKillPopupScore();

	const float CX = Canvas->ClipX * 0.5f;
	const float CY = Canvas->ClipY * 0.42f - Rise;

	const FLinearColor Main = bHeadshot ? FLinearColor(1.0f, 0.80f, 0.18f, Alpha) : FLinearColor(1.0f, 0.34f, 0.22f, Alpha);
	const FString Label = bHeadshot ? TEXT("爆头击杀") : TEXT("击杀");

	DrawTextCentered(Label, CX, CY, 18.0f * Scale, Main);
	DrawTextCentered(FString::Printf(TEXT("+%d"), Score), CX, CY + 22.0f * Scale, 26.0f * Scale, FLinearColor(1.0f, 0.95f, 0.8f, Alpha));
}

void AShooterHUD::DrawDamageIndicators(float Now)
{
	const AShooterPlayerController* ShooterPC = Cast<AShooterPlayerController>(GetOwningPlayerController());
	const APawn* Pawn = ShooterPC ? ShooterPC->GetPawn() : nullptr;
	if (!ShooterPC || !Pawn)
	{
		return;
	}

	const float Life = 1.3f;
	const float CX = Canvas->ClipX * 0.5f;
	const float CY = Canvas->ClipY * 0.5f;
	const float Radius = FMath::Min(Canvas->ClipX, Canvas->ClipY) * 0.16f;
	const float ViewYaw = ShooterPC->GetControlRotation().Yaw;
	const FVector PawnLoc = Pawn->GetActorLocation();

	for (const AShooterPlayerController::FDamageIndicator& Indicator : ShooterPC->GetDamageIndicators())
	{
		const float Age = Now - Indicator.Time;
		if (Age < 0.0f || Age > Life)
		{
			continue;
		}

		const float Alpha = (1.0f - Age / Life) * 0.85f;

		// angle of the damage source relative to where the player is looking (0 = straight ahead)
		FVector ToSource = Indicator.WorldLocation - PawnLoc;
		ToSource.Z = 0.0f;
		if (ToSource.IsNearlyZero())
		{
			continue;
		}

		const float RelYaw = FRotator::NormalizeAxis(ToSource.Rotation().Yaw - ViewYaw);
		const float Rad = FMath::DegreesToRadians(RelYaw);

		// screen direction toward the source: forward = up, right = +X
		const FVector2D Dir(FMath::Sin(Rad), -FMath::Cos(Rad));
		const FVector2D Tangent(-Dir.Y, Dir.X);

		const FLinearColor Warn(0.98f, 0.16f, 0.12f, Alpha);
		const FLinearColor Shadow(0.0f, 0.0f, 0.0f, 0.5f * Alpha);

		// a thick red arc on the side the damage came from reads as an unmistakable threat bearing
		const float SpanDeg = 62.0f;
		const int32 Segments = 12;
		const float BaseAngle = Rad - FMath::DegreesToRadians(SpanDeg * 0.5f);
		FVector2D Prev = FVector2D::ZeroVector;
		for (int32 i = 0; i <= Segments; ++i)
		{
			const float A = BaseAngle + FMath::DegreesToRadians(SpanDeg) * (static_cast<float>(i) / Segments);
			const FVector2D P(CX + FMath::Sin(A) * Radius, CY - FMath::Cos(A) * Radius);
			if (i > 0)
			{
				DrawLine(Prev.X + 1.0f, Prev.Y + 1.0f, P.X + 1.0f, P.Y + 1.0f, Shadow, 5.0f);
				DrawLine(Prev.X, Prev.Y, P.X, P.Y, Warn, 3.5f);
			}
			Prev = P;
		}

		// outward arrowhead at the arc centre, pointing toward the source ("turn this way")
		const FVector2D Mid(CX + Dir.X * Radius, CY + Dir.Y * Radius);
		const FVector2D OutTip(CX + Dir.X * (Radius + 13.0f), CY + Dir.Y * (Radius + 13.0f));
		const FVector2D L(Mid.X + Tangent.X * 7.0f, Mid.Y + Tangent.Y * 7.0f);
		const FVector2D R(Mid.X - Tangent.X * 7.0f, Mid.Y - Tangent.Y * 7.0f);
		DrawLine(L.X, L.Y, OutTip.X, OutTip.Y, Warn, 3.0f);
		DrawLine(R.X, R.Y, OutTip.X, OutTip.Y, Warn, 3.0f);
	}
}

void AShooterHUD::DrawDamageNumbers(float Now)
{
	const AShooterPlayerController* ShooterPC = Cast<AShooterPlayerController>(GetOwningPlayerController());
	if (!ShooterPC)
	{
		return;
	}

	const float Life = 0.9f;
	for (const AShooterPlayerController::FDamageNumber& Number : ShooterPC->GetDamageNumbers())
	{
		const float Age = Now - Number.Time;
		if (Age < 0.0f || Age > Life)
		{
			continue;
		}

		const float T = Age / Life;
		const float Alpha = 1.0f - T * T;

		// the number floats upward from the impact point
		const FVector World = Number.WorldLocation + FVector(0.0f, 0.0f, 16.0f + 48.0f * T);
		const FVector Screen = Project(World, false);
		if (Screen.Z <= 0.0f)
		{
			continue;
		}

		const float Size = Number.bHeadshot ? 25.0f : 18.0f;
		const FLinearColor Color = Number.bHeadshot ? FLinearColor(1.0f, 0.80f, 0.18f, Alpha) : FLinearColor(1.0f, 0.95f, 0.86f, Alpha);
		DrawTextCentered(FString::FromInt(FMath::RoundToInt(Number.Amount)), Screen.X, Screen.Y, Size, Color);
	}
}

void AShooterHUD::DrawMultiKillBanner(float Now)
{
	const AShooterPlayerController* ShooterPC = Cast<AShooterPlayerController>(GetOwningPlayerController());
	if (!ShooterPC)
	{
		return;
	}

	const int32 Count = ShooterPC->GetMultiKillCount();
	if (Count < 2)
	{
		return;
	}

	const float Age = Now - ShooterPC->GetMultiKillTime();
	const float Life = 1.8f;
	if (Age < 0.0f || Age > Life)
	{
		return;
	}

	const float T = Age / Life;
	const float PopIn = FMath::Clamp(Age / 0.12f, 0.0f, 1.0f);
	const float Scale = FMath::InterpEaseOut(0.55f, 1.0f, PopIn, 2.0f);
	const float Alpha = 1.0f - FMath::Clamp((T - 0.55f) / 0.45f, 0.0f, 1.0f);

	FString Text;
	FLinearColor Color;
	switch (Count)
	{
	case 2:  Text = TEXT("双 杀");   Color = FLinearColor(1.0f, 0.62f, 0.18f, Alpha); break;
	case 3:  Text = TEXT("三 杀");   Color = FLinearColor(1.0f, 0.44f, 0.16f, Alpha); break;
	case 4:  Text = TEXT("四 杀");   Color = FLinearColor(1.0f, 0.28f, 0.16f, Alpha); break;
	default: Text = TEXT("暴 走");   Color = FLinearColor(1.0f, 0.16f, 0.14f, Alpha); break;
	}

	const float CX = Canvas->ClipX * 0.5f;
	const float CY = Canvas->ClipY * 0.30f;
	DrawTextCentered(Text, CX, CY, 36.0f * Scale, Color);
}

void AShooterHUD::DrawDamageFlash(float Now)
{
	const AShooterPlayerController* ShooterPC = Cast<AShooterPlayerController>(GetOwningPlayerController());
	if (!ShooterPC)
	{
		return;
	}

	const float Age = Now - ShooterPC->GetDamageFlashTime();
	const float Life = 0.35f;
	if (Age < 0.0f || Age > Life)
	{
		return;
	}

	const float Alpha = (1.0f - Age / Life) * 0.34f;
	const float W = Canvas->ClipX;
	const float H = Canvas->ClipY;
	const float Band = FMath::Clamp(H * 0.18f, 60.0f, 200.0f);
	const FLinearColor Warn(0.86f, 0.06f, 0.05f, Alpha);

	DrawRect(Warn, 0.0f, 0.0f, W, Band);
	DrawRect(Warn, 0.0f, H - Band, W, Band);
	DrawRect(Warn, 0.0f, 0.0f, Band * 0.55f, H);
	DrawRect(Warn, W - Band * 0.55f, 0.0f, Band * 0.55f, H);
}

void AShooterHUD::DrawCnText(const FString& Text, float X, float Y, float Size, const FLinearColor& Color, bool bShadow)
{
	FCanvasTextItem TextItem(FVector2D(X, Y), FText::FromString(Text), MakeHUDFont(Size), Color);
	if (bShadow)
	{
		TextItem.EnableShadow(FLinearColor(0.0f, 0.0f, 0.0f, 0.85f), FVector2D(1.0f, 1.0f));
	}
	Canvas->DrawItem(TextItem);
}

void AShooterHUD::DrawTextCentered(const FString& Text, float CenterX, float Y, float Size, const FLinearColor& Color, bool bShadow)
{
	const FVector2D TextSize = MeasureCnText(Text, Size);
	DrawCnText(Text, CenterX - TextSize.X * 0.5f, Y, Size, Color, bShadow);
}

void AShooterHUD::DrawSoftRect(float X, float Y, float Width, float Height, const FLinearColor& Color)
{
	DrawRect(Color, X, Y, Width, Height);
	DrawRect(FLinearColor(1.0f, 1.0f, 1.0f, 0.05f), X, Y, Width, 1.0f);
	DrawRect(FLinearColor(0.0f, 0.0f, 0.0f, 0.18f), X, Y + Height - 1.0f, Width, 1.0f);
}

void AShooterHUD::DrawOutlineRect(float X, float Y, float Width, float Height, const FLinearColor& Color, float Thickness)
{
	DrawRect(Color, X, Y, Width, Thickness);
	DrawRect(Color, X, Y + Height - Thickness, Width, Thickness);
	DrawRect(Color, X, Y, Thickness, Height);
	DrawRect(Color, X + Width - Thickness, Y, Thickness, Height);
}

void AShooterHUD::DrawBar(float X, float Y, float Width, float Height, float Percent, const FLinearColor& FillColor, const FLinearColor& BackColor)
{
	const float ClampedPercent = FMath::Clamp(Percent, 0.0f, 1.0f);
	DrawRect(BackColor, X, Y, Width, Height);
	DrawRect(FillColor, X, Y, Width * ClampedPercent, Height);
}

void AShooterHUD::DrawTextRight(const FString& Text, float RightX, float Y, float Size, const FLinearColor& Color)
{
	DrawCnText(Text, RightX - MeasureCnText(Text, Size).X, Y, Size, Color);
}

void AShooterHUD::DrawPanel(float X, float Y, float Width, float Height)
{
	DrawSoftRect(X, Y, Width, Height, Panel);
	DrawOutlineRect(X, Y, Width, Height, Hairline, 1.0f);
}

void AShooterHUD::DrawCornerBrackets(float X, float Y, float Width, float Height, const FLinearColor& Color, float Len, float Thick)
{
	// top-left
	DrawRect(Color, X, Y, Len, Thick);
	DrawRect(Color, X, Y, Thick, Len);
	// top-right
	DrawRect(Color, X + Width - Len, Y, Len, Thick);
	DrawRect(Color, X + Width - Thick, Y, Thick, Len);
	// bottom-left
	DrawRect(Color, X, Y + Height - Thick, Len, Thick);
	DrawRect(Color, X, Y + Height - Len, Thick, Len);
	// bottom-right
	DrawRect(Color, X + Width - Len, Y + Height - Thick, Len, Thick);
	DrawRect(Color, X + Width - Thick, Y + Height - Len, Thick, Len);
}

void AShooterHUD::DrawWavePips(float X, float Y, int32 CurrentWave, int32 TotalWaves)
{
	const float Size = 7.0f;
	const float Gap = 5.0f;
	for (int32 Index = 0; Index < TotalWaves; ++Index)
	{
		const float PX = X + Index * (Size + Gap);
		if (Index < CurrentWave)
		{
			DrawRect(Accent, PX, Y, Size, Size);
		}
		else
		{
			DrawRect(BarBack, PX, Y, Size, Size);
			DrawOutlineRect(PX, Y, Size, Size, Hairline, 1.0f);
		}
	}
}

void AShooterHUD::DrawSegmentedBar(float X, float Y, float Width, float Height, int32 Filled, int32 Total, const FLinearColor& OnColor, const FLinearColor& OffColor)
{
	if (Total <= 0)
	{
		DrawRect(OffColor, X, Y, Width, Height);
		return;
	}

	// cap the tick count so large magazines stay readable
	const int32 Segments = FMath::Min(Total, 30);
	const int32 FilledSegments = FMath::CeilToInt(static_cast<float>(FMath::Clamp(Filled, 0, Total)) / Total * Segments);
	const float Gap = Segments > 20 ? 1.0f : 2.0f;
	const float TickW = FMath::Max(1.0f, (Width - Gap * (Segments - 1)) / Segments);

	for (int32 Index = 0; Index < Segments; ++Index)
	{
		const float PX = X + Index * (TickW + Gap);
		DrawRect(Index < FilledSegments ? OnColor : OffColor, PX, Y, TickW, Height);
	}
}

FSlateFontInfo AShooterHUD::MakeHUDFont(float Size) const
{
	if (HUDTextFontObject)
	{
		return FSlateFontInfo(HUDTextFontObject.Get(), Size);
	}

	return FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), Size);
}

FVector2D AShooterHUD::MeasureCnText(const FString& Text, float Size) const
{
	// accurate measurement from the same font Slate uses to render, so centering is pixel-correct
	if (FSlateApplication::IsInitialized())
	{
		const TSharedRef<FSlateFontMeasure> FontMeasure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
		return FontMeasure->Measure(Text, MakeHUDFont(Size));
	}

	// heuristic fallback if Slate isn't available
	float Width = 0.0f;
	for (int32 Index = 0; Index < Text.Len(); ++Index)
	{
		const TCHAR Character = Text[Index];
		if (FChar::IsWhitespace(Character))
		{
			Width += Size * 0.35f;
		}
		else if (Character < 128)
		{
			Width += Size * 0.58f;
		}
		else
		{
			Width += Size;
		}
	}

	return FVector2D(Width, Size * 1.2f);
}
