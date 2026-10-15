#ifndef __MYLAUNCHER_H__
#define __MYLAUNCHER_H__

#include "include_case.h"

/* App resources live next to the .app on the SD card:
 *   /mnt/diska/apps/launcher/launcher.app
 *   /mnt/diska/apps/launcher/<style>/launcher.res|.sty|<lang>.str
 */

int launcher_scene(fusion_enter_e enter_type);

#endif /* __MYLAUNCHER_H__ */
