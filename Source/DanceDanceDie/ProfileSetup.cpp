#include "ProfileSetup.h"
#include "IntroChat.h"

#include "Animation/AnimationAsset.h"
#include "Animation/SkeletalMeshActor.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/EditableText.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/SizeBox.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Font.h"
#include "Engine/SceneCapture2D.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "Styling/CoreStyle.h"

DEFINE_LOG_CATEGORY_STATIC(LogProfileSetup, Log, All);

namespace ProfileSetup
{
	constexpr float FadeInTime = 0.8f;
	constexpr float JoinFadeTime = 0.45f;
	constexpr float PhotoW = 260.f;
	constexpr float PhotoH = 325.f;
	constexpr int32 RtW = 320;
	constexpr int32 RtH = 400;
	constexpr float CaptureFOV = 30.f;
	const FVector StageOrigin(0.f, 0.f, -60000.f); // far below the (empty) IntroChat level, one stage per character
	constexpr float StageSpacing = 5000.f;
	const FLinearColor Dark(0.035f, 0.035f, 0.045f, 0.9f);
	const FLinearColor Grey(0.55f, 0.55f, 0.6f, 1.f);

	FSlateBrush Rounded(const FLinearColor& Tint, const FLinearColor& Outline, float Width, float Radius)
	{
		FSlateBrush B;
		B.DrawAs = ESlateBrushDrawType::RoundedBox;
		B.TintColor = FSlateColor(Tint);
		B.OutlineSettings.CornerRadii = FVector4(Radius, Radius, Radius, Radius);
		B.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
		B.OutlineSettings.Width = Width;
		B.OutlineSettings.Color = FSlateColor(Outline);
		return B;
	}

	// invisible button: the bordered content inside draws everything
	FButtonStyle ClearButton()
	{
		FButtonStyle S;
		FSlateBrush None;
		None.DrawAs = ESlateBrushDrawType::NoDrawType;
		S.SetNormal(None).SetHovered(None).SetPressed(None).SetDisabled(None);
		S.SetNormalPadding(FMargin(0.f)).SetPressedPadding(FMargin(0.f));
		return S;
	}
}

FSlateFontInfo UProfileSetupWidget::MakeFont(int32 Size) const
{
	return Font ? FSlateFontInfo(Font.Get(), Size) : FCoreStyle::GetDefaultFontStyle("Regular", Size);
}

void UProfileSetupWidget::NativeConstruct()
{
	Super::NativeConstruct();
}

