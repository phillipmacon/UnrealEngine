// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Misc/MemStack.h"
//#include "Animation/AnimationAsset.h"
#include "Animation/AnimLinkableElement.h"
#include "AnimTypes.generated.h"

struct FMarkerPair;
struct FMarkerSyncAnimPosition;
struct FPassedMarker;

// Disable debugging information for shipping and test builds.
#define ENABLE_ANIM_DEBUG (1 && !(UE_BUILD_SHIPPING || UE_BUILD_TEST))

#define DEFAULT_SAMPLERATE			30.f
#define MINIMUM_ANIMATION_LENGTH	(1/DEFAULT_SAMPLERATE)

namespace EAnimEventTriggerOffsets
{
	enum Type
	{
		OffsetBefore,
		OffsetAfter,
		NoOffset
	};
}

ENGINE_API float GetTriggerTimeOffsetForType(EAnimEventTriggerOffsets::Type OffsetType);

/** Enum for specifying a specific axis of a bone */
UENUM()
enum EBoneAxis
{
	BA_X UMETA(DisplayName = "X Axis"),
	BA_Y UMETA(DisplayName = "Y Axis"),
	BA_Z UMETA(DisplayName = "Z Axis"),
};


/** Enum for controlling which reference frame a controller is applied in. */
UENUM()
enum EBoneControlSpace
{
	/** Set absolute position of bone in world space. */
	BCS_WorldSpace UMETA(DisplayName = "World Space"),
	/** Set position of bone in SkeletalMeshComponent's reference frame. */
	BCS_ComponentSpace UMETA(DisplayName = "Component Space"),
	/** Set position of bone relative to parent bone. */
	BCS_ParentBoneSpace UMETA(DisplayName = "Parent Bone Space"),
	/** Set position of bone in its own reference frame. */
	BCS_BoneSpace UMETA(DisplayName = "Bone Space"),
	BCS_MAX,
};

/** Enum for specifying the source of a bone's rotation. */
UENUM()
enum EBoneRotationSource
{
	/** Don't change rotation at all. */
	BRS_KeepComponentSpaceRotation UMETA(DisplayName = "No Change (Preserve Existing Component Space Rotation)"),
	/** Keep forward direction vector relative to the parent bone. */
	BRS_KeepLocalSpaceRotation UMETA(DisplayName = "Maintain Local Rotation Relative to Parent"),
	/** Copy rotation of target to bone. */
	BRS_CopyFromTarget UMETA(DisplayName = "Copy Target Rotation"),
};

/** Ticking method for AnimNotifies in AnimMontages. */
UENUM()
namespace EMontageNotifyTickType
{
	enum Type
	{
		/** Queue notifies, and trigger them at the end of the evaluation phase (faster). Not suitable for changing sections or montage position. */
		Queued,
		/** Trigger notifies as they are encountered (Slower). Suitable for changing sections or montage position. */
		BranchingPoint,
	};
}

/** Filtering method for deciding whether to trigger a notify. */
UENUM()
namespace ENotifyFilterType
{
	enum Type
	{
		/** No filtering. */
		NoFiltering,

		/** Filter By Skeletal Mesh LOD. */
		LOD,
	};
}

USTRUCT(BlueprintType)
struct FPerBoneBlendWeight
{
	GENERATED_USTRUCT_BODY()

	/** Source index of the buffer. */
	UPROPERTY()
	int32 SourceIndex;

	UPROPERTY()
	float BlendWeight;

	FPerBoneBlendWeight()
		: SourceIndex(0)
		, BlendWeight(0.0f)
	{
	}
};

USTRUCT(BlueprintType)
struct FPerBoneBlendWeights
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	TArray<FPerBoneBlendWeight> BoneBlendWeights;


	FPerBoneBlendWeights() {}

};

struct FGraphTraversalCounter
{
private:
	// Number of the last external frame where we updated the counter (total global frame count)
	uint64 LastSyncronizedFrame;

