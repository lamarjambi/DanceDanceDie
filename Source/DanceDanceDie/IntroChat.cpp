#include "IntroChat.h"
#include "LoadingScreen.h"

#include "Blueprint/WidgetTree.h"
#include "Components/AudioComponent.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/Texture2D.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "Styling/CoreStyle.h"
#include "UObject/ConstructorHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogIntroChat, Log, All);

namespace IntroChat
{
	constexpr float FadeInTime = 0.8f;
	constexpr float PopTime = 0.38f;
	constexpr float LeaveTime = 0.45f;
	constexpr int32 MaxBubbles = 7;
	constexpr int32 BrightCount = 4; // newest N stay full opacity, older ones dim
	constexpr float DimOpacity = 0.4f;
	constexpr float BubbleMaxWidth = 520.f;
	constexpr float PhotoWidth = 320.f;
	constexpr int32 PhotoWeight = 4; // a photo counts as 4 text bubbles toward MaxBubbles
}

// ---------------------------------------------------------------- widget

void UIntroChatWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	check(WidgetTree);

	// font comes from BP_IntroChatGameMode > Chat Font
	const AIntroChatGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<AIntroChatGameMode>() : nullptr;
	HeavyFont = GM ? GM->ChatFont.Get() : nullptr;
	if (!HeavyFont)
	{
		UE_LOG(LogIntroChat, Error, TEXT("IntroChat: Chat Font not set, falling back to Roboto"));
	}

	Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
	WidgetTree->RootWidget = Root;

	auto Fill = [](UCanvasPanelSlot* S)
	{
		S->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
		S->SetOffsets(FMargin(0.f));
	};

	// backdrop
	Backdrop = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("Backdrop"));
	Backdrop->SetColorAndOpacity(FLinearColor(0.006f, 0.006f, 0.01f, 1.f));
	Backdrop->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
	Fill(Root->AddChildToCanvas(Backdrop));

	// header
	UVerticalBox* Header = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Header"));
	TitleText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Title"));
	TitleText->SetFont(SenderFont(34));
	TitleText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	SubtitleText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Subtitle"));
	SubtitleText->SetFont(BodyFont(19));
	SubtitleText->SetColorAndOpacity(FSlateColor(FLinearColor(0.55f, 0.55f, 0.6f, 1.f)));
	Header->AddChildToVerticalBox(TitleText);
	Header->AddChildToVerticalBox(SubtitleText);
	{
		UCanvasPanelSlot* S = Root->AddChildToCanvas(Header);
		S->SetAnchors(FAnchors(0.5f, 0.06f));
		S->SetAlignment(FVector2D(0.f, 0.f));
		S->SetAutoSize(true);
		S->SetPosition(FVector2D(0.f, 0.f));
	}

	// bubble stack, newest at the bottom
	Stack = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Stack"));
	USpacer* Spacer = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass(), TEXT("TopSpacer"));
	Stack->AddChildToVerticalBox(Spacer)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	{
		UCanvasPanelSlot* S = Root->AddChildToCanvas(Stack);
		S->SetAnchors(FAnchors(0.5f, 0.15f, 0.92f, 0.9f));
		S->SetOffsets(FMargin(0.f));
	}

	// skip hint
	UTextBlock* Hint = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("SkipHint"));
	Hint->SetText(FText::FromString(TEXT("press space to skip")));
	Hint->SetFont(BodyFont(18));
	Hint->SetColorAndOpacity(FSlateColor(FLinearColor(1.f, 1.f, 1.f, 0.25f)));
	{
		UCanvasPanelSlot* S = Root->AddChildToCanvas(Hint);
		S->SetAnchors(FAnchors(1.f, 1.f));
		S->SetAlignment(FVector2D(1.f, 1.f));
		S->SetAutoSize(true);
		S->SetPosition(FVector2D(-40.f, -30.f));
	}

	// fade overlay on top
	FadeOverlay = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("Fade"));
	FadeOverlay->SetColorAndOpacity(FLinearColor::Black);
	FadeOverlay->SetVisibility(ESlateVisibility::HitTestInvisible);
	Fill(Root->AddChildToCanvas(FadeOverlay));
}

void UIntroChatWidget::NativeConstruct()
{
	Super::NativeConstruct();

	TitleText->SetText(FText::FromString(ChatTitle));
	SubtitleText->SetText(FText::FromString(ChatSubtitle));
	if (Background)
	{
		Backdrop->SetBrushFromTexture(Background);
		Backdrop->SetColorAndOpacity(FLinearColor(0.4f, 0.4f, 0.45f, 1.f));
	}
	if (Messages.Num() == 0)
	{
		UE_LOG(LogIntroChat, Error, TEXT("IntroChat has no messages"));
	}
	Phase = EPhase::FadeIn;
	PhaseTime = 0.f;
}

