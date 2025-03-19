
<!-- TABLE OF CONTENTS -->
<details>
  <summary>Table of Contents</summary>
  <ol>
    <li>
      <a href="#about-the-project">About The Project</a>
      <ul>
        <li><a href="#built-with">Built With</a></li>
      </ul>
    </li>
    <li>
      <a href="#getting-started">Getting Started</a>
      <ul>
        <li><a href="#prerequisites">Prerequisites</a></li>
        <li><a href="#installation">Installation</a></li>
      </ul>
    </li>
    <li><a href="#usage">Usage</a></li>
    <li><a href="#roadmap">Roadmap</a></li>
    <li><a href="#contributing">Contributing</a></li>
    <li><a href="#license">License</a></li>
    <li><a href="#contact">Contact</a></li>
    <li><a href="#acknowledgments">Acknowledgments</a></li>
  </ol>
</details>

<!-- ABOUT THE PROJECT -->
## About The Project

esp32s3-eye
This is the code directory where I worked to make the sdcard/usb-otg function in visual studio code.
I compiled these things in idf 5.1.0 version.

Here's why:
* on esp32s3-eye board
* support sdcard,usbotg
* you can save all things into files on sdcard

<p align="right">(<a href="#readme-top">back to top</a>)</p>

### Built With

This section should list any major frameworks/libraries used to bootstrap your project. Leave any add-ons/plugins for the acknowledgements section. Here are a few examples.

* Visual Studio Code with idf espressif plug-in
* idf 

<p align="right">(<a href="#readme-top">back to top</a>)</p>

<!-- GETTING STARTED -->
## Getting Started

This is an example of how to list things you need to use the software and how to install them.
* to be determined

### Prerequisites

This is an example of how to list things you need to use the software and how to install them.
* to be determined
  

### Installation

Setting Development Environment

1. Install vscode(https://code.visualstudio.com/)
You can download and install it from https://code.visualstudio.com/. It will be installed by default without any settings.
  
2. Install ESP-IDF
2.1 After running vscode, click the extension icon on the right or execute the extension command with the shortcut key "ctrl+shift+x".
2.2 Search for esp32s (espressif, esp-idf, esp32 etc) in the extension.
2.3 Install Espressif IDF

3. Setting ESP-IDF
3.1 Press "F1", type "ESP-IDF", find "ESP-IDF: Select where to save configuration settings" and select it. Select where to save the settings.

You can choose one of the following three: (Select Global, see https://github.com/espressif/vscode-esp-idf-extension/blob/master/docs/MULTI_PROJECTS.md)

- Global
- Workspace
- Workspace Folder

Press "F1", type "ESP-IDF", find "ESP-IDF: Configure ESP-IDF extension" and select it.
You can also select "View -> Command Palette" instead of pressing "F1".

3.2 The ESP-IDF extension installation screen appears and asks you to choose between "EXPRESS" and "ADVANCED" mode (for initial installation).
3.3 Select "EXPRESS" to proceed with the installation.

The user folder is set as the default value as shown below. Click "Install" to proceed with the installation.(It takes some time.)

4. You can now start working by creating a new project or opening an imported project folder. 
