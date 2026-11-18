// song title card + progress bar. all c++, no blueprint wiring.
//
// ASongHUD (one in DemoMap, placed by Scripts/setup_song_hud.py) watches BP_Spline:
//   1. rhythm part starts (DemoMap trigger -> BP_Spline StartSong -> `CueAudio` plays) -> SongHUD calls BP_Spline StopSong
//      right away (no notes yet), shows the big centred title for IntroHold secs, shrinks it into the corner over MoveTime,
//      then calls StartSong again = the rhythm game starts after the title.
//   2. BP_Spline `SongTimer` running (= the rhythm part, e.g. 45 s; ends with SongComplete) -> time ("0:12 / 0:45", under the
//      title) + bar fade in and follow that timer. SongLength > 0 overrides: count from the music start instead.
//   3. fail (jumpscare -> BP clears SongTimer, restarts) -> title + time + bar stay in the corner, reset to 0. no second intro.
//      song done (stopped after 90%) -> everything fades out.
// test without the song: PIE in DemoMap, console `ddd.SongHUD [secs]` (intro + fake song, no BP calls).

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameFramework/Actor.h"
#include "SongHUD.generated.h"

class UAudioComponent;
class UCanvasPanelSlot;
class UFont;
class UHorizontalBox;
class UProgressBar;
class UTextBlock;
class UVerticalBox;

UCLASS()
class DANCEDANCEDIE_API USongHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	struct FParams
	{
		FString Title;
		UFont* Font = nullptr;
		float BigSize = 110.f, SmallSize = 44.f, TimeSize = 28.f;
		FLinearColor TextColor, ShadowColor, BarFill, BarBack;
		FVector2D ShadowOffset, CornerAnchor, CornerOffset, BarSize;
		bool bShowTime = true;
	};

	void Setup(const FParams& P);
	void SetLayout(float T);      // 0 = big + centred, 1 = small in the corner
	void SetBarAlpha(float A);    // 0 = bar row collapsed
	void SetProgress(float Elapsed, float Duration);

protected:
	virtual void NativeOnInitialized() override;

private:
	FParams Params;
	UPROPERTY() TObjectPtr<UVerticalBox> Box;
	UPROPERTY() TObjectPtr<UCanvasPanelSlot> BoxSlot;
	UPROPERTY() TObjectPtr<UTextBlock> Title;
	UPROPERTY() TObjectPtr<UHorizontalBox> BarRow;
	UPROPERTY() TObjectPtr<UProgressBar> Bar;
	UPROPERTY() TObjectPtr<UTextBlock> Time;
};

UCLASS()
class DANCEDANCEDIE_API ASongHUD : public AActor
{
	GENERATED_BODY()

public:
	ASongHUD();

	UPROPERTY(EditAnywhere, Category = "Song HUD")
	FString SongTitle = TEXT("RiVER - Another");

	// BP_Spline variable holding the playing music (set in StartSong). drives the bar
	UPROPERTY(EditAnywhere, Category = "Song HUD|BP_Spline")
	FName MusicProperty = TEXT("MusicAudio");

	// BP_Spline variable holding the note cue MetaSound. it playing = the rhythm part started
	UPROPERTY(EditAnywhere, Category = "Song HUD|BP_Spline")
	FName CueProperty = TEXT("CueAudio");

	// hold the rhythm game for the intro: StopSong when it starts, StartSong once the title is in the corner
	UPROPERTY(EditAnywhere, Category = "Song HUD|BP_Spline")
	bool bHoldSongForIntro = true;

	// BP_Spline FTimerHandle that ends the rhythm part (SongComplete). the time + bar show its length
	UPROPERTY(EditAnywhere, Category = "Song HUD|BP_Spline")
	FName SongTimerProperty = TEXT("SongTimer");

	// secs. 0 = use SongTimer's length. > 0 = count this many secs from the music start instead
	UPROPERTY(EditAnywhere, Category = "Song HUD|BP_Spline", meta = (ClampMin = 0))
	float SongLength = 0.f;

	UPROPERTY(EditAnywhere, Category = "Song HUD|BP_Spline", meta = (EditCondition = "bHoldSongForIntro"))
	FName StopSongEvent = TEXT("StopSong");

	UPROPERTY(EditAnywhere, Category = "Song HUD|BP_Spline", meta = (EditCondition = "bHoldSongForIntro"))
	FName StartSongEvent = TEXT("StartSong");

	UPROPERTY(EditAnywhere, Category = "Song HUD|Look")
	TObjectPtr<UFont> Font;

	UPROPERTY(EditAnywhere, Category = "Song HUD|Look", meta = (ClampMin = 1))
	float BigSize = 110.f;

