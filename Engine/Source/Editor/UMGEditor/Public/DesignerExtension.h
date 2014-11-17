// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "WidgetReference.h"

class UWidget;
class SWidget;
class UWidgetBlueprint;

/**
 * The location where the widget should appear
 */
namespace EExtensionLayoutLocation
{
	enum Type
	{
		Absolute,

		TopLeft,
		TopCenter,
		TopRight,

		CenterLeft,
		CenterCenter,
		CenterRight,

		BottomLeft,
		BottomCenter,
		BottomRight,
	};
}

/**
 * The basic element returned for extending the design surface.
 */
class UMGEDITOR_API FDesignerSurfaceElement : public TSharedFromThis < FDesignerSurfaceElement >
{
public:
	FDesignerSurfaceElement(TSharedRef<SWidget> InWidget, EExtensionLayoutLocation::Type InLocation, FVector2D InOffset = FVector2D(0,0))
		: Widget(InWidget)
		, Location(InLocation)
		, Offset(InOffset)
	{
	}

	/** Gets the widget that will be laid out in the design surface for extending the capability of the selected widget */
	TSharedRef<SWidget> GetWidget() const
	{
		return Widget;
	}

	/** Gets the location where the widget will be appear */
	EExtensionLayoutLocation::Type GetLocation() const
	{
		return Location;
	}

	/** Sets the offset after laid out in that location */
	void SetOffset(FVector2D InOffset)
	{
		Offset = InOffset;
	}

	/** Gets the offset after being laid out. */
	FVector2D GetOffset() const
	{
		return Offset;
	}

protected:
	TSharedRef<SWidget> Widget;

	EExtensionLayoutLocation::Type Location;

	FVector2D Offset;
};

/**
 * The Designer extension allows developers to provide additional widgets and custom painting to the designer surface for
 * specific widgets.  Allowing for a more customized and specific editors for the different widgets.
 */
class UMGEDITOR_API FDesignerExtension : public TSharedFromThis<FDesignerExtension>
{
public:
	/** Constructor */
	FDesignerExtension();

	/** Initializes the designer extension, this is called the first time a designer extension is registered */
	virtual void Initialize(UWidgetBlueprint* InBlueprint);

	virtual bool CanExtendSelection(const TArray< FWidgetReference >& Selection) const
	{
		return false;
	}

	/** Called every time the selection in the designer changes. */
	virtual void ExtendSelection(const TArray< FWidgetReference >& Selection, TArray< TSharedRef<FDesignerSurfaceElement> >& SurfaceElements)
	{
	}

	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
	{
	}

	virtual void Paint(const TSet< FWidgetReference >& Selection, const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
	{
	}

	/** Gets the ID identifying this extension. */
	FName GetExtensionId() const;

protected:
	void BeginTransaction(const FText& SessionName);

	void EndTransaction();

protected:
	FName ExtensionId;
	UWidgetBlueprint* Blueprint;

	TArray< FWidgetReference > SelectionCache;

private:
	FScopedTransaction* ScopedTransaction;
};