// real loading screen. shows on EVERY OpenLevel automatically (menu -> chat -> city -> club ...).
// phase 1: MoviePlayer thread shows the widget while the map file loads (doesn't freeze).
// phase 2: after the map loads, same widget stays on top (world running, input blocked) until
//          shaders, PSOs, textures and auto-exposure have settled, so nothing pops in on reveal.
// no screen at boot (first map = main menu).
// set the widget in Project Settings > Game > Loading Screen.
// NOTE: MoviePlayer is off in PIE. test with Play > Standalone Game (or a packaged build).

#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Engine/DeveloperSettings.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "LoadingScreen.generated.h"

class UUserWidget;

UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Loading Screen"))
class DANCEDANCEDIE_API ULoadingScreenSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	ULoadingScreenSettings() { CategoryName = TEXT("Game"); }

	// widget shown while loading (WBP_LoadingScreen)
	UPROPERTY(Config, EditAnywhere, Category = "Loading Screen")
	TSoftClassPtr<UUserWidget> WidgetClass;

	// keeps the screen up at least this long even if everything is instant
	UPROPERTY(Config, EditAnywhere, Category = "Loading Screen", meta = (ClampMin = 0))
	float MinDisplayTime = 2.f;

	// shaders/textures must report "nothing left" for this long in a row before we reveal
	UPROPERTY(Config, EditAnywhere, Category = "Loading Screen", meta = (ClampMin = 0))
	float SettleTime = 2.f;

	// stop waiting on texture streaming after this long (pool can stay busy forever)
	UPROPERTY(Config, EditAnywhere, Category = "Loading Screen", meta = (ClampMin = 0))
	float MaxTextureWait = 10.f;

	// safety cap. if still compiling after this, reveal anyway + log an error
	UPROPERTY(Config, EditAnywhere, Category = "Loading Screen", meta = (ClampMin = 1))
	float MaxWaitTime = 180.f;

	// maps that should NOT get a loading screen (substring match on the map path)
	UPROPERTY(Config, EditAnywhere, Category = "Loading Screen")
	TArray<FString> SkipMaps;
};

UCLASS()
class DANCEDANCEDIE_API ULoadingScreenSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	// true while the loading screen covers the screen. gameplay that shouldn't start yet can wait on this.
	UFUNCTION(BlueprintPure, Category = "Loading Screen", meta = (WorldContext = "WorldContext"))
	static bool IsShowing(const UObject* WorldContext);

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

private:
	void OnPreLoadMap(const FString& MapName);
	void OnPostLoadMap(UWorld* World);
	bool TickWarmup(float Dt);
	int32 PendingWork(FString& OutWhat) const;
	void Finish();
	UUserWidget* MakeWidget();

	UPROPERTY() TObjectPtr<UUserWidget> MovieWidget;   // phase 1 (movie thread)
	UPROPERTY() TObjectPtr<UUserWidget> OverlayWidget; // phase 2 (viewport)

	FDelegateHandle PreLoadHandle;
	FDelegateHandle PostLoadHandle;
	FTSTicker::FDelegateHandle TickHandle;

	bool bActive = false;
	bool bBootDone = false;
	double StartTime = 0.0;
	float Waited = 0.f;
	float Settled = 0.f;
};
