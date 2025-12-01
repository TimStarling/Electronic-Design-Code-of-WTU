📡 Electronic Design Code of WTU / 综合电子课程设计

武汉纺织大学 Comprehensive Electronics Course Design Project
基于 GitHub 维护和更新

🔧 Project Overview / 项目概述

本项目是武汉纺织大学综合电子课程设计工程，包含以下模块：

模块 / Module	内容 / Content	文件名 / Filename
🧠 单片机 / MCU	使用串口向上位机发送一个周期正弦波数组数据	stm32code
💻 上位机 / Host Computer	串口调试助手，基于 QT & C++ 读取正弦波并显示	uartdebugger
📊 MATLAB 正弦数组生成	生成 1024 点 0–4095 量化的单周期正弦波数组	code.m
📐 设计文件 / Design Files	电路原理图 / Schematic、 PCB 设计文件 / PCB design、通信逻辑	/

Status / 当前状态： 持续更新中 (Ongoing updates)

⚡ Core Functions / 核心功能

✅ MATLAB 生成 1024 采样点、一个周期、0–4095 量化的正弦波 LUT
(12-bit ADC-style sine wave lookup table with 1024 samples, 1 period)

✅ STM32 通过 UART 串口向 PC 发送 sine array data
(MCU sends sine wave values to host computer via UART serial)

✅ QT 上位机串口调试助手（C++）读取并窗口显示数据
(QT Host UART Debugger reads serial data and displays in GUI window)

🚀 Development Environment Recommendation / 开发环境建议
工具 / Tool	推荐 / Recommended
MCU 开发 / STM32 Development	STM32CubeIDE
上位机 / Host Computer	QT Creator
版本管理 / Version Control	Git
📦 Before Use / 使用前准备
1️⃣ Install chip package / 安装芯片支持包

请提前下载并安装：

STM32G0xx_DFP

使用前确保下载该芯片包（DFP）
(Make sure STM32G0xx device package is installed before flashing code)

2️⃣ Download STM32G0xx_DFP package / 下载芯片包

在开始编译/烧录之前，请确保安装了 STM32 G0 设备支持包。

3️⃣ Configure MCU Reference / 单片机配置参考

MCU configuration reference (串口配置参考):

→ PDF 参考文档来自：STM32配置参考链接（见侧边资料）
(You already have the reference link, so not repeated here.)

4️⃣ Install STM32 chip pack / 安装 STM32 芯片包支持

使用前需下载 DFP 芯片包：

📌 先手动安装芯片包：STM32G0xx_DFP

✅ 安装完成后即可使用串口发送功能

5️⃣ Install QT on host / 上位机运行前安装 QT

QT 上位机程序基于 QT6 构建，建议使用 QT Creator 直接打开运行。
(The host debugger program is built with QT C++ and should be opened with QT Creator.)

📂 Repository File Structure / 仓库文件结构
Electronic-Design-Code-of-WTU/
│
├── uartdebugger/       % QT Host UART Debugger (QT C++)
│
├── stm32code/          % STM32 MCU Serial Sine Sender
│
├── code.m              % MATLAB sine wave LUT generator
│
└── README.md           % This document

🔌 Notes / 注意事项

⚠️ DFP chip pack installation is mandatory before compiling and flashing
编译和烧录 STM32 代码之前，必须先下载并安装 STM32G0xx_DFP 芯片支持包，否则无法完成 device 识别。

⚠️ Window copying format output support by default
数组和串口数据输出格式已经设计为命令行可复制格式 (Copyable comma‐separated integers)，可直接粘贴使用。

✒️ Maintainer / 维护者

武汉纺织大学 Electronics Comprehensive Course Design Team
(Original schematic + PCB + embedded C/C++ code + QT UART Host Debugger Program + MATLAB Sine LUT)
