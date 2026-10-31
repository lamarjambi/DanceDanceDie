#include "LoadingScreen.h"

#include "Blueprint/UserWidget.h"
#include "ContentStreaming.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "MoviePlayer.h"
#include "ShaderCompiler.h"
#include "ShaderPipelineCache.h"
#if WITH_EDITOR
#include "AssetCompilingManager.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogLoadingScreen, Log, All);

void ULoadingScreenSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	PreLoadHandle = FCoreUObjectDelegates::PreLoadMap.AddUObject(this, &ULoadingScreenSubsystem::OnPreLoadMap);
	PostLoadHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &ULoadingScreenSubsystem::OnPostLoadMap);
}

void ULoadingScreenSubsystem::Deinitialize()
{
	FCoreUObjectDelegates::PreLoadMap.Remove(PreLoadHandle);
	FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(PostLoadHandle);
	FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
	MovieWidget = nullptr;
	OverlayWidget = nullptr;
	Super::Deinitialize();
}

UUserWidget* ULoadingScreenSubsystem::MakeWidget()
{
	UClass* Cls = GetDefault<ULoadingScreenSettings>()->WidgetClass.LoadSynchronous();
	if (!Cls)
	{
		// fail loudly
		UE_LOG(LogLoadingScreen, Error, TEXT("Loading Screen widget not set: Project Settings > Game > Loading Screen > Widget Class"));
		if (GEngine) { GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Red, TEXT("LoadingScreen: WidgetClass not set!")); }
		return nullptr;
	}
	UUserWidget* W = CreateWidget<UUserWidget>(GetGameInstance(), Cls);
	check(W);
	return W;
}

// ---------- phase 1: map file loading (movie thread) ----------

void ULoadingScreenSubsystem::OnPreLoadMap(const FString& MapName)
{
	if (IsRunningDedicatedServer() || !IsMoviePlayerEnabled()) { return; } // off in PIE, by design

	// no loading screen at boot (straight to the main menu, like most games)
	if (!bBootDone) { bBootDone = true; return; }

	const ULoadingScreenSettings* S = GetDefault<ULoadingScreenSettings>();
	for (const FString& Skip : S->SkipMaps)
	{
		if (!Skip.IsEmpty() && MapName.Contains(Skip)) { bActive = false; return; }
	}

	MovieWidget = MakeWidget();
	if (!MovieWidget) { return; }

	FLoadingScreenAttributes A;
	A.WidgetLoadingScreen = MovieWidget->TakeWidget();
	A.MinimumLoadingScreenDisplayTime = 0.f; // min time enforced in phase 2 instead
	A.bAutoCompleteWhenLoadingCompletes = true;
	A.bWaitForManualStop = false;
	A.bAllowEngineTick = false;
	GetMoviePlayer()->SetupLoadingScreen(A);

	bActive = true;
	StartTime = FPlatformTime::Seconds();
	UE_LOG(LogLoadingScreen, Log, TEXT("Loading %s"), *MapName);
}

// ---------- phase 2: shader / texture warmup (viewport overlay) ----------

void ULoadingScreenSubsystem::OnPostLoadMap(UWorld* World)
{
	if (!bActive || !World || World->GetGameInstance() != GetGameInstance()) { return; }

	OverlayWidget = MakeWidget();
	if (!OverlayWidget) { bActive = false; return; }
	OverlayWidget->AddToViewport(10000); // on top of everything

	// world keeps running behind the screen (NOT paused) so camera effects, particles and
	// auto-exposure all warm up + settle before reveal. just block the player's input.
	if (APlayerController* PC = World->GetFirstPlayerController()) { PC->DisableInput(PC); }

	Waited = 0.f;
	Settled = 0.f;
	FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
	TickHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &ULoadingScreenSubsystem::TickWarmup));
}

int32 ULoadingScreenSubsystem::PendingWork(FString& OutWhat) const
{
	const int32 Shaders = GShaderCompilingManager ? GShaderCompilingManager->GetNumRemainingJobs() : 0;
	const int32 PSOs = (int32)FShaderPipelineCache::NumPrecompilesRemaining();
	const int32 Textures = IStreamingManager::Get().GetNumWantingResources();
	int32 Assets = 0;
#if WITH_EDITOR
	Assets = FAssetCompilingManager::Get().GetNumRemainingAssets();
#endif
	OutWhat = FString::Printf(TEXT("shaders=%d psos=%d textures=%d assets=%d"), Shaders, PSOs, Textures, Assets);
	// texture streaming can stay >0 forever if the pool is over budget, so only wait on it for a bit
	const bool bWaitTextures = Waited < GetDefault<ULoadingScreenSettings>()->MaxTextureWait;
	return Shaders + PSOs + Assets + (bWaitTextures ? Textures : 0);
}

bool ULoadingScreenSubsystem::TickWarmup(float Dt)
{
	const ULoadingScreenSettings* S = GetDefault<ULoadingScreenSettings>();
	Waited += Dt;

	FString What;
	const int32 Left = PendingWork(What);
	Settled = (Left == 0) ? Settled + Dt : 0.f;

	const bool bMinTimeOk = (FPlatformTime::Seconds() - StartTime) >= S->MinDisplayTime;
	if (Settled >= S->SettleTime && bMinTimeOk)
	{
		UE_LOG(LogLoadingScreen, Log, TEXT("Warmup done in %.1fs"), Waited);
		Finish();
		return false;
	}
	if (Waited >= S->MaxWaitTime)
	{
		UE_LOG(LogLoadingScreen, Error, TEXT("Warmup timed out after %.0fs, still pending: %s"), Waited, *What);
		if (GEngine) { GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Red, FString::Printf(TEXT("LoadingScreen timeout: %s"), *What)); }
		Finish();
		return false;
	}
	return true;
}

bool ULoadingScreenSubsystem::IsShowing(const UObject* WorldContext)
{
	const UWorld* W = WorldContext ? WorldContext->GetWorld() : nullptr;
	const UGameInstance* GI = W ? W->GetGameInstance() : nullptr;
	const ULoadingScreenSubsystem* S = GI ? GI->GetSubsystem<ULoadingScreenSubsystem>() : nullptr;
	return S && S->bActive;
}

void ULoadingScreenSubsystem::Finish()
{
	bActive = false;
	if (OverlayWidget) { OverlayWidget->RemoveFromParent(); OverlayWidget = nullptr; }
	if (UWorld* W = GetGameInstance()->GetWorld())
	{
		if (APlayerController* PC = W->GetFirstPlayerController()) { PC->EnableInput(PC); }
	}
	TickHandle.Reset();
}
