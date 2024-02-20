pip3 install bleak
source bin/activate

python scan_for_devices.py
python service_explorer.py <BLE_ADDR>
python get_notification.py <BLE_ADDR>