// the tree must exist before the widget goes on screen (NativeConstruct is too late: the slate widget is already built),
// so the options are read straight from the game mode here, like the chat does for its font
void UProfileSetupWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	check(WidgetTree);
	const AIntroChatGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<AIntroChatGameMode>() : nullptr;
	checkf(GM, TEXT("ProfileSetup only runs under AIntroChatGameMode"));
	Characters = GM->Characters;
	Title = GM->ProfileTitle;
	Subtitle = GM->ProfileSubtitle;
	Font = GM->ChatFont.Get();
	JoinSound = GM->PingSound;
	PhotoMaterial = GM->PhotoMaterial;
	checkf(Characters.Num() >= 1 && Characters.Num() <= 2, TEXT("ProfileSetup: 1 or 2 Characters (got %d)"), Characters.Num());

	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("ProfileRoot"));
	WidgetTree->RootWidget = Root;
	auto Fill = [](UCanvasPanelSlot* S) { S->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f)); S->SetOffsets(FMargin(0.f)); };

	UImage* Back = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("ProfileBack"));
	Back->SetColorAndOpacity(FLinearColor(0.006f, 0.006f, 0.01f, 1.f));
	Fill(Root->AddChildToCanvas(Back));

	UVerticalBox* Col = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("ProfileColumn"));
	{
		UCanvasPanelSlot* S = Root->AddChildToCanvas(Col);
		S->SetAnchors(FAnchors(0.5f, 0.5f));
		S->SetAlignment(FVector2D(0.5f, 0.5f));
		S->SetAutoSize(true);
	}
	auto AddCentered = [Col](UWidget* W, float Bottom) -> UVerticalBoxSlot*
	{
		UVerticalBoxSlot* S = Col->AddChildToVerticalBox(W);
		S->SetHorizontalAlignment(HAlign_Center);
		S->SetPadding(FMargin(0.f, 0.f, 0.f, Bottom));
		return S;
	};

	UTextBlock* TitleText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ProfileTitle"));
	TitleText->SetText(FText::FromString(Title));
	TitleText->SetFont(MakeFont(44));
	TitleText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	AddCentered(TitleText, 4.f);

	UTextBlock* SubText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ProfileSubtitle"));
	SubText->SetText(FText::FromString(Subtitle));
	SubText->SetFont(MakeFont(22));
	SubText->SetColorAndOpacity(FSlateColor(ProfileSetup::Grey));
	AddCentered(SubText, 34.f);

	// photos
	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("ProfilePhotos"));
	for (int32 i = 0; i < Characters.Num(); ++i)
	{
		UButton* Btn = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
		Btn->SetStyle(ProfileSetup::ClearButton());
		if (i == 0) { Btn->OnClicked.AddDynamic(this, &UProfileSetupWidget::OnPick0); }
		else        { Btn->OnClicked.AddDynamic(this, &UProfileSetupWidget::OnPick1); }

		UVerticalBox* Tile = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		UBorder* Frame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Frame->SetPadding(FMargin(6.f));
		UImage* Photo = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		USizeBox* PhotoBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		PhotoBox->SetWidthOverride(ProfileSetup::PhotoW);
		PhotoBox->SetHeightOverride(ProfileSetup::PhotoH);
		PhotoBox->SetContent(Photo);
		Frame->SetContent(PhotoBox);
		Tile->AddChildToVerticalBox(Frame);

		UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		Label->SetText(FText::FromString(UPlayerProfileSubsystem::CharacterName(Characters[i].Character).ToUpper()));
		Label->SetFont(MakeFont(24));
		UVerticalBoxSlot* LS = Tile->AddChildToVerticalBox(Label);
		LS->SetHorizontalAlignment(HAlign_Center);
		LS->SetPadding(FMargin(0.f, 10.f, 0.f, 0.f));

		Btn->SetContent(Tile);
		UHorizontalBoxSlot* HS = Row->AddChildToHorizontalBox(Btn);
		HS->SetPadding(FMargin(i == 0 ? 0.f : 22.f, 0.f, i == 0 ? 22.f : 0.f, 0.f));

		Frames.Add(Frame);
		Labels.Add(Label);
		Photos.Add(Photo);
	}
	AddCentered(Row, 30.f);

	// name
	UTextBlock* NameLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ProfileNameLabel"));
	NameLabel->SetText(FText::FromString(TEXT("DISPLAY NAME")));
	NameLabel->SetFont(MakeFont(18));
	NameLabel->SetColorAndOpacity(FSlateColor(ProfileSetup::Grey));
	AddCentered(NameLabel, 6.f);

	UBorder* NameFrame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("ProfileNameFrame"));
	NameFrame->SetBrush(ProfileSetup::Rounded(ProfileSetup::Dark, FLinearColor(1.f, 1.f, 1.f, 0.25f), 1.f, 14.f));
	NameFrame->SetPadding(FMargin(18.f, 8.f, 18.f, 8.f));
	NameBox = WidgetTree->ConstructWidget<UEditableText>(UEditableText::StaticClass(), TEXT("ProfileName"));
	{
		FEditableTextStyle Style = FCoreStyle::Get().GetWidgetStyle<FEditableTextStyle>("NormalEditableText");
		Style.SetFont(MakeFont(34));
		Style.SetColorAndOpacity(FSlateColor(FLinearColor::White));
		NameBox->SetWidgetStyle(Style);
	}
	NameBox->SetHintText(FText::FromString(TEXT("your name")));
	NameBox->SetJustification(ETextJustify::Center);
	NameBox->SetMinimumDesiredWidth(420.f);
	NameBox->OnTextChanged.AddDynamic(this, &UProfileSetupWidget::OnNameChanged);
	NameFrame->SetContent(NameBox);
	AddCentered(NameFrame, 26.f);

	// join
	UButton* JoinBtn = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("ProfileJoin"));
	JoinBtn->SetStyle(ProfileSetup::ClearButton());
	JoinBtn->OnClicked.AddDynamic(this, &UProfileSetupWidget::OnJoinClicked);
	UBorder* JoinFrame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("ProfileJoinFrame"));
	JoinFrame->SetBrush(ProfileSetup::Rounded(FLinearColor(1.f, 1.f, 1.f, 0.9f), FLinearColor::White, 1.f, 20.f));
	JoinFrame->SetPadding(FMargin(34.f, 6.f, 34.f, 8.f));
	UTextBlock* JoinText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	JoinText->SetText(FText::FromString(TEXT("JOIN CHAT")));
	JoinText->SetFont(MakeFont(30));
	JoinText->SetColorAndOpacity(FSlateColor(FLinearColor(0.02f, 0.02f, 0.03f, 1.f)));
	JoinFrame->SetContent(JoinText);
	JoinBtn->SetContent(JoinFrame);
	AddCentered(JoinBtn, 0.f);

	UTextBlock* Hint = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ProfileHint"));
	Hint->SetText(FText::FromString(Characters.Num() > 1 ? TEXT("click a photo or press tab to switch  ·  enter to join") : TEXT("enter to join")));
	Hint->SetFont(MakeFont(18));
	Hint->SetColorAndOpacity(FSlateColor(FLinearColor(1.f, 1.f, 1.f, 0.25f)));
	{
		UCanvasPanelSlot* S = Root->AddChildToCanvas(Hint);
		S->SetAnchors(FAnchors(1.f, 1.f));
		S->SetAlignment(FVector2D(1.f, 1.f));
		S->SetAutoSize(true);
		S->SetPosition(FVector2D(-40.f, -30.f));
	}

	FadeOverlay = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("ProfileFade"));
	FadeOverlay->SetColorAndOpacity(FLinearColor::Black);
	FadeOverlay->SetVisibility(ESlateVisibility::HitTestInvisible);
	Fill(Root->AddChildToCanvas(FadeOverlay));

	// start from the saved profile (if any)
	const UPlayerProfileSubsystem* Profile = UPlayerProfileSubsystem::Get(this);
	Selected = 0;
	if (Profile)
	{
		const int32 Saved = Characters.IndexOfByPredicate([Profile](const FProfileCharacter& C) { return C.Character == Profile->GetCharacter(); });
		if (Saved != INDEX_NONE) { Selected = Saved; }
	}
	SpawnPhotos();
	bTyped = Profile && Profile->HasProfile() && Profile->GetPlayerName() != UPlayerProfileSubsystem::CharacterName(Characters[Selected].Character);
	bSettingName = true;
	NameBox->SetText(FText::FromString(Profile && Profile->HasProfile() ? Profile->GetPlayerName() : UPlayerProfileSubsystem::CharacterName(Characters[Selected].Character)));
	bSettingName = false;
	UE_LOG(LogProfileSetup, Log, TEXT("start: saved %s -> photo %d of %d"),
		Profile ? *UPlayerProfileSubsystem::CharacterName(Profile->GetCharacter()) : TEXT("(no profile)"), Selected, Characters.Num());
	Pick(Selected);
}

