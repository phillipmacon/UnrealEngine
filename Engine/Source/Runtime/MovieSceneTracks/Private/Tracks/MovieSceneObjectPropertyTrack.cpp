// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "Tracks/MovieSceneObjectPropertyTrack.h"
#include "Sections/MovieSceneObjectPropertySection.h"
#include "Evaluation/MovieSceneObjectPropertyTemplate.h"


UMovieSceneObjectPropertyTrack::UMovieSceneObjectPropertyTrack(const FObjectInitializer& ObjInit)
	: Super(ObjInit)
{
	PropertyClass = nullptr;
}

UMovieSceneSection* UMovieSceneObjectPropertyTrack::CreateNewSection()
{
	UMovieSceneObjectPropertySection* Section = NewObject<UMovieSceneObjectPropertySection>(this);
	Section->ObjectChannel.SetPropertyClass(PropertyClass);
	return Section;
}

FMovieSceneEvalTemplatePtr UMovieSceneObjectPropertyTrack::CreateTemplateForSection(const UMovieSceneSection& InSection) const
{
	return FMovieSceneObjectPropertyTemplate(*CastChecked<UMovieSceneObjectPropertySection>(&InSection), *this);
}
