// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

/**
 * Console input box with command-completion support
 */

struct FSearchEntry
{
	FString Title;
	FString URL;
	bool bCategory;

	static FSearchEntry * MakeCategoryEntry(const FString & InTitle);
};


class SSuperSearchBox
	: public SCompoundWidget
{

public:

	SLATE_BEGIN_ARGS( SSuperSearchBox )
		: _SuggestionListPlacement( MenuPlacement_BelowAnchor )
		{}

		/** Where to place the suggestion list */
		SLATE_ARGUMENT( EMenuPlacement, SuggestionListPlacement )

	SLATE_END_ARGS()

	/** Protected console input box widget constructor, called by Slate */
	SSuperSearchBox();

	/**
	 * Construct this widget.  Called by the SNew() Slate macro.
	 *
	 * @param	InArgs	Declaration used by the SNew() macro to construct this widget
	 */
	void Construct( const FArguments& InArgs );

	/** Returns the editable text box associated with this widget.  Used to set focus directly. */
	TSharedRef< SEditableTextBox > GetEditableTextBox()
	{
		return InputText.ToSharedRef();
	}

	/** SWidget interface */
	virtual void Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime ) override;

protected:

	virtual bool SupportsKeyboardFocus() const { return true; }

	// e.g. Tab or Key_Up
	virtual FReply OnKeyDown( const FGeometry& MyGeometry, const FKeyboardEvent& KeyboardEvent );

	void OnKeyboardFocusLost( const FKeyboardFocusEvent& InKeyboardFocusEvent );

	/** Handles entering in a command */
	void OnTextCommitted(const FText& InText, ETextCommit::Type CommitInfo);

	void OnTextChanged(const FText& InText);

	/** Makes the widget for the suggestions messages in the list view */
	TSharedRef<ITableRow> MakeSuggestionListItemWidget(TSharedPtr<FSearchEntry> Suggestion, const TSharedRef<STableViewBase>& OwnerTable);

	void Query_HttpRequestComplete(FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded);

	void SuggestionSelectionChanged(TSharedPtr<FSearchEntry> NewValue, ESelectInfo::Type SelectInfo);
	
	void OnMenuOpenChanged(bool bIsOpen);

	void ActOnSuggestion(TSharedPtr<FSearchEntry> SearchEntry);
		
	void UpdateSuggestions();

	void MarkActiveSuggestion();

	void ClearSuggestions();

	FString GetSelectionText() const;


private:

	/** Editable text widget */
	TSharedPtr< SEditableTextBox > InputText;

	/** history / auto completion elements */
	TSharedPtr< SMenuAnchor > SuggestionBox;

	/** All suggestions stored in this widget for the list view */
	TArray< TSharedPtr<FSearchEntry> > Suggestions;

	/** The list view for showing all log messages. Should be replaced by a full text editor */
	TSharedPtr< SListView< TSharedPtr<FSearchEntry> > > SuggestionListView;

	//TODO: properly clean map up as time goes by requests are processed and can be discarded
	TMap<FHttpRequestPtr, FText> RequestQueryMap;

	typedef TMap<FString, TArray<FSearchEntry> > FCategoryResults;
	struct FSearchResults
	{
		 FCategoryResults OnlineResults;
	};

	//TODO: properly clean map up as time goes by requests are processed and can be discarded
	/** A cache of results based on search */
	TMap<FString, FSearchResults> SearchResultsCache;

	/** -1 if not set, otherwise index into Suggestions */
	int32 SelectedSuggestion;

	/** to prevent recursive calls in UI callback */
	bool bIgnoreUIUpdate; 
};