void UProfileSetupWidget::SpawnPhotos()
{
	UWorld* World = GetWorld();
	check(World);
	for (int32 i = 0; i < Characters.Num(); ++i)
	{
		const FProfileCharacter& C = Characters[i];
		if (!C.Mesh)
		{
			UE_LOG(LogProfileSetup, Error, TEXT("ProfileSetup: character %d has no Mesh"), i);
			continue;
		}
		const FVector Stage = ProfileSetup::StageOrigin + FVector(ProfileSetup::StageSpacing * i, 0.f, 0.f);

		FActorSpawnParameters P;
		P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		ASkeletalMeshActor* Body = World->SpawnActor<ASkeletalMeshActor>(Stage, FRotator::ZeroRotator, P); // Mixamo meshes face +Y
		check(Body);
		USkeletalMeshComponent* Mesh = Body->GetSkeletalMeshComponent();
		Mesh->SetMobility(EComponentMobility::Movable);
		Mesh->SetSkeletalMeshAsset(C.Mesh);
		Mesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones; // only the capture sees it
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		if (C.Idle) { Mesh->PlayAnimation(C.Idle, true); }
		else { UE_LOG(LogProfileSetup, Warning, TEXT("ProfileSetup: character %d has no Idle"), i); }

		const FVector Cam = Stage + FVector(0.f, C.CameraDistance, C.CameraHeight + 8.f);
		const FRotator Look = (Stage + FVector(0.f, 0.f, C.CameraHeight) - Cam).Rotation();
		ASceneCapture2D* Capture = World->SpawnActor<ASceneCapture2D>(Cam, Look, P);
		check(Capture);
		USceneCaptureComponent2D* CC = Capture->GetCaptureComponent2D();
		UTextureRenderTarget2D* RT = NewObject<UTextureRenderTarget2D>(this);
		RT->RenderTargetFormat = RTF_RGBA8;
		RT->ClearColor = FLinearColor(0.006f, 0.006f, 0.01f, 1.f);
		RT->InitAutoFormat(ProfileSetup::RtW, ProfileSetup::RtH);
		RT->UpdateResourceImmediate(true);
		CC->TextureTarget = RT;
		CC->CaptureSource = ESceneCaptureSource::SCS_BaseColor; // unlit albedo: no lights or exposure needed in the empty chat level
		CC->FOVAngle = ProfileSetup::CaptureFOV;
		CC->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
		CC->ShowOnlyActors.Add(Body);
		CC->bCaptureEveryFrame = true;
		CC->bCaptureOnMovement = false;

		if (Photos.IsValidIndex(i))
		{
			if (PhotoMaterial)
			{
				UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(PhotoMaterial, this);
				MID->SetTextureParameterValue(TEXT("Photo"), RT);
				Photos[i]->SetBrushFromMaterial(MID);
			}
			else
			{
				UE_LOG(LogProfileSetup, Error, TEXT("ProfileSetup: no Photo Material (run Scripts/setup_profile_photo.py), photos will be blank"));
				Photos[i]->SetBrushResourceObject(RT);
			}
		}
		Spawned.Add(Body);
		Spawned.Add(Capture);
	}
}

