// intro group chat cutscene. everything is built in c++ so no wbp needed.
// edit messages in BP_IntroChatGameMode class defaults.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameFramework/GameModeBase.h"
#include "IntroChat.generated.h"

class UCanvasPanel;
class UVerticalBox;
class UImage;
class UTextBlock;
class UWidget;
class USoundBase;
class UTexture2D;
class UAudioComponent;
class UFont;

USTRUCT(BlueprintType)
struct FIntroChatMessage
{
	GENERATED_BODY()

	// empty sender = system line (e.g. "mateo left the chat")
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chat")
	FString Sender;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chat", meta = (MultiLine = true))
	FString Body;

	// secs of "typing..." before it pops in (0 = no typing bubble)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chat")
	float TypingTime = 0.7f;

	// secs to wait after it pops in
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chat")
	float HoldTime = 1.3f;

	// red bubble + jitter
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chat")
	bool bCreepy = false;

	// optional photo shown in the bubble (body can be empty for photo-only)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chat")
	TObjectPtr<UTexture2D> Photo;
};

UCLASS()
class DANCEDANCEDIE_API UIntroChatWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	TArray<FIntroChatMessage> Messages;
	FString ChatTitle;
	FString ChatSubtitle;
	TObjectPtr<USoundBase> PingSound;
	TObjectPtr<USoundBase> CreepySound;
	TObjectPtr<USoundBase> TypingSound;
	TObjectPtr<UTexture2D> Background;
	float EndHoldTime = 2.f;
	float FadeOutTime = 1.5f;
	FLinearColor NameColor = FLinearColor::White;

	// driven by the game mode tick
	void Step(float Dt);
	void Skip();
	bool IsDone() const { return Phase == EPhase::Done; }

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;

private:
	enum class EPhase : uint8 { FadeIn, Typing, Holding, EndHold, FadeOut, Done };

	struct FBubble
	{
		TObjectPtr<UWidget> W;
		float Age = 0.f;
		float LeaveAge = -1.f; // >=0 means leaving
		bool bCreepy = false;
		bool bSystem = false;
		float Dim = 1.f;
		int32 Weight = 1; // photos take more room in the stack
	};

	UWidget* MakeBubble(const FString& Sender, const FString& Body, bool bCreepy, UTexture2D* Photo, UTextBlock*& OutBody);
	UWidget* MakeSystemLine(const FString& Body);
	void PushWidget(UWidget* W, bool bCreepy, bool bSystem, int32 Weight = 1);
	void ShowTyping(const FString& Sender);
	void HideTyping();
	void ShowMessage(const FIntroChatMessage& M);
	FSlateFontInfo SenderFont(int32 Size) const;
	FSlateFontInfo BodyFont(int32 Size) const;

	UPROPERTY() TObjectPtr<UCanvasPanel> Root;
	UPROPERTY() TObjectPtr<UImage> Backdrop;
	UPROPERTY() TObjectPtr<UImage> FadeOverlay;
	UPROPERTY() TObjectPtr<UVerticalBox> Stack;
	UPROPERTY() TObjectPtr<UTextBlock> TitleText;
	UPROPERTY() TObjectPtr<UTextBlock> SubtitleText;
	UPROPERTY() TObjectPtr<UWidget> TypingBubble;
	UPROPERTY() TObjectPtr<UTextBlock> TypingDots;
	UPROPERTY() TObjectPtr<UObject> HeavyFont;
	UPROPERTY() TObjectPtr<UAudioComponent> TypingAudio;

	TArray<FBubble> Bubbles;
	EPhase Phase = EPhase::FadeIn;
	float PhaseTime = 0.f;
	float TotalTime = 0.f;
	int32 Index = 0;
	bool bPlayedCreepy = false;
};

UCLASS()
class DANCEDANCEDIE_API AIntroChatGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AIntroChatGameMode();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Intro Chat")
	TArray<FIntroChatMessage> Messages;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Intro Chat")
	FString ChatTitle = TEXT("Halloween Booooooooor Crawl");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Intro Chat")
	FString ChatSubtitle = TEXT("Mateo, Ava, Cass, Ade, You");

	// level opened when the chat ends or is skipped
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Intro Chat")
	FName NextLevel = TEXT("NightCityCreator");

	// plays on every friend message (receiving)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Intro Chat")
	TObjectPtr<USoundBase> PingSound;

	// loops while the typing dots bubble is up, stops when the msg lands
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Intro Chat")
	TObjectPtr<USoundBase> TypingSound;

	// plays once on the first creepy message 
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Intro Chat")
	TObjectPtr<USoundBase> CreepySound;

	// every text in the chat uses this (title, names, messages, hint)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Intro Chat")
	TObjectPtr<UFont> ChatFont;

	// one color for every name label (creepy msgs stay red)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Intro Chat")
	FLinearColor NameColor = FLinearColor(0.62f, 0.62f, 0.68f, 1.f);

	// michelle as bgk
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Intro Chat")
	TObjectPtr<UTexture2D> Background;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Intro Chat")
	float EndHoldTime = 2.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Intro Chat")
	float FadeOutTime = 1.5f;

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

private:
	UPROPERTY() TObjectPtr<UIntroChatWidget> Chat;
	bool bLeaving = false;
};
