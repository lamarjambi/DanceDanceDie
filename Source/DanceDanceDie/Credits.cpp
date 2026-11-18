#include "Credits.h"

#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/RetainerBox.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "Sound/SoundBase.h"
#include "Styling/CoreStyle.h"
#include "UObject/ConstructorHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogCredits, Log, All);

static void CreditsError(const FString& Msg)
{
	UE_LOG(LogCredits, Error, TEXT("%s"), *Msg);
	if (GEngine) { GEngine->AddOnScreenDebugMessage(-1, 15.f, FColor::Red, Msg); }
}

// ---------------------------------------------------------------- widget

void UCreditsWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	check(WidgetTree);

	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
	WidgetTree->RootWidget = Root;

	UImage* Backdrop = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("Backdrop"));
	Backdrop->SetColorAndOpacity(FLinearColor::Black);
	{
		UCanvasPanelSlot* S = Root->AddChildToCanvas(Backdrop);
		S->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
		S->SetOffsets(FMargin(0.f));
	}

	// whole roll goes through the retainer so the PS1 effect pixelates it like the menu buttons
	Retainer = WidgetTree->ConstructWidget<URetainerBox>(URetainerBox::StaticClass(), TEXT("Retainer"));
	{
		UCanvasPanelSlot* S = Root->AddChildToCanvas(Retainer);
		S->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
		S->SetOffsets(FMargin(0.f));
	}

	UCanvasPanel* Stage = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Stage"));
	Retainer->AddChild(Stage);

	Roll = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Roll"));
	RollSlot = Stage->AddChildToCanvas(Roll);
	RollSlot->SetAnchors(FAnchors(0.5f, 0.f));
	RollSlot->SetAlignment(FVector2D(0.5f, 0.f));
	RollSlot->SetAutoSize(true);
	RollSlot->SetPosition(FVector2D(0.f, 100000.f)); // off screen until the first tick knows the screen height
}

void UCreditsWidget::Setup(const FParams& In)
{
	check(Roll && Retainer);
	P = In;

	if (P.Effect)
	{
		Retainer->SetEffectMaterial(P.Effect);
		Retainer->SetTextureParameter(TEXT("Texture"));
	}

	Roll->ClearChildren();
	LastLine = nullptr;
	for (int32 i = 0; i < P.Lines.Num(); ++i)
	{
		const FCreditLine& L = P.Lines[i];
		UWidget* W = nullptr;
		float PadTop = 0.f;
		if (L.Style == ECreditStyle::Gap)
		{
			USpacer* Sp = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass());
			Sp->SetSize(FVector2D(1.f, P.GapSize));
			W = Sp;
		}
		else
		{
			const FCreditLook& Look = L.Style == ECreditStyle::Title ? P.Title : L.Style == ECreditStyle::Heading ? P.Heading : P.Name;
			UTextBlock* T = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
			T->SetText(FText::FromString(L.Text));
			T->SetFont(P.Font ? FSlateFontInfo(P.Font, Look.Size) : FCoreStyle::GetDefaultFontStyle("Regular", Look.Size));
			T->SetColorAndOpacity(FSlateColor(Look.Color));
			T->SetShadowColorAndOpacity(Look.ShadowColor);
			T->SetShadowOffset(Look.ShadowOffset);
			T->SetJustification(ETextJustify::Center);
			W = T;
			PadTop = Look.PadTop;
		}
		UVerticalBoxSlot* S = Roll->AddChildToVerticalBox(W);
		S->SetHorizontalAlignment(HAlign_Center);
		S->SetPadding(FMargin(0.f, PadTop, 0.f, 0.f));
		LastLine = W;
	}
	Phase = EPhase::Wait;
	Clock = 0.f;
	bPlaced = false;
}

void UCreditsWidget::Leave()
{
	if (Phase == EPhase::FadeOut || Phase == EPhase::GlitchOut || Phase == EPhase::Done) { return; }
	Clock = 0.f;
	if (P.bGlitchOut) { Phase = EPhase::GlitchOut; Glitch.Start(Retainer, P.Effect, P.GlitchSound, P.PixelRows); }
	else { Phase = EPhase::FadeOut; }
}

