# AGENTS.md — Concentration_V2 开发协作指南

> 本文件是 AI agent（以及人类接手者）协助本项目**开发**的入口文档：包含项目事实、硬约束、开发惯例、常见任务指引与已知风险。
>
> 目标 MCU：**STM32F103C8T6**（LQFP48，64 KB Flash / 20 KB RAM；末页已预留给校准数据，见 2.6）。
> 构建系统：**CMake + Ninja**，工具链 **arm-none-eabi-gcc / g++ 15.2.1**。
> 语言：用户模块 **C++20**（`Core/{Inc,Src}/app/`），胶水与 CubeMX 生成文件 **C17**。
> 历史：代码整理阶段 0–5、C++ 现代化阶段 A–F、激进重写阶段 S1–S9（模块化拆分、统一命名、修 R3/R4）及缺陷修复 F1–F3（修 R2/R5/R8）均已完成，见[附录 B](#附录-b-重构历史已完成)。
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
- 跨语言 C ABI 接缝函数名与签名（见 [4.9](#49-c-使用规范)）、持久化格式与字段布局。
- 已有依赖、构建系统、烧录配置、时钟/PLL/电源/看门狗/DMA/缓存/MPU 配置。
- “看似无用”的代码、空循环、`nop`、`delay`、强制类型转换、对齐填充、保留字段（如需删除先确认）。
- 引入新库、新 RTOS、动态内存、异常、RTTI、STL 容器、`printf` 重依赖。**项目既定的 C++ 零开销子集（C++20：禁堆/禁异常/禁 RTTI）是唯一例外**，白名单与陷阱见 [4.9](#49-c-使用规范)。
- **CubeMX 控制/生成的内容**：`.ioc`、`startup_stm32f103xb.s`、`cmake/stm32cubemx/CMakeLists.txt`、以及生成文件中的 `MX_*_Init` 等区域。链接脚本 `STM32F103XX_FLASH.ld` 现由 `patch_cubemx` 维护其保留区（见 2.6 / 4.1）。
- 大规模自动格式化整个仓库（除非开发者同意并单独提交）。

> 若一项开发任务**必须**改变上述行为（例如修复缺陷需要改动逻辑），先说明必要性、影响范围与风险，得到确认后再动手；无法在本环境验证的行为改动，标注 **未做硬件验证** 并给出实测建议。

### 0.3 开发惯例（鼓励遵循）
- 按项目既有风格与 `.clang-format` 编写；可格式化**正在修改的文件**（不要顺手全仓格式化）。
- 文件编码 **UTF-8**、行尾 **CRLF**（照现有文件），缩进 4 空格。
- 用户 C++ 模块放 `Core/Inc/app/` 与 `Core/Src/app/`（`.hpp`/`.cpp`），并在根 `CMakeLists.txt` 用户源列表登记（见 [3.2](#32-构建命令对应-vscodetasksjson)）。
- 命名规范（见 [4.9](#49-c-使用规范)）：类型/枚举 PascalCase、方法 camelCase、常量 `k` 前缀、全局单例 `g_`、`namespace app`。
- 复用小而专的模块（`Hardware`/`AppConfig` 等），不自创并行机制。

### 0.4 每次改动的工作方式
1. 先说明**计划与影响面**，再动手。
2. 改动最小化，一次聚焦一类改动。
3. 编译通过（`cmake --build --preset Debug`），尽量零新增警告。
4. **可等价验证的改动**对比 `bin`/`hex` 或做行为等价审查；**行为性改动**必须说明并建议实测。
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
- 通过 **USART1（9600）** 接收上位机数据帧（IDLE+DMA；收帧输出当前无消费者，见 R6）。
- 按键/开关：`TILT=PA2`、`KEY=PA7`、`SW=PA8`；充电/待机指示 `STDBY=PB4`、`CHRG=PB5`；负载控制 `DC_ctrl=PB8`；状态灯 `LED=PB12`。
- 校准结果持久化到 Flash 末页绝对地址 `0x0800FC00`。

### 1.2 仓库结构
```
Concentration_V2/
├── Core/
│   ├── Inc/   main.h, adc.h, dma.h, gpio.h, i2c.h, tim.h, usart.h,
│   │          stm32f1xx_hal_conf.h, stm32f1xx_it.h,          // C（CubeMX 生成）
│   │          App.h                                          // C ABI 接缝
│   │   └── app/   AppConfig.hpp, Hardware.hpp, Measurement.hpp, Persistence.hpp,
│   │              BatteryMonitor.hpp, InputDebounce.hpp, AppState.hpp,
│   │              OledPanel.hpp, Display.hpp, UartReceiver.hpp   // C++20 用户模块
│   └── Src/   main.c, adc.c, dma.c, gpio.c, i2c.c, tim.c, usart.c,
│              stm32f1xx_it.c, stm32f1xx_hal_msp.c,
│              syscalls.c, sysmem.c, system_stm32f1xx.c,      // C（CubeMX 生成/保留）
│              App.cpp                                        // C++20 用户模块
│       └── app/   Measurement.cpp, Persistence.cpp, BatteryMonitor.cpp,
│                  Display.cpp, OledPanel.cpp, UartReceiver.cpp
├── Drivers/        STM32F1xx_HAL_Driver + CMSIS（第三方，勿改）
├── cmake/          gcc-arm-none-eabi.cmake（工具链）、stm32cubemx/CMakeLists.txt（CubeMX 生成）
├── tools/          openocd.cfg
├── .scripts/       patch_cubemx（CubeMX 生成后修补脚本，含链接脚本保留区维护）
├── .vscode/        tasks.json（构建/烧录任务）
├── docs/           DEAD_CODE.md（已删除死代码归档，含原位置）
├── CMakeLists.txt, CMakePresets.json
├── STM32F103XX_FLASH.ld, startup_stm32f103xb.s
├── Concentration_V2.ioc, .mxproject
├── .clang-format, .clangd, .gitignore, .dockerignore
└── AGENTS.md（本文件）
```
> `MDK-ARM/`、`.eide/`、`.cmsis/` 已在重构阶段 1a 删除（提交 `babcada`）。
> 仅 `Core/Inc/app/` 与 `Core/Src/app/` 下的模块及 `App.cpp`/`App.h` 为用户代码；其余 `.c/.h` 为 CubeMX 生成（只改 `USER CODE BEGIN/END` 区域）。`.clangd` 依赖 `build/Debug` 的编译数据库（见 [3.5](#35-常见问题)）。

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
  App_Init()                     // app::App::init()：启动 UART/ADC/TIM，读持久化参数，
                                 //   初始化按键状态与显示（原 main.c 的 OLED_Init/Set_Dot 已内聚）
  while (1) { App_Poll(); __WFI(); }
```
- ADC DMA 完成中断仅拷贝样本快照并置位（R5）；滤波/状态机/测量等业务在主循环 `App_Poll()` 中推进，显示按 `kDisplayPeriodMs`(113 ms) 节流。系统时钟：SYSCLK 72 MHz，FLASH_LATENCY_2，ADC 时钟 12 MHz。

### 1.4 主要模块
| 模块 | 文件 | 职责 |
|---|---|---|
| 启动/胶水 | `Core/Src/main.c` (C) | CubeMX 初始化编排；仅调用 `App_Init()` 与主循环 `App_Poll()` |
| 应用编排 | `Core/Src/App.cpp` + `Core/Inc/App.h` (**C++20**) | `app::App`：组合各模块，在主循环 `poll()`/`processAdc()` 推进业务与状态机，中断仅做样本快照；对外 C 接缝 |
| 配置常量 | `app/AppConfig.hpp` | ADC 通道数/样本数、电池 V_MAX/V_MIN、检测时间、UV 关闭等级、消抖阈值 |
| 硬件操作 | `app/Hardware.hpp` | `setUvLevel()` / `dcCtrlOn()` / `dcCtrlOff()` 内联封装 |
| 测量/校准 | `app/Measurement.hpp/.cpp` | `app::Measurement`：校准/测量积分状态机（原 `Concentration_Conversion`） |
| 持久化 | `app/Persistence.hpp/.cpp` | `app::PersistentStore` + `ConversionValue`/`ConversionFlag`，Flash 读写、RAII 解锁 |
| 电池监视 | `app/BatteryMonitor.hpp/.cpp` | `app::BatteryMonitor`：电池 ADC、充电/待机状态、电量等级映射 |
| 按键消抖 | `app/InputDebounce.hpp` | `app::InputDebounce`：SW/KEY/TILT 通用消抖 |
| 应用数据 | `app/AppState.hpp` | `WorkState`、`Inputs`、`AppData` 聚合 |
| 显示底层 | `app/OledPanel.hpp/.cpp` | `app::OledPanel`：SSD1306 128×32 I2C 底层驱动 |
| 显示界面 | `app/Display.hpp/.cpp` | `app::Display`：电池/蓝牙/圆点/进度条/文字的绘制与旋转 |
| 串口接收 | `app/UartReceiver.hpp/.cpp` | `app::UartReceiver`：IDLE+DMA 收帧（R8：中断仅清标志+置挂起，拷贝在主循环 `poll()`） |
| 中断 | `Core/Src/stm32f1xx_it.c` (C) | 异常/中断处理，调用 `UartReceiver_HandleIdle()` 与 HAL |
| 外设初始化 | `adc.c/dma.c/gpio.c/i2c.c/tim.c/usart.c` (C) | CubeMX 生成，与 `.ioc` 一致（勿改） |

> 已删除模块：`Concentration_Conversion.*`（→ `Measurement`+`Persistence`）、`OLED.*`（→ `OledPanel`+`Display`）、`FIFO_LOCKFREE.*`（未使用）。死代码归档见 `docs/DEAD_CODE.md`。

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
                     │      main.c  (C, 薄胶水)      │
                     │  初始化编排 / App_Init / 主循环│
                     └───────────────┬──────────────┘
                                     │ extern "C"
                                     ▼
        ┌──────────────────────────────────────────────┐
        │            App.cpp  (C++20, app::App)         │
        │  init / poll / processAdc / onAdcComplete     │
        │  updateWorkStatus / averageFiltering / ...    │
        └──┬────────┬─────────┬─────────┬───────┬───────┘
           ▼        ▼         ▼         ▼       ▼
     Measurement Display  Battery   Inputs  UartReceiver
     (+Persistence) OledPanel Monitor  (Debounce)
           │        │
           ▼        ▼
      stm32f1xx_hal (HAL) ◄── CubeMX 生成外设 (adc/dma/i2c/tim/usart/gpio)
           │
           ▼
      CMSIS/启动/链接脚本

   中断: stm32f1xx_it.c ──► HAL ──► HAL_ADC_ConvCpltCallback (App.cpp)
                            USART1 ─► UartReceiver_HandleIdle (UartReceiver.cpp)
```
- `app::g_app` 为文件级单例，持有 `AppData` 与 `Display`；`main.c` 不保存业务状态。
- C ABI 接缝共 5 个函数：`App_Init`、`App_UpdateDisplay`、`App_Poll`、`HAL_ADC_ConvCpltCallback`、`UartReceiver_HandleIdle`（另保留未使用的 `battery_level`，见 4.9）。
- `main.h` 仅保留 CubeMX 引脚宏、`RX_BUFFER_SIZE` 与 `Error_Handler` 声明；业务类型已迁至 `app/AppState.hpp`。

### 2.2 数据流（主链路）
```
ADC1(PA0 光, PA1 电池) --DMA1_Ch1 循环--> AppData::adcValue[20]
   --(DMA 完成中断) HAL_ADC_ConvCpltCallback (App.cpp)-->
        onAdcComplete()                                  // 仅拷贝样本快照 + 置 adcPending_（R5）
   --(主循环 App_Poll -> processAdc)-->
        averageFiltering(adcSnapshot_, 2ch, 20, adcAvg[2])   // 每通道 10 次平均
        updateWorkStatus()                                    // 状态机推进
        ├─ adcInt      = adcAvg[0]   (光电流/浓度相关)
        └─ battery.setAdc(adcAvg[1]) (电池电压)
        runMeasurementTask()                                  // 校准/测量/保存
        SW/TILT/KEY 消抖（读值≠缓存值累加，>5 次翻转状态）
主循环: App_Poll() --(每 kDisplayPeriodMs)--> Display::update(battery.level(), bluetooth_state=1, progress)
        App_Poll() --(UART 挂起)--> UartReceiver::poll()   // R8：帧长度计算与拷贝
```
- 屏幕进度 `progress`：校准/测量中来自 `Measurement::detectionTime()`，否则来自 `battery.adc()/V_MAX*100`（当 `BattStatus != Normal`）。
- 电池电量 `battery.level()` 历史上从不被自动计算（保持原行为），因此未显式设置时显示为 0。
- `App::poll()` 每轮处理一次挂起的 ADC 快照（与历史“每次 DMA 完成处理一次”节拍一致），并调用 `UartReceiver::poll()`；显示与 UART 处理均在主循环上下文，不阻塞中断。

### 2.3 控制流 / 状态机
`app::WorkState`（`enum class : uint8_t`）：`Init, Save, Calibration, Working, Ready, ErrTilt, ErrOpen, ErrLowPower, ErrNoContainer`（数值 0..8 与原枚举一致）。
- `App::updateWorkStatus()`（原 `CC_set_work_status`）：按状态分派，处理 `Measurement::reset`、UV 灯亮度、`dcCtrlOn/Off`、`Measurement::result`；含各错误态恢复；并用 `STDBY/CHRG` 引脚更新 `BattStatus`（`STDBY` 有效→Standby，否则 `CHRG` 有效→Charging，否则 Normal）。
- `App::runMeasurementTask()`：`Calibration→Measurement::calibrate`；`Working→Measurement::update`；`Save→PersistentStore::save` 后置 `Ready`；`Init→` 空操作。
- 校准/测量计时：`Measurement::detectionTime`（`TimeUnit` 换算），达到 `onceDetectionTime`（初始化传入 `100.0f`）置 `Finished`。
- 注释中保留的编号 `1 / 3 / 4 / 5`（缺 `2`，低电量检测块被注释）与原代码一致（见 R10）。

### 2.4 中断
| 中断 | 优先级 (抢占,子) | 处理 |
|---|---|---|
| `SysTick_Handler` | (15,0) | `HAL_IncTick()` |
| `DMA1_Channel1_IRQHandler` | (4,0) | `HAL_DMA_IRQHandler(&hdma_adc1)` → `HAL_ADC_ConvCpltCallback`：仅拷贝样本快照并置挂起（**已按 R5 精简**） |
| `USART1_IRQHandler` | (0,0) | `UartReceiver_HandleIdle()`（仅清 IDLE 标志 + 置挂起，**已按 R8 精简**），再 `HAL_UART_IRQHandler` |
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
| GPIO | TILT=PA2、KEY=PA7、SW=PA8（输入浮空）；LED=PB12（输出开漏）；STDBY=PB4、CHRG=PB5（输入浮空）；DC_ctrl=PB8（推挽输出） |
| SWJ | `__HAL_AFIO_REMAP_SWJ_NOJTAG()`（保留 SWD） |

时钟：HSE 8 MHz 晶振（PD0/PD1）→ PLL ×9 = 72 MHz；APB1=36 M、APB2=72 M、ADC=12 M。

### 2.6 持久化（Flash）
- 绝对地址 `0x0800FC00`（Flash 最后一页，1 KB）。
- 结构：`app::ConversionValue { float rawValue; uint16_t uvLightLevel; }`，持久化外壳 `StoredConversion { uint32_t magic(=0x12345678); ConversionValue data; }`，按字（32 位）编程；常量 `kDataAddress`/`kMagic` 及默认值（`0.3f`/`70`）在 `app/Persistence.cpp` 内。
- `PersistentStore::load()` 读时校验 magic，无效则给默认值；`save()` 仅在 `ConversionFlag::Finished` 时擦除该页并写入。
- **R3 已修复（S2）**：写前判断改用 `hasValidMagic()`，不再解引用空指针。
- **R4 已修复（S8）**：`STM32F103XX_FLASH.ld` 将 `FLASH LENGTH` 由 64K 改为 **63K**，保留 `0x0800FC00–0x0800FFFF`；`.scripts/patch_cubemx` 的 `patch_linker()` 会在 CubeMX 重新生成后强制恢复该值。
- 布局用 `static_assert`（`sizeof`/`offsetof`/枚举值）冻结。

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
| C 标准 | **C17**（`CMAKE_C_EXTENSIONS` 开启） |
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
- **CubeMX 会重写 `cmake/stm32cubemx/CMakeLists.txt`**；用户源在根 `CMakeLists.txt` 中登记（当前：`Core/Src/app/Persistence.cpp`、`Measurement.cpp`、`BatteryMonitor.cpp`、`Display.cpp`、`OledPanel.cpp`、`UartReceiver.cpp`、`Core/Src/App.cpp`；用户 include：`Core/Inc`、`Core/Inc/app`）。
- `python3 .scripts/patch_cubemx`（幂等）会：重写 `CMakePresets.json` 为规范预设、规范化 `cmake/stm32cubemx/CMakeLists.txt` 路径前缀、**强制链接脚本保留区（FLASH 63K）**、删除陈旧 `cmake/starm-clang.cmake`，并在根 `CMakeLists.txt` 未登记用户源时告警。

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
- **C++ 模块**：用户模块为 `.hpp`/`.cpp`，被 C 包含的头文件需 `extern "C"` 守卫；不要引入 `std::array`/STL（会带入 libstdc++/printf/malloc，见 [4.9](#49-c-使用规范)）。
- **CubeMX 重新生成后**：必须运行 `python3 .scripts/patch_cubemx`（否则 include 路径/预设/链接脚本保留区可能回退）。
- **新增/移动用户文件**：需同步根 `CMakeLists.txt` 用户源列表。
- **构建目录**：`build/Debug`、`build/Release` 均在 gitignore 内。

---

## 4. 开发指引（常见任务）

### 4.1 CubeMX 工作流
- 修改外设/引脚/时钟/中断：**用 STM32CubeMX 打开 `Concentration_V2.ioc` 重新生成**，不要手改生成区域。
- 生成时 `KeepUserCode=true`，**`USER CODE BEGIN/END` 之间的内容会被保留**——用户逻辑只写在这些区域内。
- 生成后**务必**执行 `python3 .scripts/patch_cubemx`（幂等修补 `cmake/stm32cubemx/CMakeLists.txt`、预设，并强制 `STM32F103XX_FLASH.ld` 的 FLASH=63K 保留区）。
- 重新生成可能覆盖生成区域内的一切（含注释与格式）；提交前检查 `git diff` 是否只动了预期的 `USER CODE` 区。
- `main.c` 的 `USER CODE` 仅保留薄钩子（`App_Init()` / 主循环 `App_Poll()`）。

### 4.2 新增/修改用户模块
1. 在 `Core/Inc/app/` 建 `.hpp`、`Core/Src/app/` 建 `.cpp`（UTF-8 + CRLF）。
2. 在根 `CMakeLists.txt` 的用户源列表加入新 `.cpp`（include 路径 `Core/Inc/app` 已包含）。
3. 头文件用保护宏（非保留标识符，如 `APP_MODULE_HPP`）；若会被 C 代码包含，必须加 `extern "C"` 守卫（见 [4.9](#49-c-使用规范)）。
4. 需要访问 HAL 时 `#include "main.h"` 或对应外设头；遵守 C++ 零开销子集与禁堆/禁异常策略。
5. `cmake --preset Debug && cmake --build --preset Debug` 验证；检查导出符号与尺寸基线（见 4.7）。

### 4.3 增加外设 / 引脚
- 走 CubeMX（见 4.1），在 `.ioc` 配置后重新生成；`MX_*_Init()` 会加入生成代码。
- 若需在中断中使用，注意 NVIC 优先级与现有优先级（DMA1_Ch1=4，USART1=0，SysTick=15）的相互影响。
- 新增中断需补 `stm32f1xx_it.c` 的对应 handler（CubeMX 会生成骨架）；复杂逻辑应委托给 `app/` 模块并保持 ISR 简短（见 R5）。

### 4.4 调整显示
- 界面组合入口 `app::Display::update(batteryLevel, bluetoothState, percent)`；`App::updateDisplay()` 调用（`bluetoothState` 固定为 1）。
- `app::Display` 负责布局/旋转/文字：逻辑坐标 32×128，旋转为物理 128×32；区域高度 `kTopAreaHeight=15`/`kMidAreaHeight=15`/`kBottomAreaHeight=98`；文字参数为 `Display.cpp` 匿名命名空间常量。
- 字体/位图为 `Display.cpp` 内 `constexpr` 数组；新增字符需更新 `glyphIndex` 与字模表。
- `app::OledPanel` 负责底层 I2C（地址 `0x78`、超时 100 ms、逐字节页写、整屏刷新）；改动需评估对主循环节奏的影响。
- 底层驱动与界面已解耦：新增控件在 `Display` 中实现，避免改动 `OledPanel`。

### 4.5 修改状态机 / 业务逻辑
- 状态迁移集中在 `Core/Src/App.cpp` 的 `App::updateWorkStatus()`；`App::runMeasurementTask()` 按状态分派。
- 业务计算（滤波/状态机/测量）在**主循环** `App::poll() -> processAdc()` 中执行；`HAL_ADC_ConvCpltCallback` 仅拷贝样本快照并置挂起位（R5 已修复）——新增逻辑应放在 `processAdc()`，不要在 ISR 内做重活。
- 调整计时/采样时注意 `kOnceDetectionTime`、`TimeUnit`、`Measurement::detectionTime` 的关系与单位换算。

### 4.6 修改 Flash 持久化数据（谨慎）
- 结构 `app::ConversionValue` 与 `app::PersistentStore` 位于 `app/Persistence.hpp/.cpp`；改字段会改变持久化格式。
- **改动前必须设计版本/迁移方案**：无 magic 或版本不符时应回退默认值，且避免旧数据被误解析。
- 布局与枚举值有 `static_assert` 守卫；配置页已在链接脚本预留（R4 已修复）。写入语义见 `PersistentStore::save`。

### 4.7 资源约束
- 当前占用（R8 基线，Debug `-O0`）：FLASH **28840 B / 63 KB ≈ 44.7%**，RAM **3808 B / 20 KB ≈ 18.6%**（`text 28828 / data 12 / bss 3792`）。
- 配置页 `0x0800FC00` 已从 FLASH 区域剔除（63K）；固件增长至 63K 上限前需扩容或迁移数据。
- 资源紧张时优先减小 `printf`/浮点重依赖；**不要引入 STL / `std::array`**（会带入 libstdc++/printf/malloc，见 [4.9](#49-c-使用规范)）。

### 4.8 编码 / 风格 / 提交
- UTF-8、CRLF、4 空格；沿用 `.clang-format`。
- 提交信息用类型前缀：`feat:` / `fix:` / `refactor:` / `docs:` / `build:` / `chore:`；保持单一主题、可回滚。
- 纯等价改动（命名/注释/格式/结构拆分）建议注明 `no logic change`。

### 4.9 C++ 使用规范（用户模块）
用户模块（`Core/{Inc,Src}/app/` 与 `App.{h,cpp}`）用 **C++20** 编写；`main.c`/`stm32f1xx_it.c` 与 CubeMX 生成文件保持 **C17**，通过 `extern "C"` 接缝调用。

**命名**：类型/枚举 `PascalCase`（`Measurement`、`WorkState`）；方法 `camelCase`（`updateWorkStatus`）；常量 `kPascalCase`（`kAdcSampleCount`）；全局单例 `g_`（`g_app`、`g_uartReceiver`）；模块置于 `namespace app`。

**语言 / 标准**
- C++20（`CMAKE_CXX_STANDARD 20`、`CMAKE_CXX_STANDARD_REQUIRED ON`、`CMAKE_CXX_EXTENSIONS OFF`）、C17，定义在**根 `CMakeLists.txt`**（不放 CubeMX 可重写的工具链文件）。
- 编译标志 `-fno-rtti -fno-exceptions -fno-threadsafe-statics`；链接以 `g++` 驱动（`LINKER_LANGUAGE CXX`）。

**允许（零开销子集）**：`constexpr`/`consteval`/`constinit`、`enum class`、`if constexpr`、概念（concepts）、`namespace`、类 / RAII、`std::span`、`std::bit_cast`、模板、`[[nodiscard]]`、`static_assert`。

**禁止**：堆分配（`new`/`delete`/`malloc`）、异常、RTTI、虚函数、`iostream`、`std::string`、STL 容器、`std::function`、协程、模块。

**已知陷阱 —— 勿用 `std::array` / libstdc++ 断言**：使用 `std::array`（及 `.fill()`）会拉入 libstdc++ 的 `__glibcxx_assert_fail`，进而带入 `fprintf`/`abort`/`_malloc_r`/stdio，FLASH **+约 5 KB**。缓冲因此使用裸数组 + `std::memset`。**引入任何标准库设施前，先用 `arm-none-eabi-nm` 对比是否多出 libstdc++/stdio/malloc 符号。**

**C ABI 接缝**：会被 C 代码包含的头文件加 `extern "C"` 守卫；`main.c`/`stm32f1xx_it.c` 仅调用 `App_Init`、`App_UpdateDisplay`、`App_Poll`、`HAL_ADC_ConvCpltCallback`、`UartReceiver_HandleIdle`（以及历史保留但未使用的 `battery_level`，其声明在 `App.h` 之外由 C 侧不引用；保留以冻结 ABI）。注意 `--gc-sections` 会裁掉无引用的导出符号。结构体布局用 `static_assert`（`sizeof`/`offsetof`/标准布局）冻结。

**初始化顺序**：`.init_array` 在 `HAL_Init()` **之前**运行；**禁止会触及 HAL/外设的全局构造函数**。单例（`app::g_app`、`app::g_uartReceiver`）为静态度量、不作动态初始化（避免 `= 0` 之类的默认成员初始化器引入动态构造）。

**验证方法**：以“干净重建 + 零新增警告 + 逐语句人工审查 + `static_assert` 布局守卫 + 尺寸/中断顺序核对”为准；`bin`/`hex` 逐字节 sha256 不再作为验收标准。所有阶段**未做硬件验证**。

---

## 5. 已知风险与陷阱（开发时当心）

> 严重度：🔴 高危 / 🟠 中 / 🟡 低 / ⚪ 信息。凡涉及行为改变的一律 **需要人工决策**。以符号名为准。

- **R1 ✅ 已解决 文件编码不一致**：`Concentration_Conversion.c/.h` 原为 GBK，已在阶段 3-1（`30866f0`）转为 UTF-8。
- **R2 ✅ 已解决**：`app::Measurement::init` 中未初始化的 `c_Unit`（写入只读成员 `unit_`）已随 `unit_`/`c_Unit` 一并移除（提交 `01b9986`）；构建现已**零告警**。
- **R3 ✅ 已解决**：原 `Write_Conversion_Value` 内 `Read_Conversion_Value(NULL)` 空指针风险，S2 改为 `PersistentStore::hasValidMagic()`，擦除条件语义不变。
- **R4 ✅ 已解决**：配置页 `0x0800FC00` 已在链接脚本保留（FLASH 63K），并由 `patch_cubemx` 幂等维护。
- **R5 ✅ 已解决**：ADC DMA 完成中断原先承载大量浮点运算与状态机；现已改为“中断仅拷贝 20 个样本快照并置挂起”（`App::onAdcComplete`），滤波/状态机/测量移至主循环 `App::poll() -> processAdc()`（提交 `ea380d8`）。处理节拍保持“每次 DMA 完成一次”。
- **R6 🟡 死代码 / 未使用符号**：大部分死代码已归档到 `docs/DEAD_CODE.md`（`custom_exp10`、`Get_...Detection_Time`、`Phys_DrawHorizontalString` 等）；**UART 收帧输出仍无消费者**（`UartReceiver` 收到帧但无使用方）→ 记录。
- **R7 ✅ 已处理**：未使用的 `FIFO_LOCKFREE` 模块已在 S7 删除（原无内存屏障问题随之消失）。
- **R8 ✅ 已解决**：`app::UartReceiver::handleIdleInterrupt()` 原先在中断内执行 `HAL_UART_DMAStop` + `memcpy` + 重启 DMA；现改为仅清 IDLE 标志 + 置挂起位，帧长度计算与拷贝移至主循环 `UartReceiver::poll()`（利用 RX DMA 循环模式与写位置增量），不再在 ISR 内阻塞（提交 `322d916`）。
- **R9 🟡 TIM2 更新中断“悬空”**：`HAL_TIM_Base_Start_IT(&htim2)` 置位更新中断使能，但无 `TIM2_IRQHandler` 且未使能 `TIM2_IRQn`；若启用 NVIC 将落入 `Default_Handler` 死循环。→ 记录。
- **R10 ⚪ 注释编号缺口**：`App.cpp` 状态机注释 `1.` 后直接 `3.`，缺 `2.`；低电量检测块仍被注释。→ 记录。
- **R11 ✅ 已处理**：`main.h` 中未使用且写法不一致的 `set_light_level`/`set_UVlight_level` 等宏已在 S2 删除。
- **R12 ✅ 已处理**：历史 `MDK-ARM/` 等构建产物已在阶段 1a 清理。
- **R13 ✅ 已处理**：`Concentration_Conversion_updata`→`_update`、`readay`→`ready` 已修正；模块重命名后导出符号随之更新（阶段 F / 重写阶段）。

---

## 附录 A. 基线与静态分析

- 当前基线（Debug `-O0`，63 KB 区域）：FLASH **28840 B（44.7%）**、RAM **3808 B（18.6%）**（`text 28828 / data 12 / bss 3792`）；编译**零告警**。
- 历史基线（阶段 1，C 时代，64 KB）：FLASH 27744 B（42.33%）、RAM 3784 B（18.48%）；`bin` sha256 `dbd2a8f1…8cf6`、`hex` sha256 `d3a782a9…d710`。逐字节 sha256 仅用于阶段 0–5 的等价验证，重写/修复后不再作为验收标准（见 [4.9](#49-c-使用规范)）。
- 静态分析（clang-tidy 历史结果）：`FixedAddressDereference`（对应 R3/R4，均已修）、`DeadStores`（原 `OLED.c` 的 `ny`，已随重写消除）；其余风格类告警已随重写大部分消除。

---

## 附录 B. 重构历史（已完成）

| 阶段 | 内容 | 提交 |
|---|---|---|
| 0–5 | 只读审计；删除 `MDK-ARM/`/`.eide/`/`.cmsis/`；基线+静态分析；合并交接文档（本文件前身）；GBK→UTF-8；`clang-format`；注释统一；局部重命名；提取文件内魔数+修复保留标识符 include 守卫 | `babcada` `f5309d8` `30866f0` `72cf152` `44586be` `fcaa1bc` `b597c69` |
| A–F | 用户模块转 **C++20**（`extern "C"` 守卫、`OledDriver`/`App` 封装、作用域枚举）；修正 `updata`→`_update`、`readay`→`ready` | `17abbc8` `15ae0c1` `3821472` `d65b728` 及后续 |
| S1–S9 | **激进重写**：拆分 `Core/{Inc,Src}/app/` 模块（Config/Hardware/Measurement/Persistence/BatteryMonitor/InputDebounce/AppState/OledPanel/Display/UartReceiver）；启用 **C17**；删死代码（含 `FIFO_LOCKFREE`）归档至 `docs/DEAD_CODE.md`；**修 R3、R4** | `dcaf4b0` `fd05121` `9e8106e` `964e181` `0ae4b16` `bf43933` `57e5f9f` `34c0c2c` `(S9 文档)` |
| F1–F3 | **修 R2、R5、R8**（删除未初始化 `c_Unit`；ADC 处理移出 DMA 中断到主循环 `App::poll()`；UART IDLE 中断改非阻塞） | `01b9986` `ea380d8` `322d916` |

验证方式随阶段演进：阶段 0–5 以“干净重建 + `bin`/`hex` sha256 一致”验证；C++ 阶段 A–F、重写阶段 S1–S9 与修复 F1–F3 以“干净重建 + 零新增警告 + `static_assert` 布局守卫 + 尺寸/中断顺序人工审查”验证。全部**未做硬件验证**。回滚：`git revert <commit>`。

---

## 附录 C. 需要人工决策清单

| 编号 | 事项 | 影响 |
|---|---|---|
| R6 | UART 收帧输出无消费者 | 功能未完成；清理需评估 |
| R9 | TIM2 更新中断“悬空” | 启用 NVIC 会死循环 |
| R10 | 状态机注释编号缺口 + 低电量检测被注释 | 可读性/功能完整性 |

> R2、R3、R4、R5、R8 已修复（提交 `01b9986`/S2/`34c0c2c`/`ea380d8`/`322d916`）；其余均**只记录、未修改**。修复任何一项都可能改变行为，须由开发者决策并单独验证。

---

*文档定位：面向 AI agent 与人类的**开发协作指南**。项目事实、约束与风险以本文件为准；行为相关改动一律先确认，未实机验证时标注“未做硬件验证”。*
