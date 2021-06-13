/**
 * Marlin 3D Printer Firmware
 * Copyright (c) 2020 MarlinFirmware [https://github.com/MarlinFirmware/Marlin]
 *
 * Based on Sprinter and grbl.
 * Copyright (c) 2011 Camiel Gubbels / Erik van der Zalm
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "../../../inc/MarlinConfigPre.h"

#if IS_DWIN_MARLINUI

#include "marlinui_dwin.h"
#include "../dwin_lcd.h"
#include "../dwin_string.h"

//#include "../../lcdprint.h"
#include "lcdprint_dwin.h"
#include "../../fontutils.h"
#include "../../../libs/numtostr.h"
#include "../../marlinui.h"

#include "../../../sd/cardreader.h"
#include "../../../module/motion.h"
#include "../../../module/temperature.h"
#include "../../../module/printcounter.h"

#if ENABLED(SDSUPPORT)
  #include "../../../libs/duration_t.h"
#endif


#define S(V) (char*)(V)

//
// Before homing, blink '123' <-> '???'.
// Homed but unknown... '123' <-> '   '.
// Homed and known, display constantly.
//
FORCE_INLINE void _draw_axis_value(const AxisEnum axis, const char *value, const bool blink, uint16_t x, uint16_t y) {
  uint8_t vallen = utf8_strlen(value);

  dwin_string.set();
  dwin_string.add('X' + axis);
  DWIN_Draw_String(false, true, font16x32, Color_IconBlue, Color_Bg_Black, x + (vallen * 14 / 2 ) - 7, y + 2, S(dwin_string.string()));

  dwin_string.set();
  if(blink) dwin_string.add(value);
  else {
    if (!TEST(axis_homed, axis))
      while (const char c = *value++) dwin_string.add(c <= '.' ? c : '?');
    else {
      #if NONE(HOME_AFTER_DEACTIVATE, DISABLE_REDUCED_ACCURACY_WARNING)
        if (!TEST(axis_trusted, axis))
          #ifdef DWIN_MARLINUI_PORTRAIT
          dwin_string.add(axis == Z_AXIS ? PSTR("       ") : PSTR("    "));
          #else
          dwin_string.add(PSTR("       "));
          #endif
        else
      #endif
          dwin_string.add(value);
    }
  }
  DWIN_Draw_String(false, true, font14x28, Color_White, Color_Bg_Black, x, y + 32, (char*)dwin_string.string());
}

FORCE_INLINE void _draw_fan_status(uint16_t x, uint16_t y) {
  dwin_string.set();
  dwin_string.add(i8tostr3rj(thermalManager.scaledFanSpeedPercent(0)));
  dwin_string.add('%');
  const bool animate = thermalManager.scaledFanSpeedPercent(0) > 0;

  DWIN_ICON_Animation(0, animate, ICON, ICON_Fan0, ICON_Fan3, x + 15, y + 28, 25);
  if (animate)
    DWIN_Draw_String(false, true, font14x28, Color_White, Color_Bg_Black, x, y + 70, (char*)dwin_string.string());
  else {
    DWIN_ICON_Show(ICON, ICON_Fan0, x + 15, y + 28);
    dwin_string.set(PSTR("    "));
    DWIN_Draw_String(false, true, font14x28, Color_White, Color_Bg_Black, x, y + 70, S(dwin_string.string()));
  }
}

FORCE_INLINE void _draw_heater_status(const heater_id_t heater, uint16_t x, uint16_t y) {
  #if HAS_HEATED_BED
    const bool isBed = heater < 0;
    const float t1 = (isBed ? thermalManager.degBed()       : thermalManager.degHotend(heater)),
                t2 = (isBed ? thermalManager.degTargetBed() : thermalManager.degTargetHotend(heater));
    const uint8_t isActive = isBed ? thermalManager.isHeatingBed() : thermalManager.isHeatingHotend(heater);
  #else
    const bool isBed = false;
    const float t1 = thermalManager.degHotend(heater), t2 = thermalManager.degTargetHotend(heater);
    const uint8_t isActive = thermalManager.isHeatingHotend(heater);
  #endif


  dwin_string.set();
  dwin_string.add(i16tostr3rj(t2 + 0.5));
  dwin_string.add(LCD_STR_DEGREE);
  DWIN_Draw_String(false, true, font14x28, Color_White, Color_Bg_Black, x, y, (char*)dwin_string.string());

  DWIN_ICON_Show(ICON, (isBed ? ICON_BedOff : ICON_HotendOff) + isActive, x, y + 30);


  dwin_string.set();
  dwin_string.add(i16tostr3rj(t1 + 0.5));
  dwin_string.add(LCD_STR_DEGREE);
  DWIN_Draw_String(false, true, font14x28, Color_White, Color_Bg_Black, x, y + 70, (char*)dwin_string.string());
}

FORCE_INLINE void _draw_feedrate_status(const char *value, uint16_t x, uint16_t y) {
  //DWIN_ICON_Show(ICON, ICON_Setspeed, x, y + 4);

  //dwin_string.set(PSTR(">>"));
  dwin_string.set(LCD_STR_FEEDRATE);
  DWIN_Draw_String(false, true, font14x28, Color_IconBlue, Color_Bg_Black, x, y, S(dwin_string.string()));

  dwin_string.set();
  dwin_string.add(value);
  dwin_string.add(PSTR("%"));
  DWIN_Draw_String(false, true, font14x28, Color_White, Color_Bg_Black, x + 14, y, (char*)dwin_string.string());
}

void MarlinUI::draw_status_screen() {
  const bool blink = get_blink();
  // Logo/Status Icon
  DWIN_ICON_Show(ICON,ICON_LOGO, (LCD_PIXEL_WIDTH / 2) - (130 / 2),15);

  _draw_heater_status(H_E0, 15, 60);

  // Hotend 1 / Bed Temp
  #if HAS_MULTI_HOTEND
    _draw_header_status(H_E1, 85, 60);
  #else
    _draw_heater_status(H_BED, 85, 60);
  #endif

  // Fan display
  #if HAS_FAN
    _draw_fan_status(175, 60);
  #endif

  // Draw a frame around the x/y/z values
  #ifdef DWIN_MARLINUI_PORTRAIT
  DWIN_Draw_Rectangle(0, Select_Color, 0, 163, 272, 230);
  #else
  DWIN_Draw_Rectangle(0, Select_Color, LCD_PIXEL_WIDTH-106, 50, LCD_PIXEL_WIDTH-1, 230);
  #endif

  // Axis values
  // TODO: E instead of X/Y
  const xy_pos_t lpos = current_position.asLogical();
  #ifdef DWIN_MARLINUI_PORTRAIT
  _draw_axis_value(X_AXIS, ftostr4sign(lpos.x), blink, 5, 165);
  _draw_axis_value(Y_AXIS, ftostr4sign(lpos.y), blink, 95, 165);
  _draw_axis_value(Z_AXIS, ftostr52sp(LOGICAL_Z_POSITION(current_position.z)), blink, 165, 165);
  #else
  _draw_axis_value(X_AXIS, ftostr52sp(lpos.x), blink, LCD_PIXEL_WIDTH - 104, 52);
  _draw_axis_value(Y_AXIS, ftostr52sp(lpos.y), blink, LCD_PIXEL_WIDTH - 104, 111);
  _draw_axis_value(Z_AXIS, ftostr52sp(LOGICAL_Z_POSITION(current_position.z)), blink, LCD_PIXEL_WIDTH - 104, 169);
  #endif

  // Feedrate
  #ifdef DWIN_MARLINUI_PORTRAIT
  _draw_feedrate_status(i16tostr3rj(feedrate_percentage), 5, 250);
  #else
  _draw_feedrate_status(i16tostr3rj(feedrate_percentage), 292, 60);
  #endif

  // Elapsed time
  // Portrait mode only shows one value at a time, and will rotate if ROTATE_PROGRESS_DISPLAY
  #ifdef DWIN_MARLINUI_PORTRAIT
  dwin_string.set();
  char buffer[14];
  duration_t time;
  char prefix = ' ';
  #ifdef SHOW_REMAINING_TIME
    #ifdef ROTATE_PROGRESS_DISPLAY
      if (blink && print_job_timer.isRunning()) {
        time = get_remaining_time();
        prefix = 'R';
      } else
    #else
      if (print_job_timer.isRunning()) {
        time = get_remaining_time();
        prefix = 'R';
      } else
    #endif
  #endif
        time = print_job_timer.duration();

  time.toDigital(buffer);
  dwin_string.add(prefix);
  dwin_string.add(buffer);
  DWIN_Draw_String(false, true, font14x28, Color_White, Color_Bg_Black, (LCD_PIXEL_WIDTH - ((dwin_string.length() + 1) * 14)), 250, (char*)dwin_string.string());
  #else
  // landscape mode shows both elapsed and remaining (if SHOW_REMAINING_TIME)
  // TODO: show E here if it's enabled and rotate with remaining???
  dwin_string.set();
  char buffer[14];
  duration_t time;
  dwin_string.set(" ");
  time = print_job_timer.duration();
  time.toDigital(buffer);
  dwin_string.add(buffer);
  DWIN_Draw_String(false, true, font14x28, Color_White, Color_Bg_Black, 270, 100, (char*)dwin_string.string());
    #ifdef SHOW_REMAINING_TIME
    time = get_remaining_time();
    dwin_string.set("R");
    time.toDigital(buffer);
    dwin_string.add(buffer);
    DWIN_Draw_String(false, true, font14x28, Color_White, Color_Bg_Black, 270, 135, (char*)dwin_string.string());
    #endif
  #endif

  // progress
  const progress_t progress = TERN(HAS_PRINT_PROGRESS_PERMYRIAD, get_progress_permyriad, get_progress_percent)();

  uint16_t pb_width = (LCD_PIXEL_WIDTH - 12 - 107) * (progress / PROGRESS_SCALE * 0.01f);
  #ifdef DWIN_MARLINUI_PORTRAIT
  DWIN_Draw_Rectangle(1, Color_Bg_Black, 5, 300, LCD_PIXEL_WIDTH - 5, 360); // 'clear' it first
  DWIN_Draw_Rectangle(0, Select_Color, 5, 300, LCD_PIXEL_WIDTH - 5, 360);
  DWIN_Draw_Rectangle(1, Select_Color, 6, 301, 6 + pb_width, 359);
  #else
  DWIN_Draw_Rectangle(1, Color_Bg_Black, 5, 170, LCD_PIXEL_WIDTH - 5 - 107, 230); // 'clear' it first
  DWIN_Draw_Rectangle(0, Select_Color, 5, 170, LCD_PIXEL_WIDTH - 5 - 107, 230);
  DWIN_Draw_Rectangle(1, Select_Color, 6, 171, 6 + pb_width, 229);
  #endif

  dwin_string.set();
  dwin_string.add(TERN(PRINT_PROGRESS_SHOW_DECIMALS, permyriadtostr4(progress), ui8tostr3rj(progress / (PROGRESS_SCALE))));
  dwin_string.add(PSTR("%"));
  DWIN_Draw_String(
    false,
    false,
    font16x32,
    Percent_Color,
    Color_Bg_Black,
    #ifdef DWIN_MARLINUI_PORTRAIT
    6 + ((LCD_PIXEL_WIDTH - 12)/2) - (dwin_string.length() * 16 / 2),
    300 + 12,
    #else
    6 + ((LCD_PIXEL_WIDTH - 12 - 107)/2) - (dwin_string.length() * 16 / 2),
    170 + 12,
    #endif
    (char*)dwin_string.string());

  draw_status_message(blink);
}

#endif
