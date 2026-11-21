// "set up your profile" character select shown before the intro chat: pick a character, type a display name, join.
// built in c++ like the chat (no wbp). it reads as a text invite: the invite is a message bubble, the picked character has the
// chat's "typing" bubble over their head, the name goes in a message bar with JOIN CHAT as its send button.
// the game's lime + magenta, the chat's font, M_UI_PS1 over everything.
// the characters are live: each one stands in front of its own scene capture, far below the IntroChat level.
// options live on BP_IntroChatGameMode (Characters, Profile*).

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PlayerProfile.h"
#include "GlitchFX.h"
#include "ProfileSetup.generated.h"

class UButton;
class UEditableText;
class UImage;
class UTextBlock;
class UWidget;
class UAnimationAsset;
class UMaterialInterface;
class USkeletalMesh;
class USkeletalMeshComponent;
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

	// looping idle: runs while this character is picked or under the mouse, frozen otherwise
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Profile")
	TObjectPtr<UAnimationAsset> Idle;

	// full-body framing: the height the camera looks at (cm above the feet) and its distance.
	// same values for everyone = true relative heights
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Profile")
	float CameraHeight = 92.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Profile")
	float CameraDistance = 284.f;
};

UCLASS()
class DANCEDANCEDIE_API UProfileSetupWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	TArray<FProfileCharacter> Characters;
	FString Title;
	FString Subtitle;
	TObjectPtr<UObject> Font;          // VT323, the chat's font: everything on this screen
	TObjectPtr<USoundBase> JoinSound;
	TObjectPtr<USoundBase> PickSound;
	TObjectPtr<UMaterialInterface> PhotoMaterial;
	// M_UI_PS1 over the whole screen, like the menu. join = the button goes red + glitch burst, then a hard cut to the chat
	TObjectPtr<UMaterialInterface> GlitchEffect;
	TObjectPtr<USoundBase> GlitchSound;
	float GlitchTime = 0.7f;
	FLinearColor JoinRed = FLinearColor(1.f, 0.03f, 0.03f);
	// the two menu colours: lime (picked name, typing bubble, name field, button) and magenta (shadows, hover)
	FLinearColor HoverColor = FLinearColor(0.25f, 0.9f, 0.08f);
	FLinearColor AccentColor = FLinearColor(0.45f, 0.02f, 0.42f);
	float PixelRows = 270.f; // PS1 blocks per screen height, = credits

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
	void StyleCharacter(int32 Index); // brightness, typing bubble, hover scale of one character
	void StyleJoin();                 // button colours (idle, hovered, held down)
	UWidget* MakeTypingBubble();
	TArray<URetainerBox*> AllRetainers() const;
	FSlateFontInfo MakeFont(int32 Size, UObject* Face = nullptr) const;

	UPROPERTY() TObjectPtr<UEditableText> NameBox;
	UPROPERTY() TObjectPtr<UImage> FadeOverlay;
	// M_UI_PS1 in pieces, so only the piece under the mouse glitches: the still parts (full screen), one per character,
	// the name field, the button
	UPROPERTY() TObjectPtr<URetainerBox> Retainer;
	UPROPERTY() TArray<TObjectPtr<URetainerBox>> CharRetainers;
	UPROPERTY() TObjectPtr<URetainerBox> FieldRetainer;
	UPROPERTY() TObjectPtr<URetainerBox> JoinRetainer;
	UPROPERTY() TObjectPtr<UTextBlock> JoinText;
	UPROPERTY() TObjectPtr<class UBorder> JoinFace;
	UPROPERTY() TArray<TObjectPtr<UTextBlock>> Dots; // 3 per character: the typing bubble
	UPROPERTY() TObjectPtr<UTextBlock> Hint;
	UPROPERTY() TObjectPtr<UButton> JoinBtn;
	UPROPERTY() TArray<TObjectPtr<UButton>> PhotoBtns;
	UPROPERTY() TArray<TObjectPtr<UImage>> Photos;
	UPROPERTY() TArray<TObjectPtr<UWidget>> Arrows; // typing bubble, one per character, only the picked one's shows
	UPROPERTY() TArray<TObjectPtr<AActor>> Spawned;
	UPROPERTY() TArray<TObjectPtr<USkeletalMeshComponent>> Bodies; // one per character (null = no mesh)

	int32 Selected = 0;
	bool bSettingName = false;
	bool bJoining = false;
	bool bDone = false;
	bool bJoinHover = false;
	bool bJoinDown = false;
	float Time = 0.f;
	float JoinTime = 0.f;
	TArray<float> PhotoHover;  // 0..1 per character, eased
	TArray<float> ArrowDrop;   // per character: how far the typing bubble hangs into the picture to sit over the head
	TArray<FGlitchFX> Glitches; // join glitch, one per retainer (AllRetainers order)
};
