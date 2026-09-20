# OrbitronMod
[中文](/README_CN.md)

Contains some modifications to Orbitron.

## Features
- Replaced the ephemeris format from TLE with OMM

- Completely fixed the Y2K problem in TLE

- Added support for HTTPS servers to TLE updater


## Usage
- Prepare a DLL injector (e.g., [Xenos](https://github.com/DarthTon/Xenos/releases))

- Run `Xenos.exe`

- Click `New`, and in the pop-up window select `Orbitron.exe`

- Click `Add`, and select `OrbitronMod.dll`

- Click `Inject` to launch Orbitron


## How to Build
- Download [MinHook](https://github.com/TsudaKageyu/minhook/releases)

- Create a new folder named `minhook` under the `packages` folder

- Extract the downloaded MinHook into this folder

- Open `OrbitronMod.slnx`