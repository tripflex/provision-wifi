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
    Results: {
        success: ffi('bool mgos_provision_wifi_get_last_test_results(void)'),
        ssid: ffi('char *mgos_provision_wifi_get_last_test_ssid(void)'),
        // check: ffi(''), // TODO: allow passing SSID to check if last test was for SSID and return results
    },
    isRunning: ffi( 'bool mgos_provision_wifi_is_test_running(void)'),
    Test: {
        run: ffi('void mgos_provision_wifi_test(void(*)(int,char*,userdata),userdata)'),
        SSIDandPass: ffi('void mgos_provision_wifi_test_ssid_pass(char*,char*,void(*)(int,char*,userdata),userdata)')
    },
    run: ffi('void mgos_provision_wifi_run_test(void)')
};