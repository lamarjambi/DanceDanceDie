#include "Cliffhanger.h"
#include "NPCTalk.h"

#include "Animation/AnimationAsset.h"
#include "Animation/Skeleton.h"
#include "Blueprint/WidgetTree.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/AudioComponent.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/TextBlock.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "Styling/CoreStyle.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"
#include "UObject/UObjectIterator.h"

DEFINE_LOG_CATEGORY_STATIC(LogCliffhanger, Log, All);

// test hook: `ddd.FakeWin` in the console (~) does what BP_Spline does on win, so the ending can be tested without playing the song
static FAutoConsoleCommandWithWorld GFakeWin(
	TEXT("ddd.FakeWin"),
	TEXT("Unhides the actors tagged PartyNPC, like winning the song does."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		TArray<AActor*> Found;
		UGameplayStatics::GetAllActorsWithTag(World, TEXT("PartyNPC"), Found);
		for (AActor* A : Found) { A->SetActorHiddenInGame(false); }
		UE_LOG(LogCliffhanger, Warning, TEXT("ddd.FakeWin: unhid %d actors"), Found.Num());
	}));

// ---------------------------------------------------------------- director

ACliffhangerDirector::ACliffhangerDirector()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	Lines = { TEXT("Michelle!! You made it!"), TEXT("You dance so well."), TEXT("Stay.") };

	static ConstructorHelpers::FObjectFinder<USoundBase> Suspense(TEXT("/Game/Horror/Audio/suspension-sfx.suspension-sfx"));
	FlickerStartSound = Suspense.Object;
	static ConstructorHelpers::FObjectFinder<USoundBase> Click(TEXT("/Game/Rhythm/Audio/glitch-click.glitch-click"));
	FlickerTickSound = Click.Object;
}

void ACliffhangerDirector::Fail(const FString& Why)
{
	UE_LOG(LogCliffhanger, Error, TEXT("Cliffhanger: %s"), *Why);
	if (GEngine) { GEngine->AddOnScreenDebugMessage(-1, 15.f, FColor::Red, TEXT("Cliffhanger: ") + Why); }
	Phase = EPhase::Done;
}

void ACliffhangerDirector::BeginPlay()
{
	Super::BeginPlay();

	TArray<AActor*> Found;
	UGameplayStatics::GetAllActorsWithTag(this, PartyTag, Found);
	for (AActor* A : Found) { Dancers.Add(A); }

	if (Dancers.IsEmpty()) { Fail(FString::Printf(TEXT("no actors tagged '%s'"), *PartyTag.ToString())); return; }
	if (Lines.IsEmpty()) { Fail(TEXT("no Lines")); return; }
	if (EndLevel.IsNone()) { Fail(TEXT("EndLevel is empty")); return; }
}

void ACliffhangerDirector::Tick(float Dt)
{
	Super::Tick(Dt);
	Clock += Dt;

	switch (Phase)
	{
	case EPhase::Waiting:
	{
		bool bAnyVisible = false;
		for (const AActor* A : Dancers)
		{
			if (A && !A->IsHidden()) { bAnyVisible = true; break; }
		}
		if (!bAnyVisible) { bSeenHidden = true; }
		else if (bSeenHidden) { StartParty(); }
		else if (Clock > 2.f) { Fail(TEXT("partygoers are visible before the win (they must start hidden)")); }
		break;
	}
	case EPhase::Party:
		if (bStareQueued || Clock >= IdleTimeout) { StartFlicker(); }
		break;

	case EPhase::Flicker:
		SinceTrigger += Dt;
		TickMusicFade();
		while (FlickerEdge < FlickerEdges.Num() && Clock >= FlickerEdges[FlickerEdge])
		{
			const bool bDark = (FlickerEdge % 2) == 0;
			SetDark(bDark);
			if (bDark)
			{
				if (FlickerTickSound) { UGameplayStatics::PlaySound2D(this, FlickerTickSound); }
				if (FlickerEdge == FlickerEdges.Num() - 2)
				{
					// last blackout: nobody sees them change
					FVector CamLoc; FRotator CamRot;
					UGameplayStatics::GetPlayerController(this, 0)->GetPlayerViewPoint(CamLoc, CamRot);
					for (AActor* D : Dancers)
					{
						if (D) { PoseAndFace(D, CamLoc); }
					}
				}
			}
			++FlickerEdge;
		}
		if (FlickerEdge >= FlickerEdges.Num()) { StartStare(); }
		break;

	case EPhase::Stare:
		SinceTrigger += Dt;
		TickMusicFade();
		if (Clock >= StareHold)
		{
			Phase = EPhase::Done;
			UE_LOG(LogCliffhanger, Log, TEXT("Cliffhanger: cut to %s"), *EndLevel.ToString());
			for (const FFadingSound& S : Sounds)
			{
				if (UAudioComponent* C = S.Comp.Get()) { C->Stop(); }
			}
			SetDark(true);
			UGameplayStatics::OpenLevel(this, EndLevel);
		}
		break;

	case EPhase::Done:
		break;
	}
}