void UCreditsWidget::NativeTick(const FGeometry& MyGeometry, float Dt)
{
	Super::NativeTick(MyGeometry, Dt);
	const float H = MyGeometry.GetLocalSize().Y;
	if (H <= 0.f || !RollSlot) { return; }
	if (!bPlaced) { Y = H; bPlaced = true; } // first line starts just below the bottom edge

	bool bFast = false;
	APlayerController* PC = P.bPlayerInput ? GetOwningPlayer() : nullptr;
	if (PC)
	{
		static const FKey SkipKeys[] = {
			EKeys::Escape, EKeys::Enter, EKeys::LeftMouseButton, EKeys::Gamepad_FaceButton_Right, EKeys::Gamepad_Special_Right,
		};
		for (const FKey& K : SkipKeys)
		{
			if (PC->WasInputKeyJustPressed(K)) { Leave(); break; }
		}
		bFast = PC->IsInputKeyDown(EKeys::SpaceBar) || PC->IsInputKeyDown(EKeys::Down) || PC->IsInputKeyDown(EKeys::S)
			|| PC->IsInputKeyDown(EKeys::Gamepad_FaceButton_Bottom) || PC->IsInputKeyDown(EKeys::Gamepad_DPad_Down);
	}
	const float Mult = bFast ? P.FastMultiplier : 1.f;
	if (Phase != EPhase::GlitchOut) { FGlitchFX::FitBlocks(Retainer, P.PixelRows); }
	Clock += Dt;

	switch (Phase)
	{
	case EPhase::Wait:
		if (Clock >= P.StartDelay) { Phase = EPhase::Roll; Clock = 0.f; }
		break;

	case EPhase::Roll:
	{
		Y -= P.Speed * Mult * Dt;
		const float RollH = Roll->GetDesiredSize().Y;
		if (P.bStopLastLine && LastLine)
		{
			// stop when the last line's centre hits the screen centre
			const float Target = H * 0.5f - (RollH - LastLine->GetDesiredSize().Y * 0.5f);
			if (Y <= Target) { Y = Target; Phase = EPhase::Hold; Clock = 0.f; }
		}
		else if (Y <= -RollH) { Phase = EPhase::FadeOut; Clock = P.FadeTime; } // already off screen
		break;
	}

	case EPhase::Hold:
		Clock += Dt * (Mult - 1.f);
		if (Clock >= P.EndHold) { Leave(); }
		break;

	case EPhase::GlitchOut:
		Glitch.Frame(Retainer, Clock / P.GlitchTime, P.GlitchSound);
		if (Clock >= P.GlitchTime) { Retainer->SetRenderOpacity(0.f); Phase = EPhase::Done; } // hard cut to black
		break;

	case EPhase::FadeOut:
		Retainer->SetRenderOpacity(FMath::Clamp(1.f - Clock / P.FadeTime, 0.f, 1.f));
		if (Clock >= P.FadeTime) { Phase = EPhase::Done; }
		break;

	case EPhase::Done:
		break;
	}

	RollSlot->SetPosition(FVector2D(0.f, Y));
}

// ---------------------------------------------------------------- game mode