	// Number of counter increments since last reset
	int16 Counter;

	// Number of external frames that URO is currently scheduled to skip
	int16 SkipFrames;

public:

	FGraphTraversalCounter()
		: LastSyncronizedFrame(INDEX_NONE)
		, Counter(INDEX_NONE)
		, SkipFrames(0)

	{}

	int16 Get() const
	{
		return Counter;
	}

	bool HasEverBeenUpdated() const
	{
		return (Counter != INDEX_NONE) && (LastSyncronizedFrame != INDEX_NONE);
	}

	/** Increases the internal counter, and refreshes the current global frame counter */
	void Increment()
	{
		Counter++;
		LastSyncronizedFrame = GFrameCounter;

		// Avoid wrapping over back to INDEX_NONE, as this means 'never been traversed'
		if (Counter == INDEX_NONE)
		{
			Counter++;
		}
	}

	/** Sets the number of expected frames to be skipped by URO */
	void SetMaxSkippedFrames(int16 InMaxSkippedFrames)
	{
		SkipFrames = InMaxSkippedFrames;
	}

	/** Clear the internal counters and frame skip */
	void Reset()
	{
		Counter = INDEX_NONE;
		LastSyncronizedFrame = INDEX_NONE;
		SkipFrames = 0;
	}

	/** Sync this counter with another counter */
	void SynchronizeWith(const FGraphTraversalCounter& InMasterCounter)
	{
		Counter = InMasterCounter.Counter;
		LastSyncronizedFrame = InMasterCounter.LastSyncronizedFrame;
		SkipFrames = InMasterCounter.SkipFrames;
	}

	UE_DEPRECATED(4.19, "Use the specific IsSynchronized_* functions to determing sync state")
	bool IsSynchronizedWith(const FGraphTraversalCounter& InMasterCounter) const
	{
		return IsSynchronized_Counter(InMasterCounter);
	}

	UE_DEPRECATED(4.19, "Use the specific WasSynchronizedLast* functions to determine sync state")
	bool WasSynchronizedInTheLastFrame(const FGraphTraversalCounter& InMasterCounter) const
	{
		return WasSynchronizedCounter(InMasterCounter);
	}

	/** Check whether the internal counter is synchronized between this and another counter */
	bool IsSynchronized_Counter(const FGraphTraversalCounter& InOtherCounter) const
	{
		return HasEverBeenUpdated() && Counter == InOtherCounter.Counter;
	}

	/** Check whether this counter and another were synchronized on the same global frame */
	bool IsSynchronized_Frame(const FGraphTraversalCounter& InOtherCounter) const
	{
		return HasEverBeenUpdated() && LastSyncronizedFrame == InOtherCounter.LastSyncronizedFrame;
	}

	/** Check that both the internal counter and global frame are both in sync between this counter and another */
	bool IsSynchronized_All(const FGraphTraversalCounter& InOtherCounter) const
	{
		return HasEverBeenUpdated() && Counter == InOtherCounter.Counter && LastSyncronizedFrame == InOtherCounter.LastSyncronizedFrame;
	}

	/** Check if this counter is either synchronized with another or is one update behind */
	bool WasSynchronizedCounter(const FGraphTraversalCounter& InOtherCounter) const
	{
		// Test if we're currently in sync with our master counter
		if(IsSynchronized_Counter(InOtherCounter))
		{
			return true;
		}

		// If not, test if the Master Counter is a frame ahead of us
		FGraphTraversalCounter TestCounter(*this);
		TestCounter.Increment();

		return TestCounter.IsSynchronized_Counter(InOtherCounter);
	}

