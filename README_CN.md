# OrbitronMod
包含一些对Orbitron的修改

## 功能
- 将星历格式从TLE替换为OMM

- 彻底修复了TLE格式的“千年虫问题”

- 为星历更新器加入了对HTTPS的支持

## 使用方法
- 准备一个DLL注入器（例如[Xenos](https://github.com/DarthTon/Xenos/releases)）

- 运行 `Xenos.exe`

- 点击 `New`，在弹出的窗口中选择`Orbitron.exe`

- 点击 `Add`， 选择 `OrbitronMod.dll`

- 点击 `Inject`，启动Orbitron

### 或者
- 下载 `Software\Orbitron-modified.exe` 和 `OrbitronMod.dll`

- 将这两个文件复制到Orbitron安装目录下

- 运行 `Orbitron-modified.exe`

## 如何编译
- 下载 [MinHook](https://github.com/TsudaKageyu/minhook/releases)

- 在 `packages`文件夹下新建一个名为 `minhook` 的文件夹

- 将下载好的Minhook解压到这个文件夹中

- 打开`OrbitronMod.slnx`
