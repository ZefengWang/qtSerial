#ifndef SRC_CORE_FIELD_SCHEMA_HPP
#define SRC_CORE_FIELD_SCHEMA_HPP

// 字段 schema：可配置协议的运行时描述。
// 这是"协议解析"与"可视化配置"之间的共享契约：
//   - GenericBinaryProtocol 依据它把二进制流解析成 Frame；
//   - FieldPool / ViewManager 依据它列出可选数据源。
// 纯 C++、无 Qt 依赖，可跨前端（TUI/Qt/QML/浏览器）复用。

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace sd {

// 字段基础类型。
enum class FieldType {
    U8, U16, U32,   // 无符号整数
    I8, I16, I32,   // 有符号整数
    F32, F64,       // 浮点
    Bool,           // 布尔
};

// 计算某类型在内存中的字节长度（单位：字节）。
inline int fieldTypeBytes(FieldType t) {
    switch (t) {
        case FieldType::U8:  case FieldType::I8:  case FieldType::Bool: return 1;
        case FieldType::U16: case FieldType::I16: return 2;
        case FieldType::U32: case FieldType::I32: case FieldType::F32: return 4;
        case FieldType::F64: return 8;
    }
    return 1;
}

// 单个字段描述。
struct FieldDesc {
    std::string name;      // 字段名（唯一，供可视化引用）
    FieldType   type = FieldType::F32;
    int         byteLength = -1;  // -1 表示"按类型自动推导"；>0 手动覆盖
    bool        bigEndian = false;// 本字段字节序（默认小端）
    double      scale  = 1.0;     // 比例：实际值 = 原始值 * scale + offset
    double      offset = 0.0;
    std::string unit;             // 单位（如 V / A / ℃）
    bool        isPadding = false;// padding 占位：只占位、不产出数据源
    bool        dataSource = true;// 是否进入字段池作为可视化数据源（勾选=是）
};

// 一个完整协议的帧布局描述。
struct ProtocolSchema {
    std::string name;                          // 协议名（注册/显示用）
    std::vector<FieldDesc> fields;             // 有序字段列表
    bool defaultBigEndian = false;             // 默认字节序（小端）

    // 帧总字节长 = 所有字段长度之和（byteLength<=0 时按类型推导）。
    std::size_t totalBytes() const {
        std::size_t n = 0;
        for (const auto& f : fields) {
            n += static_cast<std::size_t>(f.byteLength > 0 ? f.byteLength : fieldTypeBytes(f.type));
        }
        return n;
    }

    // 便利：按名取字段描述；不存在返回 nullptr。
    const FieldDesc* findBy(const std::string& name) const {
        for (const auto& f : fields) {
            if (f.name == name) return &f;
        }
        return nullptr;
    }
};

} // namespace sd

#endif // SRC_CORE_FIELD_SCHEMA_HPP