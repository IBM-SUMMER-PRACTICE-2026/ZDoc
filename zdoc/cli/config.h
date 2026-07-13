#ifndef ZD_CONFIG_H
#define ZD_CONFIG_H

#include "options.h"

/*Applies ./zdoc.yaml on top of the defaults already in *o. 
  Missing file is not an error; malformed lines warn and are skipped
*/

void zd_config_load(zd_options *o);

#endif