	/** 
	 * Check whether this counter and another were either synchronized this global frame or were
	 * synced one frame interval previously. A frame interval is the size of SkipFrames. This means
	 * that if under URO a number of frames are skipped, this check will consider counters from the
	 * last frame before the skip and the first frame after the skip as "was synchronized"
	 *
	 * Use this to test whether or not there's been a period when the owning animation instance has
	 * not been updated.
	 */
	bool WasSynchronizedLastFrame(const FGraphTraversalCounter& InOtherCounter) const
	{
		// Test if we're currently in sync with our master counter
		if(IsSynchronized_Frame(InOtherCounter))
		{
			return true;
		}

		// If we aren't synced then check if we were synced last frame
		return LastSyncronizedFrame >= (GFrameCounter - SkipFrames) - 1;
	}
};

/**
 * Triggers an animation notify.  Each AnimNotifyEvent contains an AnimNotify object
 * which has its Notify method called and passed to the animation.
 */
USTRUCT(BlueprintType)
struct FAnimNotifyEvent : public FAnimLinkableElement
{
	GENERATED_USTRUCT_BODY()

	/** The user requested time for this notify */
	UPROPERTY()
	float DisplayTime_DEPRECATED;

	/** An offset from the DisplayTime to the actual time we will trigger the notify, as we cannot always trigger it exactly at the time the user wants */
	UPROPERTY()
	float TriggerTimeOffset;

	/** An offset similar to TriggerTimeOffset but used for the end scrub handle of a notify state's duration */
	UPROPERTY()
	float EndTriggerTimeOffset;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=AnimNotifyEvent)
	float TriggerWeightThreshold;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=AnimNotifyEvent)
	FName NotifyName;

	UPROPERTY(EditAnywhere, Instanced, BlueprintReadWrite, Category=AnimNotifyEvent)
	class UAnimNotify * Notify;

	UPROPERTY(EditAnywhere, Instanced, BlueprintReadWrite, Category=AnimNotifyEvent)
	class UAnimNotifyState * NotifyStateClass;

	UPROPERTY()
	float Duration;

	/** Linkable element to use for the end handle representing a notify state duration */
	UPROPERTY()
	FAnimLinkableElement EndLink;

	/** If TRUE, this notify has been converted from an old BranchingPoint. */
	UPROPERTY()
	bool bConvertedFromBranchingPoint;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=AnimNotifyEvent)
	TEnumAsByte<EMontageNotifyTickType::Type> MontageTickType;

	/** Defines the chance of of this notify triggering, 0 = No Chance, 1 = Always triggers */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AnimNotifyTriggerSettings, meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float NotifyTriggerChance;

	/** Defines a method for filtering notifies (stopping them triggering) e.g. by looking at the meshes current LOD */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AnimNotifyTriggerSettings)
	TEnumAsByte<ENotifyFilterType::Type> NotifyFilterType;

	/** LOD to start filtering this notify from.*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AnimNotifyTriggerSettings, meta = (ClampMin = "0"))
	int32 NotifyFilterLOD;

	/** If disabled this notify will be skipped on dedicated servers */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AnimNotifyTriggerSettings)
	bool bTriggerOnDedicatedServer;

	/** If enabled this notify will trigger when the animation is a follower in a sync group (by default only the sync group leaders notifies trigger */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AnimNotifyTriggerSettings)
	bool bTriggerOnFollower;

#if WITH_EDITORONLY_DATA
	/** Color of Notify in editor */
	UPROPERTY()
	FColor NotifyColor;
#endif // WITH_EDITORONLY_DATA

	/** 'Track' that the notify exists on, used for visual placement in editor and sorting priority in runtime */
	UPROPERTY()
	int32 TrackIndex;

private:
	/** NotifyName appended to "AnimNotify_" */
	mutable FName CachedNotifyEventName;

	/** NotifyName used to generate CachedNotifyEventName, for invalidation */
	mutable FName CachedNotifyEventBaseName;