FSlateFontInfo UIntroChatWidget::SenderFont(int32 Size) const
{
	return HeavyFont ? FSlateFontInfo(HeavyFont.Get(), Size) : FCoreStyle::GetDefaultFontStyle("Bold", Size);
}

FSlateFontInfo UIntroChatWidget::BodyFont(int32 Size) const
{
	return HeavyFont ? FSlateFontInfo(HeavyFont.Get(), Size) : FCoreStyle::GetDefaultFontStyle("Regular", Size);
}

UWidget* UIntroChatWidget::MakeBubble(const FString& Sender, const FString& Body, bool bCreepy, UTexture2D* Photo, UTextBlock*& OutBody)
{
	USizeBox* Box = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	Box->SetMaxDesiredWidth(IntroChat::BubbleMaxWidth);

	UBorder* B = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	FSlateBrush Brush;
	Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
	Brush.TintColor = FSlateColor(bCreepy ? FLinearColor(0.3f, 0.f, 0.02f, 0.95f) : FLinearColor(0.035f, 0.035f, 0.045f, 0.9f));
	Brush.OutlineSettings.CornerRadii = FVector4(20.f, 20.f, 20.f, 6.f); // tail corner bottom-left
	Brush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
	Brush.OutlineSettings.Width = bCreepy ? 1.5f : 1.f;
	Brush.OutlineSettings.Color = FSlateColor(bCreepy ? FLinearColor(1.f, 0.1f, 0.1f, 0.7f) : FLinearColor(1.f, 1.f, 1.f, 0.07f));
	B->SetBrush(Brush);
	B->SetPadding(FMargin(18.f, 10.f, 18.f, 12.f));

	UVerticalBox* V = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());

	UTextBlock* NameText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	NameText->SetText(FText::FromString(Sender.ToUpper()));
	NameText->SetFont(SenderFont(16));
	NameText->SetColorAndOpacity(FSlateColor(bCreepy ? FLinearColor(1.f, 0.2f, 0.2f) : NameColor));
	V->AddChildToVerticalBox(NameText)->SetPadding(FMargin(0.f, 0.f, 0.f, 3.f));

	if (Photo)
	{
		const float W = Photo->GetSurfaceWidth();
		const float H = Photo->GetSurfaceHeight();
		checkf(W > 0.f && H > 0.f, TEXT("IntroChat photo %s has no size"), *Photo->GetName());
		UImage* Img = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		Img->SetBrushFromTexture(Photo);
		// fixed box so the photo is always PhotoWidth wide regardless of texture res
		USizeBox* PhotoBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		PhotoBox->SetWidthOverride(IntroChat::PhotoWidth);
		PhotoBox->SetHeightOverride(IntroChat::PhotoWidth * H / W);
		PhotoBox->SetContent(Img);
		V->AddChildToVerticalBox(PhotoBox)->SetPadding(FMargin(0.f, 2.f, 0.f, Body.IsEmpty() ? 0.f : 6.f));
	}

	UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	Text->SetText(FText::FromString(Body));
	Text->SetFont(BodyFont(28));
	Text->SetColorAndOpacity(FSlateColor(bCreepy ? FLinearColor(1.f, 0.85f, 0.85f) : FLinearColor::White));
	Text->SetAutoWrapText(true);
	if (!Photo || !Body.IsEmpty())
	{
		V->AddChildToVerticalBox(Text);
	}

	B->SetContent(V);
	Box->SetContent(B);
	OutBody = Text;
	return Box;
}

UWidget* UIntroChatWidget::MakeSystemLine(const FString& Body)
{
	UTextBlock* T = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	T->SetText(FText::FromString(Body));
	T->SetFont(BodyFont(19));
	T->SetColorAndOpacity(FSlateColor(FLinearColor(0.6f, 0.6f, 0.65f, 1.f)));
	return T;
}

