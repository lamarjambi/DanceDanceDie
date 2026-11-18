#include "AdaptiveRes.h"

#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "HAL/IConsoleManager.h"
#include "UnrealClient.h" // FViewport

DEFINE_LOG_CATEGORY_STATIC(LogAdaptiveRes, Log, All);

static TAutoConsoleVariable<int32> CVarRenderRows(
	TEXT("ddd.RenderRows"),
	432,
	TEXT("Rows the 3D scene is rendered at before the PS1 filter (180 rows). r.ScreenPercentage follows the screen height.\n")
	TEXT("Keep it at 360 or more (2x the filter). 0 = off, r.ScreenPercentage is left alone."),
	ECVF_Default);

void UAdaptiveResSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	// polled: catches the first real viewport size, window resizes, Alt+Enter and console changes alike
	TickHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &UAdaptiveResSubsystem::Tick), 0.25f);
}

void UAdaptiveResSubsystem::Deinitialize()
{
	FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
	Super::Deinitialize();
}

bool UAdaptiveResSubsystem::Tick(float Dt)
{
	const int32 Rows = CVarRenderRows.GetValueOnGameThread();
	const UGameInstance* GI = GetGameInstance();
	const UGameViewportClient* GVC = GI ? GI->GetGameViewportClient() : nullptr;
	const FViewport* VP = GVC ? GVC->Viewport : nullptr;
	const int32 Height = VP ? VP->GetSizeXY().Y : 0;

	if (Rows <= 0 || Height <= 0)
	{
		LastHeight = 0; // re-apply when it comes back
		LastRows = Rows;
		return true;
	}
	if (Height == LastHeight && Rows == LastRows)
	{
		return true;
	}
	LastHeight = Height;
	LastRows = Rows;

	if (IConsoleVariable* ScreenPct = IConsoleManager::Get().FindConsoleVariable(TEXT("r.ScreenPercentage")))
	{
		const float Pct = FMath::Clamp(100.f * (float)Rows / (float)Height, 10.f, 100.f);
		ScreenPct->Set(Pct, ECVF_SetByCode);
		UE_LOG(LogAdaptiveRes, Log, TEXT("viewport height %d -> r.ScreenPercentage %.1f (%d rows)"), Height, Pct, Rows);
	}
	return true;
}
