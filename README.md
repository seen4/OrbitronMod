# OrbitronMod

为Orbitron增加对OMM星历格式的支持，并修复了TLE的千年虫问题

## 使用方法
- 准备一个DLL注入器（例如[Xenos](https://github.com/DarthTon/Xenos/releases)）

- 运行 `Xenos.exe`

- 点击 `New`，在弹出的窗口中选择`Orbitron.exe`

- 点击 `Add`， 选择 `OrbitronMod.dll`

- 点击 `Inject`，启动Orbitron


## 如何编译
- 下载 [MinHook](https://github.com/TsudaKageyu/minhook/releases)

- 在 `packages`文件夹下新建一个名为 `minhook` 的文件夹

- 将下载好的Minhook解压到这个文件夹中

- 打开`OrbitronMod.slnx`
