// the player's name + character, picked on the phone profile screen before the intro chat (ProfileSetup.*).
// lives for the whole run (game instance subsystem) and is saved to the "Profile" slot so the next launch remembers it.
// read it anywhere: UPlayerProfileSubsystem::Get(this)->GetPlayerName() / GetCharacter(), or the BP nodes below.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "PlayerProfile.generated.h"

// order = order of the photos on the profile screen (AIntroChatGameMode > Characters)
UENUM(BlueprintType)
enum class EPlayerCharacter : uint8
{
	Michelle,
	Abe,
};

UCLASS()
class DANCEDANCEDIE_API UPlayerProfileSave : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY() FString PlayerName;
	UPROPERTY() EPlayerCharacter Character = EPlayerCharacter::Michelle;
};

UCLASS()
class DANCEDANCEDIE_API UPlayerProfileSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	static constexpr int32 MaxNameLength = 16;

	static UPlayerProfileSubsystem* Get(const UObject* WorldContext);

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	// typed name, or the character's own name if the player left it empty
	UFUNCTION(BlueprintPure, Category = "Player Profile")
	FString GetPlayerName() const;

	UFUNCTION(BlueprintPure, Category = "Player Profile")
	EPlayerCharacter GetCharacter() const { return Character; }

	// true once a name was confirmed (this run or a saved one)
	UFUNCTION(BlueprintPure, Category = "Player Profile")
	bool HasProfile() const { return !PlayerName.IsEmpty(); }

	// trims + caps the name, then saves to disk
	UFUNCTION(BlueprintCallable, Category = "Player Profile")
	void SetProfile(const FString& InName, EPlayerCharacter InCharacter);

	static FString CharacterName(EPlayerCharacter C);
	static FString CleanName(const FString& In);

private:
	FString PlayerName;
	EPlayerCharacter Character = EPlayerCharacter::Michelle;
};