	UPROPERTY(EditAnywhere, Category = "Song HUD|Look", meta = (ClampMin = 1))
	float SmallSize = 56.f;

	UPROPERTY(EditAnywhere, Category = "Song HUD|Look", meta = (ClampMin = 1))
	float TimeSize = 32.f;

	// same green / purple as PLAY/QUIT + credits
	UPROPERTY(EditAnywhere, Category = "Song HUD|Look")
	FLinearColor TextColor = FLinearColor(0.25f, 0.9f, 0.08f);

	UPROPERTY(EditAnywhere, Category = "Song HUD|Look")
	FLinearColor ShadowColor = FLinearColor(0.45f, 0.02f, 0.42f);

	UPROPERTY(EditAnywhere, Category = "Song HUD|Look")
	FVector2D ShadowOffset = FVector2D(4.f, 4.f);

	UPROPERTY(EditAnywhere, Category = "Song HUD|Look")
	FLinearColor BarFill = FLinearColor(0.25f, 0.9f, 0.08f);

	UPROPERTY(EditAnywhere, Category = "Song HUD|Look")
	FLinearColor BarBack = FLinearColor(0.f, 0.f, 0.f, 0.6f);

	// px
	UPROPERTY(EditAnywhere, Category = "Song HUD|Look")
	FVector2D BarSize = FVector2D(420.f, 12.f);

	// "0:12 / 0:45" under the title
	UPROPERTY(EditAnywhere, Category = "Song HUD|Look")
	bool bShowTime = true;

	// where the small title ends up: (0,0) = top-left, (1,0) = top-right, ...
	UPROPERTY(EditAnywhere, Category = "Song HUD|Look")
	FVector2D CornerAnchor = FVector2D(0.f, 0.f);

	// px from CornerAnchor (use -x for a right corner)
	UPROPERTY(EditAnywhere, Category = "Song HUD|Look")
	FVector2D CornerOffset = FVector2D(48.f, 40.f);

	// secs the big title fades in
	UPROPERTY(EditAnywhere, Category = "Song HUD|Timing", meta = (ClampMin = 0))
	float FadeInTime = 0.5f;

	// secs the big title stays centred
	UPROPERTY(EditAnywhere, Category = "Song HUD|Timing", meta = (ClampMin = 0))
	float IntroHold = 1.5f;

	// secs to shrink into the corner
	UPROPERTY(EditAnywhere, Category = "Song HUD|Timing", meta = (ClampMin = 0.01))
	float MoveTime = 1.2f;

	// secs for the bar to fade in when the music starts
	UPROPERTY(EditAnywhere, Category = "Song HUD|Timing", meta = (ClampMin = 0.01))
	float BarFadeTime = 0.4f;

	// secs to fade everything out when the song is done
	UPROPERTY(EditAnywhere, Category = "Song HUD|Timing", meta = (ClampMin = 0.01))
	float OutroFade = 1.f;

	virtual void Tick(float Dt) override;

	// ddd.SongHUD: intro + bar with a fake song, no BP calls
	void FakeStart(float Secs);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Why) override;

private:
	enum class EPhase : uint8
	{
		Waiting, // hidden, waiting for the rhythm part
		Intro,   // big title -> corner. song stopped (bHoldSongForIntro)
		Lead,    // title in the corner, waiting for SongTimer (or the music if SongLength > 0); also after a fail
		Playing, // time + bar running
		Outro,   // song done, fading out
		Done,
	};

	void Fail(const FString& Why);
	UAudioComponent* GetComp(AActor* Src, FName Prop) const;
	AActor* FindStarted() const; // source whose cue or music is playing
	const FTimerHandle* GetSongTimer() const; // null if SongLength > 0
	bool GetTimerProgress(float& OutElapsed, float& OutDuration) const; // false = SongTimer not running
	void CallEvent(AActor* Src, FName Event);
	void BeginIntro(AActor* Src);

	UPROPERTY() TArray<TObjectPtr<AActor>> Sources; // actors with MusicProperty (BP_Spline)
	UPROPERTY() TObjectPtr<USongHUDWidget> Widget;
	TWeakObjectPtr<AActor> Source;
	EPhase Phase = EPhase::Waiting;
	float Clock = 0.f;     // secs in the current phase
	float Duration = 0.f;  // fake / SongLength mode
	float OutroFrom = 1.f; // opacity when the outro began
	bool bFake = false;
	bool bBarShown = false;  // bar faded in once -> stays up through fails/restarts
	float LastElapsed = 0.f; // SongTimer elapsed last frame (a fail clears the timer, so 'timer gone' != 'song done')
	bool bFailed = false;
};
