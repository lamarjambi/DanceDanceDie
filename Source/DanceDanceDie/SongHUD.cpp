#include "SongHUD.h"

#include "Blueprint/WidgetTree.h"
#include "Brushes/SlateColorBrush.h"
#include "Components/AudioComponent.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"
#include "Styling/CoreStyle.h"
#include "UObject/ConstructorHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogSongHUD, Log, All);

static FAutoConsoleCommandWithWorldAndArgs GSongHUDTest(
	TEXT("ddd.SongHUD"),
	TEXT("Plays the song title intro + progress bar without music. Optional arg: secs (default 60)."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		const float Secs = Args.Num() > 0 ? FCString::Atof(*Args[0]) : 60.f;
		int32 N = 0;
		for (TActorIterator<ASongHUD> It(World); It; ++It) { It->FakeStart(Secs); ++N; }
		if (N == 0) { UE_LOG(LogSongHUD, Error, TEXT("ddd.SongHUD: no SongHUD actor in this level (run Scripts/setup_song_hud.py)")); }
	}));

static float Smooth(float T) { T = FMath::Clamp(T, 0.f, 1.f); return T * T * (3.f - 2.f * T); }

static FString MinSec(float S)
{
	const int32 I = FMath::Max(0, FMath::FloorToInt(S));
	return FString::Printf(TEXT("%d:%02d"), I / 60, I % 60);
}

// ---------------------------------------------------------------- widget

void USongHUDWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	check(WidgetTree);

	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
	WidgetTree->RootWidget = Root;

	Box = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Box"));
	BoxSlot = Root->AddChildToCanvas(Box);
	BoxSlot->SetAutoSize(true);

	Title = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Title"));
	Box->AddChildToVerticalBox(Title);

	Time = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Time"));
	if (UVerticalBoxSlot* S = Box->AddChildToVerticalBox(Time)) { S->SetPadding(FMargin(4.f, 0.f, 0.f, 0.f)); }

	BarRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("BarRow"));
	if (UVerticalBoxSlot* S = Box->AddChildToVerticalBox(BarRow)) { S->SetPadding(FMargin(4.f, 6.f, 0.f, 0.f)); }

	USizeBox* BarBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("BarBox"));
	if (UHorizontalBoxSlot* S = BarRow->AddChildToHorizontalBox(BarBox)) { S->SetVerticalAlignment(VAlign_Center); }
	Bar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("Bar"));
	BarBox->AddChild(Bar);

}

void USongHUDWidget::Setup(const FParams& P)
{
	Params = P;
	check(Title && Bar && Time);

	Title->SetText(FText::FromString(P.Title));
	Title->SetColorAndOpacity(P.TextColor);
	Title->SetShadowColorAndOpacity(P.ShadowColor);
	Title->SetShadowOffset(P.ShadowOffset);

	Time->SetFont(P.Font ? FSlateFontInfo(P.Font, P.TimeSize) : FCoreStyle::GetDefaultFontStyle("Regular", P.TimeSize));
	Time->SetColorAndOpacity(P.TextColor);
	Time->SetShadowColorAndOpacity(P.ShadowColor);
	Time->SetShadowOffset(P.ShadowOffset * 0.5f);

	// flat blocks, no rounded engine brush: reads better through the PS1 post process
	FProgressBarStyle Style = Bar->GetWidgetStyle();
	Style.BackgroundImage = FSlateColorBrush(P.BarBack);
	Style.FillImage = FSlateColorBrush(FLinearColor::White);
	Style.MarqueeImage = FSlateColorBrush(FLinearColor::White);
	Bar->SetWidgetStyle(Style);
	Bar->SetFillColorAndOpacity(P.BarFill);
	Bar->SetPercent(0.f);
	CastChecked<USizeBox>(Bar->GetParent())->SetWidthOverride(P.BarSize.X);
	CastChecked<USizeBox>(Bar->GetParent())->SetHeightOverride(P.BarSize.Y);

	SetVisibility(ESlateVisibility::HitTestInvisible);
	SetLayout(0.f);
	SetBarAlpha(0.f);
}

