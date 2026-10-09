#include "RhythmLane.h"
#include "Components/SphereComponent.h"
#include "Components/SplineComponent.h"

ARhythmLane::ARhythmLane()
{
	PrimaryActorTick.bCanEverTick = false; // no tick needed, everything's input driven
}

bool ARhythmLane::Judge(ELane Lane)
{
	const int32 Index = static_cast<int32>(Lane);

	// yell if the bp setup is missing stuff
	if (!ensureMsgf(HitSpheres.IsValidIndex(Index), TEXT("hitspheres not filled in beginplay")) ||
	    !ensureMsgf(NoteClass, TEXT("noteclass not set in class defaults")))
	{
		return false;
	}

	// grab any notes sitting in this lane's hit zone
	TArray<AActor*> Notes;
	HitSpheres[Index]->GetOverlappingActors(Notes, NoteClass);

	if (Notes.Num() > 0)
	{
		Notes[0]->Destroy(); // kill it so it can't get hit twice
		OnHit.Broadcast(Lane);
		return true;
	}

	// pressed but nothing was there
	RegisterMiss(Lane);
	return false;
}

void ARhythmLane::RegisterMiss(ELane Lane)
{
	++MissCount;
	OnMiss.Broadcast(Lane, MissCount);

	// == not >= so the jumpscare only fires once
	if (MissCount == MissThreshold)
	{
		OnJumpscare.Broadcast();
	}
}

void ARhythmLane::ResetSong()
{
	MissCount = 0; // fresh start after a restart
}