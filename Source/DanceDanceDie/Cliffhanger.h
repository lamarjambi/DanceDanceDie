// demo ending. all c++, no blueprint wiring.
//
// ACliffhangerDirector (one in the club, placed by Scripts/setup_cliffhanger.py):
//   1. waits for the win: BP_Spline unhides the actors tagged PartyNPC.
//   2. party: every partygoer gets a press-E prompt (ANPCTalker). the 1st/2nd/3rd E press says Lines[0]/[1]/[2].
//   3. flicker: starts on the last line, or after IdleTimeout secs without an E press.
//      FlickerStartSound plays, music winds down, the screen blacks out FlickerCount times (longer each time).
//      during the last blackout the dancers switch to their idle anim and turn to the camera, unseen.
//   4. stare: lights back, everyone is standing still facing the player, who can look but not walk.
//   5. hard cut to EndLevel (DemoEnd).
//
// ADemoEndGameMode (game mode override of the DemoEnd level): black screen, one line of text, back to the menu.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameFramework/Actor.h"
#include "GameFramework/GameModeBase.h"
#include "Cliffhanger.generated.h"

class ANPCTalker;
class UAnimationAsset;
class UAudioComponent;
class USkeletalMeshComponent;
class USoundBase;
class UFont;
class UImage;
class UTextBlock;

UCLASS()
class DANCEDANCEDIE_API ACliffhangerDirector : public AActor
{
	GENERATED_BODY()

public:
	ACliffhangerDirector();

	// actors BP_Spline unhides on win
	UPROPERTY(EditAnywhere, Category = "Cliffhanger")
	FName PartyTag = TEXT("PartyNPC");

	// said in order, whoever the player talks to. the last one starts the stare.
	UPROPERTY(EditAnywhere, Category = "Cliffhanger", meta = (MultiLine = true))
	TArray<FString> Lines;

	// secs without an E press before the stare starts anyway (counted from the party start, reset by each E)
	UPROPERTY(EditAnywhere, Category = "Cliffhanger", meta = (ClampMin = 0))
	float IdleTimeout = 10.f;

	// cm from a dancer where the E prompt appears
	UPROPERTY(EditAnywhere, Category = "Cliffhanger", meta = (ClampMin = 0))
	float TalkRadius = 300.f;

	// secs for the music to wind down (pitch + volume)
	UPROPERTY(EditAnywhere, Category = "Cliffhanger", meta = (ClampMin = 0.01))
	float MusicFadeTime = 1.5f;

	// secs of flickering between the trigger and the stare
	UPROPERTY(EditAnywhere, Category = "Cliffhanger", meta = (ClampMin = 0.1))
	float FlickerTime = 1.5f;

	// number of blackouts in FlickerTime. the dancers change pose during the last (longest) one.
	UPROPERTY(EditAnywhere, Category = "Cliffhanger", meta = (ClampMin = 1))
	int32 FlickerCount = 5;

	// secs everyone stares before the cut
	UPROPERTY(EditAnywhere, Category = "Cliffhanger", meta = (ClampMin = 0))
	float StareHold = 3.f;

	// one idle per skeleton (filled by Scripts/setup_cliffhanger.py). each dancer plays the one made for its skeleton.
	UPROPERTY(EditAnywhere, Category = "Cliffhanger")
	TArray<TObjectPtr<UAnimationAsset>> IdleAnims;

	// 2D, once, when the flickering starts
	UPROPERTY(EditAnywhere, Category = "Cliffhanger|Sound")
	TObjectPtr<USoundBase> FlickerStartSound;

	// 2D, on every blackout
	UPROPERTY(EditAnywhere, Category = "Cliffhanger|Sound")
	TObjectPtr<USoundBase> FlickerTickSound;

	// 2D, once, when the lights come back and everyone is staring (empty = silence)
	UPROPERTY(EditAnywhere, Category = "Cliffhanger|Sound")
	TObjectPtr<USoundBase> StareSound;

	UPROPERTY(EditAnywhere, Category = "Cliffhanger")
	FName EndLevel = TEXT("/Game/UI/DemoEnd");

	virtual void Tick(float Dt) override;

protected:
	virtual void BeginPlay() override;

private:
	enum class EPhase : uint8 { Waiting, Party, Flicker, Stare, Done };

	struct FFadingSound
	{
		TWeakObjectPtr<UAudioComponent> Comp;
		float Volume = 1.f;
		float Pitch = 1.f;
	};

	void Fail(const FString& Why);
	void StartParty();
	void StartFlicker();
	void StartStare();
	void SetDark(bool bDark);
	void TickMusicFade();
	void PoseAndFace(AActor* Dancer, const FVector& CamLoc);
	UAnimationAsset* FindIdle(const USkeletalMeshComponent* Mesh) const;
	FString NextLine();

	UPROPERTY() TArray<TObjectPtr<AActor>> Dancers;
	UPROPERTY() TArray<TObjectPtr<ANPCTalker>> Talkers;
	TArray<FFadingSound> Sounds;

	EPhase Phase = EPhase::Waiting;
	TArray<float> FlickerEdges; // secs since the trigger: light->dark, dark->light, light->dark, ...
	int32 FlickerEdge = 0;      // next edge to cross

	float Clock = 0.f;        // secs in the current phase (Party: since the last E)
	float SinceTrigger = 0.f; // secs since the flicker started (drives the music fade)
	int32 Said = 0;
	bool bSeenHidden = false;
	bool bStareQueued = false;
};

UCLASS()
class DANCEDANCEDIE_API UDemoEndWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void Setup(const FString& Line, UFont* Font, int32 Size, const FLinearColor& Color);
	void ShowLine();

protected:
	virtual void NativeOnInitialized() override;

private:
	UPROPERTY() TObjectPtr<UImage> Backdrop;
	UPROPERTY() TObjectPtr<UTextBlock> Text;
};

UCLASS()
class DANCEDANCEDIE_API ADemoEndGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ADemoEndGameMode();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Demo End")
	FString Line = TEXT("it's not over");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Demo End")
	TObjectPtr<UFont> Font;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Demo End", meta = (ClampMin = 1))
	int32 FontSize = 72;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Demo End")
	FLinearColor TextColor = FLinearColor::White;

	// secs of plain black before the line appears
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Demo End", meta = (ClampMin = 0))
	float TextDelay = 1.f;

	// secs the line stays up before NextLevel opens
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Demo End", meta = (ClampMin = 0))
	float HoldTime = 4.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Demo End")
	FName NextLevel = TEXT("/Game/UI/MainMenu");

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY() TObjectPtr<UDemoEndWidget> Screen;
};
