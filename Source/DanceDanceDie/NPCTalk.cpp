#include "NPCTalk.h"

#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Image.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/TextBlock.h"
#include "Components/WidgetComponent.h"
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
	constexpr float FlyDist = 40.f;   // cm it rises from
	constexpr float BobAmp = 3.f;     // cm idle bob
	constexpr float IconPx = 128.f;   // widget draw size
	constexpr float BobSpeed = 3.f;

	float EaseOutBack(float T)
	{
		const float C1 = 1.70158f, C3 = C1 + 1.f;
		return 1.f + C3 * FMath::Pow(T - 1.f, 3.f) + C1 * FMath::Pow(T - 1.f, 2.f);
	}
}

TWeakObjectPtr<UUserWidget> ANPCTalker::ActiveSubtitle;
FTimerHandle ANPCTalker::SubtitleTimer;
TArray<TWeakObjectPtr<UUserWidget>> ANPCTalker::HiddenSubtitles;

// ---------------------------------------------------------------- icon widget

void UNPCIconWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	check(WidgetTree);
	Icon = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("Icon"));
	WidgetTree->RootWidget = Icon;
}

void UNPCIconWidget::SetIcon(UTexture2D* Tex)
{
	check(Icon);
	Icon->SetBrushFromTexture(Tex);
}

// ---------------------------------------------------------------- talker

ANPCTalker::ANPCTalker()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	Prompt = CreateDefaultSubobject<UWidgetComponent>(TEXT("Prompt"));
	Prompt->SetupAttachment(RootComponent);
	Prompt->SetWidgetSpace(EWidgetSpace::World);
	Prompt->SetWidgetClass(UNPCIconWidget::StaticClass());
	Prompt->SetDrawSize(FVector2D(IconPx, IconPx));
	Prompt->SetPivot(FVector2D(0.5f, 1.f)); // bottom-center sits on the head point
	Prompt->SetTwoSided(true);
	Prompt->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Prompt->SetHiddenInGame(true);

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
	if (NPC)
	{
		const USkeletalMeshComponent* Mesh = NPC->FindComponentByClass<USkeletalMeshComponent>();
		if (!Mesh || Mesh->GetBoneIndex(HeadBone) == INDEX_NONE)
		{
			UE_LOG(LogNPCTalk, Error, TEXT("%s: NPC has no bone '%s'"), *GetName(), *HeadBone.ToString());
		}
	}

	Prompt->InitWidget();
	UNPCIconWidget* W = Cast<UNPCIconWidget>(Prompt->GetUserWidgetObject());
	check(W);
	W->SetIcon(PromptIcon);
}

void ANPCTalker::EndPlay(const EEndPlayReason::Type Reason)
{
	// statics outlive a PIE session
	ActiveSubtitle.Reset();
	HiddenSubtitles.Reset();
	Super::EndPlay(Reason);
}

FVector ANPCTalker::HeadLocation() const
{
	const USkeletalMeshComponent* Mesh = NPC->FindComponentByClass<USkeletalMeshComponent>();
	return (Mesh && Mesh->GetBoneIndex(HeadBone) != INDEX_NONE) ? Mesh->GetBoneLocation(HeadBone) : NPC->GetActorLocation();
}

bool ANPCTalker::IsClosestInRange(const FVector& PlayerLoc, float MyDist) const
{
	for (TActorIterator<ANPCTalker> It(GetWorld()); It; ++It)
	{
		const ANPCTalker* O = *It;
		if (O == this || !O->NPC) { continue; }
		const float D = FVector::Dist2D(PlayerLoc, O->HeadLocation());
		if (D <= O->Radius && D < MyDist) { return false; }
	}
	return true;
}

void ANPCTalker::SuppressOtherSubtitles()
{
	// RemoveFromParent doesn't kill the widget object, so the weak ptr stays valid until GC.
	// Without this check a finished NPC line kept hiding all trigger text.
	UUserWidget* Mine = ActiveSubtitle.Get();
	if (Mine && !Mine->IsInViewport())
	{
		ActiveSubtitle.Reset();
		Mine = nullptr;
	}
	if (!Mine)
	{
		// NPC line gone: bring back any trigger text that's still alive
		for (const TWeakObjectPtr<UUserWidget>& W : HiddenSubtitles)
		{
			if (W.IsValid()) { W->SetVisibility(ESlateVisibility::HitTestInvisible); }
		}
		HiddenSubtitles.Reset();
		return;
	}

	TArray<UUserWidget*> Found;
	UWidgetBlueprintLibrary::GetAllWidgetsOfClass(this, Found, SubtitleClass, false);
	for (UUserWidget* W : Found)
	{
		if (W == Mine || W->GetVisibility() == ESlateVisibility::Collapsed) { continue; }
		W->SetVisibility(ESlateVisibility::Collapsed);
		HiddenSubtitles.AddUnique(W);
	}
}

void ANPCTalker::Tick(float Dt)
{
	Super::Tick(Dt);
	if (SubtitleClass) { SuppressOtherSubtitles(); }
	if (!NPC) { return; }

	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	APawn* P = PC ? PC->GetPawn() : nullptr;

	const FVector Head = HeadLocation();
	bool bActive = false;
	if (P)
	{
		const FVector PlayerLoc = P->GetActorLocation();
		const float Dist = FVector::Dist2D(PlayerLoc, Head);
		bActive = Dist <= Radius && IsClosestInRange(PlayerLoc, Dist);
	}

	// fly in / out
	PromptTime += Dt;
	PromptAlpha = FMath::FInterpConstantTo(PromptAlpha, bActive ? 1.f : 0.f, Dt, 1.f / FlyTime);
	Prompt->SetHiddenInGame(PromptAlpha <= 0.f);
	if (PromptAlpha > 0.f)
	{
		const float E = EaseOutBack(PromptAlpha);
		const float Bob = FMath::Sin(PromptTime * BobSpeed) * BobAmp * PromptAlpha;
		const FVector Loc = Head + FVector(0, 0, PromptGap - (1.f - E) * FlyDist + Bob);

		// face the camera
		FVector CamLoc; FRotator CamRot;
		PC->GetPlayerViewPoint(CamLoc, CamRot);
		const FRotator Face = FRotationMatrix::MakeFromX(CamLoc - Loc).Rotator();

		Prompt->SetWorldLocationAndRotation(Loc, Face);
		Prompt->SetWorldScale3D(FVector(FMath::Max(E, 0.01f) * PromptSize / IconPx));
		if (UUserWidget* W = Prompt->GetUserWidgetObject())
		{
			W->SetRenderOpacity(FMath::Clamp(PromptAlpha * 1.5f, 0.f, 1.f));
		}
	}

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
	SuppressOtherSubtitles();

	TWeakObjectPtr<UUserWidget> Weak = Sub;
	GetWorldTimerManager().SetTimer(SubtitleTimer, [Weak]()
	{
		if (UUserWidget* W = Weak.Get()) { W->RemoveFromParent(); }
		if (ActiveSubtitle == Weak) { ActiveSubtitle.Reset(); }
	}, SubtitleTime + PerCharTime * Line.Len(), false);
}
