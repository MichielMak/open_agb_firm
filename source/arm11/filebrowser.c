/*
 *   This file is part of open_agb_firm
 *   Copyright (C) 2021 derrek, profi200
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdlib.h>
#include <string.h>
#include "types.h"
#include "error_codes.h"
#include "fs.h"
#include "util.h"
#include "arm11/drivers/hid.h"
#include "arm11/fmt.h"
#include "drivers/gfx.h"


// Notes on these settings:
// These are hard caps acting as a memory backstop only. scanDir() pre-counts the
// directory and allocates exactly what is needed up to these limits, so a typical
// folder uses far less memory than the worst case. The caps are sized to comfortably
// hold a few thousand entries (e.g. a full untrimmed regional GBA set).
// MAX_ENT_BUF_SIZE should be big enough to hold the average file/dir name length * MAX_DIR_ENTRIES.
#define MAX_ENT_BUF_SIZE  (1024u * 1960) // ~1.91 MiB. Proportional to MAX_DIR_ENTRIES.
#define MAX_DIR_ENTRIES   (10000u)
#define DIR_READ_BLOCKS   (10u)
#define SCREEN_COLS       (53u - 1) // - 1 because the console inserts a newline after the last line otherwise.
#define SCREEN_ROWS       (24u)

#define ENT_TYPE_FILE  (0)
#define ENT_TYPE_DIR   (1)


typedef struct
{
	u32 num;        // Total number of entries in the list.
	u32 hidden;     // Number of matching entries dropped because a cap was hit.
	char *entBuf;   // Format: char entryType; char name[X]; // null terminated. Heap allocated.
	char **ptrs;    // For fast sorting. Each entry points into entBuf. Heap allocated.
} DirList;



int dlistCompare(const void *a, const void *b)
{
	const char *entA = *(char**)a;
	const char *entB = *(char**)b;

	// Compare the entry type. Dirs have priority over files.
	if(*entA != *entB) return (int)*entB - *entA;

	// Compare the string.
	int res;
	do
	{
		res = *++entA - *++entB;
	} while(res == 0 && *entA != '\0' && *entB != '\0');

	return res;
}

// Returns whether a directory entry should be included in the list.
// Directories are always kept; files must match the filter (suffix) and not be hidden.
static bool keepEntry(const FILINFO *const fi, const char *const filter, const u32 filterLen)
{
	if(fi->fattrib & AM_DIR) return true;

	const u32 nameLen = strlen(fi->fname);
	if(nameLen <= filterLen || strcmp(filter, fi->fname + nameLen - filterLen) != 0
	   || fi->fname[0] == '.')
		return false;

	return true;
}

static Result scanDir(const char *const path, DirList *const dList, const char *const filter)
{
	// Release any previous result so this function can be called repeatedly.
	free(dList->entBuf);
	free(dList->ptrs);
	dList->entBuf = NULL;
	dList->ptrs   = NULL;
	dList->num    = 0;
	dList->hidden = 0;

	FILINFO *const fis = (FILINFO*)malloc(sizeof(FILINFO) * DIR_READ_BLOCKS);
	if(fis == NULL) return RES_OUT_OF_MEM;

	const u32 filterLen = strlen(filter);

	// Pass 1: pre-count matching entries and the buffer size they need.
	Result res;
	DHandle dh;
	u32 wantEntries = 0; // Number of matching entries in the directory.
	u64 wantBytes   = 0; // Bytes needed to store them (entry type + name + NULL each).
	if((res = fOpenDir(&dh, path)) != RES_OK)
	{
		free(fis);
		return res;
	}
	{
		u32 read;
		do
		{
			if((res = fReadDir(dh, fis, DIR_READ_BLOCKS, &read)) != RES_OK)
			{
				fCloseDir(dh);
				free(fis);
				return res;
			}

			for(u32 i = 0; i < read; i++)
			{
				if(!keepEntry(&fis[i], filter, filterLen)) continue;
				wantEntries++;
				wantBytes += strlen(fis[i].fname) + 2;
			}
		} while(read == DIR_READ_BLOCKS);
	}
	fCloseDir(dh);

	// Nothing to list (empty or fully filtered directory).
	if(wantEntries == 0)
	{
		free(fis);
		return RES_OK;
	}

	// Clamp to the hard caps (memory backstop).
	const u32 maxEntries = (wantEntries > MAX_DIR_ENTRIES ? MAX_DIR_ENTRIES : wantEntries);
	const u32 bufSize    = (wantBytes > MAX_ENT_BUF_SIZE ? MAX_ENT_BUF_SIZE : (u32)wantBytes);

	// Single up-front allocation. ptrs[] point into entBuf so entBuf must never move.
	char *const entBuf = (char*)malloc(bufSize);
	char **const ptrs  = (char**)malloc(sizeof(char*) * maxEntries);
	if(entBuf == NULL || ptrs == NULL)
	{
		free(entBuf);
		free(ptrs);
		free(fis);
		return RES_OUT_OF_MEM;
	}

	// Pass 2: fill the freshly allocated buffers.
	if((res = fOpenDir(&dh, path)) != RES_OK)
	{
		free(entBuf);
		free(ptrs);
		free(fis);
		return res;
	}

	u32 numEntries = 0; // Total number of stored entries.
	u32 entBufPos  = 0; // Entry buffer position/number of bytes used.
	{
		u32 read;
		do
		{
			if((res = fReadDir(dh, fis, DIR_READ_BLOCKS, &read)) != RES_OK) break;

			for(u32 i = 0; i < read; i++)
			{
				if(!keepEntry(&fis[i], filter, filterLen)) continue;

				const u32 nameLen = strlen(fis[i].fname);
				// Stop if either cap is reached. Remaining matches are reported as hidden.
				if(numEntries >= maxEntries || entBufPos + nameLen + 2 > bufSize) goto scanEnd;

				const char entType = (fis[i].fattrib & AM_DIR ? ENT_TYPE_DIR : ENT_TYPE_FILE);
				char *const entry = &entBuf[entBufPos];
				*entry = entType;
				safeStrcpy(&entry[1], fis[i].fname, 256);
				ptrs[numEntries++] = entry;
				entBufPos += nameLen + 2;
			}
		} while(read == DIR_READ_BLOCKS);
	}

scanEnd:
	fCloseDir(dh);
	free(fis);

	dList->entBuf = entBuf;
	dList->ptrs   = ptrs;
	dList->num    = numEntries;
	dList->hidden = wantEntries - numEntries;

	qsort(dList->ptrs, dList->num, sizeof(char*), dlistCompare);

	return res;
}

// Shows a full-screen warning when a directory was too large to list fully and
// waits for a key press. The console is a fixed 53x24 grid with no spare rows
// for a persistent footer, so a modal warning is used instead.
static void showTruncationWarning(const u32 hidden)
{
	ee_printf("\x1b[2J\x1b[0;0H"
	          "\x1b[31;1mList truncated!\n\n"
	          "\x1b[37m%lu file(s) in this folder are hidden\n"
	          "because it exceeds the %lu entry limit.\n\n"
	          "Press any button to continue.",
	          hidden, (u32)MAX_DIR_ENTRIES);
	GFX_flushBuffers();

	u32 kDown;
	do
	{
		GFX_waitForVBlank0();

		hidScanInput();
		if(hidGetExtraKeys(0) & (KEY_POWER_HELD | KEY_POWER)) return;
		kDown = hidKeysDown();
	} while(kDown == 0);
}

static void showDirList(const DirList *const dList, u32 start)
{
	// Clear screen.
	ee_printf("\x1b[2J");

	const u32 listLength = (dList->num - start > SCREEN_ROWS ? start + SCREEN_ROWS : dList->num);
	for(u32 i = start; i < listLength; i++)
	{
		const char *const printStr =
			(*dList->ptrs[i] == ENT_TYPE_FILE ? "\x1b[%lu;H\x1b[37;1m %.52s" : "\x1b[%lu;H\x1b[33;1m %.52s");

		ee_printf(printStr, i - start + 1, &dList->ptrs[i][1]);
	}
}

Result browseFiles(const char *const basePath, char selected[512])
{
	if(basePath == NULL || selected == NULL) return RES_INVALID_ARG;
	// TODO: Check if the base path is empty.

	char *curDir = (char*)malloc(512);
	if(curDir == NULL) return RES_OUT_OF_MEM;
	safeStrcpy(curDir, basePath, 512);

	DirList *const dList = (DirList*)malloc(sizeof(DirList));
	if(dList == NULL) return RES_OUT_OF_MEM;
	dList->num    = 0;
	dList->hidden = 0;
	dList->entBuf = NULL;
	dList->ptrs   = NULL;

	Result res;
	if((res = scanDir(curDir, dList, ".gba")) != RES_OK) goto end;
	if(dList->hidden != 0) showTruncationWarning(dList->hidden);
	showDirList(dList, 0);

	s32 cursorPos = 0; // Within the entire list.
	u32 windowPos = 0; // Window start position within the list.
	s32 oldCursorPos = 0;
	while(1)
	{
		ee_printf("\x1b[%lu;H ", oldCursorPos - windowPos + 1);      // Clear old cursor.
		ee_printf("\x1b[%lu;H\x1b[37m>", cursorPos - windowPos + 1); // Draw cursor.
		GFX_flushBuffers();

		u32 kDown;
		do
		{
			GFX_waitForVBlank0();

			hidScanInput();
			if(hidGetExtraKeys(0) & (KEY_POWER_HELD | KEY_POWER)) goto end;
			kDown = hidKeysDown();
		} while(kDown == 0);

		const u32 num = dList->num;
		if(num != 0)
		{
			oldCursorPos = cursorPos;
			if(kDown & KEY_DRIGHT)
			{
				cursorPos += SCREEN_ROWS;
				if((u32)cursorPos > num) cursorPos = num - 1;
			}
			if(kDown & KEY_DLEFT)
			{
				cursorPos -= SCREEN_ROWS;
				if(cursorPos < -1) cursorPos = 0;
			}
			if(kDown & KEY_DUP)    cursorPos -= 1;
			if(kDown & KEY_DDOWN)  cursorPos += 1;
		}

		if(cursorPos < 0)              cursorPos = num - 1; // Wrap to end of list.
		if((u32)cursorPos > (num - 1)) cursorPos = 0;       // Wrap to start of list.

		if((u32)cursorPos < windowPos)
		{
			windowPos = cursorPos;
			showDirList(dList, windowPos);
		}
		if((u32)cursorPos >= windowPos + SCREEN_ROWS)
		{
			windowPos = cursorPos - (SCREEN_ROWS - 1);
			showDirList(dList, windowPos);
		}

		if(kDown & (KEY_A | KEY_B))
		{
			u32 pathLen = strlen(curDir);

			if(kDown & KEY_A && num != 0)
			{
				// TODO: !!! Insecure !!!
				if(curDir[pathLen - 1] != '/') curDir[pathLen++] = '/';
				safeStrcpy(curDir + pathLen, &dList->ptrs[cursorPos][1], 256);

				if(*dList->ptrs[cursorPos] == ENT_TYPE_FILE)
				{
					safeStrcpy(selected, curDir, 512);
					break;
				}
			}
			if(kDown & KEY_B)
			{
				char *tmpPathPtr = curDir + pathLen;
				while(*--tmpPathPtr != '/');
				if(*(tmpPathPtr - 1) == ':') tmpPathPtr++;
				*tmpPathPtr = '\0';
			}

			if((res = scanDir(curDir, dList, ".gba")) != RES_OK) break;
			if(dList->hidden != 0) showTruncationWarning(dList->hidden);
			cursorPos = 0;
			windowPos = 0;
			showDirList(dList, 0);
		}
	}

end:
	free(dList->entBuf);
	free(dList->ptrs);
	free(dList);
	free(curDir);

	// Clear screen.
	ee_printf("\x1b[2J");

	return res;
}