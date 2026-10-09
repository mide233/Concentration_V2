#ifndef APP_BLUETOOTH_HPP
#define APP_BLUETOOTH_HPP

#include <cstdint>

#include <cstring>

#include "app/UartReceiver.hpp"
#include "main.h"
#include "usart.h"

namespace app {

/* JDY-31 蓝牙（串口透传）连接与收发。
 *
 * 状态检测：软件方式。连接时模块在串口输出状态文本（AT+ENLOG=1 开启），
 *           本类以宽松子串匹配识别 CONNECT/DISCONNECT，无需额外 GPIO。
 * 收发协议：文本行协议，行尾以 '\n' 结束（容忍 "\r\n"）。
 * AT 配置：上电等待模块就绪后，依次发送 AT+VERSION / AT+NAME<名> / AT+ENLOG1；
 *           若模块已处于连接态（热复位）则 AT 无应答，超时后落回被动 Idle，不阻塞启动。
 * 设备名：运行时由 STM32F103 芯片唯一 ID（96-bit UID，0x1FFFF7E8）低 16 位生成
 *         "CONC_XXXX"（4 位大写十六进制）。
 */
inline constexpr uint16_t kLineMax = 96;               // 单行最大长度（含结尾）
inline constexpr uint16_t kTxStagingSize = 96;         // 单条待发文本最大长度
inline constexpr uint8_t kTxSlots = 4;                 // 待发队列槽数
inline constexpr uint8_t kRxSlots = 4;                 // 已收行队列槽数
inline constexpr uint16_t kNameMax = 16;               // 设备名缓冲
inline constexpr uint32_t kAtTimeoutMs = 500;          // AT 指令应答超时
inline constexpr uint8_t kAtRetries = 3;               // AT 探测重试次数
inline constexpr uint32_t kBootWaitMs = 800;           // 上电等待模块就绪
inline constexpr uint32_t kReprobeMs = 2000;           // 未配置成功时的重试间隔
inline constexpr char kLineEnd = '\n';                 // 文本行结束符
inline constexpr char kNamePrefix[] = "CONC_";         // 设备名前缀
inline constexpr uint32_t kUidBase = 0x1FFFF7E8u;      // STM32F103 96-bit UID

class BluetoothLink {
public:
    // 复位内部状态、由芯片 UID 生成设备名，进入上电等待阶段。
    void init() {
        buildDeviceName();

        txHead_ = 0;
        txTail_ = 0;
        txBusy_ = false;
        rxHead_ = 0;
        rxTail_ = 0;
        lineLen_ = 0;

        atRetries_ = 0;
        atReplied_ = false;
        atOk_ = false;
        connected_ = false;
        configured_ = false;
        phase_ = AtPhase::BootWait;
        phaseStart_ = HAL_GetTick();
        cmdStart_ = phaseStart_;
    }

    // 主循环推进：消费 UART 帧 -> 组装行 -> AT 状态机 -> 发送队列。
    void poll() {
        uint8_t frame[RX_BUFFER_SIZE];
        const uint16_t n = g_uartReceiver.takeFrame(frame, sizeof(frame));
        if (n > 0u) {
            feed(frame, n);
        }

        runAtStateMachine();
        pumpTx();
    }

    [[nodiscard]] bool connected() const { return connected_; }

    [[nodiscard]] bool available() const { return rxHead_ != rxTail_; }

    // 发送一行文本（自动补 '\n'）；仅在已连接且队列未满时成功。
    bool sendLine(const char* s) {
        if (!connected_ || s == nullptr) {
            return false;
        }
        const size_t sl = std::strlen(s);
        if (sl >= kTxStagingSize - 1u) {
            return false;
        }
        char tmp[kTxStagingSize];
        std::memcpy(tmp, s, sl);
        tmp[sl] = kLineEnd;
        return enqueue(tmp, sl + 1u);
    }

    // 取出一行已收数据（去掉行尾）；返回长度，无数据返回 0。
    uint16_t readLine(char* out, uint16_t cap) {
        if (rxHead_ == rxTail_ || out == nullptr || cap == 0u) {
            return 0;
        }
        const char* src = rxBuf_[rxHead_];
        uint16_t len = 0;
        while (src[len] != '\0' && static_cast<uint16_t>(len + 1u) < cap) {
            out[len] = src[len];
            ++len;
        }
        out[len] = '\0';
        rxHead_ = static_cast<uint8_t>((rxHead_ + 1u) % kRxSlots);
        return len;
    }

    // TX 完成中断回调（由 HAL_UART_TxCpltCallback 转发至此）。
    void onTxComplete() {
        if (!txBusy_) {
            return;
        }
        txBusy_ = false;
        txHead_ = static_cast<uint8_t>((txHead_ + 1u) % kTxSlots);
    }

private:
    enum class AtPhase : uint8_t {
        BootWait = 0,
        Probe,
        SetName,
        EnableLog,
        Idle,
        Connected,
    };

    [[nodiscard]] static uint32_t nowMs() { return HAL_GetTick(); }