void USongHUDWidget::SetLayout(float T)
{
	const float E = Smooth(T);
	const float Size = FMath::Lerp(Params.BigSize, Params.SmallSize, E);
	Title->SetFont(Params.Font ? FSlateFontInfo(Params.Font, Size) : FCoreStyle::GetDefaultFontStyle("Regular", Size));
	Title->SetShadowOffset(Params.ShadowOffset * (Size / Params.SmallSize) * 0.6f);

	const FVector2D Centre(0.5f, 0.5f);
	const FVector2D A = FMath::Lerp(Centre, Params.CornerAnchor, E);
	BoxSlot->SetAnchors(FAnchors(A.X, A.Y));
	BoxSlot->SetAlignment(A);
	BoxSlot->SetPosition(FMath::Lerp(FVector2D::ZeroVector, Params.CornerOffset, E));
}

void USongHUDWidget::SetBarAlpha(float A)
{
	const ESlateVisibility Vis = A <= 0.f ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible;
	BarRow->SetVisibility(Vis);
	BarRow->SetRenderOpacity(FMath::Clamp(A, 0.f, 1.f));
	Time->SetVisibility(Params.bShowTime ? Vis : ESlateVisibility::Collapsed);
	Time->SetRenderOpacity(FMath::Clamp(A, 0.f, 1.f));
}

void USongHUDWidget::SetMask(float A)
{
	WidgetTree->RootWidget->SetRenderOpacity(FMath::Clamp(A, 0.f, 1.f));
}

void USongHUDWidget::SetProgress(float Elapsed, float Duration)
{
	Bar->SetPercent(Duration > 0.f ? FMath::Clamp(Elapsed / Duration, 0.f, 1.f) : 0.f);
	Time->SetText(FText::FromString(MinSec(FMath::Min(Elapsed, Duration)) + TEXT(" / ") + MinSec(Duration)));
}

// ---------------------------------------------------------------- actor

ASongHUD::ASongHUD()
{
	PrimaryActorTick.bCanEverTick = true;
	static ConstructorHelpers::FObjectFinder<UFont> Vt323(TEXT("/Game/UI/Fonts/VT323-Regular_Font.VT323-Regular_Font"));
	Font = Vt323.Object;
}

void ASongHUD::Fail(const FString& Why)
{
	bFailed = true;
	UE_LOG(LogSongHUD, Error, TEXT("%s"), *Why);
	if (GEngine) { GEngine->AddOnScreenDebugMessage(-1, 15.f, FColor::Red, TEXT("SongHUD: ") + Why); }
}

void ASongHUD::BeginPlay()
{
	Super::BeginPlay();

	if (!Font) { Fail(TEXT("Font not set (VT323 missing?)")); }
	if (SongTitle.IsEmpty()) { Fail(TEXT("SongTitle is empty")); }

	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		UClass* C = It->GetClass();
		if (!FindFProperty<FObjectProperty>(C, MusicProperty)) { continue; }
		if (!FindFProperty<FObjectProperty>(C, CueProperty)) { Fail(FString::Printf(TEXT("%s has '%s' but no '%s'"), *It->GetName(), *MusicProperty.ToString(), *CueProperty.ToString())); }
		if (bHoldSongForIntro && (!It->FindFunction(StopSongEvent) || !It->FindFunction(StartSongEvent)))
		{
			Fail(FString::Printf(TEXT("%s has no '%s'/'%s' event (or untick Hold Song For Intro)"), *It->GetName(), *StopSongEvent.ToString(), *StartSongEvent.ToString()));
		}
		if (SongLength <= 0.f)
		{
			const FStructProperty* T = FindFProperty<FStructProperty>(C, SongTimerProperty);
			if (!T || T->Struct != TBaseStructure<FTimerHandle>::Get())
			{
				Fail(FString::Printf(TEXT("%s has no FTimerHandle '%s' (or set Song Length)"), *It->GetName(), *SongTimerProperty.ToString()));
			}
		}
		Sources.Add(*It);
	}
	if (Sources.IsEmpty()) { Fail(FString::Printf(TEXT("no actor has a '%s' variable (BP_Spline missing from this level?)"), *MusicProperty.ToString())); }
	if (bFailed) { return; }

	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (!PC) { Fail(TEXT("no player controller")); return; }
	Widget = CreateWidget<USongHUDWidget>(PC, USongHUDWidget::StaticClass());
	check(Widget);
	USongHUDWidget::FParams P;
	P.Title = SongTitle;
	P.Font = Font;
	P.BigSize = BigSize; P.SmallSize = SmallSize; P.TimeSize = TimeSize;
	P.TextColor = TextColor; P.ShadowColor = ShadowColor; P.ShadowOffset = ShadowOffset;
	P.BarFill = BarFill; P.BarBack = BarBack; P.BarSize = BarSize;
	P.CornerAnchor = CornerAnchor; P.CornerOffset = CornerOffset;
	P.bShowTime = bShowTime;
	Widget->Setup(P);
	Widget->SetRenderOpacity(0.f);
	Widget->AddToViewport(-1); // under every other widget (blackout, credits, ...)
	UE_LOG(LogSongHUD, Log, TEXT("watching %d source(s)"), Sources.Num());
}

