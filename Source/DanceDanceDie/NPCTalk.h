// press-E-to-talk for city NPCs. all c++, no wbp for the prompt.
// one ANPCTalker per NPC (placed by Scripts/setup_npc_talk.py). it follows its NPC actor,
// shows the E key icon flying in above the NPC's head when the player is close,
// and on E shows WBP_Subtitle with a random line (never the same one twice in a row).
// only the closest in-range NPC shows the prompt, so one E press = one NPC.
// NPC lines win over trigger text: other WBP_Subtitle instances are hidden while an NPC line is up, then restored.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameFramework/Actor.h"
#include "NPCTalk.generated.h"

class UCanvasPanelSlot;
class UImage;
class UTexture2D;

// screen-space "E" icon. ANPCTalker drives it every tick.
UCLASS()
class DANCEDANCEDIE_API UNPCPromptWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetIcon(UTexture2D* Tex, float Size);
	// ScreenPos in viewport pixels. bWant = should be visible.
	void Step(float Dt, bool bWant, const FVector2D& ScreenPos, bool bOnScreen);

protected:
	virtual void NativeOnInitialized() override;

private:
	UPROPERTY() TObjectPtr<UImage> Icon;
	UPROPERTY() TObjectPtr<UCanvasPanelSlot> IconSlot;
	float Alpha = 0.f; // 0 hidden .. 1 fully in
	float IconSize = 64.f;
	float Time = 0.f;
};

UCLASS()
class DANCEDANCEDIE_API ANPCTalker : public AActor
{
	GENERATED_BODY()

public:
	ANPCTalker();

	// the NPC this talks for (NPC_Jackie etc.)
	UPROPERTY(EditInstanceOnly, Category = "Talk")
	TObjectPtr<AActor> NPC;

	// shown before the line: "Jackie: ..." (empty = no name)
	UPROPERTY(EditAnywhere, Category = "Talk")
	FString SpeakerName;

	UPROPERTY(EditAnywhere, Category = "Talk", meta = (MultiLine = true))
	TArray<FString> Lines;

	// cm from the NPC where the prompt appears
	UPROPERTY(EditAnywhere, Category = "Talk", meta = (ClampMin = 0))
	float Radius = 350.f;

	// the E icon sits on this bone (where the mesh is actually drawn; mixamo walks drift from the actor origin)
	UPROPERTY(EditAnywhere, Category = "Talk")
	FName HeadBone = TEXT("HeadTop_End"); // UE strips the "mixamorig:" prefix on import

	// cm above the head bone
	UPROPERTY(EditAnywhere, Category = "Talk")
	float PromptGap = 25.f;

	// on-screen px
	UPROPERTY(EditAnywhere, Category = "Talk", meta = (ClampMin = 8))
	float PromptSize = 80.f;

	// subtitle stays this long + PerCharTime per character
	UPROPERTY(EditAnywhere, Category = "Talk", meta = (ClampMin = 0))
	float SubtitleTime = 2.f;

	UPROPERTY(EditAnywhere, Category = "Talk", meta = (ClampMin = 0))
	float PerCharTime = 0.04f;

	UPROPERTY(EditAnywhere, Category = "Talk")
	TObjectPtr<UTexture2D> PromptIcon;

	// any widget with a TextBlock in it. the first TextBlock found gets the line.
	UPROPERTY(EditAnywhere, Category = "Talk")
	TSubclassOf<UUserWidget> SubtitleClass;

	virtual void Tick(float Dt) override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
	bool IsClosestInRange(const FVector& PlayerLoc, float MyDist) const;
	FVector HeadLocation() const;
	void Say();
	void SuppressOtherSubtitles();

	UPROPERTY() TObjectPtr<UNPCPromptWidget> Prompt;
	int32 LastLine = INDEX_NONE;

	// one subtitle on screen at a time, shared by all talkers
	static TWeakObjectPtr<UUserWidget> ActiveSubtitle;
	static FTimerHandle SubtitleTimer;
	// trigger-text subtitles we hid while an NPC is talking
	static TArray<TWeakObjectPtr<UUserWidget>> HiddenSubtitles;
};