void UProfileSetupWidget::NativeDestruct()
{
	for (AActor* A : Spawned)
	{
		if (IsValid(A)) { A->Destroy(); }
	}
	Spawned.Reset();
	Super::NativeDestruct();
}

void UProfileSetupWidget::FocusName()
{
	if (NameBox) { NameBox->SetKeyboardFocus(); }
}

void UProfileSetupWidget::Pick(int32 Index)
{
	if (!Characters.IsValidIndex(Index) || bJoining) { return; }
	const bool bChanged = Index != Selected;
	Selected = Index;
	UE_LOG(LogProfileSetup, Log, TEXT("pick %d"), Index);
	for (int32 i = 0; i < Frames.Num(); ++i)
	{
		const bool bOn = i == Selected;
		Frames[i]->SetBrush(ProfileSetup::Rounded(ProfileSetup::Dark, FLinearColor(1.f, 1.f, 1.f, bOn ? 0.95f : 0.08f), bOn ? 3.f : 1.f, 18.f));
		Photos[i]->SetColorAndOpacity(bOn ? FLinearColor::White : FLinearColor(0.35f, 0.35f, 0.38f, 1.f));
		Labels[i]->SetColorAndOpacity(FSlateColor(bOn ? FLinearColor::White : ProfileSetup::Grey));
	}
	// an untouched name follows the photo (Michelle -> Abe); a typed one stays
	if (!bTyped && NameBox)
	{
		bSettingName = true;
		NameBox->SetText(FText::FromString(UPlayerProfileSubsystem::CharacterName(Characters[Selected].Character)));
		bSettingName = false;
	}
	if (bChanged && PickSound) { UGameplayStatics::PlaySound2D(this, PickSound); }
	FocusName();
}

