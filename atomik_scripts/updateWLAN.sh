#!/bin/bash

wlan0_ssid=$(mysql -uroot -praspberry -se "SELECT wlan0_ssid FROM atomik_controller.atomik_settings");
wlan0_method=$(mysql -uroot -praspberry -se "SELECT wlan0_method FROM atomik_controller.atomik_settings"); # 0 = Disable, 1 = Open WEP, 2 = Shared Wep, 3 = WPAPSK, 4 = WPA2PSK
wlan0_algorithm=$(mysql -uroot -praspberry -se "SELECT wlan0_algorithm FROM atomik_controller.atomik_settings"); # 0 = ASCII, 1 = HEX, 2 = TKIP, 3 = AES
wlan0_password=$(mysql -uroot -praspberry -se "SELECT wlan0_password FROM atomik_controller.atomik_settings");

wpa_sup_file=$(sudo cat /etc/wpa_supplicant/wpa_supplicant.conf | sed -e '/network={/,/}/c\network={\n')

sudo echo -e "$wpa_sup_file"
if [ $wlan0_method == "0" ] 
then 
printf "\tssid=\"%s\"\n" "$wlan0_ssid"
echo -e "\tkey_mgmt=NONE"
fi

if [ $wlan0_method == "1" ]
then
printf "\tssid=\"%s\"\n" "$wlan0_ssid"
echo -e "\tkey_mgmt=NONE"
if [ $wlan0_algorithm == "0" ]
then
printf "\twep_key0=\"%s\"\n" "$wlan0_password"
fi
if [ $wlan0_algorithm == "1" ]
then
printf "\twep_key0=%s\n" "$wlan0_password"
fi

echo -e 'wep_tx_keyidx=0'
fi

if [ $wlan0_method == "2" ]
then
printf "\tssid=\"%s\"\n" "$wlan0_ssid"
echo -e "\tkey_mgmt=NONE"
if [ $wlan0_algorithm == "0" ]
then
echo -e 'wep_key0="$wlan0_password"'
fi
if [ $wlan0_algorithm == "1" ]
then
echo -e 'wep_key0=$wlan0_password'
fi

echo -e 'wep_tx_keyidx=0'
echo -e 'auth_alg=SHARED'
fi

if [ $wlan0_method == "3" ]
then
printf "\tssid=\"%s\"\n" "$wlan0_ssid"
echo -e 'proto=WPA'
echo -e 'key_mgmt=WPA-PSK'
echo -e 'pairwise=CCMP TKIP'
echo -e 'group=CCMP TKIP WEP104 WEP40'
echo -e 'psk="$wlan0_password"'
fi

if [ $wlan0_method == "4" ]
then
printf "\tssid=\"%s\"\n" "$wlan0_ssid"
echo -e 'psk="$wlan0_password"'
echo -e 'proto=RSN'
echo -e 'key_mgmt=WPA-PSK'
if [ $wlan0_algorithm == "2" ]
then
echo -e ' pairwise=TKIP'
fi
if [ $wlan0_algorithm == "3" ]
then
echo -e 'pairwise=CCMP'
fi

fi

sudo echo -e "\n}"
exit 0
