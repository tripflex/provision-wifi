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

#include "mgos_provision_wifi.h"
#include "mgos_provision_wifi_hal.h"

#include <stdbool.h>
#include <stdlib.h>

#include "common/cs_dbg.h"
// #include "common/queue.h"

#include "mgos.h"
#include "mgos_wifi.h"
#include "mgos_timers.h"
#include "mgos_sys_config.h"
#include "mgos_system.h"

#include "mgos_mongoose.h"
#include "mgos_net_hal.h"

#include "mongoose.h"

static mgos_wifi_provision_cb_t s_provision_wifi_test_cb = NULL;
static void *s_provision_wifi_test_cb_userdata = NULL;

static mgos_timer_id s_provision_wifi_timer_id = MGOS_INVALID_TIMER_ID;

static int s_provision_wifi_con_attempts = 0;
static bool b_provision_wifi_testing = false;
static bool s_sta_should_reconnect = false;
static bool b_sta_was_connected = false;

struct mgos_rlock_type *s_provision_wifi_lock = NULL;

static inline void wifi_lock(void) {
  mgos_rlock(s_provision_wifi_lock);
}

static inline void wifi_unlock(void) {
  mgos_runlock(s_provision_wifi_lock);
}

static bool mgos_provision_wifi_enable_net_cb();
static bool mgos_provision_wifi_disable_net_cb();

static void mgos_provision_wifi_set_last_test(bool last_test_results){
  // Disable Provision WiFi in configuration
  LOG(LL_INFO, ("Provision WiFi setting last test results to %d", last_test_results));
  mgos_sys_config_set_provision_wifi_results_success(last_test_results); // Set results
  mgos_sys_config_set_provision_wifi_results_ssid( mgos_sys_config_get_provision_wifi_sta_ssid() ); // Set SSID

  // TODO: add event triggers
  // mgos_event_trigger(MGOS_EVENT_PROVISION_WIFI_TEST_COMPLETE, &test_results); 

  if( s_provision_wifi_test_cb != NULL ){
    s_provision_wifi_test_cb( last_test_results, mgos_sys_config_get_provision_wifi_sta_ssid(), s_provision_wifi_test_cb_userdata );
  }

  char *err = NULL;
  if( ! save_cfg(&mgos_sys_config, &err) ){
    LOG(LL_ERROR, ("Provision WiFi Set Last Test Results, Save Config Error: %s", err) );
    free(err);
    return false;
  }

  return true;
}

static void mgos_provision_wifi_clear_values(void){
  mgos_clear_timer(s_provision_wifi_timer_id);
  s_provision_wifi_timer_id = MGOS_INVALID_TIMER_ID;
  b_provision_wifi_testing = false;
  s_provision_wifi_con_attempts = 0;
}

static void mgos_provision_wifi_connection_failed(void){

  LOG(LL_INFO, ("%s", "Provision WiFi STA Connection Failed!" ) );
  mgos_provision_wifi_clear_values();
  mgos_provision_wifi_disable_boot_test();

  mgos_provision_wifi_set_last_test( false ); // Must be run before clearing STA values (to set SSID)

  if( mgos_sys_config_get_provision_wifi_fail_clear() ){
    mgos_provision_wifi_clear_sta_values();
  }

  mgos_provision_wifi_disable_net_cb();

  if( mgos_sys_config_get_provision_wifi_fail_reboot() ){
    mgos_system_restart();
    return; // return to prevent attempting to reconnect sta as reboot will do that anyways
  }

  // We only want to attempt to reconnect if reboot on fail is false
  if( b_sta_was_connected && mgos_sys_config_get_provision_wifi_reconnect() ){
    mgos_wifi_disconnect();
    LOG(LL_INFO, ("%s", "Provision WiFi attempting previous STA connection!" ) );

    // AP will go down for a few seconds, while reinit wifi, but should be transparent to user
    mgos_wifi_setup((struct mgos_config_wifi *) mgos_sys_config_get_wifi());
  }

}