public:
	FAnimNotifyEvent()
		: FAnimLinkableElement()
		, DisplayTime_DEPRECATED(0)
		, TriggerTimeOffset(0)
		, EndTriggerTimeOffset(0)
		, TriggerWeightThreshold(ZERO_ANIMWEIGHT_THRESH)
		, Notify(NULL)
		, NotifyStateClass(NULL)
		, Duration(0)
		, bConvertedFromBranchingPoint(false)
		, MontageTickType(EMontageNotifyTickType::Queued)
		, NotifyTriggerChance(1.f)
		, NotifyFilterType(ENotifyFilterType::NoFiltering)
		, NotifyFilterLOD(0)
		, bTriggerOnDedicatedServer(true)
		, bTriggerOnFollower(false)
#if WITH_EDITORONLY_DATA
		, NotifyColor(FColor::Black)
#endif // WITH_EDITORONLY_DATA
		, TrackIndex(0)
		, CachedNotifyEventName(NAME_None)
		, CachedNotifyEventBaseName(NAME_None)
	{
	}

	virtual ~FAnimNotifyEvent()
	{
	}

	/** Updates trigger offset based on a combination of predicted offset and current offset */
	ENGINE_API void RefreshTriggerOffset(EAnimEventTriggerOffsets::Type PredictedOffsetType);

	/** Updates end trigger offset based on a combination of predicted offset and current offset */
	ENGINE_API void RefreshEndTriggerOffset(EAnimEventTriggerOffsets::Type PredictedOffsetType);

	/** Returns the actual trigger time for this notify. In some cases this may be different to the DisplayTime that has been set */
	ENGINE_API float GetTriggerTime() const;

	/** Returns the actual end time for a state notify. In some cases this may be different from DisplayTime + Duration */
	ENGINE_API float GetEndTriggerTime() const;

	ENGINE_API float GetDuration() const;

	ENGINE_API void SetDuration(float NewDuration);

	/** Returns true is this AnimNotify is a BranchingPoint */
	ENGINE_API bool IsBranchingPoint() const;

	/** Returns true if this is blueprint derived notifies **/
	bool IsBlueprintNotify() const
	{
		return Notify != NULL || NotifyStateClass != NULL;
	}

	bool operator ==(const FAnimNotifyEvent& Other) const
	{
		return(
			(Notify && Notify == Other.Notify) || 
			(NotifyStateClass && NotifyStateClass == Other.NotifyStateClass) ||
			(!IsBlueprintNotify() && NotifyName == Other.NotifyName)
			);
	}

	/** This can be used with the Sort() function on a TArray of FAnimNotifyEvents to sort the notifies array by time, earliest first. */
	ENGINE_API bool operator <(const FAnimNotifyEvent& Other) const;

	ENGINE_API virtual void SetTime(float NewTime, EAnimLinkMethod::Type ReferenceFrame = EAnimLinkMethod::Absolute) override;

	/** Get the NotifyName pre-appended to "AnimNotify_", for calling the event */
	ENGINE_API FName GetNotifyEventName() const;
};

// Used by UAnimSequenceBase::SortNotifies() to sort its Notifies array
FORCEINLINE bool FAnimNotifyEvent::operator<(const FAnimNotifyEvent& Other) const
{
	float ATime = GetTriggerTime();
	float BTime = Other.GetTriggerTime();

	// When we've got the same time sort on the track index. Explicitly
	// using SMALL_NUMBER here incase the underlying default changes as
	// notifies can have an offset of KINDA_SMALL_NUMBER to be consider
	// distinct
	if (FMath::IsNearlyEqual(ATime, BTime, SMALL_NUMBER))
	{
		return TrackIndex < Other.TrackIndex;
	}
	else
	{
		return ATime < BTime;
	}
}

USTRUCT(BlueprintType)
struct FAnimSyncMarker
{
	GENERATED_USTRUCT_BODY()

	// The name of this marker
	UPROPERTY(BlueprintReadOnly, Category=Animation)
	FName MarkerName;

	// Time in seconds of this marker
	UPROPERTY(BlueprintReadOnly, Category = Animation)
	float Time;

#if WITH_EDITORONLY_DATA
	// The editor track this marker sits on
	UPROPERTY()
	int32 TrackIndex;
#endif

