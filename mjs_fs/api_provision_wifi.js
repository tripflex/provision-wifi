
let ProvisionWiFi = {
    onBoot: {
        enable: ffi('bool mgos_provision_wifi_enable_boot_test(void)'),
        disable: ffi('bool mgos_provision_wifi_disable_boot_test(void)')
    },
    STA: {
        connect: ffi('bool mgos_provision_wifi_connect_sta(void)'),
        disconnect: ffi('bool mgos_provision_wifi_disconnect_sta(void)'),
        copy: ffi('bool mgos_provision_wifi_copy_sta_values(void)'),
        clear: ffi('bool mgos_provision_wifi_clear_sta_values(void)'),
    },
    LastTest: {
        success: ffi('bool mgos_provision_wifi_get_last_test_results(void)'),
        ssid: ffi('char *mgos_provision_wifi_get_last_test_ssid(void)')
    },
    isRunning: ffi( 'bool mgos_provision_wifi_is_test_running(void)'),
    runTest: ffi('void mgos_provision_wifi_run_test(void)'),
    // ## **`Timer.set(milliseconds, flags, handler, userdata)`**
    // Setup timer with `milliseconds` timeout and `handler` as a callback.
    // `flags` can be either 0 or `Timer.REPEAT`. In the latter case, the call
    // will be repeated indefinitely (but can be cancelled with `Timer.del()`),
    // otherwise it's a one-off.
    //
    // Return value: numeric timer ID.
    //
    // Example:
    // ```javascript
    // // Call every second
    // Timer.set(1000, Timer.REPEAT, function() {
    //   let value = GPIO.toggle(2);
    //   print(value ? 'Tick' : 'Tock');
    // }, null);
    // ```
    test: ffi('void mgos_provision_wifi_test(int,int,void(*)(userdata),userdata)'),
};