static void mgos_provision_wifi_connection_success(void){

  if( mgos_sys_config_get_provision_wifi_success_copy() ){
    mgos_provision_wifi_copy_sta_values();
  }

  mgos_provision_wifi_disable_net_cb();

  if( mgos_sys_config_get_provision_wifi_success_disconnect() ){
    LOG( LL_INFO, ("%s", "Provision WiFi Connection Success, Disconnecting...") );
    bool result = mgos_provision_wifi_disconnect_sta();

    // Successful disconnection
    if( result ){
      LOG( LL_INFO, ("%s", "Provision WiFi Connection Successfully Disconnected!") );
    }
  }

  mgos_provision_wifi_clear_values();

  mgos_provision_wifi_set_last_test( true ); // Must be ran before clearing values (to set SSID)

  // Disable testing credentials on boot
  mgos_provision_wifi_disable_boot_test();

  if( mgos_sys_config_get_provision_wifi_success_clear() ){
    mgos_provision_wifi_clear_sta_values();
  }

  if( mgos_sys_config_get_provision_wifi_success_reboot() ){
    mgos_system_restart();
  }
}

static void mgos_provision_wifi_sta_connect_timeout_timer_cb(void *arg) {
  LOG(LL_ERROR, ("%s", "Provision WiFi STA: Connect timeout"));
  mgos_provision_wifi_connection_failed();
  (void) arg;
}

static void mgos_provision_wifi_run_test_timer_cb(void *arg) {
  LOG(LL_ERROR, ("%s", "Provision WiFi STA: Boot Timeout Timer Called"));
  mgos_provision_wifi_run_test();
  (void) arg;
}

static void mgos_provision_wifi_net_cb(int ev, void *evd, void *arg) {
  // We only want to process events when we are testing
  if( ! b_provision_wifi_testing ){
    return;
  }

  char * connected_ssid = NULL;
  int i_provision_wifi_total_attempts = mgos_sys_config_get_provision_wifi_attempts();
  connected_ssid = mgos_wifi_get_connected_ssid();

  switch (ev) {
    case MGOS_NET_EV_DISCONNECTED:

      LOG(LL_INFO, ("Provision WiFi STA DISCONNECTED, Attempts %d, Max Attempt %d", s_provision_wifi_con_attempts, i_provision_wifi_total_attempts ));

      if ( s_provision_wifi_con_attempts >= i_provision_wifi_total_attempts ) {
        LOG(LL_ERROR, ("Provision WiFi STA FAILED after %d total attempts (Max of %d)", s_provision_wifi_con_attempts, i_provision_wifi_total_attempts ));
        mgos_provision_wifi_connection_failed();
      } else {
        // Reattempt connection as long as we haven't exceeded total attempts
        mgos_provision_wifi_connect_sta();
      }

      break;

    case MGOS_NET_EV_CONNECTING:
      // Increase connection attempts total
      s_provision_wifi_con_attempts++; 
      LOG(LL_INFO, ("Provision WiFi STA CONNECTING, Attempt %d of %d", s_provision_wifi_con_attempts, i_provision_wifi_total_attempts ));
      break;

    case MGOS_NET_EV_CONNECTED:
      LOG(LL_INFO, ("%s", "Provision WiFi STA CONNECTED"));
      break;
    case MGOS_NET_EV_IP_ACQUIRED:
      LOG(LL_INFO, ("%s", "Provision WiFi STA IP ACQUIRED"));
      break;
  }

  // CONNECTED or IP_ACQUIRED means STA is actively connected already
  if( ev == MGOS_NET_EV_CONNECTED || ev == MGOS_NET_EV_IP_ACQUIRED ){

      // We need to make sure we are connected to the SSID we are testing for
      LOG(LL_INFO, ("Provision WiFi STA CONNECTED to %s", connected_ssid ? connected_ssid : "UNKNOWN"));

      const char * testing_ssid = mgos_sys_config_get_provision_wifi_sta_ssid();

      if( connected_ssid != NULL && ( strcmp(connected_ssid, testing_ssid) == 0 ) ){
        LOG(LL_INFO, ("Provision WiFi STA Connected after %d attempts", s_provision_wifi_con_attempts));
        mgos_provision_wifi_connection_success();
      } else {
        LOG(LL_INFO, ("Provision WiFi STA Connected to %s", connected_ssid ));        
      }
  }

  // LOG(LL_INFO, ("Provision WiFi STA NET Callback SSID: %s", connected_ssid ? connected_ssid : "UNKNOWN"));

  if( connected_ssid != NULL ){
    free(connected_ssid);
  }

  (void) evd;
  (void) arg;
}

static bool mgos_provision_wifi_enable_net_cb(){
  return mgos_event_add_group_handler(MGOS_EVENT_GRP_NET, mgos_provision_wifi_net_cb, NULL);  
}

static bool mgos_provision_wifi_disable_net_cb(){
  return mgos_event_remove_group_handler(MGOS_EVENT_GRP_NET, mgos_provision_wifi_net_cb, NULL);
}

