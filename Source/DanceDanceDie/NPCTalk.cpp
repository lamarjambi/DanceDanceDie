#include "NPCTalk.h"

#include "Blueprint/WidgetLayoutLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogNPCTalk, Log, All);

namespace
{
	constexpr float FlyTime = 0.28f;  // secs to fly in / out
	constexpr float FlyDist = 70.f;   // px it rises from
	constexpr float BobAmp = 4.f;     // px idle bob
	constexpr float BobSpeed = 3.f;

	float EaseOutBack(float T)
	{
		const float C1 = 1.70158f, C3 = C1 + 1.f;
		return 1.f + C3 * FMath::Pow(T - 1.f, 3.f) + C1 * FMath::Pow(T - 1.f, 2.f);
	}
}

TWeakObjectPtr<UUserWidget> ANPCTalker::ActiveSubtitle;
FTimerHandle ANPCTalker::SubtitleTimer;

// ---------------------------------------------------------------- prompt widget

void UNPCPromptWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	check(WidgetTree);

	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
	WidgetTree->RootWidget = Root;

	Icon = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("Icon"));
	IconSlot = Root->AddChildToCanvas(Icon);
	IconSlot->SetAlignment(FVector2D(0.5f, 1.f)); // bottom-center sits on the head point
	IconSlot->SetAutoSize(false);
	Icon->SetRenderOpacity(0.f);
	Icon->SetVisibility(ESlateVisibility::HitTestInvisible);
	SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UNPCPromptWidget::SetIcon(UTexture2D* Tex, float Size)
{
	check(Icon);
	IconSize = Size;
	Icon->SetBrushFromTexture(Tex);
	IconSlot->SetSize(FVector2D(Size, Size));
}

void UNPCPromptWidget::Step(float Dt, bool bWant, const FVector2D& ScreenPos, bool bOnScreen)
{
	Time += Dt;
	const float Target = (bWant && bOnScreen) ? 1.f : 0.f;
	Alpha = FMath::FInterpConstantTo(Alpha, Target, Dt, 1.f / FlyTime);

	if (Alpha <= 0.f)
	{
		Icon->SetRenderOpacity(0.f);
		return;
	}

	const float E = EaseOutBack(Alpha);
	const float Scale = UWidgetLayoutLibrary::GetViewportScale(this);
	const FVector2D P = ScreenPos / FMath::Max(Scale, KINDA_SMALL_NUMBER);
	const float Bob = FMath::Sin(Time * BobSpeed) * BobAmp * Alpha;

	IconSlot->SetPosition(FVector2D(P.X, P.Y + (1.f - E) * FlyDist + Bob));
	Icon->SetRenderOpacity(FMath::Clamp(Alpha * 1.5f, 0.f, 1.f));
	Icon->SetRenderTransformPivot(FVector2D(0.5f, 1.f));
	Icon->SetRenderScale(FVector2D(FMath::Max(E, 0.05f)));
}

// ---------------------------------------------------------------- talker

ANPCTalker::ANPCTalker()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	static ConstructorHelpers::FObjectFinder<UTexture2D> IconFinder(TEXT("/Game/UI/Images/keyboard_e"));
	PromptIcon = IconFinder.Object;
	static ConstructorHelpers::FClassFinder<UUserWidget> SubFinder(TEXT("/Game/UI/Navigation/WBP_Subtitle"));
	SubtitleClass = SubFinder.Class;
}

void ANPCTalker::BeginPlay()
{
	Super::BeginPlay();

	if (!NPC) { UE_LOG(LogNPCTalk, Error, TEXT("%s: NPC not set"), *GetName()); }
	if (!PromptIcon) { UE_LOG(LogNPCTalk, Error, TEXT("%s: PromptIcon not set"), *GetName()); }
	if (!SubtitleClass) { UE_LOG(LogNPCTalk, Error, TEXT("%s: SubtitleClass not set"), *GetName()); }
	if (Lines.IsEmpty()) { UE_LOG(LogNPCTalk, Error, TEXT("%s: no Lines"), *GetName()); }

	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	check(PC);
	Prompt = CreateWidget<UNPCPromptWidget>(PC, UNPCPromptWidget::StaticClass());
	Prompt->SetIcon(PromptIcon, PromptSize);
	Prompt->AddToViewport(50);
}

void ANPCTalker::EndPlay(const EEndPlayReason::Type Reason)
{
	if (Prompt) { Prompt->RemoveFromParent(); }
	Super::EndPlay(Reason);
}

bool ANPCTalker::IsClosestInRange(const FVector& PlayerLoc, float MyDist) const
{
	for (TActorIterator<ANPCTalker> It(GetWorld()); It; ++It)
	{
		const ANPCTalker* O = *It;
		if (O == this || !O->NPC) { continue; }
		const float D = FVector::Dist(PlayerLoc, O->NPC->GetActorLocation());
		if (D <= O->Radius && D < MyDist) { return false; }
	}
	return true;
}

void ANPCTalker::Tick(float Dt)
{
	Super::Tick(Dt);
	if (!Prompt || !NPC) { return; }

	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	APawn* P = PC ? PC->GetPawn() : nullptr;
	if (!P) { Prompt->Step(Dt, false, FVector2D::ZeroVector, false); return; }

	const FVector PlayerLoc = P->GetActorLocation();
	const float Dist = FVector::Dist(PlayerLoc, NPC->GetActorLocation());
	const bool bActive = Dist <= Radius && IsClosestInRange(PlayerLoc, Dist);

	FVector2D Screen;
	const bool bOnScreen = PC->ProjectWorldLocationToScreen(NPC->GetActorLocation() + FVector(0, 0, PromptHeight), Screen, true);
	Prompt->Step(Dt, bActive, Screen, bOnScreen);

	if (bActive && PC->WasInputKeyJustPressed(EKeys::E))
	{
		Say();
	}
}

void ANPCTalker::Say()
{
	if (Lines.IsEmpty() || !SubtitleClass) { return; }

	int32 I = FMath::RandRange(0, Lines.Num() - 1);
	if (Lines.Num() > 1 && I == LastLine) { I = (I + 1) % Lines.Num(); }
	LastLine = I;
	const FString Line = SpeakerName.IsEmpty() ? Lines[I] : FString::Printf(TEXT("%s: %s"), *SpeakerName, *Lines[I]);

	if (UUserWidget* Old = ActiveSubtitle.Get()) { Old->RemoveFromParent(); }
	GetWorldTimerManager().ClearTimer(SubtitleTimer);

	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	UUserWidget* Sub = CreateWidget<UUserWidget>(PC, SubtitleClass);
	check(Sub);

	UTextBlock* Text = nullptr;
	Sub->WidgetTree->ForEachWidget([&Text](UWidget* W)
	{
		if (!Text) { Text = Cast<UTextBlock>(W); }
	});
	if (!Text)
	{
		UE_LOG(LogNPCTalk, Error, TEXT("%s has no TextBlock"), *SubtitleClass->GetName());
		return;
	}
	Text->SetText(FText::FromString(Line));
	Sub->AddToViewport(60);
	ActiveSubtitle = Sub;

	TWeakObjectPtr<UUserWidget> Weak = Sub;
	GetWorldTimerManager().SetTimer(SubtitleTimer, [Weak]()
	{
		if (UUserWidget* W = Weak.Get()) { W->RemoveFromParent(); }
	}, SubtitleTime + PerCharTime * Line.Len(), false);
}
