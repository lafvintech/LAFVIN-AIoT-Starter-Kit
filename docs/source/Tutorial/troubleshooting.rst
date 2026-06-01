.. _troubleshooting:

Troubleshooting
=================

This section helps you recover from the most common setup and usage problems when using the LAFVIN AIoT Starter Kit.

Flashing and Connection
------------------------------------------

**Computer Does Not Detect the Board**

* **Symptom**: The board does not appear as a serial device and the browser flasher cannot find it.
* **Possible Causes**:
  
  1. CP210x driver is not installed
  2. USB cable supports charging only
  3. Faulty USB port or loose connection
  
* **Solutions**:
  
  1. Install the CP210x driver from :ref:`install_driver`
  2. Replace the cable with a USB Type-C data cable
  3. Reconnect the board and try a different USB port

**Online Flasher Cannot Connect**

* **Symptom**: You open the web flasher but no compatible device appears after clicking ``Connect``.
* **Possible Causes**:
  
  1. Browser does not support Web Serial
  2. Serial permission was denied
  3. Driver or USB connection issue
  
* **Solutions**:
  
  1. Use the latest version of Chrome or Edge
  2. Reopen the page and allow serial access when prompted
  3. Install the driver and retry with a different USB port
  4. Use the local flashing alternative in :ref:`firmware_upload`

**Device Cannot Connect to Wi-Fi**

* **Symptom**: The board re-enters Wi-Fi provisioning mode or shows that Wi-Fi is disconnected.
* **Possible Causes**:
  
  1. Incorrect Wi-Fi password
  2. Weak Wi-Fi signal or no internet access
  3. The selected network is 5 GHz only
  
* **Solutions**:
  
  1. Re-enter Wi-Fi setup and confirm the password
  2. Move the board closer to the router
  3. Select a 2.4 GHz Wi-Fi network. ESP32-S3 does not support 5 GHz Wi-Fi.
  4. Use another device on the same network to confirm internet access

**Wi-Fi Configuration Page Does Not Open**

* **Symptom**: You connect to ``Xiaozhi-XXXX`` but no setup page appears.
* **Possible Causes**:
  
  1. Phone browser did not open the captive portal automatically
  2. The board is not in Wi-Fi provisioning mode
  
* **Solutions**:
  
  1. Open ``http://192.168.4.1`` manually in the browser
  2. Restart the board and reconnect to the ``Xiaozhi-XXXX`` hotspot

Backend Binding
------------------------------------------

**Cannot Add the Device in Xiaozhi**

* **Symptom**: The board announces a verification code, but binding does not complete in the Xiaozhi backend.
* **Possible Causes**:
  
  1. The wrong verification code was entered
  2. The board restarted before binding completed
  
* **Solutions**:
  
  1. Re-enter the exact 6-digit verification code
  2. Keep the board powered on while adding the device

**Board Asks to Add the Device Again After Restart**

* **Symptom**: The board repeats the message asking you to add the device in the control panel.
* **Possible Causes**:
  
  1. Binding was not saved

* **Solutions**:
  
  1. Open the correct agent and use ``Add Device`` again
  2. If needed, repeat the Wi-Fi and binding flow from :ref:`xiaozhi_conf`

Voice Issues
------------------------------------------

**Microphone Cannot Recognize Speech**

* **Symptom**: The board wakes up, but your speech is not recognized.
* **Possible Causes**:
  
  1. Microphone connection error
  2. Excessive environmental noise
  3. Audio module not connected correctly or securely
  
* **Solutions**:
  
  1. Check that the audio module is connected correctly and securely
  2. Move to a quieter environment and speak closer to the microphone

**Speaker Has No Sound**

* **Symptom**: The assistant displays its responses on the screen, but there is no audio output.
* **Possible Causes**:

  1. Speaker not connected to the audio module
  2. Volume set to zero or too low
  3. Damaged speaker
  
* **Solutions**:
  
  1. Check the speaker module connection
  2. Adjust playback volume

Display Issues
------------------------------------------

**LCD Screen Does Not Display**

* **Symptom**: The LCD screen stays black after power-on.
* **Possible Causes**:
  
  1. Screen connection issues
  2. System firmware was not flashed successfully
  
* **Solutions**:
  
  1. Check that the LCD screen connections are correct
  2. Restart the device
  3. Reflash the firmware if the screen stays blank

**Abnormal Display Content**

* **Symptom**: The screen flickers or the board keeps restarting.
* **Possible Causes**:

  1. Unstable screen connection
  2. Insufficient power supply
  
* **Solutions**:
  
  1. Check the LCD module connection
  2. Use the included external power adapter or a stable USB power source

System Issues
------------------------------------------

**Device Frequently Restarts**

* **Symptom**: The board restarts unexpectedly or enters a restart loop.
* **Possible Causes**:
  
  1. Unstable power supply or low voltage
  2. Circuit connection causing a short circuit
  
* **Solutions**:
  
  1. Use the included external power adapter or a stable USB port
  2. Verify that the firmware was flashed correctly
  3. Confirm that the device is wired correctly

**Device Responds Slowly**

* **Symptom**: Voice responses take much longer than expected.
* **Possible Causes**:
  
  1. Slow network connection
  
* **Solutions**:
  
  1. Check network connection quality
  2. Change to a different network

Reset Device
------------------------------------------

If you encounter problems that cannot be resolved, try the following recovery steps:

**Re-enter Wi-Fi Provisioning**

1. Press the ``BOOT`` button briefly during the initial startup and initialization process to re-enter network configuration mode.
2. Wait for the board to re-enter Wi-Fi provisioning mode.

**Reflash Firmware**

1. Follow the steps in :ref:`firmware_upload` to reflash the firmware.
2. After reflashing, reconfigure Wi-Fi.
3. If the board was still bound before reflashing, it may reconnect to the previous agent automatically.

Contact Support
------------------------------------------

If the steps above do not solve the problem:

1. Send an email to technical support: `tech_edu_service@outlook.com <mailto:tech_edu_service@outlook.com>`_

When asking for help, include:

* a clear description of the problem
* the step where the problem occurred
* the solutions you have already tried