    // 由芯片 UID 低 16 位生成 "CONC_XXXX"。
    void buildDeviceName() {
        const uint32_t uid0 = *reinterpret_cast<const volatile uint32_t*>(kUidBase);
        static constexpr char kHex[] = "0123456789ABCDEF";
        std::memcpy(deviceName_, kNamePrefix, sizeof(kNamePrefix) - 1u);
        for (uint8_t i = 0; i < 4u; ++i) {
            const uint8_t nibble = static_cast<uint8_t>((uid0 >> (12 - 4 * i)) & 0xFu);
            deviceName_[5 + i] = kHex[nibble];
        }
        deviceName_[9] = '\0';
    }

    // 写入发送队列（s 已含结束符）；返回是否成功。
    bool enqueue(const char* s, size_t len) {
        const uint8_t next = static_cast<uint8_t>((txTail_ + 1u) % kTxSlots);
        if (next == txHead_) {
            return false; // 队列满
        }
        if (len > kTxStagingSize - 1u) {
            len = kTxStagingSize - 1u;
        }
        std::memcpy(txBuf_[txTail_], s, len);
        txBuf_[txTail_][len] = '\0';
        txLen_[txTail_] = static_cast<uint16_t>(len);
        txTail_ = next;
        return true;
    }

    // 组装一条 AT 命令（自动补 "\r\n"）并入队。arg 可为 nullptr。
    bool enqueueAt(const char* cmd, const char* arg = nullptr) {
        char tmp[kTxStagingSize];
        size_t n = std::strlen(cmd);
        if (n >= kTxStagingSize - 3u) {
            return false;
        }
        std::memcpy(tmp, cmd, n);
        if (arg != nullptr) {
            const size_t al = std::strlen(arg);
            if (n + al >= kTxStagingSize - 3u) {
                return false;
            }
            std::memcpy(tmp + n, arg, al);
            n += al;
        }
        tmp[n++] = '\r';
        tmp[n++] = '\n';
        tmp[n] = '\0';
        return enqueue(tmp, n);
    }

    // 真正启动一次发送（一次发出完整一条，字符间无间隔）。
    // 使用中断发送：复用已使能的 USART1_IRQn（HAL_UART_IRQHandler ->
    // HAL_UART_TxCpltCallback），不依赖 USART1 TX DMA（DMA1_Channel4 的 NVIC 中断未使能、
    // 也无对应 IRQHandler，发送完成回调永不触发会使发送队列永久卡死）。
    void pumpTx() {
        if (txBusy_ || txHead_ == txTail_) {
            return;
        }
        txBusy_ = true;
        HAL_UART_Transmit_IT(
            &huart1, reinterpret_cast<uint8_t*>(txBuf_[txHead_]), txLen_[txHead_]);
    }

    // 小写化（仅用于状态文本的宽松匹配）。
    static char toLower(char c) {
        return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
    }

    // 大小写不敏感子串查找（needle 需为小写）。
    static bool containsCi(const char* hay, const char* needle) {
        for (; *hay != '\0'; ++hay) {
            const char* h = hay;
            const char* n = needle;
            while (*n != '\0' && *h != '\0' && toLower(*h) == *n) {
                ++h;
                ++n;
            }
            if (*n == '\0') {
                return true;
            }
        }
        return false;
    }

    // 依据文本判定连接状态（DISCONN 优先于 CONN）；识别到状态文本返回 true。
    // 仅识别模块状态行（以 '+' 开头），避免把对端数据中恰好含 conn 的行误判。
    bool applyStatus(const char* s) {
        if (s == nullptr || s[0] != '+') {
            return false;
        }
        if (containsCi(s, "disconn")) {
            connected_ = false;
            phase_ = AtPhase::Idle;
            phaseStart_ = nowMs();
            return true;
        }
        if (containsCi(s, "conn")) {
            connected_ = true;
            phase_ = AtPhase::Connected;
            return true;
        }
        return false;
    }

    // 字节流 -> 行组装（以 '\n' 结束，忽略 '\r'）。
    void feed(const uint8_t* data, uint16_t len) {
        for (uint16_t i = 0; i < len; ++i) {
            const uint8_t c = data[i];
            if (c == '\r') {
                continue;
            }
            if (c == '\n') {
                if (lineLen_ > 0u) {
                    lineBuf_[lineLen_] = '\0';
                    onLine(lineBuf_);
                    lineLen_ = 0;
                }
                continue;
            }
            if (lineLen_ < kLineMax - 1u) {
                lineBuf_[lineLen_++] = static_cast<char>(c);
                lineBuf_[lineLen_] = '\0';
                applyStatus(lineBuf_); // 状态文本可能不带换行，边收边识别
            } else {
                lineLen_ = 0; // 溢出：丢弃该行
            }
        }

        // 帧结束（USART1 空闲间隔）仍未见 '\n'：把已累积内容按一整行处理，
        // 兼容对端不追加换行的发送方式，保证连接判定与回显可用。
        if (lineLen_ > 0u) {
            lineBuf_[lineLen_] = '\0';
            onLine(lineBuf_);
            lineLen_ = 0;
        }
    }

