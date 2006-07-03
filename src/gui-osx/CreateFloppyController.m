/*
  Hatari - CreateFloppyController.m

  This file is distributed under the GNU Public License, version 2 or at
  your option any later version. Read the file gpl.txt for details.

  Create floppy image window controller implementation file

  Feb-Mar 2006, Sébastien Molines - Created
*/

#import "CreateFloppyController.h"
#import "Shared.h"

#include "main.h"
#include "configuration.h"
#include "createBlankImage.h"

@implementation CreateFloppyController

- (IBAction)createFloppyImage:(id)sender
{
	// Create a SavePanel
	NSSavePanel *savePanel = [NSSavePanel savePanel];

	// Set its allowed file types
	NSArray* allowedFileTypes = [NSArray arrayWithObjects: @"st", @"msa", @"dim", @"gz", nil];
	[savePanel setAllowedFileTypes:allowedFileTypes];
	
	// Get the default images directory
	NSString* defaultDir = [NSString stringWithCString:ConfigureParams.DiskImage.szDiskImageDirectory];

	// Run the SavePanel, then check if the user clicked OK
    if ( NSOKButton == [savePanel runModalForDirectory:defaultDir file:nil] )
	{
		// Get the path to the chosen file
		NSString *path = [savePanel filename];
	
		// Make a non-const C string out of it
		const char* constSzPath = [path cString];
		size_t cbPath = strlen(constSzPath) + 1;
		char szPath[cbPath];
		strncpy(szPath, constSzPath, cbPath);
					
		// Create the image
		CreateBlankImage_CreateFile(szPath, [tracks intValue], [sectors intValue], [sides intValue]);
	}
}

- (void)awakeFromNib
{
	// Fill the "Tracks" dropdown
    [tracks removeAllItems];
	int i;
	for (i = 40; i <= 85; i++)
	{
		[tracks addItemWithTitle:[NSString stringWithFormat:@"%d", i]];	
		[[tracks lastItem] setTag:i];
	}
	
	// Select the default value of 80 tracks
	[tracks selectItemAtIndex:[tracks indexOfItemWithTag:80]]; // Equivalent to Tiger-only [tracks selectItemWithTag:80];


}

- (IBAction)runModal:(id)sender
{
	[[ModalWrapper alloc] runModal:window];
}


@end
