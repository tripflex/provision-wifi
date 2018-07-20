# Mongoose OS Wifi Provision Library

## Table of Contents
- [Mongoose OS Wifi Provision Library](#mongoose-os-wifi-provision-library)
    - [Table of Contents](#table-of-contents)
    - [Author](#author)
    - [Overview](#overview)
    - [Why?](#why)
    - [Features](#features)
    - [Install/Use](#installuse)
    - [Configurations](#configurations)
    - [MJS](#mjs)
        - [Examples](#examples)
    - [C](#c)
    - [WiFi AP Need to Know](#wifi-ap-need-to-know)
    - [Roadmap](#roadmap)
    - [Suggestions and Code Review](#suggestions-and-code-review)
    - [License](#license)


## Author
- Myles McNamara
- [You!](#suggestions-and-code-review)

## Overview

This library is meant to help with provisioning (setting up) a device with WiFi Credentials, handling timeouts, max connection attempts, and letting you know whether or not the SSID/Password are correct.

[Are you familiar with Mongoose OS and/or C programming?](#suggestions-and-code-review)

## Why?
I created this library because the default/core Mongoose OS WiFi library, which this library does depend on, does not do that good of a job when it comes to validating WiFi values that are set.  If correct SSID and Password are set, it works great ... but as we all know with IoT devices, the end user will be the one setting it up, and chances are there will be multiple instances where the type the SSID or Password incorrectly, and there's no real easy way of dealing with that.  

_**This library is my solution to this problem**_, not only solving it, but also adding additional features and handling for provisioning WiFi on a device.

## Features
- MJS Support
- Test WiFi settings on device boot (when `provision.wifi.boot.enable` is set true)
- Testing on boot delay in seconds from `provision.wifi.boot.delay` when existing STA is connected (test is immediate when no existing STA connected)
- Fail test after total connection attempts (`provision.wifi.attempts`) and timeout `provision.wifi.timeout` (in seconds)
- Reconnects to existing station (if one was connected) after testing, when `provision.wifi.reconnect` is `true` (default: `true`)
- Test STA values are stored separate from WiFi Library STA values (in `provision.wifi.sta` - matches wifi lib structure)
- Automatically copy test STA values to `wifi.sta` after succesful connection test, when `provision.wifi.success.copy` is `true` (default: `true`)
- Disable AP on successful connection test `provision.wifi.success.disable_ap` when `true` (default: `false`)
- Reboot device on successful connection test `provision.wifi.success.reboot` when `true` (default: `false`)
- Reboot device on failed connection test `provision.wifi.fail.reboot` when `true` (default: `false`)
- Clear test STA values on success test `provision.wifi.success.clear`, or fail `provision.wifi.fail.clear`, when `true` (default: `false`)

## Install/Use
To use this library in your Mongoose OS project, just add it under the `libs` in your `mos.yml` file:
```yml
  - origin: https://github.com/tripflex/provision-wifi
```

## Configurations
This library adds a `provision.wifi` object to the mos configuration, with the following settings:

**To see all of the available configurations** please look at the [mos.yml](https://github.com/tripflex/provision-wifi/blob/master/mos.yml#L12) file.  There are descriptions for each one of the settings.

## MJS
[api_provision_wifi.js](https://github.com/tripflex/provision-wifi/blob/master/mjs_fs/api_provision_wifi.js)

To use this library in your JavaScript files, make sure to load it at top of the file:
```js
load('api_provision_wifi.js');
```

You will then have access to the `ProvisionWiFi` javascript object, with the following:

```js
ProvisionWiFi.run();
``` 
- Run WiFi test based on values previously set in `provision.wifi.sta` (no callback)

```js
ProvisionWiFi.Test.run( callback_fn, userdata );
```
- Run WiFi test based on values previously set in `provision.wifi.sta`, calling `callback_fn` after completion
```js
ProvisionWiFi.Test.SSIDandPass( ssid, pass, callback_fn, userdata );
```
- Set `provision.wifi.sta.ssid` and `provision.wifi.sta.pass`, calling `callback_fn` after completion

```js
ProvisionWiFi.onBoot.enable();
```
- Enable testing on device boot

```js
ProvisionWiFi.onBoot.disable();
```
- Disable testing on device boot

```js
ProvisionWiFi.STA.copy();
```
- Copy test station values to `wifi.sta`

```js
ProvisionWiFi.STA.clear();
```
- Clear any existing station values in `provision.wifi.sta`

```js
ProvisionWiFi.Results.success();
```
- Returns whether or not last test was a success

```js
ProvisionWiFi.Results.ssid();
```
- Returns last SSID that was tested

```js
ProvisionWiFi.Results.isRunning();
```
- Returns whether or not test is currently running

**Callback Parameters**
If you use a callback function (`ProvisionWiFi.Test.run` or `ProvisionWiFi.Test.SSIDandPass`) the callback function will be passed 3 arguments: `success, ssid, userdata`

`success` will be an integer (since boolean not supported in mjs), `1` means succesful connection test, `0` means failed.
`ssid` will be the SSID that was tested against
`userdata` is whatever you passed in `userdata`

### Examples

```js
ProvisionWiFi.Test.SSIDandPass( "TestSSID", "TestPassword", function( success, ssid, userdata ){

    if( success ){
        // Hey look those credentials worked!
    } else {
        // Oh no, probably wrong/bad credentials!
    }

}, null );
```

```js
ProvisionWiFi.Test.run( function( success, ssid, userdata ){

    if( success ){
        // Hey look those credentials worked!
    } else {
        // Oh no, probably wrong/bad credentials!
    }

}, null );
```

```js
let lastTestSuccess = ProvisionWiFi.Results.success();
let lastTestSSID = ProvisionWiFi.Results.ssid();

if( lastTestSSID == 'TestSSID' && lastTestSuccess ){
    // Hey it worked!
} else {
    // Doh! Fail!
}
```

## C
[mgos_provision_wifi.h](https://github.com/tripflex/provision-wifi/blob/master/include/mgos_provision_wifi.h)


## WiFi AP Need to Know
- The AP should *NOT* go down while testing
- If `provision.wifi.reconnect` is `true` (default), there was an existing STA that was connected to before testing, and the test failed, the AP will go down for ~5 seconds or so while wifi is reinitialized.

## Roadmap
- RPC Helper library (for making RPC calls to provision wifi)
- [Your idea!](https://github.com/tripflex/provision-wifi/issues/new)
- Optimizing and reducing code base size

## Suggestions and Code Review
 **PLEASE PLEASE** if you're familiar with Mongoose OS (mos) or C programming in general, I need your help with this project!  
 
 My C and Mongoose OS experience is pretty limited (mos only month or two), but being a full time geek and developer I was able to figure out how to get this working (for most part) ... but i'm sure there's numerous ways to optimize the code base, or correct/better way to do things either in C or Mongoose OS specifically, and **i'm completely open to constructive criticism!**

 Even if you're not 100% sure, neither am I, so open up an issue and let's discuss it!

## License
Apache 2.0