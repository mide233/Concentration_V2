# AGENTS.md — Concentration_V2 交接与协作规范

> 本文件把原计划的 6 篇文档（HANDOVER / ARCHITECTURE / BUILD / RISKS / REFACTOR_PLAN / CHANGELOG_REFACTOR）合并为一篇，并纳入面向 AI agent 与人类接手者的项目治理规则。
>
> 目标 MCU：**STM32F103C8T6**（LQFP48，64 KB Flash / 20 KB RAM）。
> 构建系统：**CMake + Ninja**，工具链 **arm-none-eabi-gcc 15.2.1**。
> 本文档编写时仓库处于**重构初期**；所有“风险”仅记录、未修改代码。凡未实机运行之处均标注 **未做硬件验证**。

---

## 0. 项目治理规则（最高优先级，AI agent 必读）

### 0.1 总原则
1. **行为等价优先于代码漂亮。** 任何可能改变功能、逻辑、时序、中断行为、优化结果、内存布局、外设寄存器访问、通信协议、ABI 或构建产物的改动，默认禁止。
2. 任务不是重写，而是**保守重构 + 交接文档化**。
3. 发现疑似 bug、竞态、未定义行为、死代码、危险写法，**只记录到第 4 章 RISKS，不自动修改**。
4. 一次只做**一类**改动，保持 diff 最小；每步必须**可编译、可验证、可回滚**。
5. 每完成一步，必须说明：**改了什么 / 没改什么 / 如何验证 / 还有什么风险**。
6. 无法验证硬件行为时，必须明确写 **“未做硬件验证”**，不得假装已验证。
7. 遇到不确定的地方，**停下来问**；不猜硬件行为、不猜原作者意图。

### 0.2 禁止修改的内容（除非明确批准）
- 控制流、循环顺序、条件判断、状态机迁移、超时、重试、延时、看门狗喂狗位置。
- 中断服务程序名、中断优先级、临界区、锁、原子操作、内存屏障。
- `volatile`、`const`、`static`、`inline`、`packed`、`aligned`、`section`、`weak`、`alias`、`interrupt` 等修饰。
- 寄存器读写顺序、位操作掩码、移位、位域、外设初始化顺序。
- 启动文件、链接脚本、向量表、汇编文件、编译器选项、预处理宏的值与条件编译逻辑。
- 公共 API、导出函数签名、全局变量名、结构体布局、枚举值、联合体布局、协议字段、持久化格式、配置项键名。
- 已有依赖、构建系统、烧录配置、时钟/PLL/电源/看门狗/DMA/缓存/MPU 配置。
- “看似无用”的代码、空循环、`nop`、`delay`、强制类型转换、对齐填充、保留字段。
- 引入新库、新 RTOS API、动态内存、异常、RTTI、STL、printf 重依赖（除非项目原本已用）。
- 大规模自动格式化整个仓库（除非明确同意并单独提交）。
- **CubeMX 控制/生成的内容**（`Core/Src/*.c` 的 `MX_*_Init`、`.ioc`、`STM32F103XX_FLASH.ld`、`startup_stm32f103xb.s`、`cmake/stm32cubemx/CMakeLists.txt`）——如确需修改，先请求。

### 0.3 允许的整理（每步均需验证）
- 按项目既有风格或 `.clang-format` 做**局部**格式化（优先只格式化正在修改的文件；阶段 3 起可整 `Core/`）。
- 补充注释、函数头注释、模块说明（**不得删除原作者注释**，除非确认是纯错别字）。
- 修正局部变量、函数内参数、`static` 函数、文件内宏的命名，前提是全仓库引用同步修改且不影响外部符号。
- 将明显的魔法数字提取为 `const`/宏，值必须完全不变。
- 整理头文件保护、include 顺序、重复 include（须确认不改变条件编译）。
- 补充交接文档、模块依赖图、构建说明、风险清单。
- 对导出符号、中断函数、寄存器宏、协议字段、结构体字段**默认不改名**；如必须改，先给出兼容方案并等待确认。