bool mgos_provision_wifi_is_test_running(void){
  return b_provision_wifi_testing;
}

bool mgos_provision_wifi_copy_sta_values(void){

  LOG(LL_INFO, ( "Provision WiFi Copy Test STA Values" ) );

  char *err_msg = NULL;
  const struct mgos_config_provision_wifi_sta *cfg = mgos_sys_config_get_provision_wifi_sta();

  // Validate configuration before attempting to copy
  if (!mgos_wifi_validate_sta_cfg( (struct mgos_config_wifi_sta *) cfg, &err_msg)) {
    LOG(LL_ERROR, ("Provision WiFi Copy STA Values, Config Error: %s", err_msg));
    free(err_msg);
    return false;
  }

  // TODO: Copy cfg to config instead of having to set each individual value
  // TODO: Support setting different station index
  mgos_sys_config_set_wifi_sta_anon_identity( cfg->anon_identity );
  mgos_sys_config_set_wifi_sta_ca_cert( cfg->ca_cert );
  mgos_sys_config_set_wifi_sta_cert( cfg->cert );
  mgos_sys_config_set_wifi_sta_dhcp_hostname( cfg->dhcp_hostname );
  mgos_sys_config_set_wifi_sta_enable( mgos_sys_config_get_provision_wifi_success_enable() );
  mgos_sys_config_set_wifi_sta_gw( cfg->gw );
  mgos_sys_config_set_wifi_sta_ip( cfg->ip );
  mgos_sys_config_set_wifi_sta_key( cfg->key );
  mgos_sys_config_set_wifi_sta_nameserver( cfg->nameserver );
  mgos_sys_config_set_wifi_sta_netmask( cfg->netmask );
  mgos_sys_config_set_wifi_sta_pass( cfg->pass );
  mgos_sys_config_set_wifi_sta_ssid( cfg->ssid );
  mgos_sys_config_set_wifi_sta_user( cfg->user );

  char *err = NULL;
  if( ! save_cfg(&mgos_sys_config, &err) ){
    LOG(LL_ERROR, ("Provision WiFi Copy STA Values, Save Config Error: %s", err) );
    free(err);
    return false;
  }

  return true;
}

bool mgos_provision_wifi_clear_sta_values(void){

  LOG(LL_INFO, ( "Provision WiFi Clear Test STA Values" ) );
  // TODO: Copy cfg to config instead of having to set each individual value
  // TODO: Support setting different station index
  mgos_sys_config_set_provision_wifi_sta_anon_identity( "" );
  mgos_sys_config_set_provision_wifi_sta_ca_cert( "" );
  mgos_sys_config_set_provision_wifi_sta_cert( "" );
  mgos_sys_config_set_provision_wifi_sta_dhcp_hostname( "" );
  mgos_sys_config_set_provision_wifi_sta_enable( true ); // Must always be set to true
  mgos_sys_config_set_provision_wifi_sta_gw( "" );
  mgos_sys_config_set_provision_wifi_sta_ip( "" );
  mgos_sys_config_set_provision_wifi_sta_key( "" );
  mgos_sys_config_set_provision_wifi_sta_nameserver( "" );
  mgos_sys_config_set_provision_wifi_sta_netmask( "" );
  mgos_sys_config_set_provision_wifi_sta_pass( "" );
  mgos_sys_config_set_provision_wifi_sta_ssid( "" );
  mgos_sys_config_set_provision_wifi_sta_user( "" );

  char *err = NULL;
  if( ! save_cfg(&mgos_sys_config, &err) ){
    LOG(LL_ERROR, ("Provision WiFi Clear Provision STA Values, Save Config Error: %s", err) );
    free(err);
    return false;
  }

  return true;
}

bool mgos_provision_wifi_disable_boot_test(void){
  // Disable Provision WiFi in configuration
  LOG(LL_INFO, ("Disabling Provision WiFi Testing on Boot"));
  mgos_sys_config_set_provision_wifi_boot_enable(false);

  char *err = NULL;
  if( ! save_cfg(&mgos_sys_config, &err) ){
    LOG(LL_ERROR, ("Provision WiFi Disable Boot Test, Save Config Error: %s", err) );
    free(err);
    return false;
  }

  return true;
}