void ASongHUD::EndPlay(const EEndPlayReason::Type Why)
{
	if (Widget) { Widget->RemoveFromParent(); Widget = nullptr; }
	Super::EndPlay(Why);
}

UAudioComponent* ASongHUD::GetComp(AActor* Src, FName Prop) const
{
	if (!IsValid(Src)) { return nullptr; }
	const FObjectProperty* P = FindFProperty<FObjectProperty>(Src->GetClass(), Prop);
	check(P); // checked in BeginPlay
	UAudioComponent* C = Cast<UAudioComponent>(P->GetObjectPropertyValue_InContainer(Src));
	return IsValid(C) && C->IsPlaying() ? C : nullptr;
}

AActor* ASongHUD::FindStarted() const
{
	for (AActor* A : Sources)
	{
		if (GetComp(A, CueProperty) || GetComp(A, MusicProperty)) { return A; }
	}
	return nullptr;
}

const FTimerHandle* ASongHUD::GetSongTimer() const
{
	AActor* Src = Source.Get();
	if (SongLength > 0.f || !IsValid(Src)) { return nullptr; }
	const FStructProperty* P = FindFProperty<FStructProperty>(Src->GetClass(), SongTimerProperty);
	check(P); // checked in BeginPlay
	return P->ContainerPtrToValuePtr<FTimerHandle>(Src);
}

bool ASongHUD::GetTimerProgress(float& OutElapsed, float& OutDuration) const
{
	const FTimerHandle* H = GetSongTimer();
	if (!H) { return false; }
	const FTimerManager& TM = GetWorldTimerManager();
	if (!TM.IsTimerActive(*H)) { return false; }
	OutDuration = TM.GetTimerRate(*H);
	OutElapsed = TM.GetTimerElapsed(*H);
	return OutDuration > 0.f;
}

void ASongHUD::CallEvent(AActor* Src, FName Event)
{
	UFunction* F = IsValid(Src) ? Src->FindFunction(Event) : nullptr;
	if (!F) { Fail(FString::Printf(TEXT("cannot call '%s' on %s"), *Event.ToString(), *GetNameSafe(Src))); return; }
	if (F->ParmsSize != 0) { Fail(FString::Printf(TEXT("'%s' must take no inputs"), *Event.ToString())); return; }
	Src->ProcessEvent(F, nullptr);
	UE_LOG(LogSongHUD, Log, TEXT("called %s.%s"), *Src->GetName(), *Event.ToString());
}

void ASongHUD::BeginIntro(AActor* Src)
{
	Source = Src;
	Phase = EPhase::Intro;
	bBarShown = false;
	LastElapsed = 0.f;
	if (!bFake) { Duration = 0.f; }
	Clock = 0.f;
	Widget->SetRenderOpacity(0.f);
	Widget->SetLayout(0.f);
	Widget->SetBarAlpha(0.f);
	if (Src && bHoldSongForIntro) { CallEvent(Src, StopSongEvent); } // before the first note spawns
	UE_LOG(LogSongHUD, Log, TEXT("intro '%s'%s"), *SongTitle, bFake ? TEXT(" (fake)") : TEXT(""));
}

void ASongHUD::FakeStart(float Secs)
{
	if (!Widget) { Fail(TEXT("ddd.SongHUD: HUD not set up (see earlier error)")); return; }
	bFake = true;
	Duration = FMath::Max(Secs, 1.f);
	BeginIntro(nullptr);
}

