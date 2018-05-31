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

#ifndef SMYLES_MOS_LIBS_WIFI_SRC_MGOS_WIFI_H_
#define SMYLES_MOS_LIBS_WIFI_SRC_MGOS_WIFI_H_

#include <stdbool.h>
#include "mgos_sys_config.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/*
 * Provision WiFi Callback Results
 */
struct mgos_provision_wifi_result {
  char ssid[33];
  bool success;
};

/*
 * Callback prototype for `mgos_wifi_scan()`, called when wifi scan is done.
 * `num_res` is a number of networks found, `res` is a pointer to the first
 * one. `arg` is an arbitrary pointer given to `mgos_wifi_scan()`.
 *
 * See `mgos_wifi_scan()` for more details.
 */
typedef void (*mgos_wifi_provision_cb_t)(struct mgos_provision_wifi_result *res, void *arg);

/*
 * Connect to the previously setup wifi station (with `mgos_wifi_setup_sta()`).
 */
bool mgos_provision_wifi_connect_sta(void);

/*
 * Disconnect from wifi station.
 */
bool mgos_provision_wifi_disconnect_sta(void);

/*
 * Setup station configuration (must be ran before mgos_priovision_wifi_connect())
 */
bool mgos_provision_wifi_setup_sta(const struct mgos_config_provision_wifi_sta *cfg);

/*
 * Run provision WiFi Testing (without using boot testing)
 */
void mgos_provision_wifi_run_test(void);

/*
 * Test configured WiFi credentials, when test is done, the provided callback
 * `cb` or NULL on error.
 * 
 * Use mgos_provision_wifi_run_test() to run test without a callback
 *
 * Caller owns results, they are not freed by the callee.
 *
 * A note for implementations: invoking inline is ok.
 */
void mgos_provision_wifi_test(mgos_wifi_provision_cb_t cb, void *arg);

/*
 * Copy test WiFi Provision values to wifi.sta
 */
bool mgos_provision_wifi_copy_sta_values(void);

/*
 * Clear test WiFi Provision values from provision.wifi.sta
 */
bool mgos_provision_wifi_clear_sta_values(void);

/*
 * Disable boot testing of WiFi Provision STA values ( sets provision.wifi.enable to false )
 */
bool mgos_provision_wifi_disable_boot_test(void);

/*
 * Enable boot testing of WiFi Provision STA values ( sets provision.wifi.enable to true )
 */
bool mgos_provision_wifi_enable_boot_test(void);

/*
 * Get last station test results (use this over pulling the config value, as method for handling this may change in future versions)
 */
bool mgos_provision_wifi_get_last_test_results(void);

/*
 * Check if a STA test is currently running or not
 */
bool mgos_provision_wifi_is_test_running(void);

/*
 * Return last station test SSID; the caller should free it.
 */
char *mgos_provision_wifi_get_last_test_ssid(void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* SMYLES_MOS_LIBS_WIFI_SRC_MGOS_WIFI_H_ */