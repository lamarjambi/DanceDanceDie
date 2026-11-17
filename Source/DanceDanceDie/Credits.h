// Credits roll (CreditsScene level). Movie-style: lines scroll up from the bottom on black, the last line stops in the
// middle, holds, fades, then back to the menu. Built in code like DemoEnd, so no Widget Blueprint.
// Everything editable without a rebuild: BP_CreditsGameMode -> Class Defaults -> Credits.
// Input: Esc / Enter / click / gamepad B = back to menu. Hold Space, Down, S or gamepad A = fast-forward.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameFramework/GameModeBase.h"
#include "Credits.generated.h"

class UCanvasPanelSlot;
class UFont;
class UMaterialInterface;
class URetainerBox;
class UVerticalBox;

UENUM(BlueprintType)
enum class ECreditStyle : uint8
{
	Title,   // big, like the game title
	Heading, // role ("MUSIC", "CAST")
	Name,    // the person / thing credited
	Gap,     // empty space; Text ignored
};

USTRUCT(BlueprintType)
struct FCreditLine
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Credits")
	ECreditStyle Style = ECreditStyle::Name;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Credits")
	FString Text;

	FCreditLine() = default;
	FCreditLine(ECreditStyle InStyle, const FString& InText) : Style(InStyle), Text(InText) {}
};

USTRUCT(BlueprintType)
struct FCreditLook
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Credits", meta = (ClampMin = 1))
	int32 Size = 44;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Credits")
	FLinearColor Color = FLinearColor::White;

	// alpha 0 = no shadow
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Credits")
	FLinearColor ShadowColor = FLinearColor::Transparent;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Credits")
	FVector2D ShadowOffset = FVector2D(3.f, 3.f);

	// space above the line
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Credits", meta = (ClampMin = 0))
	float PadTop = 0.f;
};

UCLASS()
class DANCEDANCEDIE_API UCreditsWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	struct FParams
	{
		TArray<FCreditLine> Lines;
		UFont* Font = nullptr;
		FCreditLook Title, Heading, Name;
		float GapSize = 80.f;
		UMaterialInterface* Effect = nullptr;
		float Speed = 70.f;
		float FastMultiplier = 5.f;
		float StartDelay = 1.f;
		float FadeTime = 1.f;
		float EndHold = 4.f;
		bool bStopLastLine = true;
	};

	void Setup(const FParams& In);
	void SetFast(bool bInFast) { bFast = bInFast; }
	void Leave(); // fade out now (skip)
	bool IsDone() const { return Phase == EPhase::Done; }

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float Dt) override;

private:
	enum class EPhase : uint8 { Wait, Roll, Hold, FadeOut, Done };

	UPROPERTY() TObjectPtr<URetainerBox> Retainer;
	UPROPERTY() TObjectPtr<UVerticalBox> Roll;
	UPROPERTY() TObjectPtr<UCanvasPanelSlot> RollSlot;
	UPROPERTY() TObjectPtr<UWidget> LastLine;

	FParams P;
	EPhase Phase = EPhase::Wait;
	float Clock = 0.f;
	float Y = 0.f;
	bool bPlaced = false;
	bool bFast = false;
};

UCLASS()
class DANCEDANCEDIE_API ACreditsGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ACreditsGameMode();

	// top to bottom = order on screen. Last line stops in the middle if StopOnLastLine.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Credits")
	TArray<FCreditLine> Lines;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Credits")
	TObjectPtr<UFont> Font;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Credits|Look")
	FCreditLook TitleLook;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Credits|Look")
	FCreditLook HeadingLook;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Credits|Look")
	FCreditLook NameLook;

	// height of a Gap line
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Credits|Look", meta = (ClampMin = 0))
	float GapSize = 90.f;

	// Retainer effect over the whole roll (PS1 pixelate/glitch). Empty = plain text.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Credits|Look")
	TObjectPtr<UMaterialInterface> Effect;

	// UI units per second (screen is ~1080 tall)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Credits|Timing", meta = (ClampMin = 1))
	float ScrollSpeed = 70.f;

	// speed x this while fast-forward is held
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Credits|Timing", meta = (ClampMin = 1))
	float FastMultiplier = 5.f;

	// secs of black before the roll starts
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Credits|Timing", meta = (ClampMin = 0))
	float StartDelay = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Credits|Timing", meta = (ClampMin = 0.01))
	float FadeTime = 1.5f;

	// secs the last line sits in the middle before the fade
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Credits|Timing", meta = (ClampMin = 0))
	float EndHold = 4.f;

	// true = last line stops centred (end card). false = everything scrolls off the top.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Credits|Timing")
	bool bStopOnLastLine = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Credits")
	FName NextLevel = TEXT("/Game/UI/MainMenu");

	virtual void Tick(float Dt) override;

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY() TObjectPtr<UCreditsWidget> Screen;
	bool bLeaving = false;
};
