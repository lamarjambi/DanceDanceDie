#include "PlayerProfile.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

DEFINE_LOG_CATEGORY_STATIC(LogPlayerProfile, Log, All);

namespace PlayerProfile
{
	const TCHAR* Slot = TEXT("Profile");
}

UPlayerProfileSubsystem* UPlayerProfileSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	const UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
	return GI ? GI->GetSubsystem<UPlayerProfileSubsystem>() : nullptr;
}

void UPlayerProfileSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	if (const UPlayerProfileSave* Save = Cast<UPlayerProfileSave>(UGameplayStatics::LoadGameFromSlot(PlayerProfile::Slot, 0)))
	{
		PlayerName = CleanName(Save->PlayerName);
		Character = Save->Character;
		UE_LOG(LogPlayerProfile, Log, TEXT("loaded profile: '%s' as %s"), *PlayerName, *CharacterName(Character));
	}
}

FString UPlayerProfileSubsystem::GetPlayerName() const
{
	return PlayerName.IsEmpty() ? CharacterName(Character) : PlayerName;
}

void UPlayerProfileSubsystem::SetProfile(const FString& InName, EPlayerCharacter InCharacter)
{
	Character = InCharacter;
	PlayerName = CleanName(InName);
	if (PlayerName.IsEmpty()) { PlayerName = CharacterName(Character); }

	UPlayerProfileSave* Save = Cast<UPlayerProfileSave>(UGameplayStatics::CreateSaveGameObject(UPlayerProfileSave::StaticClass()));
	check(Save);
	Save->PlayerName = PlayerName;
	Save->Character = Character;
	if (!UGameplayStatics::SaveGameToSlot(Save, PlayerProfile::Slot, 0))
	{
		UE_LOG(LogPlayerProfile, Error, TEXT("could not save the profile (name still used this run)"));
	}
	UE_LOG(LogPlayerProfile, Log, TEXT("profile set: '%s' as %s"), *PlayerName, *CharacterName(Character));
}

FString UPlayerProfileSubsystem::CharacterName(EPlayerCharacter C)
{
	switch (C)
	{
	case EPlayerCharacter::Abe: return TEXT("Abe");
	default:                    return TEXT("Michelle");
	}
}

FString UPlayerProfileSubsystem::CleanName(const FString& In)
{
	FString S;
	for (const TCHAR Ch : In)
	{
		if (!FChar::IsControl(Ch)) { S.AppendChar(Ch); }
	}
	S.TrimStartAndEndInline();
	return S.Left(MaxNameLength);
}
