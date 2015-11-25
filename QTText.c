//////////
//
//	File:		QTText.c
//
//	Contains:	QuickTime text media handler sample code.
//
//	Written by:	Tim Monroe
//				parts based on QTTextSample code by Nick Thompson (see develop, issue 20).
//
//	Copyright:	� 1998-2000 by Apple Computer, Inc., all rights reserved.
//
//	Change History (most recent first):
//
//	   <14>	 	07/20/00	rtm		replaced all calls to QTText_UpdateMovieAndController by MCMovieChanged;
//									reworked QTText_RemoveIndTextTrack to call MCMovieChanged right after
//									calling QTUtils_DeleteAllReferencesToTrack; this prevents crashes when
//									calling MCMovieChanged after QTText_RemoveIndTextTrack on Windows (this
//									happened when deleting a visible chapter track)
//	   <13>	 	07/07/00	rtm		added HREFTrack support; added QTText_SyncWindowData
//	   <12>	 	06/16/00	rtm		added call to TextMediaSetTextProc to QTText_AddTextTrack
//	   <11>	 	06/02/00	rtm		added call to QTText_UpdateMovieAndController to QTText_EditText
//	   <10>	 	03/17/00	rtm		made changes to get things running under CarbonLib
//	   <9>	 	06/14/99	rtm		added a bunch more chapter track utilities
//	   <8>	 	06/11/99	rtm		added QTText_GetChapterTrackForTrack
//	   <7>	 	06/10/98	rtm		general clean-up; in QTText_AddTextTrack, set the time scale of the
//									text track media to that of the movie
//	   <6>	 	06/08/98	rtm		fixed text track duration calculations
//	   <5>	 	06/05/98	rtm		fixed track geometry calculations in QTText_AddTextTrack
//	   <4>	 	06/03/98	rtm		added QTText_AddTextTrack and QTText_RemoveIndTextTrack; borrowed
//									QTText_CopyCStringToPascal from source file CGlue.c
//	   <3>	 	05/29/98	rtm		added chapter track enabling/disabling
//	   <2>	 	04/08/98	rtm		added text offset handling, so we can now search within the current sample;
//									added QTText_EditText to allow editing a sample's text; added compile flag
//									to use MovieSearchText instead of TextMediaFindNextText (see Note 3)
//	   <1>	 	04/07/98	rtm		first file; conversion to personal coding style; updated to latest headers;
//									got basic searching working on Mac and Windows
//	 
//	This source code shows how to do searches on text media, how to use a simple text procedure to retrieve
//	the text from a text media sample, and how to edit that text. It also shows how to convert a text track
//	into a chapter track (and vice versa), and how to add (and remove) text tracks to (and from) a movie.
//
// NOTES:
//
// *** (1) ***
// This code is based in part on the code provided with the develop article on QuickTime text by Nick Thompson
// (issue 20). Make sure to read that article for complete details on the techniques employed here. I have
// taken the liberty of reworking that code as necessary to make it run on Windows platforms and to bring it 
// into line with the other QuickTime code samples.
//
// *** (2) ***
// The editted text does NOT use the font, size, color, justification, or background color of the text
// it replaces. It would be straightforward to add this capability. See the develop article mentioned above for
// code that does all these things.
//
// *** (3) ***
// The Movie Toolbox provides two different functions that you can use to search for text in a text track: 
// TextMediaFindNextText (originally called FindNextText) and MovieSearchText. TextMediaFindNextText inspects
// only a specified track, while MovieSearchText can search all text tracks in a specified movie. Moreover,
// MovieSearchText will automatically go to and highlight the found text; these operations must be done manually
// if you're using TextMediaFindNextText. This sample code illustrates BOTH of these functions; you determine
// which is used by setting the USE_MOVIESEARCHTEXT compiler flag in QTText.h.
//
//////////

#include "QTText.h"


//////////
//
// global variables
//
//////////

Boolean						gSearchForward = true;				// do we search forward or backward?
Boolean						gSearchWrap = true;					// do we wrap around when searching?
Boolean						gSearchWithCase = false;			// is the search case sensitive?
Str255						gSearchText;						// the text we're searching for
Str255						gSampleText;						// the text of the current text media sample
long						gOffset;							// offset of current found text within sample
TextMediaUPP				gTextProcUPP = NULL;				// UPP to text handling procedure

extern ModalFilterUPP		gModalFilterUPP;


//////////
//
// QTText_InitWindowData
// Initialize any window-specific data for the text media handler.
//
//////////

ApplicationDataHdl QTText_InitWindowData (WindowObject theWindowObject)
{
	ApplicationDataHdl		myAppData = NULL;
	Track					myTrack = NULL;
	MediaHandler			myHandler = NULL;

	myAppData = (ApplicationDataHdl)NewHandleClear(sizeof(ApplicationDataRecord));
	if (myAppData != NULL) {
	
		myTrack = GetMovieIndTrackType((**theWindowObject).fMovie, 1, TextMediaType, movieTrackMediaType | movieTrackEnabledOnly);
		if (myTrack != NULL) {
			// load the entire text track into RAM
			LoadTrackIntoRam(myTrack, 0L, GetTrackDuration(myTrack), 0L);

			// set the text handling procedure
			myHandler = GetMediaHandler(GetTrackMedia(myTrack));
			if (myHandler != NULL)
				TextMediaSetTextProc(myHandler, gTextProcUPP, (long)theWindowObject);
		}
	
		// remember the text track and media handler
		(**myAppData).fMovieHasText = (myTrack != NULL);
		(**myAppData).fTextIsChapter = QTText_TrackTypeHasAChapterTrack((**theWindowObject).fMovie, VideoMediaType);
		(**myAppData).fTextIsHREF = QTText_IsHREFTrack(myTrack);
		(**myAppData).fTextTrack = myTrack;
		(**myAppData).fTextHandler = myHandler;
	}
	
	return(myAppData);
}