void UIntroChatWidget::PushWidget(UWidget* W, bool bCreepy, bool bSystem, int32 Weight)
{
	UVerticalBoxSlot* S = Stack->AddChildToVerticalBox(W);
	S->SetPadding(FMargin(0.f, 0.f, 0.f, 12.f));
	S->SetHorizontalAlignment(bSystem ? HAlign_Center : HAlign_Left);

	W->SetRenderOpacity(0.f);
	W->SetRenderTransformPivot(bSystem ? FVector2D(0.5f, 0.5f) : FVector2D(0.f, 1.f));

	FBubble Bub;
	Bub.W = W;
	Bub.bCreepy = bCreepy;
	Bub.bSystem = bSystem;
	Bub.Weight = Weight;
	Bubbles.Add(Bub);

	// dim old ones, kick out the oldest
	int32 Live = 0;
	for (int32 i = Bubbles.Num() - 1; i >= 0; --i)
	{
		if (Bubbles[i].LeaveAge >= 0.f) { continue; }
		Live += Bubbles[i].Weight;
		if (Live > IntroChat::MaxBubbles) { Bubbles[i].LeaveAge = 0.f; }
	}
}

void UIntroChatWidget::ShowTyping(const FString& Sender)
{
	HideTyping();
	UTextBlock* Dots = nullptr;
	TypingBubble = MakeBubble(Sender, TEXT("•"), false, nullptr, Dots);
	TypingDots = Dots;
	// wrap would lock the bubble to the 1-dot width and stack dots vertically
	TypingDots->SetAutoWrapText(false);
	PushWidget(TypingBubble, false, false);
	if (TypingSound)
	{
		TypingAudio = UGameplayStatics::SpawnSound2D(this, TypingSound);
	}
}

void UIntroChatWidget::HideTyping()
{
	if (TypingAudio)
	{
		TypingAudio->Stop();
		TypingAudio = nullptr;
	}
	if (!TypingBubble) { return; }
	Bubbles.RemoveAll([this](const FBubble& B) { return B.W == TypingBubble; });
	TypingBubble->RemoveFromParent();
	TypingBubble = nullptr;
	TypingDots = nullptr;
}

void UIntroChatWidget::ShowMessage(const FIntroChatMessage& M)
{
	const bool bSystem = M.Sender.IsEmpty();
	if (bSystem)
	{
		PushWidget(MakeSystemLine(M.Body), false, true);
	}
	else
	{
		UTextBlock* Unused = nullptr;
		PushWidget(MakeBubble(M.Sender, M.Body, M.bCreepy, M.Photo, Unused), M.bCreepy, false, M.Photo ? IntroChat::PhotoWeight : 1);
	}

	if (PingSound && !bSystem)
	{
		UGameplayStatics::PlaySound2D(this, PingSound);
	}
	if (M.bCreepy && !bPlayedCreepy && CreepySound)
	{
		bPlayedCreepy = true;
		UGameplayStatics::PlaySound2D(this, CreepySound);
	}
}

void UIntroChatWidget::Skip()
{
	if (Phase == EPhase::FadeOut || Phase == EPhase::Done) { return; }
	if (TypingAudio) { TypingAudio->Stop(); TypingAudio = nullptr; }
	Phase = EPhase::FadeOut;
	PhaseTime = 0.f;
}

