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
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
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
	// characters: portrait render target, shown at CharW x CharH slate units (same aspect)
	constexpr int32 RtW = 216;
	constexpr int32 RtH = 384;
	constexpr float CharW = 370.f;
	constexpr float CharH = CharW * RtH / RtW;
	constexpr float CaptureFOV = 22.f; // horizontal; narrow = little perspective on a full body
	const FVector StageOrigin(0.f, 0.f, -60000.f); // far below the (empty) IntroChat level, one stage per character
	constexpr float StageSpacing = 5000.f;
	constexpr float Edge = 90.f;         // margin of the corner blocks (title, name, JOIN CHAT)
	constexpr float Px = 10.f;           // one "pixel" of the bubble tails
	constexpr float Line = 4.f;          // outline of bubbles, the name field
	constexpr float ArrowBob = 10.f;
	constexpr float BubbleSpace = 60.f;  // empty strip above each picture, room for the typing bubble over a tall character
	constexpr float BubbleH = 84.f;      // typing bubble + its tail
	constexpr float HoverGlitch = 5.f;   // M_UI_PS1 "Glitch" on the thing under the mouse (0 everywhere else: calm screen)
	constexpr float TearRoom = 16.f;     // clear strip left + right of the button inside its retainer, so torn rows have somewhere to go
	const FVector2D Shadow(4.f, 4.f);    // text shadow
	const FLinearColor Ink(0.02f, 0.02f, 0.03f, 1.f); // inside of bubbles and the name field; text on the button

	// solid block of a fixed size. UImage::SetDesiredSizeOverride is not kept before the slate widget exists: use the brush size
	void Block(UImage* Img, const FLinearColor& Color, float W, float H)
	{
		FSlateBrush B;
		B.ImageSize = FVector2D(W, H);
		Img->SetBrush(B);
		Img->SetColorAndOpacity(Color);
	}

	// hard-cornered box with an outline: message bubbles, the name field, the button
	FSlateBrush Box(const FLinearColor& Fill, const FLinearColor& Outline, float Width)
	{
		FSlateBrush B;
		B.DrawAs = ESlateBrushDrawType::RoundedBox;
		B.TintColor = FSlateColor(Fill);
		B.OutlineSettings.CornerRadii = FVector4(0.f, 0.f, 0.f, 0.f);
		B.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
		B.OutlineSettings.Width = Width;
		B.OutlineSettings.Color = FSlateColor(Outline);
		return B;
	}

	// invisible button: its content draws everything
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

FSlateFontInfo UProfileSetupWidget::MakeFont(int32 Size, UObject* Face) const
{
	UObject* F = Face ? Face : Font.Get();
	return F ? FSlateFontInfo(F, Size) : FCoreStyle::GetDefaultFontStyle("Regular", Size);
}

void UProfileSetupWidget::NativeConstruct()
{
	Super::NativeConstruct();
}