//////////
//
// QTText_DumpWindowData
// Get rid of any window-specific data for the text media handler.
//
//////////

void QTText_DumpWindowData (WindowObject theWindowObject)
{
	ApplicationDataHdl		myAppData = NULL;
		
	myAppData = (ApplicationDataHdl)QTFrame_GetAppDataFromWindowObject(theWindowObject);
	if (myAppData != NULL)
		DisposeHandle((Handle)myAppData);
}


//////////
//
// QTText_SyncWindowData
// Synchronize any window-specific data for the text media handler.
//
//////////

void QTText_SyncWindowData (WindowObject theWindowObject)
{
	ApplicationDataHdl		myAppData = NULL;
	Track					myTrack = NULL;
	MediaHandler			myHandler = NULL;

	myAppData = (ApplicationDataHdl)QTFrame_GetAppDataFromWindowObject(theWindowObject);
	if (myAppData != NULL) {
	
		myTrack = GetMovieIndTrackType((**theWindowObject).fMovie, 1, TextMediaType, movieTrackMediaType | movieTrackEnabledOnly);
		if (myTrack != NULL) {
			// load the entire text track into RAM
			LoadTrackIntoRam(myTrack, 0L, GetTrackDuration(myTrack), 0L);

			// set the text handling procedure
			myHandler = GetMediaHandler(GetTrackMedia(myTrack));
			if (myHandler != NULL)
				TextMediaSetTextProc(myHandler, gTextProcUPP, (long)theWindowObject);
		}
	
		// remember the text track and media handler
		(**myAppData).fMovieHasText = (myTrack != NULL);
		(**myAppData).fTextIsChapter = QTText_TrackTypeHasAChapterTrack((**theWindowObject).fMovie, VideoMediaType);
		(**myAppData).fTextIsHREF = QTText_IsHREFTrack(myTrack);
		(**myAppData).fTextTrack = myTrack;
		(**myAppData).fTextHandler = myHandler;
	}
}
  

//////////
//
// QTText_SetSearchText
// Let the user specify the text to be searched for.
//
//////////

void QTText_SetSearchText (void)
{
	DialogPtr		myDialog;
	short			myItem;
	short			myType;
	Handle			myItemHandle;
	Rect			myRect;
	
	// get the dialog that lets the user specify the search text
	myDialog = GetNewDialog(kTextDialogID, NULL, (WindowPtr)-1);
	if (myDialog == NULL)
		goto bail;

	SetDialogDefaultItem(myDialog, kTextOKIndex);
	
	// set the current search text into the edittext field
	GetDialogItem(myDialog, kTextTextEditIndex, &myType, &myItemHandle, &myRect);
	SetDialogItemText(myItemHandle, gSearchText);
	SelectDialogItemText(myDialog, kTextTextEditIndex, 0, 32767);	
		
	// now show the dialog
	MacShowWindow(GetDialogWindow(myDialog));
	MacSetPort(GetDialogPort(myDialog));
	
	do {
		ModalDialog(gModalFilterUPP, &myItem);
	} while (myItem != kTextOKIndex);
	
	// get the text in the edittext field
	GetDialogItemText(myItemHandle, gSearchText);
		
bail:
	if (myDialog != NULL)
		DisposeDialog(myDialog);
}


//////////
//
// QTText_FindText
// Find the specified string in the (first) text track of the specified window object.
//
//////////

