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

	BarRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("BarRow"));
	if (UVerticalBoxSlot* S = Box->AddChildToVerticalBox(BarRow)) { S->SetPadding(FMargin(4.f, 6.f, 0.f, 0.f)); }

	USizeBox* BarBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("BarBox"));
	if (UHorizontalBoxSlot* S = BarRow->AddChildToHorizontalBox(BarBox)) { S->SetVerticalAlignment(VAlign_Center); }
	Bar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("Bar"));
	BarBox->AddChild(Bar);

	Time = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Time"));
	if (UHorizontalBoxSlot* S = BarRow->AddChildToHorizontalBox(Time))
	{
		S->SetVerticalAlignment(VAlign_Center);
		S->SetPadding(FMargin(14.f, 0.f, 0.f, 0.f));
	}
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
	Time->SetVisibility(P.bShowTime ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);

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
	BarRow->SetVisibility(A <= 0.f ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	BarRow->SetRenderOpacity(FMath::Clamp(A, 0.f, 1.f));
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
	Widget->AddToViewport(50); // under credits (1000)
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
	Playing = nullptr;
	BeginIntro(nullptr);
}

void ASongHUD::Tick(float Dt)
{
	Super::Tick(Dt);
	if (bFailed || !Widget) { return; }
	Clock += Dt;

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

	case EPhase::Lead: // corner title, bar waits for the music
	{
		Widget->SetRenderOpacity(1.f);
		Widget->SetBarAlpha(0.f);
		UAudioComponent* M = bFake ? nullptr : GetComp(Source.Get(), MusicProperty);
		if (bFake || M)
		{
			if (M)
			{
				Duration = M->Sound ? M->Sound->GetDuration() : 0.f;
				if (Duration <= 0.f || Duration >= INDEFINITELY_LOOPING_DURATION) { Fail(FString::Printf(TEXT("music '%s' has no usable duration"), *GetNameSafe(M->Sound))); return; }
			}
			Playing = M;
			Phase = EPhase::Playing;
			Clock = 0.f;
			Widget->SetProgress(0.f, Duration);
			UE_LOG(LogSongHUD, Log, TEXT("music %.1fs"), Duration);
		}
		break;
	}

	case EPhase::Playing:
	{
		const bool bStopped = bFake ? Clock >= Duration : !(Playing.IsValid() && Playing->IsPlaying());
		if (bStopped)
		{
			if (Clock >= Duration * 0.9f) // done (win / cliffhanger wind-down)
			{
				Phase = EPhase::Outro;
				OutroFrom = Widget->GetRenderOpacity();
				UE_LOG(LogSongHUD, Log, TEXT("song done"));
			}
			else // failed: BP restarts it. keep the title, wait for the music again
			{
				Phase = EPhase::Lead;
				Widget->SetProgress(0.f, Duration);
				UE_LOG(LogSongHUD, Log, TEXT("music stopped early (restart)"));
			}
			Clock = 0.f;
			Playing = nullptr;
			bFake = false;
			break;
		}
		Widget->SetBarAlpha(Clock / BarFadeTime);
		Widget->SetProgress(Clock, Duration);
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