### 0.4 工作流程（严格按阶段，未获“执行阶段 X”指令前只输出审计/计划/文档）
- **阶段 0**：只读审计（不改文件）。
- **阶段 1**：建立基线（构建 + 指标 + 静态检查）。
- **阶段 2**：生成交接文档。
- **阶段 3**：低风险整理（格式化、注释、文档补充），单独提交。
- **阶段 4**：命名整理（局部/static/文件内宏，先全仓库搜索引用），单独提交。
- **阶段 5**：结构整理（默认不做，需明确批准）。

### 0.5 每步验证要求
1. `git diff` 确认改动范围符合计划。
2. 编译通过，尽量零新增警告。
3. 运行已有测试；没有则说明。
4. 对比修改前后的 size / map 关键符号大小；如可行对比关键函数反汇编。
5. 如条件允许做硬件冒烟测试，否则注明 **未做硬件验证**。
6. 失败立即回滚，不叠加改动。

### 0.6 每轮回复格式
`阶段 / 目标 / 范围 / 未做·不做 / 变更文件 / 关键 diff 摘要 / 验证结果 / 风险与不确定项 / 需要我确认的问题 / 下一步建议`

### 0.7 遇到不确定时
不猜。列出可选方案、影响范围、风险，然后等待确认。若必须改变行为才能修复的问题，只记录到第 4 章并标记 **“需要人工决策”**。

---

## 1. HANDOVER — 项目概览（原 HANDOVER.md）

### 1.1 项目是什么
一个基于 STM32F103C8T6 的**便携式浓度/电量检测仪**固件（工程名 `Concentration_V2`）：
- 通过 **ADC1（PA0 光电流 / PA1 电池分压）** 采样，DMA 循环搬运。
- 通过 **I2C1 SSD1306 OLED（128×32）** 显示电量、蓝牙状态、进度条与文本。
- 通过 **TIM2**（1 kHz）产生 ADC 触发（CH2，TRGO=OC2REF）与 UV 灯 PWM（CH4）。
- 通过 **USART1（9600）** 接收上位机数据帧（IDLE+DMA）。
- 按键/开关：`TILT=PA2`、`KEY=PA7`、`SW=PA8`；充电/待机指示 `SHDBY=PB4`、`CHRG=PB5`；负载控制 `DC_ctrl=PB8`；状态灯 `LED=PB12`。
- 校准结果持久化到 Flash 末页绝对地址 `0x0800FC00`。

> 注：原设计意图与算法细节缺少说明文档，部分逻辑（UART 收帧、低电量检测）疑似未完成，见第 4 章。

### 1.2 仓库结构（清理后）
```
Concentration_V2/
├── Core/
│   ├── Inc/   main.h, adc.h, dma.h, gpio.h, i2c.h, tim.h, usart.h,
│   │          stm32f1xx_hal_conf.h, stm32f1xx_it.h,
│   │          Concentration_Conversion.h, FIFO_LOCKFREE.h, OLED.h
│   └── Src/   main.c, adc.c, dma.c, gpio.c, i2c.c, tim.c, usart.c,
│              stm32f1xx_it.c, stm32f1xx_hal_msp.c,
│              syscalls.c, sysmem.c, system_stm32f1xx.c,
│              Concentration_Conversion.c, FIFO_LOCKFREE.c, OLED.c
├── Drivers/        STM32F1xx_HAL_Driver + CMSIS（第三方，勿改）
├── cmake/          gcc-arm-none-eabi.cmake（工具链）、stm32cubemx/CMakeLists.txt（CubeMX 生成）
├── tools/          openocd.cfg
├── .scripts/       patch_cubemx（CubeMX 生成后修补脚本）
├── .vscode/        tasks.json（构建/烧录任务）
├── CMakeLists.txt, CMakePresets.json
├── STM32F103XX_FLASH.ld, startup_stm32f103xb.s
├── Concentration_V2.ioc, .mxproject
├── .clang-format, .clangd, .gitignore, .dockerignore
└── AGENTS.md（本文件）
```
> 清理历史：`MDK-ARM/`、`.eide/`、`.cmsis/` 已在阶段 1a 删除（提交 `babcada`），见第 6 章。

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
  Concentration_Conversion_task_init(&my_data)   // 启动 DMA/ADC/TIM/UART，读取持久化参数
  OLED_Init(); OLED_Set_Dot(1)
  while (1) { OLED_Update(my_data.BAT.battery_level, 1, my_data.progress); HAL_Delay(113); }