ACreditsGameMode::ACreditsGameMode()
{
	DefaultPawnClass = nullptr; // nothing to control, the widget covers the screen
	PrimaryActorTick.bCanEverTick = true;

	static ConstructorHelpers::FObjectFinder<UFont> Vt323(TEXT("/Game/UI/Fonts/VT323-Regular_Font.VT323-Regular_Font"));
	Font = Vt323.Object;
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Ps1(TEXT("/Game/PostProcess/M_UI_PS1.M_UI_PS1"));
	Effect = Ps1.Object;
	static ConstructorHelpers::FObjectFinder<USoundBase> Click(TEXT("/Game/Rhythm/Audio/glitch-click.glitch-click"));
	GlitchSound = Click.Object;

	// exact PLAY / QUIT values (WBP_MainMenu Quit_Text)
	const FLinearColor Lime(0.25f, 0.9f, 0.08f);
	const FLinearColor Magenta(0.45f, 0.02f, 0.42f);

	TitleLook.Size = 120;
	TitleLook.Color = Lime;
	TitleLook.ShadowColor = Magenta;
	TitleLook.ShadowOffset = FVector2D(5.f, 5.f); // = menu

	HeadingLook.Size = 44; // smaller gets eaten by the PS1 pixelation
	HeadingLook.Color = Magenta;
	HeadingLook.PadTop = 0.f;

	NameLook.Size = 54;
	NameLook.Color = Lime;
	NameLook.ShadowColor = Magenta;
	NameLook.ShadowOffset = FVector2D(5.f, 5.f); // = menu
	NameLook.PadTop = 2.f;

	auto T = [this](const TCHAR* S) { Lines.Emplace(ECreditStyle::Title, S); };
	auto Hd = [this](const TCHAR* S) { Lines.Emplace(ECreditStyle::Gap, TEXT("")); Lines.Emplace(ECreditStyle::Heading, S); };
	auto N = [this](const TCHAR* S) { Lines.Emplace(ECreditStyle::Name, S); };

	T(TEXT("DANCE, DANCE, DIE!!"));
	Hd(TEXT("A GAME BY"));           N(TEXT("J@mbo"));
	Hd(TEXT("DESIGN, CODE & ART"));  N(TEXT("J@mbo"));
	Hd(TEXT("STARRING"));            N(TEXT("Michelle"));
	Hd(TEXT("AND HER FRIENDS"));     N(TEXT("Mateo")); N(TEXT("Ava")); N(TEXT("Cass")); N(TEXT("Ade"));
	Hd(TEXT("MUSIC"));               N(TEXT("RiVER - Another")); N(TEXT("RiVER - 7"));
	Hd(TEXT("CHARACTERS & DANCES")); N(TEXT("Mixamo"));
	Hd(TEXT("MAPS"));               N(TEXT("City Map Creator by Dexsoft Games")); N(TEXT("Night Club [UE] by IL.ranch"));
	Hd(TEXT("PLAYTESTERS"));         N(TEXT("Mateo")); N(TEXT("River"));
	Hd(TEXT("MADE WITH"));           N(TEXT("Unreal Engine 5.4"));
	Lines.Emplace(ECreditStyle::Gap, TEXT(""));
	Lines.Emplace(ECreditStyle::Gap, TEXT(""));
	T(TEXT("thanks for playing"));
}

void ACreditsGameMode::BeginPlay()
{
	Super::BeginPlay();

	if (!Font) { CreditsError(TEXT("Credits: Font not set, falling back to Roboto")); }
	if (!Effect) { CreditsError(TEXT("Credits: Effect (M_UI_PS1) not set, no PS1 look")); }
	if (Lines.Num() == 0) { CreditsError(TEXT("Credits: Lines is empty")); }

	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	check(PC);
	Screen = CreateWidget<UCreditsWidget>(PC, UCreditsWidget::StaticClass());
	check(Screen);

	Screen->Setup(MakeParams());
	Screen->AddToViewport(100);

	PC->SetInputMode(FInputModeGameOnly());
	PC->bShowMouseCursor = false;
}

UCreditsWidget::FParams ACreditsGameMode::MakeParams() const
{
	UCreditsWidget::FParams Prm;
	Prm.Lines = Lines;
	Prm.Font = Font;
	Prm.Title = TitleLook;
	Prm.Heading = HeadingLook;
	Prm.Name = NameLook;
	Prm.GapSize = GapSize;
	Prm.PixelRows = PixelRows;
	Prm.Effect = Effect;
	Prm.Speed = ScrollSpeed;
	Prm.FastMultiplier = FastMultiplier;
	Prm.StartDelay = StartDelay;
	Prm.FadeTime = FadeTime;
	Prm.EndHold = EndHold;
	Prm.bStopLastLine = bStopOnLastLine;
	Prm.GlitchTime = GlitchTime;
	Prm.GlitchSound = GlitchSound;
	return Prm;
}

void ACreditsGameMode::Tick(float Dt)
{
	Super::Tick(Dt);
	if (!Screen || bLeaving || !Screen->IsDone()) { return; }
	bLeaving = true;
	if (NextLevel.IsNone()) { CreditsError(TEXT("Credits: NextLevel is empty, staying put")); return; }
	UGameplayStatics::OpenLevel(this, NextLevel);
}
