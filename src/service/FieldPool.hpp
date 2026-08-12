#ifndef SRC_SERVICE_FIELD_POOL_HPP
#define SRC_SERVICE_FIELD_POOL_HPP

// 字段池：管理"协议字段 -> 可视化数据源"的映射。
// 这是"协议解析"与"可视化"之间的桥梁：
//   - 协议解析界面配置 ProtocolSchema，生成本协议的字段；
//   - FieldPool 从 schema 抽取可用数据源字段（剔除 padding/无效字段）；
//   - 可视化配置界面读取 FieldPool 的字段列表，绑定数据源；
//   - 运行时 ProtocolEngine 发布 Frame，FieldPool 更新各字段最新值。
//   - 协议化发送由 ProtocolEngine 负责（本类不重复实现编码）。
//
// 纯 C++、无 Qt 依赖，可被 TUI / Qt / QML / 浏览器前端复用。

#include "../core/FieldSchema.hpp"
#include "../core/Frame.hpp"

#include <string>
#include <vector>

namespace sd {

// 一个可绑定的数据源字段（UI 展示用）。
struct FieldSource {
    std::string name;      // 字段名
    FieldType   type;      // 基础类型
    std::string unit;      // 单位
    double      scale;     // 比例（回显配置用）
    double      offset;    // 偏移
    bool        bigEndian; // 字节序（回显配置用）
    double      lastValue = 0.0; // 最近一次收到的值
    bool        hasData   = false; // 是否已收到至少一帧
};

// 字段池：装载协议 schema，暴露可选数据源，并跟踪最新值。
class FieldPool {
public:
    // 装载一个协议 schema（协议名变化时调用；会清空旧数据）。
    void setSchema(const ProtocolSchema& schema);

    // 有可用 schema 吗？（至少含一个非 padding 字段）
    bool hasSchema() const { return !schema_.fields.empty() && hasDataFields_; }

    // 协议名；未装载返回空串。
    const std::string& protocolName() const { return schema_.name; }

    // 全部字段（含 padding，供协议解析界面回显）。
    const std::vector<FieldDesc>& allFields() const { return schema_.fields; }

    // 可用作可视化数据源的字段（剔除 padding）。
    const std::vector<FieldSource>& sources() const { return sources_; }

    // 按名找数据源；不存在返回 nullptr。
    const FieldSource* findSource(const std::string& name) const;

    // 运行时：收到一帧，更新所有字段的最新值。
    void onFrame(const Frame& frame);

    // 清空（切换协议/断开时）。
    void clear();

private:
    ProtocolSchema          schema_;
    std::vector<FieldSource> sources_;
    bool                     hasDataFields_ = false;
};

} // namespace sd

#endif // SRC_SERVICE_FIELD_POOL_HPP