void QTText_FindText (WindowObject theWindowObject, Str255 theText) 
{
	ApplicationDataHdl		myAppData = NULL;
	Movie					myMovie = NULL;
	MediaHandler			myHandler = NULL;
	MovieController			myMC = NULL;
	long					myFlags = 0L;
	TimeValue				myTimeValue;
	OSErr					myErr = noErr;
		
	myAppData = (ApplicationDataHdl)QTFrame_GetAppDataFromWindowObject(theWindowObject);
	if (myAppData == NULL)
		return;
		
	myMC = (**theWindowObject).fController;
	myMovie = (**theWindowObject).fMovie;
	myHandler = (**myAppData).fTextHandler;
	
	// set the search features
	myFlags = findTextUseOffset;

	if (!gSearchForward)
		myFlags |= findTextReverseSearch;
		
	if (gSearchWrap)
		myFlags |= findTextWrapAround;
	
	if (gSearchWithCase)
		myFlags |= findTextCaseSensitive;

	myTimeValue = GetMovieTime(myMovie, NULL);

#if USE_MOVIESEARCHTEXT
	//////////
	//
	// METHOD ONE: Use MovieSearchText, your one-stop, find-the-text-and-do-the-right-thing function.
	//
	//////////
	
	myFlags |= searchTextEnabledTracksOnly;
	
	myErr = MovieSearchText(myMovie, (Ptr)(&theText[1]), theText[0], myFlags, NULL, &myTimeValue, &gOffset);
	if (myErr != noErr)
		QTFrame_Beep();		// if the desired string wasn't found, beep
#else
	//////////
	//
	// METHOD TWO: Use TextMediaFindNextText. Here, you need to explicitly go to the found text and select it. 
	//
	//////////

	if (myHandler != NULL) {
		TimeValue	myFoundTime, myFoundDuration;
		TimeRecord	myNewTime;
		RGBColor	myColor;
		
		myColor.red = myColor.green = myColor.blue = 0x8000;	// grey
		
		// search for the specified text
		myErr = TextMediaFindNextText(myHandler, (Ptr)(&theText[1]), theText[0], myFlags, myTimeValue, &myFoundTime, &myFoundDuration, &gOffset);	
		if (myFoundTime != -1) {
			// convert the TimeValue to a TimeRecord
			myNewTime.value.hi = 0;
			myNewTime.value.lo = myFoundTime;
			myNewTime.scale = GetMovieTimeScale(myMovie);
			myNewTime.base = NULL;
						
			// go to the found text	
			MCDoAction(myMC, mcActionGoToTime, &myNewTime);

			// highlight the text
			TextMediaHiliteTextSample(myHandler, myFoundTime, gOffset, gOffset + theText[0], &myColor);
			
		} else {
			// if the desired string wasn't found, beep
			QTFrame_Beep();
		}
	}
#endif // USE_MOVIESEARCHTEXT

	// update the current offset, if we're searching forward
	if (gSearchForward && (myErr == noErr))
		gOffset += theText[0];
}


//////////
//
// QTText_EditText
// Edit the text in the current sample of the (first) text track of the specified window object.
//
//////////

void QTText_EditText (WindowObject theWindowObject) 
{
	ApplicationDataHdl		myAppData = NULL;
	Movie					myMovie = NULL;
	Track					myTrack = NULL;
	Media					myMedia = NULL;
	MediaHandler			myHandler = NULL;
	DialogPtr				myDialog = NULL;
	short					myItem;
	short					myType;
	Handle					myItemHandle = NULL;
	Rect					myRect;
	OSErr					myErr = noErr;
		
	// get the movie and related stuff	
	myAppData = (ApplicationDataHdl)QTFrame_GetAppDataFromWindowObject(theWindowObject);
	if (myAppData == NULL)
		goto bail;
		
	myMovie = (**theWindowObject).fMovie;
	myTrack = (**myAppData).fTextTrack;
	myMedia = GetTrackMedia(myTrack);
	myHandler = (**myAppData).fTextHandler;

	// get the dialog that lets the user specify the text for the current sample
	myDialog = GetNewDialog(kEditDialogID, NULL, (WindowPtr)-1);
	if (myDialog == NULL)
		goto bail;

	SetDialogDefaultItem(myDialog, kEditOKIndex);
	SetDialogCancelItem(myDialog, kEditCancelIndex);
	
	// set the current sample text into the edittext field
	GetDialogItem(myDialog, kEditTextEditIndex, &myType, &myItemHandle, &myRect);
	SetDialogItemText(myItemHandle, gSampleText);
	SelectDialogItemText(myDialog, kEditTextEditIndex, 0, 32767);	

	// now show the dialog
	MacShowWindow(GetDialogWindow(myDialog));
	MacSetPort(GetDialogPort(myDialog));
	
	do {
		ModalDialog(gModalFilterUPP, &myItem);
	} while ((myItem != kEditOKIndex) && (myItem != kEditCancelIndex));
	
	// if the user hit the OK button, save the text and update the text sample 
	if (myItem == kEditOKIndex) {
		Fixed			myWidth;
		Fixed			myHeight;
		Rect			myBounds;	
		TimeValue		myMovieTime;
		TimeValue		mySampleTime;			
		TimeValue		myDuration;
		TimeValue		myMediaSampleDuration;
		TimeValue		myMediaSampleStartTime;
		TimeValue		myMediaCurrentTime;
		TimeValue		myInterestingTime;
		long			myMediaSampleIndex;

		// get the text in the edittext field
		GetDialogItemText(myItemHandle, gSampleText);
		
		// install that text as the current text media sample

		// first, we need to find the start and duration for this media sample;
		// get the current movie time and the time in media time of the current sample
		myMovieTime = GetMovieTime(myMovie, NULL);
			
		myMediaCurrentTime = TrackTimeToMediaTime(myMovieTime, myTrack);
				
		// get detailed information about the start and duration of the current sample		
		MediaTimeToSampleNum(	myMedia, 
								myMediaCurrentTime, 
								&myMediaSampleIndex, 
								&myMediaSampleStartTime,
								&myMediaSampleDuration);
								
		// see where this text starts
		GetTrackNextInterestingTime(myTrack, nextTimeEdgeOK | nextTimeMediaSample, myMovieTime, -fixed1, &myInterestingTime, NULL);		

		// get the duration of the current sample
		myMovieTime = myInterestingTime;					
		GetTrackNextInterestingTime(myTrack, nextTimeEdgeOK | nextTimeMediaSample, myMovieTime, fixed1, NULL, &myDuration);
							
		myErr = BeginMediaEdits(myMedia);
		if (myErr != noErr) 
			goto bail;
				
		// delete the existing text			
		myErr = DeleteTrackSegment(myTrack, myInterestingTime, myDuration);
		if (myErr != noErr) 
			goto bail;
			
		// get the track bounds
		GetTrackDimensions(myTrack, &myWidth, &myHeight);
		myBounds.top = 0;
		myBounds.left = 0;
		myBounds.right = Fix2Long(myWidth);
		myBounds.bottom = Fix2Long(myHeight);	

		// write out the new data to the media
		myErr = TextMediaAddTextSample(	myHandler, 
										(Ptr)(&gSampleText[1]), 
										gSampleText[0],
										0,
										0,
										0,
										NULL, 
										NULL, 
										teCenter,
										&myBounds, 
										dfClipToTextBox, 
										0, 
										0, 
										0, 
										NULL, 
										myMediaSampleDuration, 
										&mySampleTime);
								
		if (myErr != noErr) 
			goto bail;
			
		EndMediaEdits(myMedia);
		
		// insert the new media into the track
		InsertMediaIntoTrack(myTrack, myInterestingTime, mySampleTime, myMediaSampleDuration, fixed1);

		// stamp the movie as dirty
		(**theWindowObject).fIsDirty = true;
		
		// update the chapter pop-up
		if ((**theWindowObject).fController != NULL)
			MCMovieChanged((**theWindowObject).fController, myMovie);
	}
	
bail:
	if (myDialog != NULL)
		DisposeDialog(myDialog);
}