bool mgos_provision_wifi_enable_boot_test(void){
  // Disable Provision WiFi in configuration
  LOG(LL_INFO, ("Enabling Provision WiFi Testing on Boot"));
  mgos_sys_config_set_provision_wifi_boot_enable(false);

  char *err = NULL;
  if( ! save_cfg(&mgos_sys_config, &err) ){
    LOG(LL_ERROR, ("Provision WiFi Enable Boot Test, Save Config Error: %s", err) );
    free(err);
    return false;
  }

  return true;
}

bool mgos_provision_wifi_setup_sta(const struct mgos_config_provision_wifi_sta *cfg) {
  char *err_msg = NULL;
  if (!mgos_wifi_validate_sta_cfg( (struct mgos_config_wifi_sta *) cfg, &err_msg)) {
    LOG(LL_ERROR, ("Provision WiFi STA Config Error: %s", err_msg));
    free(err_msg);
    return false;
  }
  wifi_lock();
  /* Note: even if the config didn't change, disconnect and
  * reconnect to re-init WiFi. */
  mgos_wifi_disconnect();
  bool ret = mgos_wifi_dev_sta_setup( (struct mgos_config_wifi_sta *) cfg);
  if (ret) {
    LOG(LL_INFO, ("Provision WiFi Setup STA: Connecting to %s", cfg->ssid));
    ret = mgos_provision_wifi_connect_sta();
  }

  wifi_unlock();
  return ret;
}

bool mgos_provision_wifi_disconnect_sta(void) {
  LOG(LL_INFO, ( "Provision WiFi DISCONNECT" ) );
  wifi_lock();
  s_sta_should_reconnect = false;
  // bool ret = mgos_wifi_dev_sta_disconnect();
  bool ret = mgos_wifi_disconnect(); // Use wifi lib to make sure it doesn't attempt to reconnect
  wifi_unlock();
  return ret;
}

bool mgos_provision_wifi_connect_sta(void) {
  wifi_lock();
  bool ret = mgos_wifi_dev_sta_connect();
  
  s_sta_should_reconnect = ret;

  LOG(LL_INFO, ( "Provision WiFi CONNECT, Should Reconnect %d", s_sta_should_reconnect ) );

  if (ret) {

    // mgos_wifi_dev_on_change_cb(MGOS_NET_EV_CONNECTING);
    mgos_provision_wifi_net_cb(MGOS_NET_EV_CONNECTING, NULL, NULL);
    
    int connect_timeout = mgos_sys_config_get_provision_wifi_timeout();

    // Add timer if not already set (but should be already set by mgos_provision_wifi_run_test() )
    if (connect_timeout > 0 && s_provision_wifi_timer_id == MGOS_INVALID_TIMER_ID) {
      s_provision_wifi_timer_id = mgos_set_timer(connect_timeout * 1000, 0, mgos_provision_wifi_sta_connect_timeout_timer_cb, NULL);
    }
  }

  wifi_unlock();
  return ret;
}

bool mgos_provision_wifi_get_last_test_results(void){
  return mgos_sys_config_get_provision_wifi_results_success();
}

// char *mgos_provision_wifi_get_last_test_ssid(void){
//   const char *ssid = NULL;
//   ssid = mgos_sys_config_get_provision_wifi_results_ssid();
//   return (ssid != NULL ? strdup(ssid) : NULL);
// }

const char *mgos_provision_wifi_get_last_test_ssid(void){
  return mgos_sys_config_get_provision_wifi_results_ssid();
}

bool mgos_provision_wifi_disconnect_connected_sta(void){
  
  enum mgos_wifi_status sta_status = mgos_wifi_get_status();
  // enum mgos_wifi_status sta_status = mgos_wifi_dev_sta_get_status();

  char * connected_ssid = NULL;
  connected_ssid = mgos_wifi_get_connected_ssid();

  switch (sta_status) {
    case MGOS_WIFI_DISCONNECTED:
      b_sta_was_connected = false;
      LOG( LL_INFO, ( "Provision WiFi not currently connected to any STA" ) );
      break;
    case MGOS_WIFI_CONNECTING:
      b_sta_was_connected = true;
      LOG( LL_INFO, ( "Provision WiFi CONNECTING to existing STA..." ) );
      break;
    case MGOS_WIFI_CONNECTED:
      b_sta_was_connected = true;
      LOG( LL_INFO, ( "Provision WiFi CONNECTED to existing STA %s", connected_ssid ? connected_ssid : "unknown" ) );
      break;
    case MGOS_WIFI_IP_ACQUIRED:
      b_sta_was_connected = true;
      LOG( LL_INFO, ( "Provision WiFi IP ACQUIRED from existing STA %s", connected_ssid ? connected_ssid : "unknown" ) );
      break;
  }

  if( b_sta_was_connected ){
    LOG( LL_INFO, ( "Provision WiFi DISCONNECTING existing STA %s ...", connected_ssid ? connected_ssid : "unknown" ) );
    mgos_wifi_disconnect();
    mgos_msleep(1000); // Sleep 1 second just to be safe
  }

  if( connected_ssid != NULL){
    free(connected_ssid);
  }

  return b_sta_was_connected;
}