void UIntroChatWidget::Step(float Dt)
{
	TotalTime += Dt;
	PhaseTime += Dt;

	// slow push-in on the background
	const float Zoom = 1.f + 0.06f * FMath::Clamp(TotalTime / 30.f, 0.f, 1.f);
	Backdrop->SetRenderScale(FVector2D(Zoom));

	// bubble anims
	int32 Rank = 0;
	for (int32 i = Bubbles.Num() - 1; i >= 0; --i)
	{
		FBubble& B = Bubbles[i];
		B.Age += Dt;

		const float T = FMath::Clamp(B.Age / IntroChat::PopTime, 0.f, 1.f);
		const float Ease = 1.f - FMath::Pow(1.f - T, 3.f);
		const float C1 = 1.70158f, C3 = C1 + 1.f, Tb = T - 1.f;
		const float Back = 1.f + C3 * Tb * Tb * Tb + C1 * Tb * Tb;

		FVector2D Trans = B.bSystem ? FVector2D(0.f, (1.f - Ease) * 12.f) : FVector2D((1.f - Ease) * 70.f, (1.f - Ease) * 24.f);
		const float Scale = B.bSystem ? 1.f : FMath::Lerp(0.8f, 1.f, Back);
		float Opacity = Ease;

		if (B.LeaveAge < 0.f)
		{
			const float Target = Rank < IntroChat::BrightCount ? 1.f : IntroChat::DimOpacity;
			B.Dim = FMath::FInterpTo(B.Dim, Target, Dt, 5.f);
			++Rank;
		}
		Opacity *= B.Dim;

		if (B.bCreepy && T >= 1.f)
		{
			Trans += FVector2D(FMath::FRandRange(-2.5f, 2.5f), FMath::FRandRange(-1.5f, 1.5f));
		}

		if (B.LeaveAge >= 0.f)
		{
			B.LeaveAge += Dt;
			const float L = FMath::Clamp(B.LeaveAge / IntroChat::LeaveTime, 0.f, 1.f);
			Opacity *= 1.f - L;
			Trans.Y -= 30.f * L;
			if (L >= 1.f)
			{
				B.W->RemoveFromParent();
				Bubbles.RemoveAt(i);
				continue;
			}
		}

		B.W->SetRenderOpacity(Opacity);
		B.W->SetRenderTransform(FWidgetTransform(Trans, FVector2D(Scale), FVector2D::ZeroVector, 0.f));
	}

	auto BeginMessage = [this]()
	{
		PhaseTime = 0.f;
		if (!Messages.IsValidIndex(Index))
		{
			Phase = EPhase::EndHold;
			return;
		}
		const FIntroChatMessage& M = Messages[Index];
		if (M.TypingTime > 0.f && !M.Sender.IsEmpty())
		{
			ShowTyping(M.Sender);
		}
		Phase = EPhase::Typing;
	};

	switch (Phase)
	{
	case EPhase::FadeIn:
		FadeOverlay->SetRenderOpacity(1.f - FMath::Clamp(PhaseTime / IntroChat::FadeInTime, 0.f, 1.f));
		if (PhaseTime >= IntroChat::FadeInTime)
		{
			FadeOverlay->SetRenderOpacity(0.f);
			BeginMessage();
		}
		break;

	case EPhase::Typing:
	{
		const FIntroChatMessage& M = Messages[Index];
		if (TypingDots)
		{
			const int32 N = 1 + (int32(PhaseTime / 0.22f) % 3);
			FString S;
			for (int32 d = 0; d < N; ++d) { S += d ? TEXT("  •") : TEXT("•"); }
			TypingDots->SetText(FText::FromString(S));
		}
		if (PhaseTime >= M.TypingTime)
		{
			HideTyping();
			ShowMessage(M);
			Phase = EPhase::Holding;
			PhaseTime = 0.f;
		}
		break;
	}

	case EPhase::Holding:
		if (PhaseTime >= Messages[Index].HoldTime)
		{
			++Index;
			BeginMessage();
		}
		break;

	case EPhase::EndHold:
		if (PhaseTime >= EndHoldTime)
		{
			Phase = EPhase::FadeOut;
			PhaseTime = 0.f;
		}
		break;

	case EPhase::FadeOut:
	{
		const float A = FadeOutTime > 0.f ? FMath::Clamp(PhaseTime / FadeOutTime, 0.f, 1.f) : 1.f;
		FadeOverlay->SetRenderOpacity(FMath::Max(A, FadeOverlay->GetRenderOpacity()));
		if (A >= 1.f) { Phase = EPhase::Done; }
		break;
	}

	case EPhase::Done:
		break;
	}
}

// ---------------------------------------------------------------- game mode

