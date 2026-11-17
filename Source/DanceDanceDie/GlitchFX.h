// Glitch burst for UI that sits in a URetainerBox with /Game/PostProcess/M_UI_PS1 as its effect material.
// Used by the credits glitch-out and the DemoEnd "it's not over" glitch-out. Call Start once, then Frame every tick.

#pragma once

#include "CoreMinimal.h"
#include "Components/RetainerBox.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Sound/SoundBase.h"

struct FGlitchFX
{
	// M_UI_PS1's BlockPx for this retainer so the screen is always ~Rows blocks tall (270 = 4 px blocks at 1080p).
	// A fixed BlockPx made text unreadable on small screens: letters shrink, blocks don't. 0 if not laid out yet.
	static float BlockPxFor(const URetainerBox* R, float Rows)
	{
		const float H = R ? R->GetCachedGeometry().GetAbsoluteSize().Y : 0.f; // real pixels
		return (H > 0.f && Rows > 0.f) ? FMath::Max(H / Rows, 1.f) : 0.f;
	}

	// sets BlockPx from the screen size; call every tick (cheap) so window resizes are picked up
	static void FitBlocks(URetainerBox* R, float Rows)
	{
		const float B = BlockPxFor(R, Rows);
		UMaterialInstanceDynamic* M = R ? R->GetEffectMaterial() : nullptr;
		if (M && B > 0.f) { M->SetScalarParameterValue(TEXT("BlockPx"), B); }
	}

	float BaseBlock = 4.f;
	float BaseSplit = 0.6f;
	int32 SoundsPlayed = 0;

	// Effect: M_UI_PS1 (or an MI of it). Puts it on the retainer if it isn't there yet.
	void Start(URetainerBox* R, UMaterialInterface* Effect, USoundBase* Sound, float Rows = 270.f)
	{
		check(R);
		if (Effect && R->GetEffectMaterial() == nullptr) { R->SetEffectMaterial(Effect); R->SetTextureParameter(TEXT("Texture")); }
		UMaterialInstanceDynamic* M = R->GetEffectMaterial();
		if (!M)
		{
			UE_LOG(LogTemp, Error, TEXT("GlitchFX: retainer %s has no effect material, glitch will be jitter only"), *R->GetName());
		}
		else
		{
			const float Fit = BlockPxFor(R, Rows);
			BaseBlock = Fit > 0.f ? Fit : FMath::Max(M->K2_GetScalarParameterValue(TEXT("BlockPx")), 1.f);
			BaseSplit = M->K2_GetScalarParameterValue(TEXT("Split"));
		}
		SoundsPlayed = 0;
		Play(R, Sound);
	}

	// Alpha 0..1 = progress through the burst. Gets wilder towards the end; sound re-fires at 40% and 75%.
	void Frame(URetainerBox* R, float Alpha, USoundBase* Sound)
	{
		check(R);
		const float A = FMath::Clamp(0.35f + 0.65f * Alpha, 0.f, 1.f);
		if (UMaterialInstanceDynamic* M = R->GetEffectMaterial())
		{
			M->SetScalarParameterValue(TEXT("Glitch"), 25.f * A);                                  // most rows slide sideways
			M->SetScalarParameterValue(TEXT("Split"), BaseSplit + FMath::FRandRange(0.f, 5.f) * A); // RGB tears apart
			M->SetScalarParameterValue(TEXT("BlockPx"), BaseBlock * (1.f + FMath::RandRange(0, 2) * A));
			M->SetScalarParameterValue(TEXT("Dither"), 1.f + 2.f * A);
		}
		R->SetRenderTranslation(FVector2D(FMath::FRandRange(-16.f, 16.f), FMath::FRandRange(-5.f, 5.f)) * A);
		R->SetRenderOpacity(FMath::FRand() < 0.18f * A ? 0.f : 1.f); // dropped frames
		if ((SoundsPlayed == 1 && Alpha >= 0.4f) || (SoundsPlayed == 2 && Alpha >= 0.75f)) { Play(R, Sound); }
	}

private:
	void Play(URetainerBox* R, USoundBase* Sound)
	{
		++SoundsPlayed;
		if (Sound) { UGameplayStatics::PlaySound2D(R, Sound); }
	}
};
