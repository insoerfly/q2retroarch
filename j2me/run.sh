#!/bin/sh
# J2ME launcher for the Q2/A13 (FreeJ2ME + SDL2 interface, ARM32).
# usage: sh /flash/j2me/run.sh [/path/to/game.jar]
D=/flash/j2me
export JAVA_HOME="$D/jdk"
export PATH="$JAVA_HOME/bin:$PATH"
export LD_LIBRARY_PATH="$D/lib:$JAVA_HOME/lib:$LD_LIBRARY_PATH"
export JAVA_TOOL_OPTIONS="-Xverify:none -Djava.awt.headless=true -Dsun.jnu.encoding=UTF-8 -Dfile.encoding=UTF-8 -Djava.library.path=$JAVA_HOME/lib"
export SDL_VIDEODRIVER="${SDL_VIDEODRIVER:-fbcon}"
export SDL_JOYSTICK_DEVICE=/dev/input/js0
cd "$D" || exit 1
GAME="$1"
[ -n "$GAME" ] || GAME="$D/edge.jar"
echo "J2ME: launching $GAME"
"$JAVA_HOME/bin/java" -jar "$JAVA_HOME/bin/freej2me-sdl.jar" "$GAME" 240 320 30 2>/dev/null \
  | "$D/sdl_interface" 240 320
