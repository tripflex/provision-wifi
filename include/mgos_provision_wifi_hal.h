/*
 * Copyright (c) 2018 Myles McNamara
 * All rights reserved
 *
 * Licensed under the Apache License, Version 2.0 (the ""License"");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an ""AS IS"" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef SMYLES_MOS_LIBS_WIFI_SRC_MGOS_WIFI_HAL_H_
#define SMYLES_MOS_LIBS_WIFI_SRC_MGOS_WIFI_HAL_H_

#include "mgos_net.h"
#include "mgos_provision_wifi.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

bool mgos_wifi_dev_sta_setup(const struct mgos_config_wifi_sta *cfg);
bool mgos_wifi_dev_sta_connect(void); /* To the previously _setup network. */
bool mgos_wifi_dev_sta_disconnect(void);
enum mgos_wifi_status mgos_wifi_dev_sta_get_status(void);

/* Invoke this when Wifi connection state changes. */
// void mgos_wifi_dev_on_change_cb(enum mgos_net_event ev);

void mgos_wifi_dev_init(void);
void mgos_wifi_dev_deinit(void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* SMYLES_MOS_LIBS_WIFI_SRC_MGOS_WIFI_HAL_H_ */