//////////
//
// QTText_TextProc
// This function is called whenever a new text sample is about to be displayed.
// We'll use it to grab the text of the current text sample.
//
// NOTE: The theRefCon parameter is a handle to a window object record.
//
//////////

PASCAL_RTN OSErr QTText_TextProc (Handle theText, Movie theMovie, short *theDisplayFlag, long theRefCon)
{
#pragma unused(theMovie, theRefCon)
	char			*myTextPtr = NULL;
	short			myTextSize;
	short			myIndex;
	
	// on entry to this function, theText is a handle to the text sample data,
	// which is a big-endian 16-bit length word followed by the text itself
	myTextSize = EndianU16_BtoN(*(short *)(*theText));
	myTextPtr = (char *)(*theText + sizeof(short));

	// copy the text into our global variable
	for (myIndex = 1; myIndex <= myTextSize; myIndex++, myTextPtr++)
		gSampleText[myIndex] = *myTextPtr;

	gSampleText[0] = myTextSize;
	
	// ask for the default text display
	*theDisplayFlag = txtProcDefaultDisplay;

	return(noErr);
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Text track utilities.
//
// Use these functions to manipulate text tracks in a movie.
//
///////////////////////////////////////////////////////////////////////////////////////////////////////////

//////////
//
// QTText_AddTextTrack
// Add a text track containing some text data to a movie; return the new track to the caller.
//
// The theStrings parameter is an array of C strings; each element is the text for a certain span of frames
// of the movie. The theFrames parameter is an array of frame counts; each of these counts determines how many
// frames the corresponding text element in the theStrings array applies to; the sum of all the values in this
// array should be equal to the total number of frames in the movie.
//
// If the isChapterTrack parameter is true, the text track is set to be a chapter track, attached to the (first)
// track of type theType.
//
//////////

Track QTText_AddTextTrack (Movie theMovie, char *theStrings[], short theFrames[], short theNumFrames, OSType theType, Boolean isChapterTrack)
{
	Track				myTypeTrack = NULL;
	Track				myTextTrack = NULL;
	Media				myMedia = NULL;
	MediaHandler		myHandler = NULL;
	TimeScale			myTimeScale;
	MatrixRecord		myMatrix;
	Fixed				myWidth;
	Fixed				myHeight;
	OSErr				myErr = noErr;

	//////////
	//
	// find the target track
	//
	//////////

	// get the (first) track of the specified type; this track determines the width of the new text track
	// and (if isChapterTrack is true) is the target of the new chapter track
	myTypeTrack = GetMovieIndTrackType(theMovie, 1, theType, movieTrackMediaType);
	if (myTypeTrack == NULL)
		goto bail;
	
	// get the dimensions of the target track
	GetTrackDimensions(myTypeTrack, &myWidth, &myHeight);
	myTimeScale = GetMediaTimeScale(GetTrackMedia(myTypeTrack));
	
	//////////
	//
	// create the text track and media
	//
	//////////

	myTextTrack = NewMovieTrack(theMovie, myWidth, FixRatio(kTextTrackHeight, 1), kNoVolume);
	if (myTextTrack == NULL)
		goto bail;
		
	myMedia = NewTrackMedia(myTextTrack, TextMediaType, myTimeScale, NULL, 0);
	if (myMedia == NULL)
		goto bail;
		
	myHandler = GetMediaHandler(myMedia);
	if (myHandler == NULL)
		goto bail;
	
	//////////
	//
	// figure out the text track geometry
	//
	//////////
	
	GetTrackMatrix(myTextTrack, &myMatrix);
	TranslateMatrix(&myMatrix, 0, myHeight);
	
	SetTrackMatrix(myTextTrack, &myMatrix);	
	SetTrackEnabled(myTextTrack, true);
	
	//////////
	//
	// edit the track media
	//
	//////////

	myErr = BeginMediaEdits(myMedia);
	if (myErr == noErr) {
		Rect				myBounds;	
		short				myIndex;
		TimeValue			myTypeSampleDuration;
		TimeRecord			myTimeRec;
		
		myBounds.top = 0;
		myBounds.left = 0;
		myBounds.right = Fix2Long(myWidth);
		myBounds.bottom = Fix2Long(myHeight);
		
		// determine the duration of a sample in the track of the specified type
		myTypeSampleDuration = QTUtils_GetFrameDuration(myTypeTrack);
				

		for (myIndex = 0; myIndex < theNumFrames; myIndex++) {
			TimeValue		myTextSampleDuration;
			Str255			mySampleText;

			myTextSampleDuration = myTypeSampleDuration * theFrames[myIndex];
			
			// set the time scale of the media to that of the movie
			myTimeRec.value.lo = myTextSampleDuration;
			myTimeRec.value.hi = 0;
			myTimeRec.scale = GetMovieTimeScale(theMovie);
			ConvertTimeScale(&myTimeRec, GetMediaTimeScale(myMedia));
			myTextSampleDuration = myTimeRec.value.lo;

			QTText_CopyCStringToPascal(theStrings[myIndex], mySampleText);
			
#if USE_ADDMEDIASAMPLE
			{
				TextDescriptionHandle		mySampleDesc = NULL;
				Handle						mySample = NULL;
				UInt16						myLength;
				RGBColor					myBGColor = {0xffff, 0xffff, 0xffff};

				mySampleDesc = (TextDescriptionHandle)NewHandleClear(sizeof(TextDescription));
				if (mySampleDesc == NULL)
					goto bail;
					
				(**mySampleDesc).descSize = sizeof(TextDescription);
				(**mySampleDesc).dataFormat = TextMediaType;
				(**mySampleDesc).displayFlags = dfClipToTextBox;
				(**mySampleDesc).textJustification = teCenter;
				(**mySampleDesc).defaultTextBox = myBounds;
				(**mySampleDesc).bgColor = myBGColor;
				
				myLength = EndianU16_NtoB(mySampleText[0]);

				// create the text media sample: a 16-bit length word followed by the text
				myErr = PtrToHand(&myLength, &mySample, sizeof(myLength));
				if (myErr == noErr) {
					myErr = PtrAndHand((Ptr)(&mySampleText[1]), mySample, mySampleText[0]);
					if (myErr == noErr)
						AddMediaSample(	myMedia,
										mySample,
										0,
										GetHandleSize(mySample),
										myTextSampleDuration,
										(SampleDescriptionHandle)mySampleDesc,
										1,
										0,
										NULL);
					DisposeHandle(mySample);
				}
				
				DisposeHandle((Handle)mySampleDesc);
			}
#else
			// write out the new data to the media
			myErr = TextMediaAddTextSample(	myHandler,
											(Ptr)(&mySampleText[1]),
											mySampleText[0],
											0,
											0,
											0,
											NULL,
											NULL,
											teCenter,
	  										&myBounds,
											dfClipToTextBox,
											0,
											0,
											0,
											NULL,
											myTextSampleDuration,
											NULL);
#endif
		}
	}

	myErr = EndMediaEdits(myMedia);
	if (myErr != noErr)
		goto bail;
	
	// insert the text media into the text track
	myErr = InsertMediaIntoTrack(myTextTrack, 0, 0, GetMediaDuration(myMedia), fixed1);
	if (myErr != noErr)
		goto bail;

	//////////
	//
	// set the text handling procedure
	//
	//////////
	
	TextMediaSetTextProc(myHandler, gTextProcUPP, (long)QTFrame_GetWindowObjectFromFrontWindow());

	//////////
	//
	// if requested, set the new text track as a chapter track for the track of the specified type
	//
	//////////
	
	if (isChapterTrack)
		AddTrackReference(myTypeTrack, myTextTrack, kTrackReferenceChapterList, NULL);

bail:
	return(myTextTrack);
}


//////////
//
// QTText_RemoveIndTextTrack
// Remove a text track, specified by its index, from a movie.
//
// If theIndex is kAllTextTracks (0), remove all text tracks from the movie.
//
//////////

OSErr QTText_RemoveIndTextTrack (WindowObject theWindowObject, short theIndex)
{
	MovieController 	myMC = NULL;
	Movie				myMovie = NULL;
	Track				myTrack = NULL;
	OSErr				myErr = noErr;
	
	if (theWindowObject == NULL)
		return(paramErr);
			
	myMC = (**theWindowObject).fController;
	myMovie = (**theWindowObject).fMovie;

	if (theIndex == kAllTextTracks) {
		// remove ALL text tracks from the movie
		myTrack = GetMovieIndTrackType(myMovie, 1, TextMediaType, movieTrackMediaType);
		if (myTrack == NULL)
			myErr = badTrackIndex;
				
		while (myTrack != NULL) {
			QTUtils_DeleteAllReferencesToTrack(myTrack);
			MCMovieChanged(myMC, myMovie);
			DisposeMovieTrack(myTrack);
			myTrack = GetMovieIndTrackType(myMovie, 1, TextMediaType, movieTrackMediaType);
		}
	} else {
		// remove ONE text track from the movie
		myTrack = GetMovieIndTrackType(myMovie, theIndex, TextMediaType, movieTrackMediaType);
		if (myTrack == NULL) {
			myErr = badTrackIndex;
		} else {
			QTUtils_DeleteAllReferencesToTrack(myTrack);
			MCMovieChanged(myMC, myMovie);
			DisposeMovieTrack(myTrack);
		}
	}

	return(myErr);
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Chapter track utilities.
//
// Use these functions to manipulate chapter tracks in a movie.
//
// A chapter track is a text track that has been associated with some other track (often a video or sound
// track) in such a way that the movie controller will build, display, and handle a pop-up menu of titles
// of various parts of the associated track. The pop-up menu appears (space permitting) in the controller
// bar. The various parts of the associated track are called the track's "chapters". When a user selects a
// chapter title in the pop-up menu, the movie controller jumps to the start time of the selected chapter.
//
// You create the association between a text track and some other track by creating a reference from that
// other track to the text track, where the reference is of type kTrackReferenceChapterList. Note that all
// the chapter titles must be contained in a single text track; you specify the starting time for chapters
// when you add the text to the text track by calling TextMediaAddTextSample. Note also that you need to
// create the chapter association only between the text track and one other track, not between the chapter
// track and all other tracks in the movie. (That "other" track must be enabled, but typically the chapter
// track is not enabled.)
//
// The pop-up menu will disappear from the controller bar if there isn't enough space to display the menu,
// the volume slider control, the step buttons, and the other controls. 
//
///////////////////////////////////////////////////////////////////////////////////////////////////////////

//////////
//
// QTText_SetTextTrackAsChapterTrack
// Set the (first) text track in the specified movie to be or not to be a chapter track
// for the (first) enabled track of the specified type.
//
// The isChapterTrack parameter determines the final state of the text track.
//
//////////

OSErr QTText_SetTextTrackAsChapterTrack (WindowObject theWindowObject, OSType theType, Boolean isChapterTrack)
{
	ApplicationDataHdl		myAppData = NULL;
	Movie					myMovie = NULL;
	MovieController			myMC = NULL;
	Track					myTypeTrack = NULL;
	Track					myTextTrack = NULL;
	OSErr					myErr = paramErr;
		
	// get the movie, controller, and related stuff	
	myAppData = (ApplicationDataHdl)QTFrame_GetAppDataFromWindowObject(theWindowObject);
	if (myAppData == NULL)
		return(myErr);
		
	myMovie = (**theWindowObject).fMovie;
	myMC = (**theWindowObject).fController;
	myTextTrack = (**myAppData).fTextTrack;
	
	if ((myMovie != NULL) && (myMC != NULL)) {
		myTypeTrack = GetMovieIndTrackType(myMovie, 1, theType, movieTrackMediaType | movieTrackEnabledOnly);
		if ((myTypeTrack != NULL) && (myTextTrack != NULL)) {
		
			// add or delete a track reference, as determined by the desired final state
			if (isChapterTrack)
				myErr = AddTrackReference(myTypeTrack, myTextTrack, kTrackReferenceChapterList, NULL);
			else
				myErr = DeleteTrackReference(myTypeTrack, kTrackReferenceChapterList, 1);
				
			// tell the movie controller we've changed aspects of the movie
			MCMovieChanged(myMC, myMovie);
			
			// stamp the movie as dirty
			(**theWindowObject).fIsDirty = true;
		}
	}

	return(myErr);
}


//////////
//
// QTText_TrackTypeHasAChapterTrack
// Does the (first) enabled track of the specified type in the specified movie have a chapter track?
//
//////////

Boolean QTText_TrackTypeHasAChapterTrack (Movie theMovie, OSType theType)
{
	Track					myTypeTrack = NULL;
	Track					myTextTrack = NULL;
		
	myTypeTrack = GetMovieIndTrackType(theMovie, 1, theType, movieTrackMediaType | movieTrackEnabledOnly);
	if (myTypeTrack != NULL)
		myTextTrack = GetTrackReference(myTypeTrack, kTrackReferenceChapterList, 1);

	return(myTextTrack != NULL);
}


//////////
//
// QTText_TrackHasAChapterTrack
// Does the specified track have a chapter track?
//
//////////

Boolean QTText_TrackHasAChapterTrack (Track theTrack)
{
	return(GetTrackReference(theTrack, kTrackReferenceChapterList, 1) != NULL);
}


//////////
//
// QTText_MovieHasAChapterTrack
// Does the specified movie have a chapter track? In other words, is there are least one enabled track
// in the specified movie that has a chapter track associated wih it?
//
//////////

Boolean QTText_MovieHasAChapterTrack (Movie theMovie)
{
	long		myCount;
	long		myTrackCount = GetMovieTrackCount(theMovie);
	Track		myTrack = NULL;
	Boolean		myGotChapter = false;
	
	for (myCount = 1; myCount <= myTrackCount; myCount++) {
		myTrack = GetMovieIndTrack(theMovie, myCount);
		if (GetTrackEnabled(myTrack))
			myGotChapter = QTText_TrackHasAChapterTrack(myTrack);
			if (myGotChapter)
				break;
	}
	
	return(myGotChapter);
}


//////////
//
// QTText_GetChapterTrackForTrack
// Return the first chapter track (if any) associated with the specified track.
//
//////////

Track QTText_GetChapterTrackForTrack (Track theTrack)
{
	return(GetTrackReference(theTrack, kTrackReferenceChapterList, 1));
}


//////////
//
// QTText_GetChapterTrackForMovie
// Return the first chapter track (if any) in the specified movie.
//
// A movie can have more than one chapter track; if so, QuickTime uses the chapter track associated
// with the first enabled track that it finds. Accordingly, this function returns the first chapter
// track that we find as we iterate thru the movie's tracks.
//
//////////

Track QTText_GetChapterTrackForMovie (Movie theMovie)
{
	long		myCount;
	long		myTrackCount = GetMovieTrackCount(theMovie);
	Track		myTrack = NULL;
	Track		myChapTrack = NULL;
	
	for (myCount = 1; myCount <= myTrackCount; myCount++) {
		myTrack = GetMovieIndTrack(theMovie, myCount);
		if (GetTrackEnabled(myTrack))
			myChapTrack = QTText_GetChapterTrackForTrack(myTrack);
			if (myChapTrack != NULL)
				break;
	}
	
	return(myChapTrack);
}


//////////
//
// QTText_IsChapterTrack
// Is the specified track a chapter track?
//
//////////

Boolean QTText_IsChapterTrack (Track theTrack)
{
	Movie				myMovie = NULL;
	Track				myTrack = NULL;
	long				myTrackCount = 0L;
	long				myTrRefCount = 0L;
	long				myTrackIndex;
	long				myTrRefIndex;

	myMovie = GetTrackMovie(theTrack);
	if (myMovie == NULL)
		return(false);

	// a chapter track is a text track that is referred to by some other track in the movie,
	// so we need to iterate thru all those tracks to see if any of them refers to the specified track
	myTrackCount = GetMovieTrackCount(myMovie);
	for (myTrackIndex = 1; myTrackIndex <= myTrackCount; myTrackIndex++) {
		myTrack = GetMovieIndTrack(myMovie, myTrackIndex);
		if ((myTrack != NULL) && (myTrack != theTrack)) {
		
			// iterate thru all track references of type kTrackReferenceChapterList
			myTrRefCount = GetTrackReferenceCount(myTrack, kTrackReferenceChapterList);
			for (myTrRefIndex = 1; myTrRefIndex <= myTrRefCount; myTrRefIndex++) {
				Track	myRefTrack = NULL;

				myRefTrack = GetTrackReference(myTrack, kTrackReferenceChapterList, myTrRefIndex);
				if (myRefTrack == theTrack)
					return(true);
			}
		}
	}

	return(false);
}


//////////
//
// QTText_GetFirstChapterTime
// Return, through the theTime parameter, the starting time of the first chapter of the
// specified chapter track.
//
// If this function encounters an error, it returns a (bogus) starting time of -1. Note that
// GetTrackNextInterestingTime also returns -1 as a starting time if the search criteria
// specified in the myFlags parameter are not matched by any interesting time in the movie. 
//
//////////

OSErr QTText_GetFirstChapterTime (Track theChapterTrack, TimeValue *theTime)
{
	short	myFlags = nextTimeMediaSample + nextTimeEdgeOK;	// we want the first sample in the movie
	
	if (theChapterTrack == NULL) {
		*theTime = kBogusStartingTime;						// a bogus time
		return(invalidTrack);
	}
	
	GetTrackNextInterestingTime(theChapterTrack, myFlags, (TimeValue)0, fixed1, theTime, NULL);
	return(GetMoviesError());
}


//////////
//
// QTText_GetNextChapterTime
// Return, through the theTime parameter, the starting time of the chapter of the
// specified chapter track that immediately follows the chapter starting at the time
// passed in through theTime.
//
//////////

OSErr QTText_GetNextChapterTime (Track theChapterTrack, TimeValue *theTime)
{
	short	myFlags = nextTimeMediaSample;					// we want the next sample in the movie
	
	if (theChapterTrack == NULL) {
		*theTime = kBogusStartingTime;						// a bogus time
		return(invalidTrack);
	}
	
	GetTrackNextInterestingTime(theChapterTrack, myFlags, *theTime, fixed1, theTime, NULL);
	return(GetMoviesError());
}


//////////
//
// QTText_GetIndChapterTime
// Return the starting time of the chapter in the specified chapter track that has the specified index.
//
//////////

TimeValue QTText_GetIndChapterTime (Track theChapterTrack, long theIndex)
{
	long			myCount = 1;
	TimeValue		myTime = kBogusStartingTime;

	if ((theChapterTrack == NULL) || (theIndex < 1))
		return(myTime);

	QTText_GetFirstChapterTime(theChapterTrack, &myTime);
	if (theIndex == 1)
		return(myTime);
		
	while (myCount < theIndex) {
		QTText_GetNextChapterTime(theChapterTrack, &myTime);
		myCount++;
	}
	
	return(myTime);
}


//////////
//
// QTText_GetIndChapterText
// Return the text of the chapter in the specified chapter track that has the specified index.
//
// The caller is responsible for disposing of the pointer returned by this function (by calling free).
//
//////////

char *QTText_GetIndChapterText (Track theChapterTrack, long theIndex)
{
	long			myCount = 1;
	long			mySize;			// size of entire handle returned, which may include appended style atoms
	short			myTextSize;
	TimeValue		myTime;
	Handle			myHandle = NewHandleClear(0);
	char			*myText = NULL;

	if ((theChapterTrack == NULL) || (theIndex < 1))
		return(myText);

	if (myHandle == NULL)
		return(myText);

	myTime = QTText_GetIndChapterTime(theChapterTrack, theIndex);
	if (myTime != kBogusStartingTime) {
	
		GetMediaSample(	GetTrackMedia(theChapterTrack),
						myHandle,
						0,
						&mySize,
						TrackTimeToMediaTime(myTime, theChapterTrack),		// media time scale
						NULL,
						NULL,
						NULL,
						NULL,
						0,
						NULL,
						NULL);
					
		// for text media samples, the returned handle is a 16-bit size field followed by the actual text data
		myTextSize = *(short *)(*myHandle);
		myText = malloc(myTextSize + 1);
		BlockMove(*myHandle + sizeof(short), myText, myTextSize);
		myText[myTextSize] = '\0';
		
		DisposeHandle(myHandle);
	}

	return(myText);
}


//////////
//
// QTText_GetChapterCount
// Return the number of chapters in the specified chapter track.
//
//////////

long QTText_GetChapterCount (Track theChapterTrack)
{
	long			myCount = 0;
	TimeValue		myTime = kBogusStartingTime;

	if (theChapterTrack != NULL) {
		QTText_GetFirstChapterTime(theChapterTrack, &myTime);
			
		while (myTime != kBogusStartingTime) {
			myCount++;
			QTText_GetNextChapterTime(theChapterTrack, &myTime);
		}
	}

	return(myCount);
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// HREF track utilities.
//
// Use these functions to manipulate HREF tracks in a movie.
//
// An HREF track is a text track that has a special name ("HREFTrack") and some of whose samples specify
// URLs that are loaded when the user clicks in the movie box when the text sample is active or when the
// text sample first loads.
//
///////////////////////////////////////////////////////////////////////////////////////////////////////////

//////////
//
// QTText_SetTextTrackAsHREFTrack
// Set the specified track to be or not to be an HREF track.
//
//////////

OSErr QTText_SetTextTrackAsHREFTrack (Track theTrack, Boolean isHREFTrack)
{
	OSErr		myErr = noErr;
	
	myErr = QTUtils_SetTrackName(theTrack, isHREFTrack ? kHREFTrackName : kNonHREFTrackName);

	return(myErr);
}


//////////
//
// QTText_IsHREFTrack
// Is the specified track an HREF track?
//
// For the moment, we are content to count a track as an HREF track if its name is "HREFTrack";
// a more thorough test would be to look through the text samples for some actual URLs. This is
// left as an exercise for the reader.
//
//////////

Boolean QTText_IsHREFTrack (Track theTrack)
{
	Boolean		isHREFTrack = false;
	char 		*myTrackName = NULL;
	
	myTrackName = QTUtils_GetTrackName(theTrack);
	if (myTrackName != NULL)
		isHREFTrack = (strcmp(myTrackName, kHREFTrackName) == 0);
	
	free(myTrackName);
	return(isHREFTrack);
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Miscellaneous utilities.
//
///////////////////////////////////////////////////////////////////////////////////////////////////////////

//////////
//
// QTText_CopyCStringToPascal
// Convert the source C string to a destination Pascal string as it's copied.
//
// The destination string will be truncated to fit into a Str255 if necessary.
// If the C string pointer is NULL, the Pascal string's length is set to zero.
//
// This routine is borrowed from CGlue.c, by Nick Kledzik.
//
//////////

void QTText_CopyCStringToPascal (const char *theSrc, Str255 theDst)
{
	short					myLength = 0;
	
	// handle case of overlapping strings
	if ((void *)theSrc == (void *)theDst) {
		unsigned char		*myCurDst = &theDst[1];
		unsigned char		myChar;
				
		myChar = *(const unsigned char *)theSrc++;
		while (myChar != '\0') {
			unsigned char	myNextChar;
			
			// use myNextChar so we don't overwrite what we are about to read
			myNextChar = *(const unsigned char *)theSrc++;
			*myCurDst++ = myChar;
			myChar = myNextChar;
			
			if (++myLength >= 255)
				break;
		}
	} else if (theSrc != NULL) {
		unsigned char		*myCurDst = &theDst[1];
		short 				myOverflow = 255;		// count down, so test it loop is faster
		register char		myTemp;
	
		// we can't do the K&R C thing of �while (*s++ = *t++)� because it will copy the trailing zero,
		// which might overrun the Pascal buffer; instead, we use a temp variable
		while ((myTemp = *theSrc++) != 0) {
			*(char *)myCurDst++ = myTemp;
				
			if (--myOverflow <= 0)
				break;
		}
		myLength = 255 - myOverflow;
	}
	
	// set the length of the destination Pascal string
	theDst[0] = myLength;
}