void ACliffhangerDirector::StartParty()
{
	Phase = EPhase::Party;
	Clock = 0.f;
	UE_LOG(LogCliffhanger, Log, TEXT("Cliffhanger: party started, %d dancers"), Dancers.Num());

	for (AActor* D : Dancers)
	{
		if (!D) { continue; }
		ANPCTalker* T = GetWorld()->SpawnActorDeferred<ANPCTalker>(ANPCTalker::StaticClass(), D->GetActorTransform());
		check(T);
		T->NPC = D;
		T->Radius = TalkRadius;
		T->LineProvider.BindUObject(this, &ACliffhangerDirector::NextLine);
		UGameplayStatics::FinishSpawningActor(T, D->GetActorTransform());
		Talkers.Add(T);
	}
}

FString ACliffhangerDirector::NextLine()
{
	const int32 I = FMath::Min(Said, Lines.Num() - 1);
	++Said;
	Clock = 0.f;
	if (Said >= Lines.Num()) { bStareQueued = true; } // stare starts next Tick, not inside the talker's Say()
	UE_LOG(LogCliffhanger, Log, TEXT("Cliffhanger: E %d/%d"), Said, Lines.Num());
	return Lines[I];
}

void ACliffhangerDirector::SetDark(bool bDark)
{
	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (PC && PC->PlayerCameraManager)
	{
		PC->PlayerCameraManager->SetManualCameraFade(bDark ? 1.f : 0.f, FLinearColor::Black, false);
	}
}

void ACliffhangerDirector::TickMusicFade()
{
	if (Sounds.IsEmpty()) { return; }
	const float T = FMath::Clamp(SinceTrigger / MusicFadeTime, 0.f, 1.f);
	for (const FFadingSound& S : Sounds)
	{
		UAudioComponent* C = S.Comp.Get();
		if (!C) { continue; }
		if (T >= 1.f) { C->Stop(); continue; }
		C->SetPitchMultiplier(FMath::Lerp(S.Pitch, S.Pitch * 0.25f, T));
		C->SetVolumeMultiplier(S.Volume * (1.f - T * T));
	}
	if (T >= 1.f) { Sounds.Reset(); }
}

void ACliffhangerDirector::StartFlicker()
{
	Phase = EPhase::Flicker;
	Clock = 0.f;
	SinceTrigger = 0.f;
	UE_LOG(LogCliffhanger, Log, TEXT("Cliffhanger: flicker (after %d E presses)"), Said);

	for (ANPCTalker* T : Talkers)
	{
		if (T) { T->Destroy(); }
	}
	Talkers.Reset();

	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	check(PC);
	PC->SetIgnoreMoveInput(true); // can still look around

	// everything audible right now winds down (collected before our own sounds start)
	for (TObjectIterator<UAudioComponent> It; It; ++It)
	{
		UAudioComponent* C = *It;
		if (C->GetWorld() != GetWorld() || !C->IsPlaying()) { continue; }
		FFadingSound S;
		S.Comp = C;
		S.Volume = C->VolumeMultiplier;
		S.Pitch = C->PitchMultiplier;
		Sounds.Add(S);
	}
	if (FlickerStartSound) { UGameplayStatics::PlaySound2D(this, FlickerStartSound); }

	// light gaps shrink, blackouts grow, the whole thing is scaled to FlickerTime
	const int32 N = FMath::Max(FlickerCount, 1);
	TArray<float> Spans; // light, dark, light, dark, ...
	float Total = 0.f;
	for (int32 I = 0; I < N; ++I)
	{
		const float A = N > 1 ? float(I) / float(N - 1) : 1.f;
		Spans.Add(FMath::Lerp(0.28f, 0.08f, A));
		Spans.Add(FMath::Lerp(0.06f, 0.35f, A));
		Total += Spans[Spans.Num() - 2] + Spans.Last();
	}
	FlickerEdges.Reset();
	FlickerEdge = 0;
	float At = 0.f;
	for (const float Span : Spans)
	{
		At += Span * FlickerTime / Total;
		FlickerEdges.Add(At);
	}
}

void ACliffhangerDirector::StartStare()
{
	Phase = EPhase::Stare;
	Clock = 0.f;
	SetDark(false);
	UE_LOG(LogCliffhanger, Log, TEXT("Cliffhanger: stare"));
	if (StareSound) { UGameplayStatics::PlaySound2D(this, StareSound); }
}

UAnimationAsset* ACliffhangerDirector::FindIdle(const USkeletalMeshComponent* Mesh) const
{
	const USkeletalMesh* Asset = Mesh->GetSkeletalMeshAsset();
	const USkeleton* Skeleton = Asset ? Asset->GetSkeleton() : nullptr;
	for (UAnimationAsset* A : IdleAnims)
	{
		if (A && Skeleton && A->GetSkeleton() == Skeleton) { return A; }
	}
	return nullptr;
}

