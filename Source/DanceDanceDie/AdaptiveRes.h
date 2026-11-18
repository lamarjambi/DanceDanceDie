// adaptive 3D render scale. keeps the 3D picture at the same number of rows (ddd.RenderRows, default 432)
// on any screen or window size by setting r.ScreenPercentage = rows / viewport height.
// 1080p -> 40%, 1440p -> 30%, 720p -> 60%. the PS1 filter cuts everything to 180 rows anyway, so the look
// is the same everywhere and a bigger monitor doesn't cost more GPU. UI is not affected (always native res).
// console: ddd.RenderRows 540 (sharper source, slower) / ddd.RenderRows 0 (off: r.ScreenPercentage is left alone).

#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "AdaptiveRes.generated.h"

UCLASS()
class DANCEDANCEDIE_API UAdaptiveResSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

private:
	bool Tick(float Dt);

	FTSTicker::FDelegateHandle TickHandle;
	int32 LastHeight = 0;
	int32 LastRows = -1;
};
