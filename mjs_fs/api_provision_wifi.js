
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
    test: ffi('void mgos_provision_wifi_test(void(*)(userdata),userdata)'),
    testSSIDandPass: ffi('void mgos_provision_wifi_test_ssid_pass(char*,char*,void(*)(userdata),userdata)')
};