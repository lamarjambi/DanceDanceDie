#include "PlayerBody.h"
#include "PlayerProfile.h"

#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Retargeter/IKRetargeter.h"
#include "Retargeter/IKRetargetProcessor.h"
#include "UObject/ConstructorHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogPlayerBody, Log, All);

namespace PlayerBody
{
	const FName HeadBone(TEXT("Head"));
	const FName MichelleMeshName(TEXT("michelle"));
}

// ---------------------------------------------------------------- body mesh

UPlayerBodyMesh::UPlayerBodyMesh()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	// after the source's anim (kicked off in pre-physics, possibly on worker threads) has finished;
	// the camera manager reads the head socket after all tick groups
	PrimaryComponentTick.TickGroup = TG_PostPhysics;
}

USkeletalMesh* UPlayerBodyMesh::TargetMesh() const
{
	return Cast<USkeletalMesh>(GetSkinnedAsset());
}

bool UPlayerBodyMesh::Setup(USkeletalMeshComponent* InSource, UIKRetargeter* Retargeter)
{
	Source = InSource;
	Processor = NewObject<UIKRetargetProcessor>(this);
	Processor->Initialize(Source->GetSkeletalMeshAsset(), TargetMesh(), Retargeter);
	if (!Processor->IsInitialized())
	{
		UE_LOG(LogPlayerBody, Error, TEXT("retarget %s -> %s with %s failed to initialize (see LogIKRig)"),
			*GetNameSafe(Source->GetSkeletalMeshAsset()), *GetNameSafe(TargetMesh()), *GetNameSafe(Retargeter));
		return false;
	}
	AddTickPrerequisiteComponent(Source);
	return true;
}

void UPlayerBodyMesh::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!Processor || !Processor->IsInitialized() || !IsValid(Source) || !TargetMesh()) { return; }

	const TArray<FTransform>& SourceCS = Source->GetComponentSpaceTransforms();
	if (SourceCS.Num() != Source->GetSkeletalMeshAsset()->GetRefSkeleton().GetNum()) { return; } // not evaluated yet
	SourcePose = SourceCS;
	for (FTransform& T : SourcePose) { T.SetScale3D(FVector::OneVector); } // retargeter works on unscaled component space

	static const TMap<FName, float> NoSpeedCurves;
	const TArray<FTransform>& TargetCS = Processor->RunRetargeter(SourcePose, NoSpeedCurves, DeltaTime);

	const FReferenceSkeleton& Ref = TargetMesh()->GetRefSkeleton();
	const TArray<FTransform>& RefPose = Ref.GetRefBonePose();
	if (TargetCS.Num() != Ref.GetNum() || BoneSpaceTransforms.Num() != Ref.GetNum()) { return; }
	for (int32 i = 0; i < Ref.GetNum(); ++i)
	{
		const int32 Parent = Ref.GetParentIndex(i);
		FTransform Local = Parent == INDEX_NONE ? TargetCS[i] : TargetCS[i].GetRelativeTransform(TargetCS[Parent]);
		Local.SetScale3D(RefPose[i].GetScale3D());
		BoneSpaceTransforms[i] = Local;
	}
	RefreshBoneTransforms();
	UpdateHead();
}

// head hidden only while the camera is the player's own eyes; dance cams (BP_Spline SetViewTargetWithBlend) see it.
// a blend toward another camera shows it right away, a blend back hides it right away.
void UPlayerBodyMesh::UpdateHead()
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	const APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	const APlayerCameraManager* Cam = PC ? PC->PlayerCameraManager.Get() : nullptr;
	bool bFirstPerson = true;
	if (Cam)
	{
		const AActor* Pending = Cam->PendingViewTarget.Target;
		const AActor* Target = Pending ? Pending : Cam->GetViewTarget();
		bFirstPerson = !Target || Target == Pawn;
	}
	if (bFirstPerson == bHeadHidden) { return; }
	bHeadHidden = bFirstPerson;
	if (bHeadHidden) { HideBoneByName(PlayerBody::HeadBone, PBO_None); }
	else { UnHideBoneByName(PlayerBody::HeadBone); }
}

// ---------------------------------------------------------------- subsystem

UPlayerBodySubsystem::UPlayerBodySubsystem()
{
	// hard refs from a native constructor = loaded at startup and cooked with the game
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> Abe(TEXT("/Game/Characters/Abe/abe-walking.abe-walking"));
	static ConstructorHelpers::FObjectFinder<UIKRetargeter> Rtg(TEXT("/Game/Characters/Abe/Retarget/RTG_Michelle_to_Abe.RTG_Michelle_to_Abe"));
	AbeMesh = Abe.Object;
	AbeRetargeter = Rtg.Object;
}

bool UPlayerBodySubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

TStatId UPlayerBodySubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UPlayerBodySubsystem, STATGROUP_Tickables);
}

void UPlayerBodySubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	ACharacter* Player = UGameplayStatics::GetPlayerCharacter(this, 0);
	if (!Player || Handled.Get() == Player) { return; }
	Handled = Player;
	Apply(Player);
}

void UPlayerBodySubsystem::Apply(ACharacter* Player)
{
	const UPlayerProfileSubsystem* Profile = UPlayerProfileSubsystem::Get(this);
	if (!Profile || Profile->GetCharacter() != EPlayerCharacter::Abe) { return; } // Michelle = the mesh as built

	USkeletalMeshComponent* Michelle = Player->GetMesh();
	if (!Michelle || !Michelle->GetSkeletalMeshAsset() || Michelle->GetSkeletalMeshAsset()->GetFName() != PlayerBody::MichelleMeshName)
	{
		UE_LOG(LogPlayerBody, Warning, TEXT("%s's mesh is not Michelle, leaving it alone"), *Player->GetName());
		return;
	}
	if (!AbeMesh || !AbeRetargeter)
	{
		UE_LOG(LogPlayerBody, Error, TEXT("Abe's mesh or RTG_Michelle_to_Abe is missing (run Scripts/setup_abe.py), staying Michelle"));
		return;
	}

	UPlayerBodyMesh* Body = NewObject<UPlayerBodyMesh>(Player, TEXT("AbeBody"));
	Body->SetSkinnedAssetAndUpdate(AbeMesh);
	Body->SetupAttachment(Michelle->GetAttachParent());
	Body->SetRelativeTransform(Michelle->GetRelativeTransform());
	Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Body->SetCastShadow(Michelle->CastShadow);
	Body->RegisterComponent();
	if (!Body->Setup(Michelle, AbeRetargeter))
	{
		Body->DestroyComponent();
		return;
	}

	// keep Michelle animating for the BPs, but don't draw her
	Michelle->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	Michelle->SetVisibility(false, false);

	// the camera (boom on Michelle's Head socket) follows Abe's head instead
	TArray<USceneComponent*> Children = Michelle->GetAttachChildren();
	for (USceneComponent* C : Children)
	{
		if (C && C->GetAttachSocketName() == PlayerBody::HeadBone)
		{
			C->AttachToComponent(Body, FAttachmentTransformRules::KeepRelativeTransform, PlayerBody::HeadBone);
			UE_LOG(LogPlayerBody, Log, TEXT("moved %s to Abe's head"), *C->GetName());
		}
	}
	UE_LOG(LogPlayerBody, Log, TEXT("%s is Abe"), *Player->GetName());
}
