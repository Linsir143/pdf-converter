# 📄 PDF 批量转图片工具

本项目是一个轻量、高效的 PDF 转图片工具，支持批量处理、多格式输出 (PNG/JPG) 以及清晰度调节。

为了探索不同技术栈的特性，本项目实现了 **三个完全不同** 的版本！你可以根据自己的需求和环境选择最适合你的版本：

1. 🌐 **Web 网页版 (Streamlit)**：界面现代，交互极佳，支持打包下载。
2. 🐍 **Python 桌面版 (Tkinter)**：支持文件直接拖拽，智能路径识别。
3. 🚀 **C++ 原生版 (Win32 API)**：硬核重构，极限性能，仅几 MB 大小，**真正的免安装单文件体验**。

------

## 1️⃣ Web 网页版 (Streamlit)

如果你喜欢在浏览器里操作，或者想把它部署到服务器上：

1. 进入目录：`cd Web_Streamlit`

2. 安装依赖：`pip install -r requirements.txt`

3. 启动服务：`streamlit run app.py`

   *(浏览器会自动打开 `http://localhost:8501`)*

https://pdf-converter-o63urdarqckuescwn99upz.streamlit.app/

## 2️⃣ Python 桌面版 (Tkinter)

带原生窗口和拖拽功能的桌面小工具：

1. 进入目录：`cd Desktop_Python`
2. 安装依赖：`pip install -r requirements.txt`
3. 运行程序：`python main.py`

## 3️⃣ C++ 原生极速版

不想装 Python 环境？追求极致小巧？请看这个版本。

### 保姆级编译指南 (Windows MSYS2)

如果你想自己从源码编译出极小体积的 `.exe`，请严格执行以下步骤：

**准备工作：** 下载并安装 [MSYS2](https://www.msys2.org/) (X86_64，不熟悉的话建议就按默认放C:\msys64)，打开 **MSYS2 MinGW x64** 终端(mingw64.exe)。

**步骤 1：一键安装环境**



```bash
pacman -Syu
pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-mupdf mingw-w64-x86_64-upx
```

**步骤 2：一键编译与极致瘦身**

进入你的代码目录（例如 `cd /d/code/Desktop_Cpp`）(分隔符用/)，然后**直接复制并运行下面这串长命令**：



```bash
g++ main.cpp -o pdf_converter.exe -Os -s -flto -ffunction-sections -fdata-sections -Wl,--gc-sections -mwindows -municode -lmupdf -lfreetype -lharfbuzz -ljbig2dec -ljpeg -lopenjp2 -lz -lgdi32 -lcomctl32 -lole32 -lshell32 -luuid && mkdir -p Release && cp pdf_converter.exe Release/ && ldd pdf_converter.exe | grep "mingw64" | awk '{print $3}' | xargs -I '{}' cp '{}' Release/ && cd Release && strip --strip-unneeded *.dll && upx --best --lzma *
```

*(注：如果中途出现 `warning: 'UNICODE' redefined` 属于正常现象，无需理会。)*

**🎉 搞定！** 此时你的目录下会多出一个 `Release` 文件夹。你可以直接把这个文件夹打包发给任何人，里面的 `pdf_converter.exe` 配合旁边的极小 DLL 即可在任何 Windows 电脑上双击秒开，无需任何前置环境！