void UProfileSetupWidget::OnPick0() { Pick(0); }
void UProfileSetupWidget::OnPick1() { Pick(1); }
void UProfileSetupWidget::OnJoinClicked() { Join(); }

void UProfileSetupWidget::OnNameChanged(const FText& Text)
{
	if (bSettingName) { return; }
	FString S = Text.ToString();
	if (S.Len() > UPlayerProfileSubsystem::MaxNameLength)
	{
		bSettingName = true;
		NameBox->SetText(FText::FromString(S.Left(UPlayerProfileSubsystem::MaxNameLength)));
		bSettingName = false;
	}
	bTyped = true;
}

void UProfileSetupWidget::Join()
{
	if (bJoining) { return; }
	FString Name = UPlayerProfileSubsystem::CleanName(NameBox->GetText().ToString());
	if (Name.IsEmpty()) { Name = UPlayerProfileSubsystem::CharacterName(Characters[Selected].Character); }
	if (UPlayerProfileSubsystem* Profile = UPlayerProfileSubsystem::Get(this))
	{
		Profile->SetProfile(Name, Characters[Selected].Character);
	}
	else
	{
		UE_LOG(LogProfileSetup, Error, TEXT("ProfileSetup: no PlayerProfileSubsystem, name not kept"));
	}
	NameBox->SetIsReadOnly(true);
	if (JoinSound) { UGameplayStatics::PlaySound2D(this, JoinSound); }
	bJoining = true;
	JoinTime = 0.f;
}

FReply UProfileSetupWidget::NativeOnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& KeyEvent)
{
	const FKey K = KeyEvent.GetKey();
	if (K == EKeys::Enter || K == EKeys::Gamepad_FaceButton_Bottom || K == EKeys::Gamepad_Special_Right)
	{
		Join();
		return FReply::Handled();
	}
	if (K == EKeys::Tab || K == EKeys::Gamepad_DPad_Right || K == EKeys::Gamepad_RightShoulder)
	{
		Pick((Selected + 1) % Characters.Num());
		return FReply::Handled();
	}
	if (K == EKeys::Gamepad_DPad_Left || K == EKeys::Gamepad_LeftShoulder)
	{
		Pick((Selected + Characters.Num() - 1) % Characters.Num());
		return FReply::Handled();
	}
	return Super::NativeOnPreviewKeyDown(Geometry, KeyEvent);
}

void UProfileSetupWidget::Step(float Dt)
{
	Time += Dt;
	if (!bJoining)
	{
		FadeOverlay->SetRenderOpacity(1.f - FMath::Clamp(Time / ProfileSetup::FadeInTime, 0.f, 1.f));
		return;
	}
	JoinTime += Dt;
	const float A = FMath::Clamp(JoinTime / ProfileSetup::JoinFadeTime, 0.f, 1.f);
	FadeOverlay->SetRenderOpacity(FMath::Max(A, FadeOverlay->GetRenderOpacity()));
	if (A >= 1.f) { bDone = true; }
}