```
- 业务闭环主要在 **ADC DMA 完成中断** 里推进（见第 2 章），主循环仅负责 OLED 刷新。
- 系统时钟：SYSCLK 72 MHz，FLASH_LATENCY_2，ADC 时钟 12 MHz。

### 1.4 主要模块
| 模块 | 文件 | 职责 |
|---|---|---|
| 应用主控 | `Core/Src/main.c` | 初始化编排、状态机、ADC 中断回调、按键消抖、电池计算 |
| 浓度/校准 | `Concentration_Conversion.c/.h` | 校准/测量状态机，Flash 持久化（`0x0800FC00`） |
| 显示 | `OLED.c/.h` | SSD1306 128×32 驱动，逻辑缓冲→物理旋转，字体与图形 |
| 环形缓冲 | `FIFO_LOCKFREE.c/.h` | 无锁 SPSC 环形队列（`uint16_t`），**疑似未使用** |
| 中断 | `stm32f1xx_it.c` | 异常/中断处理，含 USART1 IDLE 收帧逻辑 |
| 外设初始化 | `adc.c/dma.c/gpio.c/i2c.c/tim.c/usart.c` | CubeMX 生成，与 `.ioc` 一致（勿改） |

### 1.5 如何构建 / 烧录 / 调试
见第 3 章 BUILD。烧录（需实机 ST-Link）：
```
openocd -f tools/openocd.cfg -c "program build/Debug/Concentration_V2.elf verify reset exit"
```
调试：SWD（PA13/PA14），`stm32f1xx_hal_msp.c` 中 `__HAL_AFIO_REMAP_SWJ_NOJTAG()` 保留 SWD、关闭 JTAG。
> 当前环境无目标硬件，**未做硬件验证**。

---

## 2. ARCHITECTURE（原 ARCHITECTURE.md）

### 2.1 模块职责与依赖
```
                      ┌─────────────────────────┐
                      │         main.c          │
                      │ (init/state machine/ISR)│
                      └───┬─────────┬───────┬───┘
              ┌───────────┘         │       └────────────┐
              ▼                     ▼                    ▼
   Concentration_Conversion    OLED.c              FIFO_LOCKFREE.c
   (状态机 + Flash 持久化)     (SSD1306/I2C)       (SPSC 环形, 疑似未用)
              │                     │
              ▼                     ▼
        stm32f1xx_hal (HAL) ◄── CubeMX 生成外设 (adc/dma/i2c/tim/usart/gpio)
              │
              ▼
        CMSIS/启动/链接脚本

   中断: stm32f1xx_it.c ──► HAL ──► main.c 的回调
```
- `main.h` 汇总引脚宏、`my_data_t` 等类型，并 include `Concentration_Conversion.h`。
- `OLED.h` include `main.h`（耦合较紧，但无直接循环包含）。
- `main.c` 是唯一“胶水层”，其余用户模块相对独立。

### 2.2 数据流（主链路）
```
ADC1(PA0 光, PA1 电池) --DMA1_Ch1 循环--> my_data.ADC_value[20]
   --(DMA 完成中断) HAL_ADC_ConvCpltCallback-->
        Average_filtering(ADC_value, 2ch, 20, ADC_avg[2])   // 每通道 10 次平均
        ├─ my_data.ADC_INT      = ADC_avg[0]   (光电流/浓度相关)
        └─ my_data.BAT.adc_BAT  = ADC_avg[1]   (电池电压)
        ADC_avg[1] --> battery_level() --> my_data.BAT.battery_level (0..5)
        CC_set_work_status(&my_data)          // 状态机推进
        Concentration_Conversion_task(&my_data)
        SW/TILT/KEY 消抖（读值≠缓存值累加，>5 次翻转状态）