	/** This can be used with the Sort() function on a TArray of FAnimSyncMarker to sort the notifies array by time, earliest first. */
	ENGINE_API bool operator <(const FAnimSyncMarker& Other) const { return Time < Other.Time; }
};

/**
 * Keyframe position data for one track.  Pos(i) occurs at Time(i).  Pos.Num() always equals Time.Num().
 */
USTRUCT()
struct FAnimNotifyTrack
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	FName TrackName;

	UPROPERTY()
	FLinearColor TrackColor;

	TArray<FAnimNotifyEvent*> Notifies;
	
	TArray<FAnimSyncMarker*> SyncMarkers;

	FAnimNotifyTrack()
		: TrackName(TEXT(""))
		, TrackColor(FLinearColor::White)
	{
	}

	FAnimNotifyTrack(FName InTrackName, FLinearColor InTrackColor)
		: TrackName(InTrackName)
		, TrackColor(InTrackColor)
	{
	}
};

/** 
 * Indicates whether an animation is additive, and what kind.
 */
UENUM()
enum EAdditiveAnimationType
{
	/** No additive. */
	AAT_None  UMETA(DisplayName="No additive"),
	/* Create Additive from LocalSpace Base. */
	AAT_LocalSpaceBase UMETA(DisplayName="Local Space"),
	/* Create Additive from MeshSpace Rotation Only, Translation still will be LocalSpace. */
	AAT_RotationOffsetMeshSpace UMETA(DisplayName="Mesh Space"),
	AAT_MAX,
};

UENUM()
namespace ECurveBlendOption
{
	enum Type
	{
		/** Find Max Weight of curve and use that weight. */
		MaxWeight,
		/** Normalize By Sum of Weight and use it to blend. */
		NormalizeByWeight,
		/** Blend By Weight without normalizing*/
		BlendByWeight
	};
}

/**
 * Slot node weight information - this is transient data that is used by slot node
 */
struct FSlotNodeWeightInfo
{
	/** Weight of Source Branch. This is the weight of the input pose coming from children.
	This is different than (1.f - SourceWeight) since the Slot can play additive animations, which are overlayed on top of the source pose. */
	float SourceWeight;

	/** Weight of Slot Node. Determined by Montages weight playing on this slot */
	float SlotNodeWeight;

	/** Total Weight of Slot Node. Determined by Montages weight playing on this slot, it can be more than 1 */
	float TotalNodeWeight;

	FSlotNodeWeightInfo()
		: SourceWeight(0.f)
		, SlotNodeWeight(0.f)
		, TotalNodeWeight(0.f)
	{}

	void Reset()
	{
		SourceWeight = 0.f;
		SlotNodeWeight = 0.f;
		TotalNodeWeight = 0.f;
	}
};

USTRUCT()
struct FMarkerSyncData
{
	GENERATED_USTRUCT_BODY()

	/** Authored Sync markers */
	UPROPERTY()
	TArray<FAnimSyncMarker>		AuthoredSyncMarkers;

	/** List of Unique marker names in this animation sequence */
	TArray<FName>				UniqueMarkerNames;

	void GetMarkerIndicesForTime(float CurrentTime, bool bLooping, const TArray<FName>& ValidMarkerNames, FMarkerPair& OutPrevMarker, FMarkerPair& OutNextMarker, float SequenceLength) const;
	FMarkerSyncAnimPosition GetMarkerSyncPositionfromMarkerIndicies(int32 PrevMarker, int32 NextMarker, float CurrentTime, float SequenceLength) const;
	void CollectUniqueNames();
	void CollectMarkersInRange(float PrevPosition, float NewPosition, TArray<FPassedMarker>& OutMarkersPassedThisTick, float TotalDeltaMove);
};

// Shortcut for the allocator used by animation nodes.
typedef TMemStackAllocator<> FAnimStackAllocator;

/** 
 * Structure for all Animation Weight helper functions.
 */