/**
 * @brief Run WiFi STA Provision Credential Testing
 * 
 */
void mgos_provision_wifi_run_test(void){
  bool result = false;

  b_provision_wifi_testing = true;
  s_provision_wifi_lock = mgos_rlock_create();
  // mgos_wifi_add_on_change_cb((struct mgos_wifi_add_on_change_cb *) mgos_provision_wifi_net_cb_test, NULL);

  const struct mgos_config_provision_wifi_sta *cfg = mgos_sys_config_get_provision_wifi_sta();
  
  mgos_provision_wifi_disconnect_connected_sta();
  // mgos_provision_wifi_setup_sta() calls wifi disconnect before dev setup
  result = mgos_provision_wifi_setup_sta( cfg );
  
  // cfg->enable = false; // Set to false to FORCE wifi lib not to set/create timer (since we use our own)
  // result = mgos_wifi_setup_sta( (struct mgos_config_wifi_sta *) cfg );

  // // If existing station is already connected, use wifi lib setup sta func to make sure we don't try to setup sta when lock is held by wifi lib
  // if( mgos_provision_wifi_disconnect_connected_sta() ){
  //   result = mgos_wifi_setup_sta( (struct mgos_config_wifi_sta *) cfg );
  // } else {
  //   result = mgos_provision_wifi_setup_sta( cfg );
  // }

  if (result) {

    mgos_provision_wifi_enable_net_cb();

    mgos_clear_timer(s_provision_wifi_timer_id);
    s_provision_wifi_timer_id = MGOS_INVALID_TIMER_ID;
    
    int connect_timeout = mgos_sys_config_get_provision_wifi_timeout();

    if (cfg != NULL && connect_timeout > 0) {
      s_provision_wifi_timer_id = mgos_set_timer(connect_timeout * 1000, 0, mgos_provision_wifi_sta_connect_timeout_timer_cb, NULL);
    }

  } else {

    LOG(LL_ERROR, ("%s", "Provision WiFi STA config error while attempting to run test" ) );
    mgos_provision_wifi_connection_failed();

  }

}

void mgos_provision_wifi_test(mgos_wifi_provision_cb_t cb, void *userdata) {

  if (!b_provision_wifi_testing) {
    // Only set callback when test isn't already running
    s_provision_wifi_test_cb = cb;
    s_provision_wifi_test_cb_userdata = userdata;

    LOG(LL_INFO, ("%s", "Provision WiFi Running Test with Callback Set" ) );
    mgos_provision_wifi_run_test();
  } else {
    LOG(LL_ERROR, ("%s", "Provision WiFi Running Test with Callback Error - Test already running" ) );
  }
}

void mgos_provision_wifi_test_ssid_pass(const char *ssid, const char *pass, mgos_wifi_provision_cb_t cb, void *userdata){
  if( ! ssid ){
    LOG(LL_ERROR, ("%s", "Provision WiFi Test SSID PASS, SSID is missing" ) );
    return;
  }

  mgos_sys_config_set_provision_wifi_sta_ssid( ssid );
  mgos_sys_config_set_provision_wifi_sta_pass( pass );

  save_cfg(&mgos_sys_config, NULL);

  LOG(LL_INFO, ("Provision WiFi Running Test with Callback after setting SSID %s and PASS %s", ssid, pass ) );
  mgos_provision_wifi_test( cb, userdata );
}

bool mgos_provision_wifi_init(void) {

  // Check if config is set to true to test WiFi STA on device boot
  if( mgos_sys_config_get_provision_wifi_boot_enable() ){

    int boot_test_delay = mgos_sys_config_get_provision_wifi_boot_delay();

    // Use one-shot timer for delay if existing sta is enabled (to prevent attempting connection while STA configured connection tries to connect)
    if( mgos_sys_config_get_wifi_sta_enable() && boot_test_delay > 0 ){
      mgos_set_timer(boot_test_delay * 1000, 0, mgos_provision_wifi_run_test_timer_cb, NULL);
    } else {
      mgos_provision_wifi_run_test();
    }

  }

  return true;
}