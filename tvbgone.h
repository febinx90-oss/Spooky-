/* ____________________________
   TV-B-Gone idle blaster for nRFBox
   IR LED: GPIO12 -> 100ohm -> IR LED -> GND
   Fires all power-off codes after 5s idle on main menu.
   Any button press stops it.
   ________________________________________ */

#ifndef TVBGONE_H
#define TVBGONE_H

namespace TvBGone {
  void tvbgoneInit();        // call once in setup()
  void tvbgoneTick();        // call every loop() when current_screen == 0
  void tvbgoneButtonHit();   // call whenever any button is pressed
}

#endif