AIntroChatGameMode::AIntroChatGameMode()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bTickEvenWhenPaused = true;

	static ConstructorHelpers::FObjectFinder<USoundBase> Glitch(TEXT("/Game/Rhythm/Audio/glitch-click.glitch-click"));
	CreepySound = Glitch.Object;
	static ConstructorHelpers::FObjectFinder<USoundBase> Receiving(TEXT("/Game/UI/Audio/receiving.receiving"));
	PingSound = Receiving.Object;
	static ConstructorHelpers::FObjectFinder<USoundBase> Typing(TEXT("/Game/UI/Audio/typing.typing"));
	TypingSound = Typing.Object;
	static ConstructorHelpers::FObjectFinder<UFont> Vt323(TEXT("/Game/UI/Fonts/VT323-Regular_Font.VT323-Regular_Font"));
	ChatFont = Vt323.Object;
	static ConstructorHelpers::FObjectFinder<UTexture2D> MateoPic(TEXT("/Game/UI/Images/mateo-text.mateo-text"));
	static ConstructorHelpers::FObjectFinder<UTexture2D> AvaPic(TEXT("/Game/UI/Images/ava-text.ava-text"));
	static ConstructorHelpers::FObjectFinder<UTexture2D> BarPic(TEXT("/Game/UI/Images/bar-text.bar-text"));
	check(MateoPic.Object && AvaPic.Object && BarPic.Object);

	auto Add = [this](const TCHAR* Sender, const TCHAR* Body, float Typing, float Hold, bool bCreepy = false, UTexture2D* Photo = nullptr)
	{
		FIntroChatMessage M;
		M.Sender = Sender;
		M.Body = Body;
		M.TypingTime = Typing;
		M.HoldTime = Hold;
		M.bCreepy = bCreepy;
		M.Photo = Photo;
		Messages.Add(M);
	};

	Add(TEXT("Cass"),  TEXT("ok what is everyone wearing tonight"), 0.9f, 1.2f);
	Add(TEXT("Mateo"),   TEXT("sexy zombie"), 0.6f, 0.6f);
	Add(TEXT("Mateo"),   TEXT(""), 0.8f, 1.6f, false, MateoPic.Object);
	Add(TEXT("Cass"), TEXT("sure"), 0.4f, 0.6f);
	Add(TEXT("Ava"),  TEXT("i'm going as lara croft"), 0.9f, 0.6f);
	Add(TEXT("Ava"),  TEXT(""), 0.8f, 1.6f, false, AvaPic.Object);
	Add(TEXT("Cass"), TEXT("that's literally just your outfit"), 0.7f, 0.9f);
	Add(TEXT("Ava"),  TEXT("exactly. just lesbianism"), 0.8f, 1.3f);
	Add(TEXT("Cass"), TEXT("@michelle you better be in costume"), 0.8f, 1.4f);
	Add(TEXT("Mateo"),   TEXT("bar is called ultra bar, neon green and pink sign"), 1.0f, 1.4f);
	Add(TEXT("Ava"),  TEXT("we're heading in"), 0.6f, 1.6f);
	Add(TEXT("Mateo"),   TEXT("it's sooo quiet in here lol"), 1.2f, 0.6f);
	Add(TEXT("Mateo"),   TEXT(""), 0.8f, 1.6f, false, BarPic.Object);
	Add(TEXT("Ade"), TEXT("wait where is everyone"), 0.7f, 0.8f);
	Add(TEXT("Cass"), TEXT("mateo??"), 0.4f, 1.6f);
	Add(TEXT("Ade"),   TEXT("michelle hurry"), 1.4f, 1.2f);
	Add(TEXT("Ade"),   TEXT("come dance"), 1.8f, 0.9f, true);
	Add(TEXT("Mateo"),   TEXT("come dance"), 0.3f, 0.5f, true);
	Add(TEXT("Mateo"),   TEXT("COME DANCE"), 0.2f, 1.4f, true);
	Add(TEXT(""),      TEXT("Cass left the chat"), 0.f, 1.2f);
	Add(TEXT(""),      TEXT("Ava left the chat"), 0.f, 1.0f);
}

void AIntroChatGameMode::BeginPlay()
{
	Super::BeginPlay();
	if (NextLevel.IsNone())
	{
		UE_LOG(LogIntroChat, Error, TEXT("IntroChatGameMode: NextLevel is empty"));
	}
}

void AIntroChatGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// chat starts only once the loading screen is gone
	if (ULoadingScreenSubsystem::IsShowing(this)) { return; }

	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (!PC) { return; }

	if (!Chat)
	{
		Chat = CreateWidget<UIntroChatWidget>(PC, UIntroChatWidget::StaticClass());
		check(Chat);
		Chat->Messages = Messages;
		Chat->ChatTitle = ChatTitle;
		Chat->ChatSubtitle = ChatSubtitle;
		Chat->PingSound = PingSound;
		Chat->CreepySound = CreepySound;
		Chat->TypingSound = TypingSound;
		Chat->Background = Background;
		Chat->EndHoldTime = EndHoldTime;
		Chat->FadeOutTime = FadeOutTime;
		Chat->NameColor = NameColor;
		Chat->AddToViewport(100);

		PC->SetShowMouseCursor(false);
		PC->SetInputMode(FInputModeGameOnly());
		PC->SetIgnoreMoveInput(true);
		PC->SetIgnoreLookInput(true);
	}

	static const FKey SkipKeys[] = {
		EKeys::SpaceBar, EKeys::Enter, EKeys::Escape, EKeys::LeftMouseButton,
		EKeys::Gamepad_FaceButton_Bottom, EKeys::Gamepad_Special_Right,
	};
	for (const FKey& K : SkipKeys)
	{
		if (PC->WasInputKeyJustPressed(K)) { Chat->Skip(); break; }
	}

	Chat->Step(DeltaSeconds);

	if (Chat->IsDone() && !bLeaving)
	{
		bLeaving = true;
		if (NextLevel.IsNone())
		{
			UE_LOG(LogIntroChat, Error, TEXT("IntroChatGameMode: NextLevel is empty, staying put"));
			if (GEngine) { GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Red, TEXT("IntroChat: NextLevel is empty!")); }
			return;
		}
		UGameplayStatics::OpenLevel(this, NextLevel);
	}
}
