// phone "set up your profile" screen shown before the intro chat: pick a photo (= character), type a display name, join.
// built in c++ like the chat (no wbp). the photos are live: each character idles in front of its own scene capture,
// far below the IntroChat level. options live on BP_IntroChatGameMode (Characters, Profile*).

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PlayerProfile.h"
#include "ProfileSetup.generated.h"

class UBorder;
class UEditableText;
class UImage;
class UTextBlock;
class UAnimationAsset;
class USkeletalMesh;
class USoundBase;
class AActor;

USTRUCT(BlueprintType)
struct FProfileCharacter
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Profile")
	EPlayerCharacter Character = EPlayerCharacter::Michelle;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Profile")
	TObjectPtr<USkeletalMesh> Mesh;

	// looping idle played in the photo
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Profile")
	TObjectPtr<UAnimationAsset> Idle;

	// photo framing: camera height (cm above the feet) and distance
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Profile")
	float CameraHeight = 140.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Profile")
	float CameraDistance = 190.f;
};

UCLASS()
class DANCEDANCEDIE_API UProfileSetupWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	TArray<FProfileCharacter> Characters;
	FString Title;
	FString Subtitle;
	TObjectPtr<UObject> Font;
	TObjectPtr<USoundBase> JoinSound;
	TObjectPtr<USoundBase> PickSound;
	TObjectPtr<class UMaterialInterface> PhotoMaterial;

	void Step(float Dt);
	bool IsDone() const { return bDone; }
	// keyboard focus on the name box (call after AddToViewport)
	void FocusName();

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual FReply NativeOnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& KeyEvent) override;

private:
	UFUNCTION() void OnPick0();
	UFUNCTION() void OnPick1();
	UFUNCTION() void OnJoinClicked();
	UFUNCTION() void OnNameChanged(const FText& Text);

	void Pick(int32 Index);
	void Join();
	void SpawnPhotos();
	FSlateFontInfo MakeFont(int32 Size) const;

	UPROPERTY() TObjectPtr<UEditableText> NameBox;
	UPROPERTY() TObjectPtr<UImage> FadeOverlay;
	UPROPERTY() TArray<TObjectPtr<UBorder>> Frames;
	UPROPERTY() TArray<TObjectPtr<UTextBlock>> Labels;
	UPROPERTY() TArray<TObjectPtr<UImage>> Photos;
	UPROPERTY() TArray<TObjectPtr<AActor>> Spawned;

	int32 Selected = 0;
	bool bTyped = false;     // player changed the name: switching character keeps it
	bool bSettingName = false;
	bool bJoining = false;
	bool bDone = false;
	float Time = 0.f;
	float JoinTime = 0.f;
};
