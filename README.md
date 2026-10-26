# OrbitronMod
[中文](/README_CN.md)

Contains some modifications to Orbitron.

## Features
- Replace ephemeris format from TLE with OMM (Currently supported formats: CSV, KVN)

- Completely fixed the Y2K problem in TLE

- Added support for HTTPS servers to TLE updater

- Fixed the issue where the mouse wheel could not be used to adjust the time when the window is maximized


## Usage
- Prepare a DLL injector (e.g., [Xenos](https://github.com/DarthTon/Xenos/releases))

- Run `Xenos.exe`

- Click `New`, and in the pop-up window select `Orbitron.exe`

- Click `Add`, and select `OrbitronMod.dll`

- Click `Inject` to launch Orbitron

### OR
- Download [Orbitron-modified.exe](Software\Orbitron-modified.exe) and [OrbitronMod.dll](https://github.com/seen4/OrbitronMod/releases)

- Place these two files under Orbitron installation directory

- Run `Orbitron-modified.exe`


## How to Build
- Download [MinHook](https://github.com/TsudaKageyu/minhook/releases)

- Create a new folder named `minhook` under the `packages` folder

- Extract the downloaded MinHook into this folder

- Open `OrbitronMod.slnx`