主循环: OLED_Update(battery_level, bluetooth_state=1, progress)
```
- 屏幕进度 `progress`：校准/测量中来自 `detection_time`，否则来自 `BAT.adc_BAT/V_MAX*100`（当 `BAT_status != NORMAL`）。
- `my_data.hardware_status` 指向全局 `hardware_status`。

### 2.3 控制流 / 状态机
`work_status_e`：`INIT, SAVE, calibration, working, readay, err_TILT, err_open, err_low_pow, err_no_cap`。
- `CC_set_work_status()`：按 `work_status` 分派，处理 `Concentration_Conversion_Reset`、UV 灯亮度更新、`DC_ctrl_ON/OFF`、`get_Result`；含各错误态恢复；并用 `SHDBY/CHRG` 引脚更新 `BAT_status`（`PIN_RESET→SHDBY`，否则 `CHRG` 为 reset→`CHRG`，否则 `NORMAL`）。
- `Concentration_Conversion_task()`：`calibration→Concentration_Conversion_calibration`；`working→Concentration_Conversion_updata`；`SAVE→Write_Conversion_Value` 后置 `readay`；`INIT→` 空操作。
- 校准/测量计时：`detection_time`（`time_Unit` 换算），达到 `once_detection_time`（初始化传入 `100.0f`）置 `finish`。
- **注释编号缺口**：`main.c` 中标签 `1. 意图状态转换` 后直接 `3. 低电量检测`，缺 `2`；且低电量检测块被注释掉。

### 2.4 中断
| 中断 | 优先级 (抢占,子) | 处理 |
|---|---|---|
| `SysTick_Handler` | (15,0) | `HAL_IncTick()` |
| `DMA1_Channel1_IRQHandler` | (4,0) | `HAL_DMA_IRQHandler(&hdma_adc1)` → 触发 `HAL_ADC_ConvCpltCallback`（**承载大量浮点/状态机工作**） |
| `USART1_IRQHandler` | (0,0) | 先执行用户 IDLE 收帧逻辑（`DMAStop`+`memcpy`+重启 DMA），再 `HAL_UART_IRQHandler` |
| NMI/HardFault/MemManage/BusFault/UsageFault | — | `while(1)` |
| SVC/DebugMon/PendSV | — | 空 |

- NVIC 仅使能 `DMA1_Channel1_IRQn` 与 `USART1_IRQn`。
- **注意**：`HAL_TIM_Base_Start_IT(&htim2)` 置位了 TIM2 更新中断使能位，但工程中**没有 `TIM2_IRQHandler`，也未使能 `TIM2_IRQn`**；若将来使能该 NVIC 中断，会落入 `Default_Handler` 的 `b Infinite_Loop` 而死循环（见风险 R9，仅记录）。

### 2.5 外设与引脚（与 `.ioc` 一致，勿改）
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
- 结构：`StoredConversion_t { uint32_t magic=0x12345678; Conversion_value_t data; }`，按字（32 位）编程。
- `Read_Conversion_Value()` 读时校验 magic；无效则给默认 `Raw_value=0.3f, UVlight_level=70`。
- `Write_Conversion_Value()` 仅在 `Conversion_flag==finish` 时擦除该页并写入；写入前条件判断见风险 R3（高危）。
- **该页未在链接脚本中保留**（见风险 R4）。

---

## 3. BUILD（原 BUILD.md）

### 3.1 工具链
| 组件 | 版本（实测） |
|---|---|
| arm-none-eabi-gcc | 15.2.1（Arm GNU Toolchain 15.2.Rel1, Build arm-15.86, 20251203） |
| cmake | 3.28.3 |
| ninja | 1.11.1 |
| binutils/objcopy | 2.45.1 |
| clang-tidy / clang-format | Ubuntu LLVM 22.1.8 |
| C 标准 | C11（`CMAKE_C_EXTENSIONS` 开启） |

### 3.2 构建命令（对应 `.vscode/tasks.json`）
```
cmake --preset Debug                 # 配置（生成 build/Debug）
cmake --build --preset Debug         # 构建（默认任务）
cmake --build --preset Release       # Release 构建
python3 .scripts/patch_cubemx        # CubeMX 生成后修补（幂等）
openocd -f tools/openocd.cfg -c "program build/Debug/Concentration_V2.elf verify reset exit"   # 烧录
openocd -f tools/openocd.cfg -c "init; reset halt; stm32f1x mass_erase 0; exit"               # 全片擦除
```
- 预设见 `CMakePresets.json`：生成器 Ninja，工具链文件 `cmake/gcc-arm-none-eabi.cmake`，`CMAKE_EXPORT_COMPILE_COMMANDS=ON`。
- **CubeMX 会重写 `cmake/stm32cubemx/CMakeLists.txt`**；用户源在根 `CMakeLists.txt` 中登记（当前：`Concentration_Conversion.c`、`FIFO_LOCKFREE.c`、`OLED.c`；用户 include：`Core/Inc`）。

### 3.3 编译配置
- Debug：`-O0 -g3`；Release：`-Os -g0`。
- 公共：`-mcpu=cortex-m3 -Wall -fdata-sections -ffunction-sections`。
- 链接：`-T STM32F103XX_FLASH.ld --specs=nano.specs -Wl,-Map=Concentration_V2.map -Wl,--gc-sections -Wl,--print-memory-usage -lm`。

### 3.4 产物
`build/Debug/`：`Concentration_V2.elf`、`.bin`、`.hex`、`.map`（`build/` 已 gitignore）。

### 3.5 基线指标（阶段 1b，Debug / 干净构建）
| 指标 | 值 |
|---|---|
| text / data / bss | 27732 / 12 / 3768（dec 31512） |
| FLASH 占用 | 27744 B / 64 KB = 42.33% |
| RAM 占用 | 3784 B / 20 KB = 18.48% |
| bin / hex / elf / map | 27744 / 78120 / 971748 / 576168 字节 |
| Flash 镜像末端 | `0x08006C60`（配置页 `0x0800FC00` 之上尚余 36768 B） |
| bin sha256 | `dbd2a8f1eedd8ccaa8b7e0f80b4b151e16e324ae874a7c2c7451f86016be8cf6` |
| bin md5 | `643237fd511430fd27b6d672b995ff1e` |
| hex sha256 | `d3a782a95d64c0dc880fa87fb1c78859c06366c0d8242f85903671469cc8d710` |
| elf sha256 | `110f6a5bfc8f05c210566650db375fdbd5dd7a510fe8e2963f7db4501d2691b3` |

编译警告（既有 2 条，未消除）：
1. `Concentration_Conversion.c:138: 'c_Unit' is used uninitialized [-Wuninitialized]`（= 风险 R2）
2. `OLED.c:249: 'Phys_DrawHorizontalString' defined but not used [-Wunused-function]`（= 风险 R6）

静态分析（阶段 1c，clang-tidy，仅 `Core/Src`）关键结果：
- `clang-analyzer-core.FixedAddressDereference`：`Concentration_Conversion.c:12`、`:35`（固定地址解引用）。
- `clang-analyzer-deadcode.DeadStores`：`OLED.c:228`（`ny` 初始化后未读）。
- `clang-analyzer-security.insecureAPI`：`OLED.c:148/283/324/325`（`memset`，信息性）。
- 扩展集（风格类，不建议在保守重构中改）：`bugprone-reserved-identifier`×14、`easily-swappable-parameters`×12、`performance-no-int-to-ptr`×11、`narrowing-conversions`×11、`macro-parentheses`×6、`branch-clone`×1。

### 3.6 常见问题
- **找不到 `clang` 编译器**：本环境只有 `clang-tidy`/`clang-format`，无 `clang`（用户表述的“clang”指静态分析，已按 clang-tidy 执行）。
- **CubeMX 重新生成后**：必须运行 `python3 .scripts/patch_cubemx`（否则 include 路径/预设可能回退）。
- **改用户文件位置**：需同步根 `CMakeLists.txt` 的 `USER_*` 源列表。
- **构建目录**：`build/Debug`、`build/Release` 均在 gitignore 内，不提交。

---

## 4. RISKS（原 RISKS.md）——**只记录，不修改**

> 严重度：🔴 高危 / 🟠 中 / 🟡 低 / ⚪ 信息。凡涉及行为改变的一律 **需要人工决策**。

- **R1 ⚪ 文件编码不一致**：`Concentration_Conversion.c/.h` 为 GBK，其余 UTF-8；所有文件 CRLF。任何转码/重排可能改变注释字节。**已获决策**：阶段 3 单独提交转为 UTF-8（保留注释语义）。
- **R2 🟠 未初始化变量（UB）**：`Concentration_Conversion.c:115` `float c_Unit;` 未赋值，`:138` 使用 `Unit = time_Unit * c_Unit`；且字段 `Unit` 全仓只写不读。→ **需要人工决策**。（编译器已告警）
- **R3 🔴 `Read_Conversion_Value(NULL)` 空指针解引用**：`Concentration_Conversion.c:35`。该函数两条分支都会解引用 `out`。首次保存时 `*checkAddr==0xFFFFFFFF` 短路，安全；**一旦 Flash 已含 magic（即第二次进入保存判断），将真实写地址 0**，STM32F1 上极可能触发总线/硬件错误。→ **需要人工决策**。
- **R4 🟠 配置页未在链接脚本保留**：`0x0800FC00` 页未被 `STM32F103XX_FLASH.ld` 预留。当前 Flash 镜像止于 `0x08006C60`，尚余 36768 B，**暂无重叠**；固件一旦增长越过 `0x0800FC00` 即冲突。→ **需要人工决策 / 持续关注**。
- **R5 🟠 中断中做重活**：`HAL_ADC_ConvCpltCallback` 在 DMA 中断（优先级 4）上下文执行大量浮点运算与状态机，可能造成抖动/长中断。→ **需要人工决策**。
- **R6 🟡 死代码 / 未使用符号**：UART 收帧路径 `rx_frame_ready`/`rx_work_buffer`/`rx_frame_len` 无消费者；`msg_fifo`/`msg_buf`（FIFO 子系统）、全局 `I`、`custom_exp10`、`Get_Concentration_Conversion_Detection_Time`、`test_t`、`current_Unit_e`、宏 `set_light_level`、`Phys_DrawHorizontalString` 均未使用（部分已被 `--gc-sections` 移除）。→ 记录。
- **R7 🟡 FIFO 无内存屏障**：`FIFO_LOCKFREE` 使用 `volatile` head/tail，但无显式内存屏障。SPSC 在 Cortex-M3 上通常可用，但严格性存疑。→ 记录。
- **R8 🟡 UART 收帧在中断内阻塞操作**：`USART1_IRQHandler` 中 `HAL_UART_DMAStop` + `memcpy` + 重启 DMA 均在中断上下文执行，且置于 `HAL_UART_IRQHandler` 之前。→ 记录。
- **R9 🟡 TIM2 更新中断“悬空”**：`HAL_TIM_Base_Start_IT(&htim2)` 置位更新中断使能，但无 `TIM2_IRQHandler` 且未使能 `TIM2_IRQn`。当前不触发；若启用 NVIC 将落入 `Default_Handler` 死循环。→ 记录。
- **R10 ⚪ 注释编号缺口**：`main.c` 状态机注释 `1.` 后直接 `3.`，缺 `2.`；低电量检测块被注释掉。→ 记录。
- **R11 ⚪ 未使用宏写法不一致**：`main.h` 的 `set_light_level(my_data, level)` 用 `.` 而非 `->`（且未使用）。→ 记录。
- **R12 ⚪ 遗留构建产物**：历史上 `MDK-ARM/` 曾提交 Keil 编译产物（`.o/.crf/.map/.hex/.axf` 等），已在阶段 1a 连同 `.eide/`、`.cmsis/` 一并清理。→ 已处理。

---

## 5. REFACTOR_PLAN（原 REFACTOR_PLAN.md）——按风险从低到高

| 序 | 阶段 | 内容 | 风险 | 状态 |
|---|---|---|---|---|
| 0 | — | 只读审计 | 无 | ✅ 完成 |
| 1a | 1 | 删除 `MDK-ARM/`、`.eide/`、`.cmsis/`（构建中立） | 低 | ✅ 提交 `babcada` |
| 1b | 1 | 干净 Debug 基线 + 指标 + 风险核实 | 无 | ✅ 完成 |
| 1c | 1 | clang-tidy + clang-format 报告 | 无 | ✅ 完成 |
| 2 | 2 | 交接文档（本文件） | 无 | 🔄 进行中 |
| 3 | 3 | GBK→UTF-8（单独提交，先出 diff 供确认） | 低 | ⏳ 待批 |
| 3 | 3 | 整 `Core/` 按 `.clang-format` 格式化（单独提交） | 低 | ⏳ 待批 |
| 3 | 3 | 补充注释/函数头/模块说明（不删原作者注释） | 低 | ⏳ 待批 |
| 4 | 4 | 局部/`static`/文件内宏命名整理（先全仓搜索引用） | 中 | ⏳ 待批 |
| 5 | 5 | 结构整理（提取常量、拆纯函数、头文件整理） | 中 | ⏳ 需单独批准 |

**不建议做（除非另行批准）**：修复 R2/R3/R4/R5 等任何改变行为的项；重排 HAL/中断/寄存器相关代码；文件拆分与架构重写。

---

## 6. CHANGELOG_REFACTOR（原 CHANGELOG_REFACTOR.md）

### 基线（阶段 1b，Debug）
- 工具链 arm-none-eabi-gcc 15.2.1 / cmake 3.28.3 / ninja 1.11.1。
- size：text 27732 / data 12 / bss 3768；FLASH 42.33%，RAM 18.48%。
- bin sha256 `dbd2a8f1…8cf6`（完整值见 3.5）。警告 2 条。

### 阶段 1a — 清理无关工程文件
- **变更**：删除 `MDK-ARM/`(108)、`.eide/`(3)、`.cmsis/`(261)，共 372 文件、-105851 行。
- **提交**：`babcada` `chore: remove unused Keil/EIDE project files and CMSIS pack`。
- **回滚**：`git revert babcada`。
- **验证**：删除前后产物**逐字节一致**（bin sha256 相同），配置+构建通过。
- **未做**：硬件验证。

### 阶段 1b — 基线记录
- **变更**：无（只读构建/分析）。日志 `build/_baseline/`。
- **验证**：configure/build OK；警告 2 条；核实 R2（编译器确认）、R3（逻辑确认）、R4（map 确认无重叠）。

### 阶段 1c — 静态检查
- **变更**：无。日志 `build/_baseline/clang-tidy.log`、`clang-tidy-extended.log`。
- **验证**：clang-tidy（analyzer）产出 7 条诊断；clang-format 显示 6/7 用户文件不符合现有风格。

### 阶段 2 — 交接文档
- **变更**：新增 `AGENTS.md`（合并 6 篇）。
- **验证**：纯文档，不影响构建。

---

*文档状态：阶段 2 初稿（合并 6 篇为 1 篇）。涉及行为的问题一律“只记录、不修改”，标注“未做硬件验证”。*
