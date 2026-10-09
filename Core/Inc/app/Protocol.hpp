#ifndef APP_PROTOCOL_HPP
#define APP_PROTOCOL_HPP

#include <cstdint>

namespace app {

/*
 * 上位机 <-> 设备 二进制帧协议。
 *
 * 帧格式（多字节小端）：
 *   [0] SOF1 = 0xAA
 *   [1] SOF2 = 0x55
 *   [2] LEN  = 1 + 载荷长度（即 CMD 与 PAYLOAD 的字节数）
 *   [3] CMD
 *   [4 .. 3+LEN] PAYLOAD
 *   [4+LEN]      CHK = LEN、CMD、PAYLOAD 逐字节异或
 * 整帧长度 = 4 + LEN。
 *
 * 浮点一律以“千分整数”（×1000 的四舍五入截断）传输，避免 C++ 端浮点解析。
 * 说明：JDY-31 的 AT 配置与连接状态（ENLOG）仍走文本行，与本二进制协议互不影响；
 *       0xAA/0x55 非 ASCII 可打印字符，二进制帧与文本行可共存于同一字节流。
 */

/* 上位机 -> 设备 命令字。 */
enum class HostCmd : uint8_t {
    Start = 0x01,       // 开始检测   -> hopeStatus = Working
    Calibration = 0x02, // 开始校准   -> hopeStatus = Calibration
    Stop = 0x03,        // 停止/待机  -> hopeStatus = Ready
    StatusQuery = 0x04, // 查询状态   -> 设备回 STATUS
};

/* 设备 -> 上位机 命令字。 */
enum class DeviceCmd : uint8_t {
    State = 0x81,       // [uint8 workStatus]
    Error = 0x82,       // [uint8 workStatus]
    Result = 0x83,      // [int32 milli] 检测结果
    Calibration = 0x84, // [int32 milli rawValue][uint16 uvLightLevel] 校准结果
    Status = 0x85,      // [uint8 workStatus][uint8 progress][uint8 battery][uint8 bt]
};

inline constexpr uint8_t kFrameSof1 = 0xAA;
inline constexpr uint8_t kFrameSof2 = 0x55;
inline constexpr uint8_t kFrameMax = 32;                       // 整帧最大长度
inline constexpr uint8_t kFramePayloadMax = kFrameMax - 5u;    // 载荷最大长度

/* 计算 LEN/CMD/PAYLOAD 的逐字节异或（fromLen 指向 LEN 字段，len = 1 + LEN）。 */
inline uint8_t frameChecksum(const uint8_t* fromLen, uint16_t len) {
    uint8_t chk = 0;
    for (uint16_t i = 0; i < len; ++i) {
        chk ^= fromLen[i];
    }
    return chk;
}

/* 组帧到 out；成功返回整帧长度，cap 不足或载荷过长返回 0。 */
inline uint16_t encodeFrame(
    uint8_t cmd, const uint8_t* payload, uint8_t payloadLen, uint8_t* out, uint16_t cap) {
    if (out == nullptr || payloadLen > kFramePayloadMax) {
        return 0;
    }
    const uint16_t total = static_cast<uint16_t>(5u + payloadLen);
    if (cap < total) {
        return 0;
    }
    out[0] = kFrameSof1;
    out[1] = kFrameSof2;
    out[2] = static_cast<uint8_t>(1u + payloadLen);
    out[3] = cmd;
    for (uint8_t i = 0; i < payloadLen; ++i) {
        out[4u + i] = payload[i];
    }
    out[4u + payloadLen] = frameChecksum(&out[2], static_cast<uint16_t>(2u + payloadLen));
    return total;
}

} // namespace app

#endif // APP_PROTOCOL_HPP
