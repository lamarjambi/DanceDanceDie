// shows the character picked on the profile screen as the player's body.
// Michelle stays the real player mesh (hidden, still animated by ABP_Michelle), so every BP that talks to her
// (BP_Spline's cast to ABP_Michelle, the lane dance montages, the hidden head) keeps working untouched.
// for Abe, a posable copy is added next to her and gets her pose retargeted every frame (RTG_Michelle_to_Abe,
// made by Scripts/setup_abe.py), and the first-person camera moves to his head.

#pragma once

#include "CoreMinimal.h"
#include "Components/PoseableMeshComponent.h"
#include "Subsystems/WorldSubsystem.h"
#include "PlayerBody.generated.h"

class ACharacter;
class UIKRetargeter;
class UIKRetargetProcessor;
class USkeletalMesh;
class USkeletalMeshComponent;

UCLASS()
class DANCEDANCEDIE_API UPlayerBodyMesh : public UPoseableMeshComponent
{
	GENERATED_BODY()

public:
	UPlayerBodyMesh();

	// Source = the hidden Michelle mesh to copy from
	bool Setup(USkeletalMeshComponent* InSource, UIKRetargeter* Retargeter);

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	USkeletalMesh* TargetMesh() const;
	void UpdateHead();

	UPROPERTY() TObjectPtr<USkeletalMeshComponent> Source;
	UPROPERTY() TObjectPtr<UIKRetargetProcessor> Processor;
	TArray<FTransform> SourcePose;
	bool bHeadHidden = false;
};

UCLASS()
class DANCEDANCEDIE_API UPlayerBodySubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	UPlayerBodySubsystem();

	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	void Apply(ACharacter* Player);

	UPROPERTY() TObjectPtr<USkeletalMesh> AbeMesh;
	UPROPERTY() TObjectPtr<UIKRetargeter> AbeRetargeter;
	TWeakObjectPtr<ACharacter> Handled; // the pawn already looked at (re-checks after a respawn)
};