void ACliffhangerDirector::PoseAndFace(AActor* Dancer, const FVector& CamLoc)
{
	USkeletalMeshComponent* Mesh = Dancer->FindComponentByClass<USkeletalMeshComponent>();
	if (!Mesh)
	{
		UE_LOG(LogCliffhanger, Error, TEXT("Cliffhanger: %s has no skeletal mesh"), *Dancer->GetName());
		return;
	}

	if (UAnimationAsset* Idle = FindIdle(Mesh))
	{
		Mesh->PlayAnimation(Idle, true);
	}
	else
	{
		// no idle for this skeleton: freeze mid-dance instead, and say so
		Mesh->bPauseAnims = true;
		const FString Why = FString::Printf(TEXT("no idle anim for %s in IdleAnims (run Scripts/setup_cliffhanger.py)"), *Dancer->GetActorNameOrLabel());
		UE_LOG(LogCliffhanger, Error, TEXT("Cliffhanger: %s"), *Why);
		if (GEngine) { GEngine->AddOnScreenDebugMessage(-1, 15.f, FColor::Red, TEXT("Cliffhanger: ") + Why); }
	}

	// imported meshes face +Y, so the component's right vector is where the body faces
	const FVector To = CamLoc - Dancer->GetActorLocation();
	const float Delta = FMath::FindDeltaAngleDegrees(Mesh->GetRightVector().Rotation().Yaw, To.Rotation().Yaw);
	Dancer->AddActorWorldRotation(FRotator(0.f, Delta, 0.f));
}

// ---------------------------------------------------------------- end screen

void UDemoEndWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	check(WidgetTree);

	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
	WidgetTree->RootWidget = Root;

	Backdrop = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("Backdrop"));
	Backdrop->SetColorAndOpacity(FLinearColor::Black);
	{
		UCanvasPanelSlot* S = Root->AddChildToCanvas(Backdrop);
		S->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
		S->SetOffsets(FMargin(0.f));
	}

	Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Line"));
	Text->SetJustification(ETextJustify::Center);
	Text->SetVisibility(ESlateVisibility::Hidden);
	{
		UCanvasPanelSlot* S = Root->AddChildToCanvas(Text);
		S->SetAnchors(FAnchors(0.5f, 0.5f));
		S->SetAlignment(FVector2D(0.5f, 0.5f));
		S->SetAutoSize(true);
		S->SetPosition(FVector2D::ZeroVector);
	}
}

void UDemoEndWidget::Setup(const FString& Line, UFont* Font, int32 Size, const FLinearColor& Color)
{
	check(Text);
	Text->SetText(FText::FromString(Line));
	Text->SetFont(Font ? FSlateFontInfo(Font, Size) : FCoreStyle::GetDefaultFontStyle("Regular", Size));
	Text->SetColorAndOpacity(FSlateColor(Color));
}

void UDemoEndWidget::ShowLine()
{
	check(Text);
	Text->SetVisibility(ESlateVisibility::HitTestInvisible);
}

ADemoEndGameMode::ADemoEndGameMode()
{
	DefaultPawnClass = nullptr; // nothing to control, the widget covers the screen

	static ConstructorHelpers::FObjectFinder<UFont> Vt323(TEXT("/Game/UI/Fonts/VT323-Regular_Font.VT323-Regular_Font"));
	Font = Vt323.Object;
}

void ADemoEndGameMode::BeginPlay()
{
	Super::BeginPlay();

	if (!Font) { UE_LOG(LogCliffhanger, Error, TEXT("DemoEnd: Font not set, falling back to Roboto")); }

	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	check(PC);
	Screen = CreateWidget<UDemoEndWidget>(PC, UDemoEndWidget::StaticClass());
	check(Screen);
	Screen->Setup(Line, Font, FontSize, TextColor);
	Screen->AddToViewport(100);
	PC->SetInputMode(FInputModeGameOnly());
	PC->bShowMouseCursor = false;

	FTimerHandle ShowTimer, LeaveTimer;
	GetWorldTimerManager().SetTimer(ShowTimer, FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		if (Screen) { Screen->ShowLine(); }
	}), FMath::Max(TextDelay, 0.01f), false);

	GetWorldTimerManager().SetTimer(LeaveTimer, FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		if (NextLevel.IsNone())
		{
			UE_LOG(LogCliffhanger, Error, TEXT("DemoEnd: NextLevel is empty, staying put"));
			if (GEngine) { GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Red, TEXT("DemoEnd: NextLevel is empty!")); }
			return;
		}
		UGameplayStatics::OpenLevel(this, NextLevel);
	}), FMath::Max(TextDelay + HoldTime, 0.02f), false);
}
