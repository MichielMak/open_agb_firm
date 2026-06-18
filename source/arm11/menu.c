/*
 *   This file is part of open_agb_firm
 *   Copyright (C) 2024 profi200
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

#include <math.h>
#include "types.h"
#include "arm11/config.h"
#include "arm11/open_agb_firm.h"
#include "arm11/oaf_video.h"
#include "arm11/fmt.h"
#include "arm11/drivers/hid.h"
#include "arm11/drivers/mcu.h"
#include "arm11/drivers/codec.h"
#include "arm11/drivers/lgy11.h"
#include "arm11/console.h"
#include "drivers/gfx.h"


#define NUM_ITEMS           (7u)

#define MENU_ROW0           (6u)                       // First item row (1-indexed).
#define STATUS_ROW          (MENU_ROW0 + NUM_ITEMS + 1u)

#define ITEM_BACKLIGHT      (0u)
#define ITEM_COLOR_PROFILE  (1u)
#define ITEM_CONTRAST       (2u)
#define ITEM_BRIGHTNESS     (3u)
#define ITEM_SATURATION     (4u)
#define ITEM_VOLUME         (5u)
#define ITEM_AUDIO_OUT      (6u)

static const char *const g_colorProfileNames[9] =
{
	"None", "GBA", "GB Micro", "GBA SP (AGS-101)",
	"NDS", "NDS Lite", "Switch Online", "VBA/No$GBA", "Identity"
};

static const char *const g_audioOutNames[3] =
{
	"Auto", "Speakers", "Headphones"
};

// ee_printf has no %f support; format a float as "±X.XX" using integer math.
static void fmtFloat(char *const buf, const float val)
{
	s32 scaled = (s32)lroundf(val * 100.f);
	const bool neg = scaled < 0;
	if(neg) scaled = -scaled;
	ee_sprintf(buf, "%s%d.%02d", neg ? "-" : "", (int)(scaled / 100), (int)(scaled % 100));
}

static void drawMenu(const u32 cursor, const u32 oldCursor, const bool fullRedraw)
{
	char valBuf[16];

	if(fullRedraw)
	{
		consoleClear();
		ee_puts("\x1b[37;1m== open_agb_firm Settings ==\x1b[0m\n");
		ee_puts("Up/Down: select   Left/Right: adjust\n");
		ee_puts("Start: save to config.ini   B: close\n");
	}

	// Erase the previous cursor marker (column 1 of that row).
	ee_printf("\x1b[%u;H ", oldCursor + MENU_ROW0);

	for(u32 i = 0; i < NUM_ITEMS; i++)
	{
		// On a partial redraw only repaint rows that changed.
		if(!fullRedraw && i != cursor && i != oldCursor) continue;

		const char *label;
		char valStr[40];
		bool noteRestart = false;

		switch(i)
		{
			case ITEM_BACKLIGHT:
				label = "Backlight";
				ee_sprintf(valStr, "%u", (unsigned)g_oafConfig.backlight);
				break;

			case ITEM_COLOR_PROFILE:
				label = "Color Profile";
				ee_sprintf(valStr, "%s", g_colorProfileNames[g_oafConfig.colorProfile]);
				// Switching to/from None changes the capture pipeline — requires restart.
				noteRestart = !OAF_colorCorrectionActive() || g_oafConfig.colorProfile == 0;
				break;

			case ITEM_CONTRAST:
				label = "Contrast";
				fmtFloat(valBuf, g_oafConfig.contrast);
				if(OAF_colorCorrectionActive())
					ee_sprintf(valStr, "%s", valBuf);
				else
					ee_sprintf(valStr, "%s (need color profile)", valBuf);
				break;

			case ITEM_BRIGHTNESS:
				label = "Brightness";
				fmtFloat(valBuf, g_oafConfig.brightness);
				if(OAF_colorCorrectionActive())
					ee_sprintf(valStr, "%s", valBuf);
				else
					ee_sprintf(valStr, "%s (need color profile)", valBuf);
				break;

			case ITEM_SATURATION:
				label = "Saturation";
				fmtFloat(valBuf, g_oafConfig.saturation);
				if(OAF_colorCorrectionActive())
					ee_sprintf(valStr, "%s", valBuf);
				else
					ee_sprintf(valStr, "%s (need color profile)", valBuf);
				break;

			case ITEM_VOLUME:
				label = "Volume";
				if(g_oafConfig.volume > 48)
					ee_sprintf(valStr, "Slider");
				else if(g_oafConfig.volume == -128)
					ee_sprintf(valStr, "Muted");
				else
					ee_sprintf(valStr, "%d", (int)g_oafConfig.volume);
				break;

			case ITEM_AUDIO_OUT:
				label = "Audio Output";
				ee_sprintf(valStr, "%s", g_audioOutNames[g_oafConfig.audioOut]);
				break;

			default:
				label = "";
				valStr[0] = '\0';
				break;
		}

		ee_printf("\x1b[%u;H  \x1b[37;1m%-18s\x1b[0m %s%s\x1b[K",
		          i + MENU_ROW0, label, valStr,
		          noteRestart ? " \x1b[33;1m(restart to apply)\x1b[0m" : "");
	}

	// Draw the cursor marker at column 1 of the selected row.
	ee_printf("\x1b[%u;H\x1b[37m>", cursor + MENU_ROW0);
	GFX_flushBuffers();
}

static void adjustItem(const u32 item, const s32 delta)
{
	switch(item)
	{
		case ITEM_BACKLIGHT:
			changeBacklight((s16)((s32)g_oafConfig.backlightSteps * delta));
			break;

		case ITEM_COLOR_PROFILE:
		{
			s32 v = (s32)g_oafConfig.colorProfile + delta;
			if(v < 0) v = 8;
			if(v > 8) v = 0;
			g_oafConfig.colorProfile = (u8)v;
			// Live preview only when the LUT pipeline was initialised at boot
			// and we're selecting a non-zero profile.
			if(OAF_colorCorrectionActive())
				OAF_updateColorLut();
			break;
		}

		case ITEM_CONTRAST:
		{
			float v = g_oafConfig.contrast + (float)delta * 0.05f;
			if(v < 0.f) v = 0.f;
			if(v > 3.f) v = 3.f;
			g_oafConfig.contrast = v;
			if(OAF_colorCorrectionActive())
				OAF_updateColorLut();
			break;
		}

		case ITEM_BRIGHTNESS:
		{
			float v = g_oafConfig.brightness + (float)delta * 0.05f;
			if(v < -1.f) v = -1.f;
			if(v >  1.f) v =  1.f;
			g_oafConfig.brightness = v;
			if(OAF_colorCorrectionActive())
				OAF_updateColorLut();
			break;
		}

		case ITEM_SATURATION:
		{
			float v = g_oafConfig.saturation + (float)delta * 0.05f;
			if(v < 0.f) v = 0.f;
			if(v > 3.f) v = 3.f;
			g_oafConfig.saturation = v;
			if(OAF_colorCorrectionActive())
				OAF_updateColorLut();
			break;
		}

		case ITEM_VOLUME:
		{
			// Steps of 4; skip the undefined range -19 to 48 by snapping across it.
			s32 v = (s32)g_oafConfig.volume + delta * 4;
			if(delta > 0 && g_oafConfig.volume <= -20 && v >= -19) v = 49;
			if(delta < 0 && g_oafConfig.volume >= 49  && v <=  48) v = -20;
			if(v < -128) v = -128;
			if(v >  127) v =  127;
			g_oafConfig.volume = (s8)v;
			CODEC_setVolumeOverride(g_oafConfig.volume);
			break;
		}

		case ITEM_AUDIO_OUT:
		{
			s32 v = (s32)g_oafConfig.audioOut + delta;
			if(v < 0) v = 2;
			if(v > 2) v = 0;
			g_oafConfig.audioOut = (u8)v;
			CODEC_setAudioOutput((CdcAudioOut)g_oafConfig.audioOut);
			break;
		}

		default:
			break;
	}
}

void showSettingsMenu(void)
{
	// Freeze GBA input so D-Pad/A/B used for menu navigation
	// don't reach the running game via hardware passthrough.
	LGY11_selectInput(0x3FFu);
	LGY11_setInputState(0);

	// Show the bottom screen (hidden during gameplay in release builds).
#ifdef NDEBUG
	GFX_setForceBlack(false, false);
	if(MCU_getSystemModel() != SYS_MODEL_2DS)
		GFX_powerOnBacklight(GFX_BL_BOT);
#endif

	u32 cursor    = 0;
	u32 oldCursor = 0;
	drawMenu(cursor, oldCursor, true);

	while(1)
	{
		u32 kDown;
		do
		{
			GFX_waitForVBlank0();
			hidScanInput();
			if(hidGetExtraKeys(0) & (KEY_POWER_HELD | KEY_POWER)) goto done;
			kDown = hidKeysDown();
		} while(kDown == 0);

		oldCursor = cursor;

		if(kDown & KEY_DUP)
		{
			cursor = (cursor > 0) ? cursor - 1u : NUM_ITEMS - 1u;
		}
		else if(kDown & KEY_DDOWN)
		{
			cursor = (cursor < NUM_ITEMS - 1u) ? cursor + 1u : 0;
		}
		else if(kDown & KEY_DLEFT)
		{
			adjustItem(cursor, -1);
		}
		else if(kDown & KEY_DRIGHT)
		{
			adjustItem(cursor, +1);
		}
		else if(kDown & KEY_START)
		{
			// Persist the current settings to the global config.
			const Result res = writeOafConfig(OAF_WORK_DIR "/config.ini", &g_oafConfig);
			if(res == RES_OK)
				ee_printf("\x1b[%u;H\x1b[32;1mSaved to config.ini\x1b[0m\x1b[K", STATUS_ROW);
			else
				ee_printf("\x1b[%u;H\x1b[31;1mSave failed (%d)\x1b[0m\x1b[K", STATUS_ROW, (int)res);
			GFX_flushBuffers();
			continue;
		}
		else if(kDown & KEY_B)
		{
			break;
		}
		else if(hidKeysHeld() == (KEY_X | KEY_SELECT))
		{
			// Same combo that opened the menu closes it.
			break;
		}

		drawMenu(cursor, oldCursor, false);
	}

done:
	// Restore bottom screen to normal state.
#ifdef NDEBUG
	GFX_setForceBlack(false, true);
	if(MCU_getSystemModel() != SYS_MODEL_2DS)
		GFX_powerOffBacklight(GFX_BL_BOT);
#endif
	consoleClear();

	// Restore real GBA input (passthrough for unmapped buttons,
	// override for any user-configured button remaps).
	LGY11_selectInput(oafGetButtonOverrides());
	LGY11_setInputState(0);
}
