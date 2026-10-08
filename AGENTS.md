# AGENTS.md — Concentration_V2 开发协作指南

> 本文件是 AI agent（以及人类接手者）协助本项目**开发**的入口文档：包含项目事实、硬约束、开发惯例、常见任务指引与已知风险。
>
> 目标 MCU：**STM32F103C8T6**（LQFP48，64 KB Flash / 20 KB RAM）。
> 构建系统：**CMake + Ninja**，工具链 **arm-none-eabi-gcc / g++ 15.2.1**；用户模块自 2026-10 起用 **C++20** 编写。
> 代码整理（重命名/格式化/注释/结构）已在阶段 0–5 完成，C++ 现代化在阶段 A–E 完成，历史见[附录 B](#附录-b-重构历史已完成)。
> 本环境**无目标硬件**；凡未实机运行之处一律标注 **未做硬件验证**。

---

## 0. 角色与协作原则（最高优先级，AI agent 必读）

### 0.1 角色
你是本项目的**嵌入式开发助手**。目标是帮助开发者**理解、修改、扩展、调试**这套 STM32F103C8T6 固件，而不是重写它。具体包括：新增/修改外设与业务逻辑、修复缺陷、补充模块、编写文档与调试建议、解释现有代码。

### 0.2 硬约束（改动现有行为前必须得到开发者明确指示）
默认**保持既有功能、逻辑、时序、中断行为、优化结果、内存布局、外设寄存器访问、通信协议、ABI、持久化格式与构建产物不变**。以下内容未经明确要求不得改动：
- 控制流、循环顺序、条件判断、状态机迁移、超时、重试、延时、看门狗喂狗位置。
- 中断服务程序名、中断优先级、临界区、锁、原子操作、内存屏障。
- `volatile`、`const`、`static`、`inline`、`packed`、`aligned`、`section`、`weak`、`alias`、`interrupt` 等修饰的增删。
- 寄存器读写顺序、位掩码、移位、位域、外设初始化顺序。
- 启动文件、链接脚本、向量表、汇编文件、编译器选项、预处理宏的值与条件编译逻辑。
- 公共 API、导出函数签名、全局变量名、结构体/联合体布局、枚举值、协议字段、持久化格式、配置项键名。
- 已有依赖、构建系统、烧录配置、时钟/PLL/电源/看门狗/DMA/缓存/MPU 配置。
- “看似无用”的代码、空循环、`nop`、`delay`、强制类型转换、对齐填充、保留字段（如需删除先确认）。
- 引入新库、新 RTOS、动态内存、异常、RTTI、标准库容器、`printf` 重依赖。**项目既定的 C++ 零开销子集（C++20：禁堆/禁异常/禁 RTTI）是唯一例外**，白名单与陷阱见 [4.9 C++ 使用规范](#49-c-使用规范)。
- **CubeMX 控制/生成的内容**：`.ioc`、`STM32F103XX_FLASH.ld`、`startup_stm32f103xb.s`、`cmake/stm32cubemx/CMakeLists.txt`、以及生成文件中的 `MX_*_Init` 等区域。详见 [4.1 CubeMX 工作流](#41-cubemx-工作流)。
- 大规模自动格式化整个仓库（除非开发者同意并单独提交）。

> 若一项开发任务**必须**改变上述行为（例如修复缺陷需要改动逻辑），先说明必要性、影响范围与风险，得到确认后再动手；无法在本环境验证的行为改动，标注 **未做硬件验证** 并给出实测建议。

### 0.3 开发惯例（鼓励遵循）
- 按项目既有风格与 `.clang-format` 编写；可格式化**正在修改的文件**（不要顺手全仓格式化）。
- 文件编码 **UTF-8**、行尾 **CRLF**（照现有文件），缩进 4 空格。
- 可补充文件头/函数头/模块注释；不删除既有注释，除非确认是错别字。
- 局部变量、函数内参数、`static` 函数、文件内宏可规范命名，但需全仓库引用同步修改，且不影响外部符号。
- 魔法数字可提取为 `const`/宏（值必须完全不变）。
- 新增用户模块放 `Core/Src` 与 `Core/Inc`（**C++20，`.cpp`**），并在根 `CMakeLists.txt` 的 `USER_*` 列表登记（见 [3.2](#32-构建命令对应-vscodetasksjson)）。
- 复用现有宏/类型/风格，不自创并行机制。

### 0.4 每次改动的工作方式
1. 先说明**计划与影响面**，再动手。
2. 改动最小化，一次聚焦一类改动。
3. 编译通过（`cmake --build --preset Debug`），尽量零新增警告。
4. **可等价验证的改动**（重命名、注释、格式化、仅提取常量等）对比 `bin`/`hex` sha256；**行为性改动**无法由此验证，必须说明并建议实测。
5. 无法实机验证时明确写 **“未做硬件验证”**。
6. 验证失败立即回滚，不叠加改动。

### 0.5 回复格式
开发任务建议按：`目标 / 方案 / 影响面 / 变更文件 / 验证 / 风险 / 待确认` 输出。涉及现有行为改动时必须显式点出。

### 0.6 遇到不确定时
不要猜硬件行为、不要猜原作者意图。列出可选方案、影响范围与风险，等待开发者确认。确属缺陷但需改变行为的，先记录到第 5 章并标记 **“需要人工决策”**。

---

## 1. 项目概览

### 1.1 项目是什么
一个基于 STM32F103C8T6 的**便携式浓度/电量检测仪**固件（工程名 `Concentration_V2`）：
- 通过 **ADC1（PA0 光电流 / PA1 电池分压）** 采样，DMA 循环搬运。
- 通过 **I2C1 SSD1306 OLED（128×32）** 显示电量、蓝牙状态、进度条与文本。
- 通过 **TIM2**（1 kHz）产生 ADC 触发（CH2，TRGO=OC2REF）与 UV 灯 PWM（CH4）。
- 通过 **USART1（9600）** 接收上位机数据帧（IDLE+DMA）。
- 按键/开关：`TILT=PA2`、`KEY=PA7`、`SW=PA8`；充电/待机指示 `SHDBY=PB4`、`CHRG=PB5`；负载控制 `DC_ctrl=PB8`；状态灯 `LED=PB12`。
- 校准结果持久化到 Flash 末页绝对地址 `0x0800FC00`。

> 注：原设计意图与算法细节缺少说明文档，部分逻辑（UART 收帧、低电量检测）疑似未完成，见第 5 章。

### 1.2 仓库结构
```
Concentration_V2/
├── Core/
│   ├── Inc/   main.h, adc.h, dma.h, gpio.h, i2c.h, tim.h, usart.h,
│   │          stm32f1xx_hal_conf.h, stm32f1xx_it.h,
│   │          App.h, Concentration_Conversion.h, FIFO_LOCKFREE.h, OLED.h
│   └── Src/   main.c, adc.c, dma.c, gpio.c, i2c.c, tim.c, usart.c,
│              stm32f1xx_it.c, stm32f1xx_hal_msp.c,
│              syscalls.c, sysmem.c, system_stm32f1xx.c,
│              App.cpp, Concentration_Conversion.cpp, FIFO_LOCKFREE.cpp, OLED.cpp
├── Drivers/        STM32F1xx_HAL_Driver + CMSIS（第三方，勿改）
├── cmake/          gcc-arm-none-eabi.cmake（工具链）、stm32cubemx/CMakeLists.txt（CubeMX 生成）
├── tools/          openocd.cfg
├── .scripts/       patch_cubemx（CubeMX 生成后修补脚本）
├── .vscode/        tasks.json（构建/烧录任务）
├── .devcontainer/  开发容器定义（不参与固件构建）
├── CMakeLists.txt, CMakePresets.json
├── STM32F103XX_FLASH.ld, startup_stm32f103xb.s
├── Concentration_V2.ioc, .mxproject
├── .clang-format, .clangd, .gitignore, .dockerignore
└── AGENTS.md（本文件）
```
> `MDK-ARM/`、`.eide/`、`.cmsis/` 已在重构阶段 1a 删除（提交 `babcada`），见附录 B。
> 其中 `adc/dma/gpio/i2c/tim/usart.c`、`stm32f1xx_it.c`、`stm32f1xx_hal_msp.c`、`syscalls.c`、`sysmem.c`、`system_stm32f1xx.c`、`main.c` 及其头文件为 **C（CubeMX 生成/保留）**；`App.*`、`Concentration_Conversion.*`、`FIFO_LOCKFREE.*`、`OLED.*` 为 **C++20 用户模块**。`.clangd` 依赖 `build/Debug` 的编译数据库（见 [3.5](#35-常见问题)）。

### 1.3 启动流程
```
Reset_Handler (startup_stm32f103xb.s)
  → copy .data / zero .bss → SystemInit()（本工程为空实现）
  → __libc_init_array → main()
main():
  HAL_Init()
  SystemClock_Config()          // HSE 8MHz ×PLL9 = 72MHz；APB1=36MHz，APB2=72MHz；ADC=/6=12MHz
  MX_GPIO_Init() MX_DMA_Init() MX_ADC1_Init() MX_I2C1_Init()
  MX_USART1_UART_Init() MX_TIM2_Init()
  App_Init()                     // 转发 App::init()：启动 DMA/ADC/TIM/UART，读取持久化参数
  OLED_Init(); OLED_Set_Dot(1)
  while (1) { App_UpdateDisplay(); HAL_Delay(113); }   // 转发 App::updateDisplay()
```
- 业务闭环主要在 **ADC DMA 完成中断** 里推进（见第 2 章），主循环仅负责 OLED 刷新。
- 系统时钟：SYSCLK 72 MHz，FLASH_LATENCY_2，ADC 时钟 12 MHz。

### 1.4 主要模块
| 模块 | 文件 | 职责 |
|---|---|---|
| 启动/胶水 | `Core/Src/main.c` (C) | CubeMX 初始化编排；仅调用 `App_Init()`/`App_UpdateDisplay()` 与中断钩子 |
| 应用逻辑 | `Core/Src/App.cpp` / `App.h` (**C++20**) | `App` 类：状态机 `CC_set_work_status()`、`Average_filtering()`、`battery_level()`、按键消抖、`HAL_ADC_ConvCpltCallback` 主体 |
| 浓度/校准 | `Concentration_Conversion.cpp/.h` (**C++20**) | 校准/测量状态机，Flash 持久化（`0x0800FC00`） |
| 显示 | `OLED.cpp/.h` (**C++20**) | SSD1306 128×32 驱动（`OledDriver` 类），逻辑缓冲→物理旋转，字体与图形 |
| 环形缓冲 | `FIFO_LOCKFREE.cpp/.h` (**C++20**) | 无锁 SPSC 环形队列（`uint16_t`），**当前未被使用** |
| 中断 | `stm32f1xx_it.c` (C) | 异常/中断处理，含 USART1 IDLE 收帧逻辑；`extern` 引用 `main.c` 的 rx 缓冲 |
| 外设初始化 | `adc.c/dma.c/gpio.c/i2c.c/tim.c/usart.c` (C) | CubeMX 生成，与 `.ioc` 一致（勿改） |

### 1.5 如何构建 / 烧录 / 调试
见第 3 章。烧录（需实机 ST-Link）：
```
openocd -f tools/openocd.cfg -c "program build/Debug/Concentration_V2.elf verify reset exit"
```
调试：SWD（PA13/PA14），`stm32f1xx_hal_msp.c` 中 `__HAL_AFIO_REMAP_SWJ_NOJTAG()` 保留 SWD、关闭 JTAG。
> 当前环境无目标硬件，**未做硬件验证**。

---

## 2. 架构与数据流

### 2.1 模块职责与依赖
```
                     ┌──────────────────────────────┐
                     │      main.c  (C, 薄钩子)      │
                     │  初始化编排 / App_Init / 中断钩│
                     └───────────────┬──────────────┘
                                     │ extern "C"
                                     ▼
        ┌──────────────────────────────────────────────┐
        │            App.cpp  (C++20, App 类)           │
        │  init / updateDisplay / onAdcComplete         │
        │  CC_set_work_status / Average_filtering / ... │
        └───┬───────────────┬───────────────┬──────────┘
            ▼               ▼               ▼
   Concentration_       OLED.cpp      FIFO_LOCKFREE.cpp
   Conversion.cpp       (OledDriver)  (SPSC 环形, 未用)
   (状态机 + Flash)
            │               │
            ▼               ▼
      stm32f1xx_hal (HAL) ◄── CubeMX 生成外设 (adc/dma/i2c/tim/usart/gpio)
            │
            ▼
      CMSIS/启动/链接脚本

   中断: stm32f1xx_it.c ──► HAL ──► HAL_ADC_ConvCpltCallback (App.cpp)
```
- `main.h` 汇总引脚宏、`my_data_t` 等类型，并 include `Concentration_Conversion.h`。
- 用户模块的 C ABI 由各自头文件的 `extern "C"` 守卫冻结；`main.c`（C）只经该接缝调用（见 [4.9](#49-c-使用规范)）。
- `App` 持有原全局 `my_data`/`hardware_status` 为成员（`constinit App g_app;`）；`main.c` 不再保存业务状态。
- `OLED.h` include `main.h`（耦合较紧，但无直接循环包含）。

### 2.2 数据流（主链路）
```
ADC1(PA0 光, PA1 电池) --DMA1_Ch1 循环--> App::data.ADC_value[20]
   --(DMA 完成中断) HAL_ADC_ConvCpltCallback (App.cpp)-->
        Average_filtering(ADC_value, 2ch, 20, ADC_avg[2])   // 每通道 10 次平均
        ├─ data.ADC_INT      = ADC_avg[0]   (光电流/浓度相关)
        └─ data.BAT.adc_BAT  = ADC_avg[1]   (电池电压)
        ADC_avg[1] --> battery_level() --> data.BAT.battery_level (0..5)
        CC_set_work_status()          // 状态机推进（App 成员）
        Concentration_Conversion_task()
        SW/TILT/KEY 消抖（读值≠缓存值累加，>5 次翻转状态）
主循环: App_UpdateDisplay() --> OledDriver::update(battery_level, bluetooth_state=1, progress)
```
- 屏幕进度 `progress`：校准/测量中来自 `detection_time`，否则来自 `BAT.adc_BAT/V_MAX*100`（当 `BAT_status != NORMAL`）。
- `App::data.hardware_status` 指向成员 `hw`（原全局 `hardware_status`）。

### 2.3 控制流 / 状态机
`work_status_e`：`INIT, SAVE, calibration, working, readay, err_TILT, err_open, err_low_pow, err_no_cap`。
- `CC_set_work_status()`：按 `work_status` 分派，处理 `Concentration_Conversion_Reset`、UV 灯亮度更新、`DC_ctrl_ON/OFF`、`get_Result`；含各错误态恢复；并用 `SHDBY/CHRG` 引脚更新 `BAT_status`（`PIN_RESET→SHDBY`，否则 `CHRG` 为 reset→`CHRG`，否则 `NORMAL`）。
- `Concentration_Conversion_task()`：`calibration→Concentration_Conversion_calibration`；`working→Concentration_Conversion_update`；`SAVE→Write_Conversion_Value` 后置 `readay`；`INIT→` 空操作。
- 校准/测量计时：`detection_time`（`time_Unit` 换算），达到 `once_detection_time`（初始化传入 `100.0f`）置 `finish`。
- **注释编号缺口**：`App.cpp` 中标签 `1. 意图状态转换` 后直接 `3. 低电量检测`，缺 `2`；且低电量检测块被注释掉。

### 2.4 中断
| 中断 | 优先级 (抢占,子) | 处理 |
|---|---|---|
| `SysTick_Handler` | (15,0) | `HAL_IncTick()` |
| `DMA1_Channel1_IRQHandler` | (4,0) | `HAL_DMA_IRQHandler(&hdma_adc1)` → 触发 `HAL_ADC_ConvCpltCallback`（**承载大量浮点/状态机工作**） |
| `USART1_IRQHandler` | (0,0) | 先执行用户 IDLE 收帧逻辑（`DMAStop`+`memcpy`+重启 DMA），再 `HAL_UART_IRQHandler` |
| NMI/HardFault/MemManage/BusFault/UsageFault | — | `while(1)` |
| SVC/DebugMon/PendSV | — | 空 |

- NVIC 仅使能 `DMA1_Channel1_IRQn` 与 `USART1_IRQn`。
- **注意**：`HAL_TIM_Base_Start_IT(&htim2)` 置位了 TIM2 更新中断使能位，但工程中**没有 `TIM2_IRQHandler`，也未使能 `TIM2_IRQn`**；若将来使能该 NVIC 中断，会落入 `Default_Handler` 的 `b Infinite_Loop` 而死循环（见风险 R9）。

### 2.5 外设与引脚（与 `.ioc` 一致）
| 外设 | 关键配置 |
|---|---|
| ADC1 | 扫描使能、非连续、外部触发 `T2_CC2`、2 通道：PA0(IN0,rank1)、PA1(IN1,rank2)，71.5 周期，DMA1_Ch1 循环半字 |
| TIM2 | PSC=71，ARR=999（1 kHz）；CH2 PWM 脉宽 500，TRGO=OC2REF；CH4 PWM 脉宽 0（UV 灯，`4*level`） |
| USART1 | 9600 8N1，RX→DMA1_Ch5 循环字节，TX→DMA1_Ch4 单次字节，开 IDLE 中断 |
| I2C1 | 100 kHz，7 位地址；PB6/PB7 AF_OD |
| GPIO | TILT=PA2、KEY=PA7、SW=PA8（输入浮空）；LED=PB12（输出开漏）；SHDBY=PB4、CHRG=PB5（输入浮空）；DC_ctrl=PB8（推挽输出） |
| SWJ | `__HAL_AFIO_REMAP_SWJ_NOJTAG()`（保留 SWD） |

时钟：HSE 8 MHz 晶振（PD0/PD1）→ PLL ×9 = 72 MHz；APB1=36 M、APB2=72 M、ADC=12 M。

### 2.6 持久化（Flash）
- 绝对地址 `0x0800FC00`（64 KB Flash 的最后一页，1 KB）。
- 结构：`StoredConversion_t { uint32_t magic=0x12345678; Conversion_value_t data; }`，按字（32 位）编程；相关常量（`kConversionMagic`、`kConversionDataAddr` 等）为 `Concentration_Conversion.cpp` 匿名命名空间内的 `constexpr`，并有 `static_assert` 布局守卫。
- `Read_Conversion_Value()` 读时校验 magic；无效则给默认 `Raw_value=0.3f, UVlight_level=70`。
- `Write_Conversion_Value()` 仅在 `Conversion_flag==finish` 时擦除该页并写入；写入前条件判断见风险 R3（高危）。
- **该页未在链接脚本中保留**（见风险 R4）。

---

## 3. 构建 / 烧录 / 调试

### 3.1 工具链
| 组件 | 版本（实测） |
|---|---|
| arm-none-eabi-gcc / g++ | 15.2.1（Arm GNU Toolchain 15.2.Rel1, Build arm-15.86, 20251203） |
| cmake | 3.28.3 |
| ninja | 1.11.1 |
| binutils/objcopy | 2.45.1 |
| OpenOCD | 0.12.0 |
| clang-tidy / clang-format | Ubuntu LLVM 22.1.8（本环境**无 `clang` 编译器**） |
| C 标准 | C11（`CMAKE_C_EXTENSIONS` 开启） |
| C++ 标准 | **C++20**（`CMAKE_CXX_EXTENSIONS OFF`，见 [4.9](#49-c-使用规范)） |

### 3.2 构建命令（对应 `.vscode/tasks.json`）
```
cmake --preset Debug                 # 配置（生成 build/Debug）
cmake --build --preset Debug         # 构建（默认任务）
cmake --build --preset Release       # Release 构建
python3 .scripts/patch_cubemx        # CubeMX 生成后修补（幂等）
openocd -f tools/openocd.cfg -c "program build/Debug/Concentration_V2.elf verify reset exit"   # 烧录
openocd -f tools/openocd.cfg -c "init; reset halt; stm32f1x mass_erase 0; reset run; shutdown"  # 全片擦除
```
- 预设见 `CMakePresets.json`：生成器 Ninja，工具链文件 `cmake/gcc-arm-none-eabi.cmake`，`CMAKE_EXPORT_COMPILE_COMMANDS=ON`。
- **CubeMX 会重写 `cmake/stm32cubemx/CMakeLists.txt`**；用户源在根 `CMakeLists.txt` 中登记（当前：`App.cpp`、`Concentration_Conversion.cpp`、`FIFO_LOCKFREE.cpp`、`OLED.cpp`；用户 include：`Core/Inc`）。C++ 标准与 `LINKER_LANGUAGE CXX` 也定义在根文件（不放 CubeMX 可重写的工具链文件）。
- `python3 .scripts/patch_cubemx`（幂等）会：把 `CMakePresets.json` 重写为规范预设、规范化 `cmake/stm32cubemx/CMakeLists.txt` 的路径前缀、删除陈旧的 `cmake/starm-clang.cmake`，并在根 `CMakeLists.txt` 未登记用户源时告警。

### 3.3 编译配置
- Debug：`-O0 -g3`；Release：`-Os -g0`。
- 公共：`-mcpu=cortex-m3 -Wall -fdata-sections -ffunction-sections`。
- C++ 追加：`-fno-rtti -fno-exceptions -fno-threadsafe-statics`（见 `cmake/gcc-arm-none-eabi.cmake`；**该文件可能被 CubeMX 重生成**）。
- 链接：`-T STM32F103XX_FLASH.ld --specs=nano.specs -Wl,-Map=Concentration_V2.map -Wl,--gc-sections -Wl,--print-memory-usage -lm`；链接器语言为 C++（`LINKER_LANGUAGE CXX`，以 `g++` 驱动链接）。

### 3.4 产物
`build/Debug/`：`Concentration_V2.elf`、`.bin`、`.hex`、`.map`（`build/` 已 gitignore，不提交）。

### 3.5 常见问题
- **`clang` 不存在**：本环境只有 `clang-tidy`/`clang-format`，无 `clang`。
- **`.clangd` 依赖编译数据库**：`.clangd` 配置为 `CompilationDatabase: build/Debug`，须先执行 `cmake --preset Debug` 生成，否则 IDE 报错。
- **C++ 模块**：用户模块为 `.cpp`，被 C 包含的头文件需 `extern "C"` 守卫；不要引入 `std::array`/STL（会带入 libstdc++/printf/malloc，见 [4.9](#49-c-使用规范)）。
- **CubeMX 重新生成后**：必须运行 `python3 .scripts/patch_cubemx`（否则 include 路径/预设可能回退）。
- **新增/移动用户文件**：需同步根 `CMakeLists.txt` 的 `USER_*` 源列表。
- **构建目录**：`build/Debug`、`build/Release` 均在 gitignore 内。

---

## 4. 开发指引（常见任务）

### 4.1 CubeMX 工作流
- 修改外设/引脚/时钟/中断：**用 STM32CubeMX 打开 `Concentration_V2.ioc` 重新生成**，不要手改生成区域。
- 生成时 `KeepUserCode=true`，**`USER CODE BEGIN/END` 之间的内容会被保留**——用户逻辑只写在这些区域内。
- 生成后**务必**执行 `python3 .scripts/patch_cubemx`（幂等修补 `cmake/stm32cubemx/CMakeLists.txt` 与预设）。
- 重新生成可能覆盖生成区域内的一切（含注释与格式）；提交前检查 `git diff` 是否只动了预期的 USER CODE 区。
- 注意：`main.h`、`stm32f1xx_hal_conf.h` 等也有 USER CODE 区，可安全编辑；其余生成内容勿动。
- 用户业务已抽到 `App.cpp` 等 C++ 模块；`main.c` 的 USER CODE 仅保留薄钩子（`App_Init`/`App_UpdateDisplay`）。修改 `main.c` 生成区后再生成时，仍走 CubeMX + `patch_cubemx`。

### 4.2 新增/修改用户模块
1. 在 `Core/Src` 建 `.cpp`、`Core/Inc` 建 `.h`（UTF-8 + CRLF）。
2. 在根 `CMakeLists.txt` 的用户源列表加入新 `.cpp`（include 路径 `Core/Inc` 已包含）。
3. 头文件用保护宏（非保留标识符，如 `MODULE_H`）；若会被 C 代码包含，必须加 `extern "C"` 守卫（见 [4.9](#49-c-使用规范)）。
4. 需要访问 HAL 时 `#include "main.h"` 或对应外设头；遵守 C++ 零开销子集与禁堆/禁异常策略。
5. `cmake --preset Debug && cmake --build --preset Debug` 验证；检查导出符号与尺寸基线（见 4.7）。

### 4.3 增加外设 / 引脚
- 走 CubeMX（见 4.1），在 `.ioc` 配置后重新生成；`MX_*_Init()` 会加入生成代码。
- 若需在中断中使用，注意 NVIC 优先级与现有优先级（DMA1_Ch1=4，USART1=0，SysTick=15）的相互影响。
- 新增中断需补 `stm32f1xx_it.c` 的对应 handler（CubeMX 会生成骨架）。

### 4.4 调整 OLED 显示
- 入口 `OLED_Update(uint8_t battery_level, uint8_t bluetooth_state, uint8_t percent)`：主循环调用。
- 布局/尺寸宏在 `OLED.h`（`OLED_PHYS_*`、`TOP/MID/BOTTOM_AREA_HEIGHT`、`TEXT_*`）；逻辑缓冲经 `RotateLogicToPhysical` 旋转到物理屏。
- 字体/位图为 `OLED.cpp` 匿名命名空间内的 `constexpr` 数组（`bluetooth_16x20`、`font8x16`）；模块状态封装在 `OledDriver` 类，5 个 `OLED_*` 函数为 `extern "C"` 转发。新增字符需同时更新绘制函数的分支。
- I2C 传输超时固定 100ms，改动需评估对主循环节奏的影响。

### 4.5 修改状态机 / 业务逻辑
- 状态迁移集中在 `App.cpp` 的 `App::CC_set_work_status()`；`App::Concentration_Conversion_task()` 按状态分派。
- 大量业务在 `HAL_ADC_ConvCpltCallback`（DMA 中断上下文，转发到 `App::onAdcComplete()`）内执行——**新增逻辑要评估中断时长**（见 R5）。
- 调整计时/采样时注意 `once_detection_time`、`time_Unit`、`detection_time` 的关系与单位换算。

### 4.6 修改 Flash 持久化数据（谨慎）
- 结构 `StoredConversion_t` 位于 `Concentration_Conversion.cpp`；改字段会改变持久化格式。
- **改动前必须设计版本/迁移方案**：无 magic 或版本不符时应回退默认值，且避免旧数据被误解析。
- 该页地址 `0x0800FC00` 未在链接脚本预留（R4），且写路径存在空指针风险（R3）——改动前先阅读风险并知会开发者。

### 4.7 资源约束
- 当前占用（阶段 D 基线，Debug `-O0`）：FLASH **28196 B / 64 KB ≈ 43.0%**，RAM **3784 B / 20 KB ≈ 18.5%**；距配置页 `0x0800FC00` 仍有约 35 KB。
- C++ 现代化（阶段 A–D）在 `-O0` 下共增约 452 B（Phase A +28、B ±0、C +288、D +136）；发布用 `-Os` **未单独测量**。
- 固件增长接近 `0x0800FC00` 前必须处理 R4（把配置页在链接脚本中保留）。
- 资源紧张时优先减小 `printf`/浮点重依赖的引入；**不要引入 STL / `std::array`**（会带入 libstdc++/printf/malloc，见 [4.9](#49-c-使用规范)）。

### 4.8 编码 / 风格 / 提交
- UTF-8、CRLF、4 空格；沿用 `.clang-format`。
- 提交信息用类型前缀：`feat:` / `fix:` / `refactor:` / `docs:` / `chore:`；保持单一主题、可回滚。
- 纯等价改动（命名/注释/格式/提取常量）建议注明 `no logic change`。

### 4.9 C++ 使用规范（用户模块）
自阶段 A 起，用户模块（`App`、`Concentration_Conversion`、`FIFO_LOCKFREE`、`OLED`）用 **C++20** 编写；`main.c` 与 CubeMX 生成文件保持 **C**，通过 `extern "C"` 接缝调用。

**语言 / 标准**
- C++20（`CMAKE_CXX_STANDARD 20`、`CMAKE_CXX_STANDARD_REQUIRED ON`、`CMAKE_CXX_EXTENSIONS OFF`），定义在**根 `CMakeLists.txt`**（不放 CubeMX 可重写的工具链文件）。
- 编译标志 `-fno-rtti -fno-exceptions -fno-threadsafe-statics`；链接以 `g++` 驱动（`LINKER_LANGUAGE CXX`）。

**允许（零开销子集）**：`constexpr`/`consteval`/`constinit`、`enum class`、`if constexpr`、概念（concepts）、`namespace`、类 / RAII、`std::span`、`std::bit_cast`、模板、`[[nodiscard]]`、`static_assert`。

**禁止**：堆分配（`new`/`delete`/`malloc`）、异常、RTTI、虚函数、`iostream`、`std::string`、STL 容器、`std::function`、协程、模块。

**已知陷阱 —— 勿用 `std::array` / libstdc++ 断言**：使用 `std::array`（及 `.fill()`）会拉入 libstdc++ 的 `__glibcxx_assert_fail`，进而带入 `fprintf`/`abort`/`_malloc_r`/stdio，FLASH **+约 5 KB**。OLED 缓冲因此改用裸数组 + `std::memset`。**引入任何标准库设施前，先用 `arm-none-eabi-nm` 对比是否多出 libstdc++/stdio/malloc 符号。**

**C ABI 接缝**：所有会被 C 代码包含的头文件加 `extern "C"` 守卫；导出函数名必须与 C 版本**逐字一致**。注意 `--gc-sections` 会裁掉无引用的导出符号——改动调用点后需复核符号表（阶段 C 即因内部直接调用成员函数而丢失 `OLED_Clear`/`OLED_Refresh`，后经还原原调用点修复）。结构体布局用 `static_assert`（`sizeof`/`offsetof`/标准布局）冻结。

**初始化顺序**：`.init_array` 在 `HAL_Init()` **之前**运行；**禁止会触及 HAL/外设的全局构造函数**。用 `constinit` + 平凡默认构造（**不要**写 `= 0` 之类的默认成员初始化器，否则 `constinit` 编译失败），如 `constinit App g_app;`、`constinit OledDriver g_oled;`。

**验证方法（较 C 阶段变更）**：C++ 阶段**不再以 `bin`/`hex` 逐字节 sha256 为验收标准**（改写会改变常量物化/栈布局）。改为：① 干净重建且无新增警告；② 导出全局符号集合与上一阶段一致（可用独立 worktree 构建后对比 `nm`）；③ 结构体布局 `static_assert`；④ `FLASH`/`RAM` 尺寸与中断/调用顺序逐语句人工审查。所有阶段**未做硬件验证**。

---

## 5. 已知风险与陷阱（开发时当心）——**记录，未修**

> 严重度：🔴 高危 / 🟠 中 / 🟡 低 / ⚪ 信息。凡涉及行为改变的一律 **需要人工决策**。行号可能因后续编辑而漂移，以符号名为准。

- **R1 ✅ 已解决 文件编码不一致**：`Concentration_Conversion.c/.h` 原为 GBK，已在重构阶段 3 步骤 1（提交 `30866f0`）转为 UTF-8，构建产物逐字节一致。
- **R2 🟠 未初始化变量（UB）**：`Concentration_Conversion.cpp` 中 `float c_Unit;`（`Concentration_Conversion_init`）未赋值即用于 `Unit = time_Unit * c_Unit`；字段 `Unit` 全仓只写不读。→ **需要人工决策**。（编译器已告警）
- **R3 🔴 `Read_Conversion_Value(NULL)` 空指针解引用**：`Write_Conversion_Value` 内调用 `Read_Conversion_Value(NULL)`；该函数两条分支都会解引用 `out`。首次保存时 `*checkAddr==0xFFFFFFFF` 短路，安全；**一旦 Flash 已含 magic（即第二次进入保存判断），将真实写地址 0**，STM32F1 上极可能触发总线/硬件错误。→ **需要人工决策**。
- **R4 🟠 配置页未在链接脚本保留**：`0x0800FC00` 页未被 `STM32F103XX_FLASH.ld` 预留。当前无重叠；固件一旦增长越过该地址即冲突。→ **需要人工决策 / 持续关注**。
- **R5 🟠 中断中做重活**：`HAL_ADC_ConvCpltCallback` 在 DMA 中断（优先级 4）上下文执行大量浮点运算与状态机，可能造成抖动/长中断。→ **需要人工决策**。
- **R6 🟡 死代码 / 未使用符号**：UART 收帧路径 `rx_frame_ready`/`rx_work_buffer`/`rx_frame_len` 无消费者；`msg_fifo`/`msg_buf`（FIFO 子系统）、全局 `I`、`custom_exp10`、`Get_Concentration_Conversion_Detection_Time`、`test_t`、`current_Unit_e`、宏 `set_light_level`、`Phys_DrawHorizontalString` 均未使用（部分已被 `--gc-sections` 移除）。→ 记录。
- **R7 🟡 FIFO 无内存屏障**：`FIFO_LOCKFREE` 使用 `volatile` head/tail，但无显式内存屏障。SPSC 在 Cortex-M3 上通常可用，但严格性存疑。→ 记录。
- **R8 🟡 UART 收帧在中断内阻塞操作**：`USART1_IRQHandler` 中 `HAL_UART_DMAStop` + `memcpy` + 重启 DMA 均在中断上下文执行，且置于 `HAL_UART_IRQHandler` 之前。→ 记录。
- **R9 🟡 TIM2 更新中断“悬空”**：`HAL_TIM_Base_Start_IT(&htim2)` 置位更新中断使能，但无 `TIM2_IRQHandler` 且未使能 `TIM2_IRQn`。当前不触发；若启用 NVIC 将落入 `Default_Handler` 死循环。→ 记录。
- **R10 ⚪ 注释编号缺口**：`App.cpp` 中状态机注释 `1.` 后直接 `3.`，缺 `2.`；低电量检测块被注释掉。→ 记录。
- **R11 ⚪ 未使用宏写法不一致**：`main.h` 的 `set_light_level(my_data, level)` 用 `.` 而非 `->`（且未使用）。→ 记录。
- **R12 ⚪ 遗留构建产物**：历史上 `MDK-ARM/` 曾提交 Keil 编译产物，已在阶段 1a 连同 `.eide/`、`.cmsis/` 清理。→ 已处理。
- **R13 ⚪ 导出符号命名不一致**：`Concentration_Conversion_updata` 的拼写错误 `updata` 已修正为 `Concentration_Conversion_update`（导出符号随之更名，调用点/注释同步）；`get_Result` 等命名/风格差异按治理规则保留原样（阶段 4 仅改局部变量/参数）。→ `updata` 已处理，其余记录。

---

## 附录 A. 基线与静态分析（阶段 1）

基线指标（阶段 1，C 时代，Debug / 干净构建）：text 27732 / data 12 / bss 3768（dec 31512）；FLASH 27744 B / 64 KB = 42.33%；RAM 3784 B / 20 KB = 18.48%。
bin sha256 `dbd2a8f1eedd8ccaa8b7e0f80b4b151e16e324ae874a7c2c7451f86016be8cf6`；hex sha256 `d3a782a95d64c0dc880fa87fb1c78859c06366c0d8242f85903671469cc8d710`。
> C++ 阶段 A–D 后的新基线见 [4.7 资源约束](#47-资源约束)：FLASH **28196 B（43.02%）**，RAM **3784 B（18.48%）**；逐字节 sha256 已不作为 C++ 阶段的验收标准（见 [4.9](#49-c-使用规范)）。
编译警告（既有 2 条，未消除）：
1. `Concentration_Conversion.c` — `'c_Unit' is used uninitialized`（= R2）。
2. `OLED.c` — `'Phys_DrawHorizontalString' defined but not used`（= R6）。

静态分析（clang-tidy，仅 `Core/Src`）关键结果：
- `clang-analyzer-core.FixedAddressDereference`：`Concentration_Conversion.c`（固定地址解引用，对应 R3/R4）。
- `clang-analyzer-deadcode.DeadStores`：`OLED.c`（`ny` 初始化后未读）。
- `clang-analyzer-security.insecureAPI`：`OLED.c`（`memset`，信息性）。
- 扩展集（风格类，未处理）：`bugprone-reserved-identifier`、`easily-swappable-parameters`、`performance-no-int-to-ptr`、`narrowing-conversions`、`macro-parentheses`、`branch-clone`。

---

## 附录 B. 重构历史（已完成）

| 序 | 阶段 | 内容 | 提交 |
|---|---|---|---|
| 0 | — | 只读审计 | — |
| 1a | 1 | 删除 `MDK-ARM/`、`.eide/`、`.cmsis/`（构建中立） | `babcada` |
| 1b | 1 | 干净 Debug 基线 + 指标 + 风险核实 | — |
| 1c | 1 | clang-tidy + clang-format 报告 | — |
| 2 | 2 | 交接文档（本文件） | `f5309d8` |
| 3-1 | 3 | GBK→UTF-8（仅注释文本） | `30866f0` |
| 3-2 | 3 | `clang-format` 格式化 6 个纯用户模块 | `72cf152` |
| 3-3 | 3 | 补充并统一注释（废弃代码保留并加注） | `44586be` |
| 4 | 4 | 局部变量/函数内参数命名整理 | `fcaa1bc` |
| 5 | 5 | 提取文件内魔法数字 + 修复保留标识符 include 守卫 | `b597c69` |
| A | C++ | 用户模块改 C++20、加 `extern "C"` 守卫、根 CMake 配 CXX20 与 `LINKER_LANGUAGE CXX` | `17abbc8` |
| B | C++ | 3 个纯模块内部现代化（anon namespace / `constexpr` / `static_assert` 布局），冻结 C ABI | `15ae0c1` |
| C | C++ | OLED 状态封装进 `OledDriver` 类 + `FlashWriteGuard` RAII | `3821472` |
| D | C++ | `main.c` 业务逻辑抽到 `App` 类（新增 `App.cpp`/`App.h`） | `d65b728` |
| E | C++ | 更新本文件（C++ 规范/接缝/验证方法/新基线） | 见 git log |

阶段 0–5 通过“干净重建 + `bin`/`hex` sha256 与基线一致”验证；C++ 阶段 A–E 改用“干净重建 + 导出符号集合一致 + 结构体布局 `static_assert` + 尺寸/中断时序审查”（逐字节 sha256 不再适用）。以上均**未做硬件验证**。回滚：`git revert <commit>`。

---

## 附录 C. 需要人工决策清单

| 编号 | 事项 | 影响 |
|---|---|---|
| R2 | `c_Unit` 未初始化（UB） | 未定义行为，建议修复（改行为） |
| R3 | `Read_Conversion_Value(NULL)` 空指针风险 | 第二次保存可能硬件错误，建议修复（改行为） |
| R4 | 配置页 `0x0800FC00` 未在链接脚本保留 | 固件增长会与持久化数据冲突 |
| R5 | ADC 完成回调内做重活 | 实时性/抖动风险 |
| R13 | 导出符号命名不一致（`updata`/`get_Result`） | `updata`→`update` 已修正；`get_Result` 等风格差异保留 |
| R6/R8/R9/R10/R11 | 死代码、UART 中断阻塞、TIM2 悬空中断等 | 清理/加固需评估行为影响 |

> 以上均**只记录、未修改**。修复任何一项都可能改变行为，须由开发者决策并单独验证。

---

*文档定位：面向 AI agent 与人类的**开发协作指南**（自 2026-10 起由“重构治理文档”改写而来）。项目事实、约束与风险仍以本文件为准；行为相关改动一律先确认，未实机验证时标注“未做硬件验证”。*