// what points at the picked character: the chat's "typing" bubble (three dots) with a tail hanging down to their head.
// same job as an arrow, but it says "this one is about to text"
UWidget* UProfileSetupWidget::MakeTypingBubble()
{
	UVerticalBox* V = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());

	UBorder* Bubble = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Bubble->SetBrush(ProfileSetup::Box(ProfileSetup::Ink, HoverColor, ProfileSetup::Line));
	Bubble->SetPadding(FMargin(16.f, 0.f, 16.f, 4.f));
	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	for (int32 d = 0; d < 3; ++d)
	{
		UTextBlock* Dot = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		Dot->SetText(FText::FromString(TEXT("•")));
		Dot->SetFont(MakeFont(48));
		Row->AddChildToHorizontalBox(Dot)->SetPadding(FMargin(d ? 10.f : 0.f, 0.f, 0.f, 0.f));
		Dots.Add(Dot);
	}
	Bubble->SetContent(Row);
	V->AddChildToVerticalBox(Bubble)->SetHorizontalAlignment(HAlign_Center);

	// tail: 2 blocks then 1, like a pixel speech bubble
	for (int32 W = 2; W >= 1; --W)
	{
		UImage* B = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		ProfileSetup::Block(B, HoverColor, W * ProfileSetup::Px, ProfileSetup::Px);
		V->AddChildToVerticalBox(B)->SetHorizontalAlignment(HAlign_Center);
	}
	V->SetVisibility(ESlateVisibility::Hidden);
	return V;
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
	GlitchEffect = GM->JoinGlitchEffect;
	GlitchSound = GM->JoinGlitchSound;
	GlitchTime = FMath::Max(GM->JoinGlitchTime, 0.05f);
	JoinRed = GM->JoinRedColor;
	HoverColor = GM->ProfileHoverColor;
	AccentColor = GM->ProfileAccentColor;
	if (!GlitchEffect) { UE_LOG(LogProfileSetup, Error, TEXT("ProfileSetup: Join Glitch Effect (M_UI_PS1) not set: no PS1 look, join glitch will be jitter only")); }
	checkf(Characters.Num() >= 1 && Characters.Num() <= 2, TEXT("ProfileSetup: 1 or 2 Characters (got %d)"), Characters.Num());

	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("ProfileRoot"));
	WidgetTree->RootWidget = Root;
	auto Fill = [](UCanvasPanelSlot* S) { S->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f)); S->SetOffsets(FMargin(0.f)); };

	// black, not the chat's dark blue: the captures have a black background, so the characters stand on it with no box around them
	UImage* Back = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("ProfileBack"));
	Back->SetColorAndOpacity(FLinearColor::Black);
	Fill(Root->AddChildToCanvas(Back));

	// everything goes through M_UI_PS1 (same as the menu and credits), but in pieces: the material glitches a whole retainer,
	// and only the thing under the mouse may glitch (user choice: calm until then). so each character and the button get
	// their own retainer, side by side, never nested (nested = pixelated and brightened twice)
	auto Wrap = [this](UWidget* Content) -> URetainerBox*
	{
		URetainerBox* R = WidgetTree->ConstructWidget<URetainerBox>(URetainerBox::StaticClass());
		if (GlitchEffect)
		{
			R->SetEffectMaterial(GlitchEffect);
			R->SetTextureParameter(TEXT("Texture"));
		}
		R->AddChild(Content);
		return R;
	};
	UCanvasPanel* Stage = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("ProfileStage"));
	Retainer = Wrap(Stage); // the still parts (invite, title); full screen, so it also gives the block size for the others
	Fill(Root->AddChildToCanvas(Retainer));

	// top left: the invite arrives as a text message, then what to do about it. all in the chat's font.
	// text under ~40 px gets eaten by the pixelation
	UVerticalBox* Head = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("ProfileHead"));
	{
		UCanvasPanelSlot* S = Stage->AddChildToCanvas(Head);
		S->SetAnchors(FAnchors(0.f, 0.f));
		S->SetAutoSize(true);
		S->SetPosition(FVector2D(ProfileSetup::Edge, 36.f));
	}
	UBorder* Invite = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("ProfileInvite"));
	Invite->SetBrush(ProfileSetup::Box(ProfileSetup::Ink, FLinearColor::White, ProfileSetup::Line));
	Invite->SetPadding(FMargin(22.f, 6.f, 22.f, 10.f));
	UTextBlock* SubText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ProfileSubtitle"));
	SubText->SetText(FText::FromString(Subtitle));
	SubText->SetFont(MakeFont(44));
	SubText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	Invite->SetContent(SubText);
	Head->AddChildToVerticalBox(Invite)->SetHorizontalAlignment(HAlign_Left);
	for (int32 W = 2; W >= 1; --W) // bubble tail, bottom left = someone else's message
	{
		UImage* B = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		ProfileSetup::Block(B, FLinearColor::White, W * ProfileSetup::Px, ProfileSetup::Px);
		UVerticalBoxSlot* TS = Head->AddChildToVerticalBox(B);
		TS->SetHorizontalAlignment(HAlign_Left);
		TS->SetPadding(FMargin(30.f, 0.f, 0.f, 0.f));
	}
	UTextBlock* TitleText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ProfileTitle"));
	TitleText->SetText(FText::FromString(Title));
	TitleText->SetFont(MakeFont(48));
	TitleText->SetColorAndOpacity(FSlateColor(HoverColor));
	TitleText->SetShadowColorAndOpacity(AccentColor);
	TitleText->SetShadowOffset(ProfileSetup::Shadow);
	Head->AddChildToVerticalBox(TitleText)->SetPadding(FMargin(0.f, 4.f, 0.f, 0.f));

	// the characters: full bodies side by side, no frames. arrow over the picked one, name under each
	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("ProfileCharacters"));
	{
		UCanvasPanelSlot* S = Root->AddChildToCanvas(Row);
		S->SetAnchors(FAnchors(0.5f, 0.5f));
		S->SetAlignment(FVector2D(0.5f, 0.5f));
		S->SetAutoSize(true);
		S->SetPosition(FVector2D(0.f, 30.f));
	}
	for (int32 i = 0; i < Characters.Num(); ++i)
	{
		UButton* Btn = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
		Btn->SetStyle(ProfileSetup::ClearButton());
		Btn->SetCursor(EMouseCursor::Hand);
		if (i == 0) { Btn->OnClicked.AddDynamic(this, &UProfileSetupWidget::OnPick0); }
		else        { Btn->OnClicked.AddDynamic(this, &UProfileSetupWidget::OnPick1); }

		UVerticalBox* Tile = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());

		// the bubble hangs into the picture, so it needs its own layer on top: siblings in a box share one layer and the
		// (opaque) picture can end up drawn over it
		UOverlay* Stack = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
		UImage* Photo = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		USizeBox* PhotoBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		PhotoBox->SetWidthOverride(ProfileSetup::CharW);
		PhotoBox->SetHeightOverride(ProfileSetup::CharH);
		PhotoBox->SetContent(Photo);
		Stack->AddChildToOverlay(PhotoBox)->SetPadding(FMargin(0.f, ProfileSetup::BubbleSpace, 0.f, 0.f));
		UWidget* Arrow = MakeTypingBubble();
		{
			UOverlaySlot* AS = Stack->AddChildToOverlay(Arrow);
			AS->SetHorizontalAlignment(HAlign_Center);
			AS->SetVerticalAlignment(VAlign_Top);
		}
		Tile->AddChildToVerticalBox(Stack);

		// no "MICHELLE" / "ABE" under them (user choice): the only name on this screen is the one the player types

		URetainerBox* CharRetainer = Wrap(Tile);
		CharRetainer->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
		CharRetainers.Add(CharRetainer);
		Btn->SetContent(CharRetainer);
		UHorizontalBoxSlot* HS = Row->AddChildToHorizontalBox(Btn);
		HS->SetPadding(FMargin(i == 0 ? 0.f : 30.f, 0.f, i == 0 ? 30.f : 0.f, 0.f));

		// the typing bubble sits just over this character's head, not over the (taller) picture: head height from the mesh bounds,
		// mapped through the capture camera to a distance below the top of the picture
		float Drop = 0.f;
		if (const USkeletalMesh* M = Characters[i].Mesh)
		{
			const FBoxSphereBounds B = M->GetImportedBounds();
			const float Top = B.Origin.Z + B.BoxExtent.Z; // cm above the feet
			const float HalfView = Characters[i].CameraDistance * FMath::Tan(FMath::DegreesToRadians(ProfileSetup::CaptureFOV * 0.5f)) * ProfileSetup::RtH / ProfileSetup::RtW;
			const float FromTop = 0.5f - (Top - Characters[i].CameraHeight) / (2.f * HalfView);
			Drop = FMath::Clamp(ProfileSetup::BubbleSpace + FromTop * ProfileSetup::CharH - ProfileSetup::BubbleH - 8.f, 0.f, ProfileSetup::CharH * 0.5f);
			UE_LOG(LogProfileSetup, Log, TEXT("character %d: top %.0f cm -> bubble drop %.0f"), i, Top, Drop);
		}
		ArrowDrop.Add(Drop);
		PhotoBtns.Add(Btn);
		PhotoHover.Add(0.f);
		Arrows.Add(Arrow);
		Photos.Add(Photo);
	}

	// bottom: a message bar like the one you type texts into. the name goes in the field, JOIN CHAT is its send button
	UHorizontalBox* Bar = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("ProfileBar"));
	{
		UCanvasPanelSlot* S = Root->AddChildToCanvas(Bar);
		S->SetAnchors(FAnchors(0.5f, 1.f));
		S->SetAlignment(FVector2D(0.5f, 1.f));
		S->SetAutoSize(true);
		S->SetPosition(FVector2D(0.f, -44.f));
	}
	UBorder* Field = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("ProfileNameField"));
	Field->SetBrush(ProfileSetup::Box(ProfileSetup::Ink, HoverColor, ProfileSetup::Line));
	Field->SetPadding(FMargin(20.f, 2.f, 20.f, 6.f));
	UHorizontalBox* FieldRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	UTextBlock* NameLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ProfileNameLabel"));
	NameLabel->SetText(FText::FromString(TEXT("NAME")));
	NameLabel->SetFont(MakeFont(44));
	NameLabel->SetColorAndOpacity(FSlateColor(AccentColor));
	{
		UHorizontalBoxSlot* S = FieldRow->AddChildToHorizontalBox(NameLabel);
		S->SetVerticalAlignment(VAlign_Center);
		S->SetPadding(FMargin(0.f, 4.f, 20.f, 0.f));
	}
	NameBox = WidgetTree->ConstructWidget<UEditableText>(UEditableText::StaticClass(), TEXT("ProfileName"));
	{
		FEditableTextStyle Style = FCoreStyle::Get().GetWidgetStyle<FEditableTextStyle>("NormalEditableText");
		Style.SetFont(MakeFont(56));
		Style.SetColorAndOpacity(FSlateColor(FLinearColor::White));
		Style.BackgroundImageSelected.TintColor = FSlateColor(AccentColor); // selected text: game magenta instead of Windows blue
		NameBox->SetWidgetStyle(Style);
	}
	NameBox->SetHintText(FText::FromString(TEXT("your name")));
	NameBox->SetJustification(ETextJustify::Left);
	NameBox->SetMinimumDesiredWidth(400.f);
	NameBox->OnTextChanged.AddDynamic(this, &UProfileSetupWidget::OnNameChanged);
	FieldRow->AddChildToHorizontalBox(NameBox)->SetVerticalAlignment(VAlign_Center);
	Field->SetContent(FieldRow);
	FieldRetainer = Wrap(Field);
	{
		UHorizontalBoxSlot* S = Bar->AddChildToHorizontalBox(FieldRetainer);
		S->SetVerticalAlignment(VAlign_Fill);
		S->SetPadding(FMargin(0.f, 0.f, 24.f - ProfileSetup::TearRoom, 0.f));
	}

	// a real button: one solid block of one colour (user choice: no second-colour shadow under it)
	JoinBtn = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("ProfileJoin"));
	JoinBtn->SetStyle(ProfileSetup::ClearButton());
	JoinBtn->SetCursor(EMouseCursor::Hand);
	JoinBtn->OnClicked.AddDynamic(this, &UProfileSetupWidget::OnJoinClicked);
	JoinFace = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("ProfileJoinFace"));
	JoinFace->SetPadding(FMargin(34.f, 2.f, 34.f, 6.f));
	JoinFace->SetVerticalAlignment(VAlign_Center);
	JoinText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ProfileJoinText"));
	JoinText->SetText(FText::FromString(TEXT("JOIN CHAT")));
	JoinText->SetFont(MakeFont(56));
	JoinFace->SetContent(JoinText);
	UOverlay* JoinPad = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
	{
		UOverlaySlot* S = JoinPad->AddChildToOverlay(JoinFace);
		S->SetHorizontalAlignment(HAlign_Fill);
		S->SetVerticalAlignment(VAlign_Fill);
		S->SetPadding(FMargin(ProfileSetup::TearRoom, 0.f, ProfileSetup::TearRoom, 0.f));
	}
	JoinRetainer = Wrap(JoinPad);
	JoinBtn->SetContent(JoinRetainer);
	Bar->AddChildToHorizontalBox(JoinBtn)->SetVerticalAlignment(VAlign_Fill);
	StyleJoin();

	// controls hint: small, so it sits outside the pixelation (it would be unreadable in it)
	Hint = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ProfileHint"));
	Hint->SetFont(MakeFont(20));
	Hint->SetColorAndOpacity(FSlateColor(FLinearColor(1.f, 1.f, 1.f, 0.3f)));
	Hint->SetVisibility(ESlateVisibility::HitTestInvisible);
	{
		UCanvasPanelSlot* S = Root->AddChildToCanvas(Hint);
		S->SetAnchors(FAnchors(0.5f, 1.f));
		S->SetAlignment(FVector2D(0.5f, 1.f));
		S->SetAutoSize(true);
		S->SetPosition(FVector2D(0.f, -12.f));
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
	// the field starts empty ("your name" hint) so the player makes one up. only a name they typed before comes back:
	// a saved name that is just the character's own (= joined with the field empty) counts as none
	const bool bOwnName = Profile && Profile->HasProfile() && Profile->GetPlayerName() != UPlayerProfileSubsystem::CharacterName(Characters[Selected].Character);
	bSettingName = true;
	NameBox->SetText(bOwnName ? FText::FromString(Profile->GetPlayerName()) : FText::GetEmpty());
	bSettingName = false;
	UE_LOG(LogProfileSetup, Log, TEXT("start: saved %s -> character %d of %d"),
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
			Bodies.Add(nullptr);
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
		if (C.Idle) { Mesh->PlayAnimation(C.Idle, true); Mesh->Stop(); } // frozen on the first frame: Step runs the idle of the picked / hovered one
		else { UE_LOG(LogProfileSetup, Warning, TEXT("ProfileSetup: character %d has no Idle"), i); }
		Bodies.Add(Mesh);

		const FVector Cam = Stage + FVector(0.f, C.CameraDistance, C.CameraHeight + 8.f);
		const FRotator Look = (Stage + FVector(0.f, 0.f, C.CameraHeight) - Cam).Rotation();
		ASceneCapture2D* Capture = World->SpawnActor<ASceneCapture2D>(Cam, Look, P);
		check(Capture);
		USceneCaptureComponent2D* CC = Capture->GetCaptureComponent2D();
		UTextureRenderTarget2D* RT = NewObject<UTextureRenderTarget2D>(this);
		RT->RenderTargetFormat = RTF_RGBA8;
		RT->ClearColor = FLinearColor::Black;
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
	for (int32 i = 0; i < Photos.Num(); ++i) { StyleCharacter(i); }
	if (bChanged && PickSound) { UGameplayStatics::PlaySound2D(this, PickSound); }
	FocusName();
}

TArray<URetainerBox*> UProfileSetupWidget::AllRetainers() const
{
	TArray<URetainerBox*> All;
	All.Add(Retainer);
	for (URetainerBox* R : CharRetainers) { All.Add(R); }
	All.Add(FieldRetainer);
	All.Add(JoinRetainer);
	return All;
}

void UProfileSetupWidget::StyleCharacter(int32 i)
{
	const bool bOn = i == Selected;
	const float H = PhotoHover[i];
	// picked = full colour with the typing bubble. the other one stands in the dark until the mouse is on it
	Photos[i]->SetColorAndOpacity(bOn ? FLinearColor::White : FMath::Lerp(FLinearColor(0.22f, 0.22f, 0.25f, 1.f), FLinearColor(0.7f, 0.7f, 0.72f, 1.f), H));
	Arrows[i]->SetVisibility(bOn ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	PhotoBtns[i]->GetContent()->SetRenderScale(FVector2D(1.f + 0.03f * H));
}

void UProfileSetupWidget::StyleJoin()
{
	// lime block with dark text; under the mouse it turns magenta with white text; a shade darker while held down
	FLinearColor Face = bJoinHover ? AccentColor : HoverColor;
	if (bJoinDown) { Face = FLinearColor(Face.R * 0.6f, Face.G * 0.6f, Face.B * 0.6f, 1.f); }
	JoinFace->SetBrush(ProfileSetup::Box(Face, Face, 0.f));
	JoinText->SetColorAndOpacity(FSlateColor(bJoinHover ? FLinearColor::White : ProfileSetup::Ink));
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

	// red and glitch hit together, like the "it's not over" screen
	JoinFace->SetBrush(ProfileSetup::Box(JoinRed, JoinRed, 0.f));
	JoinText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	Hint->SetVisibility(ESlateVisibility::Hidden);
	const float Block = FGlitchFX::BlockPxFor(Retainer, PixelRows);
	const TArray<URetainerBox*> All = AllRetainers();
	Glitches.SetNum(All.Num());
	for (int32 k = 0; k < All.Num(); ++k)
	{
		Glitches[k].Start(All[k], GlitchEffect, k == 0 ? GlitchSound.Get() : nullptr, PixelRows); // one of them makes the sound
		if (Block > 0.f) { Glitches[k].BaseBlock = Block; } // Start sizes blocks from its own retainer's height: wrong for the small ones
	}
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
	if (bDone) { return; }
	FadeOverlay->SetRenderOpacity(1.f - FMath::Clamp(Time / ProfileSetup::FadeInTime, 0.f, 1.f)); // fade-in keeps going under an early join
	if (!bJoining)
	{
		// every tick (follows window resizes): same block size on every piece, and no row glitch except under the mouse
		const float Block = FGlitchFX::BlockPxFor(Retainer, PixelRows);
		auto Tune = [Block](URetainerBox* R, float Amount)
		{
			if (UMaterialInstanceDynamic* M = R ? R->GetEffectMaterial() : nullptr)
			{
				if (Block > 0.f) { M->SetScalarParameterValue(TEXT("BlockPx"), Block); }
				M->SetScalarParameterValue(TEXT("Glitch"), Amount);
			}
		};
		Tune(Retainer, 0.f);
		Tune(FieldRetainer, 0.f);
		Tune(JoinRetainer, JoinBtn->IsHovered() ? ProfileSetup::HoverGlitch * 0.5f : 0.f); // half: the label has to stay readable
		const bool bJoinOver = JoinBtn->IsHovered();
		const bool bDown = JoinBtn->IsPressed();
		if (bJoinOver != bJoinHover || bDown != bJoinDown) { bJoinHover = bJoinOver; bJoinDown = bDown; StyleJoin(); }
		// typing dots: 1, 2, 3 lit, like the chat's "..." bubble
		const int32 Lit = 1 + (FMath::FloorToInt(Time / 0.3f) % 3);
		for (int32 d = 0; d < Dots.Num(); ++d)
		{
			Dots[d]->SetColorAndOpacity(FSlateColor((d % 3) < Lit ? FLinearColor::White : FLinearColor(1.f, 1.f, 1.f, 0.2f)));
		}
		const float Bob = (FMath::FloorToInt(Time * 2.5f) % 2) ? ProfileSetup::ArrowBob : 0.f; // 2 positions, no easing
		for (int32 i = 0; i < PhotoBtns.Num(); ++i)
		{
			const bool bOver = PhotoBtns[i]->IsHovered();
			const float P = FMath::FInterpTo(PhotoHover[i], bOver ? 1.f : 0.f, Dt, 16.f);
			if (P != PhotoHover[i]) { PhotoHover[i] = P; StyleCharacter(i); }
			Tune(CharRetainers[i], bOver ? ProfileSetup::HoverGlitch : 0.f);
			Arrows[i]->SetRenderTranslation(FVector2D(0.f, ArrowDrop[i] + Bob));
			// the picked character idles, and so does the one under the mouse; anyone else is frozen where they stopped
			if (USkeletalMeshComponent* Body = Bodies.IsValidIndex(i) ? Bodies[i].Get() : nullptr)
			{
				const bool bPlay = bOver || i == Selected;
				if (Body->IsPlaying() != bPlay) { if (bPlay) { Body->Play(true); } else { Body->Stop(); } }
			}
		}
		return;
	}
	JoinTime += Dt;
	const TArray<URetainerBox*> All = AllRetainers();
	if (JoinTime >= GlitchTime)
	{
		// hard cut to black; the chat fades in from there
		for (URetainerBox* R : All) { R->SetRenderOpacity(0.f); }
		FadeOverlay->SetRenderOpacity(1.f);
		bDone = true;
		return;
	}
	for (int32 k = 0; k < All.Num() && k < Glitches.Num(); ++k)
	{
		Glitches[k].Frame(All[k], JoinTime / GlitchTime, k == 0 ? GlitchSound.Get() : nullptr);
	}
}
