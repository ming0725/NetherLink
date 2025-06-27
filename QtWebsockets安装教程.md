 # Qt WebSockets 模块安装教程

本教程介绍如何在Windows环境下编译并安装Qt WebSockets模块。

## 环境要求

- Windows 10 或更高版本
- Qt 6.5.1 MinGW 64位版本
- CMake 3.16 或更高版本
- MinGW-w64 编译器

## 准备工作
https://download.qt.io/archive/qt/6.5/6.5.1/submodules/
1. 从Qt官网下载 `qtwebsockets-everywhere-src-6.5.1.zip` 源码包
2. 解压到本地目录，例如：`D:\AAAProgramerMing\qtwebsockets-everywhere-src-6.5.1`
3. 确保已安装Qt 6.5.1，本教程使用的Qt安装路径为：`D:\Qt\6.5.1\mingw_64`

## 编译安装步骤

### 1. 配置CMake

打开PowerShell或命令提示符，进入源码目录，执行以下命令：

```bash
# 创建并进入构建目录
mkdir build
cd build

# 配置CMake构建
cmake .. -G "MinGW Makefiles" ^
    -DCMAKE_PREFIX_PATH="D:/Qt/6.5.1/mingw_64" ^
    -DCMAKE_INSTALL_PREFIX="D:/Qt/6.5.1/mingw_64" ^
    -DCMAKE_BUILD_TYPE=Release
```

注意：请根据您的实际Qt安装路径修改上述命令中的路径。

### 2. 编译

在build目录中执行以下命令进行编译：

```bash
cmake --build . --config Release -j 4
```

参数说明：
- `--config Release`: 指定构建Release版本
- `-j 4`: 使用4个线程并行编译，可以根据CPU核心数调整

### 3. 安装

编译完成后，执行安装命令：

```bash
cmake --install .
```

## 安装验证

安装完成后，以下文件应该被正确安装到Qt目录中：

1. 库文件：
   - `D:/Qt/6.5.1/mingw_64/lib/libQt6WebSockets.a`
   - `D:/Qt/6.5.1/mingw_64/bin/Qt6WebSockets.dll`

2. 头文件：
   - 在 `D:/Qt/6.5.1/mingw_64/include/QtWebSockets` 目录下

3. CMake配置文件：
   - 在 `D:/Qt/6.5.1/mingw_64/lib/cmake/Qt6WebSockets` 目录下

4. QML模块：
   - 在 `D:/Qt/6.5.1/mingw_64/qml/QtWebSockets` 目录下

## 在项目中使用

### CMake项目

在您的CMake项目中，添加以下配置：

```cmake
find_package(Qt6 COMPONENTS WebSockets REQUIRED)

target_link_libraries(your_target PRIVATE
    Qt6::WebSockets
)
```

### QML项目

在QML文件中，使用以下方式导入WebSockets模块：

```qml
import QtWebSockets
```

## 常见问题

1. 如果CMake配置失败，请检查：
   - Qt安装路径是否正确
   - 是否安装了正确版本的MinGW编译器
   - CMake版本是否满足要求

2. 如果编译失败，请检查：
   - 是否有足够的磁盘空间
   - 编译器是否正确安装
   - 是否有必要的权限

3. 如果安装失败，请检查：
   - Qt目录的写入权限
   - 是否有其他程序占用相关文件

## 参考链接

- [Qt官方文档](https://doc.qt.io/)
- [Qt WebSockets文档](https://doc.qt.io/qt-6/qtwebsockets-index.html)