void ASongHUD::Tick(float Dt)
{
	Super::Tick(Dt);
	if (bFailed || !Widget) { return; }
	Clock += Dt;

	// jumpscare = first layer: hidden from the fail until the next run, then fade back
	Mask = bScareHide ? 0.f : FMath::Min(1.f, Mask + Dt / ScareReturnTime);
	Widget->SetMask(Mask);

	switch (Phase)
	{
	case EPhase::Waiting:
		Widget->SetRenderOpacity(0.f);
		if (AActor* Src = FindStarted()) { BeginIntro(Src); }
		break;

	case EPhase::Intro:
		Widget->SetRenderOpacity(FadeInTime > 0.f ? FMath::Clamp(Clock / FadeInTime, 0.f, 1.f) : 1.f);
		Widget->SetLayout((Clock - IntroHold) / MoveTime);
		if (Clock >= IntroHold + MoveTime)
		{
			Widget->SetLayout(1.f);
			Phase = EPhase::Lead;
			Clock = 0.f;
			if (!bFake && bHoldSongForIntro) { CallEvent(Source.Get(), StartSongEvent); }
		}
		break;

	case EPhase::Lead: // corner title, waits for SongTimer (or the music in SongLength mode)
	{
		Widget->SetRenderOpacity(1.f);
		Widget->SetBarAlpha(bBarShown ? 1.f : 0.f); // after a fail: stays up at 0:00
		float E = 0.f, D = 0.f;
		const bool bReal = !bFake && GetComp(Source.Get(), CueProperty); // cue playing = this StartSong's run, not a stale timer
		bool bGo = bFake;
		if (bReal && SongLength > 0.f) { bGo = GetComp(Source.Get(), MusicProperty) != nullptr; Duration = SongLength; }
		else if (bReal && GetTimerProgress(E, D)) { bGo = true; Duration = D; }
		if (bGo)
		{
			Phase = EPhase::Playing;
			Clock = 0.f;
			LastElapsed = E;
			bScareHide = false; // next run started: fade back in
			Widget->SetProgress(E, Duration);
			UE_LOG(LogSongHUD, Log, TEXT("rhythm part %.1fs, %.1fs in (%s)"), Duration, E, bFake ? TEXT("fake") : SongLength > 0.f ? TEXT("SongLength") : TEXT("SongTimer"));
		}
		break;
	}

	case EPhase::Playing:
	{
		float Elapsed = Clock, D = Duration;
		bool bStopped;
		if (bFake || SongLength > 0.f)
		{
			bStopped = bFake ? Clock >= Duration : !(GetComp(Source.Get(), MusicProperty) || GetComp(Source.Get(), CueProperty));
			bStopped = bStopped || Clock >= Duration;
		}
		else
		{
			const bool bTimer = GetTimerProgress(Elapsed, D);
			const bool bSong = GetComp(Source.Get(), CueProperty) || GetComp(Source.Get(), MusicProperty);
			bStopped = !bTimer || !bSong;
			if (bTimer) { Duration = D; LastElapsed = Elapsed; }
			else { Elapsed = LastElapsed + Dt; } // fired (near the end) or cleared by a fail (anywhere)
		}
		if (bStopped)
		{
			if (Elapsed >= Duration * 0.95f) // done (SongComplete / cliffhanger)
			{
				Widget->SetProgress(Duration, Duration);
				Phase = EPhase::Outro;
				OutroFrom = Widget->GetRenderOpacity();
				UE_LOG(LogSongHUD, Log, TEXT("rhythm part done"));
			}
			else // failed: BP restarts it. keep the title, wait for the next run
			{
				Phase = EPhase::Lead;
				bScareHide = true;
				Mask = 0.f;
				Widget->SetMask(0.f);
				Widget->SetProgress(0.f, Duration);
				UE_LOG(LogSongHUD, Log, TEXT("stopped early at %.1fs (restart)"), Elapsed);
			}
			Clock = 0.f;
			bFake = false;
			break;
		}
		if (Clock >= BarFadeTime) { bBarShown = true; }
		Widget->SetBarAlpha(bBarShown ? 1.f : Clock / BarFadeTime);
		Widget->SetProgress(Elapsed, Duration);
		break;
	}

	case EPhase::Outro:
		Widget->SetRenderOpacity(OutroFrom * (1.f - FMath::Clamp(Clock / OutroFade, 0.f, 1.f)));
		if (Clock >= OutroFade) { Phase = EPhase::Done; }
		break;

	case EPhase::Done:
		break;
	}
}
