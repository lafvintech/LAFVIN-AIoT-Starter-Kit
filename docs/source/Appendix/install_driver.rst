.. _install_driver:

Driver Installation
=========================

Windows System
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Check If CP210X Is Already Installed**

ESP32-S3 uses CP210X for code downloading. Before using it, we need to install the CP210X driver on our computer.

1. Connect your computer and ESP32-S3 with a USB cable (connect to the port indicated by the red arrow)

.. figure:: img/esp32s3-4.jpg
   :align: center

2. Go to your computer's main interface, select "This PC" and right-click to select "Manage".

.. figure:: img/driver1.png
   :align: center

3. Click "Device Manager". If your computer has already installed CP210X, you will see "USB-Enhances-SERIAL CP210X (COMx)". You can then proceed to the next step.

.. figure:: img/driver3.png
   :align: center

**Installing CP210X**

1. First, download the CP210X driver. Click `here <https://www.silabs.com/developer-tools/usb-to-uart-bridge-vcp-drivers>`_ to download the appropriate driver for your operating system.

.. figure:: img/driver3.png
   :align: center

If you don't want to download the installation package from the website, you can open "LAFVIN-AIoT-Starter-Kit/CP210X", as we have already prepared the installation package.

You can install it following the video below

.. video:: img/driver_ins.mp4
    :width: 100%

.. _macos_upload:

macOS Firmware Upload (Alternative)
---------------------------------------------

1. Find the macOS folder in the GitHub repository. You can download the whole project to your local computer.

.. figure:: img/macos0.png
   :align: center

2. We have prepared the flashing tools and files for macOS in this folder.

.. figure:: img/macos1.png
   :align: center

3. Open the terminal, run ``python3 mac.py``, and the flashing interface will appear.

4. Press and hold the ``BOOT`` button on the ESP32-S3, insert the USB Type-C data cable, and press Enter to select the AI firmware. In this example, choose ``xiaozhi-iot.bin``.

.. figure:: img/macos2.png
   :align: center

5. Click Confirm. The program will complete the flashing process automatically.
6. After flashing is complete, press the ``RST`` button on the ESP32-S3 and wait for the device to restart.
7. After restarting, follow :ref:`xiaozhi_conf <xiaozhi_conf>` for network configuration and device binding in the backend.