struct FAnimWeight
{
	/** Helper function to determine if a weight is relevant. */
	static FORCEINLINE bool IsRelevant(float InWeight)
	{
		return (InWeight > ZERO_ANIMWEIGHT_THRESH);
	}

	/** Helper function to determine if a normalized weight is considered full weight. */
	static FORCEINLINE bool IsFullWeight(float InWeight)
	{
		return (InWeight >= (1.f - ZERO_ANIMWEIGHT_THRESH));
	}

	/** Get a small relevant weight for ticking */
	static FORCEINLINE float GetSmallestRelevantWeight()
	{
		return 2.f * ZERO_ANIMWEIGHT_THRESH;
	}
};

/** 
 * Indicates how animation should be evaluated between keys.
 */
UENUM(BlueprintType)
enum class EAnimInterpolationType : uint8
{
	/** Linear interpolation when looking up values between keys. */
	Linear		UMETA(DisplayName="Linear"),

	/** Step interpolation when looking up values between keys. */
	Step		UMETA(DisplayName="Step"),
};


/*
 *  Smart name UID type definition
 * 
 * This is used by FSmartNameMapping/FSmartName to identify by number
 * This defines default type and Max number for the type and used all over animation code to identify curve
 * including bone container containing valid UID list for curves
 */
 namespace SmartName
{
	// ID type, should be used to access SmartNames as fundamental type may change.
	typedef uint16 UID_Type;
	// Max UID used for overflow checking
	static const UID_Type MaxUID = MAX_uint16;
}
/**
 * Animation Key extraction helper as we have a lot of code that messes up the key length
 */
struct FAnimKeyHelper
{
	FAnimKeyHelper(float InLength, int32 InNumKeys)
		: Length(InLength)
		, NumKeys(InNumKeys)
	{}

	float TimePerKey() const
	{
		return (NumKeys > 1) ? Length / (float)(NumKeys - 1) : MINIMUM_ANIMATION_LENGTH;
	}

	// Returns the FPS for the given length and number of keys
	float KeysPerSecond() const
	{
		return NumKeys > 0 ? (float(NumKeys - 1) / Length) : 0.0f;
	}

	int32 LastKey() const
	{
		return (NumKeys > 1) ? NumKeys - 1 : 0;
	}

	float GetLength() const 
	{
		return Length;
	}

	int32 GetNumKeys() const 
	{
		return NumKeys;
	}

private:
	float Length;
	int32 NumKeys;
};

UENUM()
namespace EAxisOption
{
	enum Type
	{
		X,
		Y,
		Z,
		X_Neg,
		Y_Neg,
		Z_Neg,
		Custom
	};
}

struct FAxisOption
{
	static FVector GetAxisVector(const TEnumAsByte<EAxisOption::Type> InAxis, const FVector& CustomAxis)
	{
		switch (InAxis)
		{
		case EAxisOption::X:
			return FVector::ForwardVector;
		case EAxisOption::X_Neg:
			return -FVector::ForwardVector;
		case EAxisOption::Y:
			return FVector::RightVector;
		case EAxisOption::Y_Neg:
			return -FVector::RightVector;
		case EAxisOption::Z:
			return FVector::UpVector;
		case EAxisOption::Z_Neg:
			return -FVector::UpVector;
		case EAxisOption::Custom:
			return CustomAxis;
		}

		return FVector::ForwardVector;
	}

	static FVector GetAxisVector(const TEnumAsByte<EAxisOption::Type> InAxis)
	{
		return GetAxisVector(InAxis, FVector::ForwardVector);
	}
};

// The transform component (attribute) to read from
UENUM()
namespace EComponentType
{
	enum Type
	{
		None = 0,
		TranslationX,
		TranslationY,
		TranslationZ,
		RotationX,
		RotationY,
		RotationZ,
		Scale UMETA(DisplayName = "Scale (largest component)"),
		ScaleX,
		ScaleY,
		ScaleZ
	};
}
