## Overview
This component provides a way to manage the pingpongbuffer and is suitable for esp-idf

## How to use

Add component from git repository to your project

* To add esp-ppbuffer to your project from git repository, create an idf_component.yml file with the following contents and place it in the component's folder that depends on esp-ppbuffer (for example, in the main folder of your project, since main is a special component in the esp-idf build system). The component will be downloaded automatically during the CMake processing step.

```
dependencies:
  idf.py add-dependency "lijunru-hub/esp-ppbuffer^*"
```