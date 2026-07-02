/**
 * @file sample.cpp
 * C++ torture cases for the ZDoc C parser.
 */

#include <string>
#include <vector>

namespace zdoc {

/** A documented class. */
class Widget {
public:
    /**
     * @brief Construct a widget.
     * @param name  display name
     * @param level nesting level
     */
    Widget(std::string name, int level) : name_{std::move(name)}, level_(level) {
        history_.push_back(1'000'000);
    }

    /// Destroy the widget and release resources.
    ~Widget() = default;

    /**
     * @brief Render the widget.
     * @param indent number of spaces
     * @return rendered text
     */
    std::string render(int indent = 0) const;

    /// Compare widgets by level.
    bool operator<(const Widget &o) const { return level_ < o.level_; }

    Widget &operator=(const Widget &) = delete;

private:
    std::string name_;
    int level_;
    std::vector<long> history_;
};

/** Strongly-typed colour. */
enum class Colour : int { Red, Green, Blue };

/** Alias for a widget list. */
using WidgetList = std::vector<Widget>;

/**
 * @brief Free function with tricky body content.
 * @tparam T element type
 * @param items input values
 * @returns concatenated dump
 */
template <typename T>
std::string dump(const std::vector<T> &items)
{
    std::string out = R"raw(prefix with } and " inside)raw";
    for (const auto &i : items) {
        out += std::to_string(i) + '\n';
    }
    return out;
}

/// Qualified definition outside the class.
std::string Widget::render(int indent) const
{
    return std::string(static_cast<size_t>(indent), ' ') + name_;
}

} // namespace zdoc

extern "C" {
/** C-linkage entry point. */
int zdoc_widget_api_version(void);
}