    // 判定一行是否为“对端数据”（而非模块的 AT 应答/状态文本）。
    // 依据：AT 应答均以 '+' 开头或包含 OK/VERSION/ERROR/JDY 等关键字。
    static bool looksLikeData(const char* line) {
        if (line[0] == '\0' || line[0] == '+') {
            return false;
        }
        if (containsCi(line, "version") || containsCi(line, "ok") || containsCi(line, "error") ||
            containsCi(line, "jdy") || containsCi(line, "at+")) {
            return false;
        }
        return true;
    }

    // 单行分派：优先识别连接状态，其次 AT 应答，最后按已连接与否作为数据行。
    void onLine(const char* line) {
        if (applyStatus(line)) {
            return; // 连接/断开状态文本不作为数据
        }
        if (containsCi(line, "version")) {
            atReplied_ = true;
        }
        if (containsCi(line, "ok")) {
            atOk_ = true;
        }
        if (connected_) {
            pushRxLine(line);
            return;
        }
        // 回退：某些模块固件不输出（或无法识别）ENLOG 连接状态文本。此时若在 Idle
        // 阶段收到一行“对端数据”，说明链路已建立（模块仅在连接时才透传对端数据），
        // 据此判定已连接并消费该行，保证回显与状态显示可用。
        if (phase_ == AtPhase::Idle && looksLikeData(line)) {
            connected_ = true;
            phase_ = AtPhase::Connected;
            pushRxLine(line);
        }
    }

    // 将数据行压入接收队列（满则丢弃最新行）。
    void pushRxLine(const char* line) {
        const uint8_t next = static_cast<uint8_t>((rxTail_ + 1u) % kRxSlots);
        if (next == rxHead_) {
            return;
        }
        std::strncpy(rxBuf_[rxTail_], line, kLineMax - 1u);
        rxBuf_[rxTail_][kLineMax - 1u] = '\0';
        rxTail_ = next;
    }

    void startProbe() {
        cmdStart_ = nowMs();
        atReplied_ = false;
        enqueueAt("AT+VERSION");
    }

    void startSetName() {
        cmdStart_ = nowMs();
        atOk_ = false;
        enqueueAt("AT+NAME", deviceName_);
    }

    void startEnableLog() {
        cmdStart_ = nowMs();
        atOk_ = false;
        enqueueAt("AT+ENLOG", "1");
    }

    void runAtStateMachine() {
        switch (phase_) {
        case AtPhase::BootWait:
            if (nowMs() - phaseStart_ >= kBootWaitMs) {
                atRetries_ = 0;
                phase_ = AtPhase::Probe;
                startProbe();
            }
            break;

        case AtPhase::Probe:
            if (atReplied_) {
                atReplied_ = false;
                phase_ = AtPhase::SetName;
                startSetName();
            } else if (nowMs() - cmdStart_ >= kAtTimeoutMs) {
                if (++atRetries_ >= kAtRetries) {
                    phase_ = AtPhase::Idle; // 无应答：被动等待，稍后重试配置
                    phaseStart_ = nowMs();
                } else {
                    startProbe();
                }
            }
            break;

        case AtPhase::SetName:
            // 无论 AT+NAME 是否收到明确 +OK，都继续尝试开启 ENLOG（默认即为 1，
            // 但显式下发可确保部分固件确实输出连接状态文本）。
            if (atOk_ || nowMs() - cmdStart_ >= kAtTimeoutMs) {
                atOk_ = false;
                phase_ = AtPhase::EnableLog;
                startEnableLog();
            }
            break;

        case AtPhase::EnableLog:
            if (atOk_ || nowMs() - cmdStart_ >= kAtTimeoutMs) {
                atOk_ = false;
                configured_ = true; // 配置流程走完（含超时兜底）
                phase_ = AtPhase::Idle;
                phaseStart_ = nowMs();
            }
            break;

        case AtPhase::Idle:
            // 冷启动时模块可能尚未就绪导致首轮配置失败；在未连接、未配置成功的
            // 情况下周期重试，保证上电后最终能完成 AT 配置（不阻塞启动）。
            if (!configured_ && !connected_ && nowMs() - phaseStart_ >= kReprobeMs) {
                atRetries_ = 0;
                phase_ = AtPhase::Probe;
                startProbe();
            }
            break;

        case AtPhase::Connected:
        default:
            break;
        }
    }

    // 发送
    char txBuf_[kTxSlots][kTxStagingSize];
    uint16_t txLen_[kTxSlots];
    volatile uint8_t txHead_; // ISR 推进
    uint8_t txTail_;          // 主循环推进
    volatile bool txBusy_;

    // 接收行
    char rxBuf_[kRxSlots][kLineMax];
    uint8_t rxHead_;
    uint8_t rxTail_;

    // 行组装
    char lineBuf_[kLineMax];
    uint16_t lineLen_;

    // AT 状态机
    AtPhase phase_;
    uint8_t atRetries_;
    bool atReplied_;
    bool atOk_;
    uint32_t phaseStart_;
    uint32_t cmdStart_;
    bool connected_;
    bool configured_;
    char deviceName_[kNameMax];
};

inline BluetoothLink g_bluetooth;

} // namespace app

#endif
