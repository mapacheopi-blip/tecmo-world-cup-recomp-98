#pragma once
#include "game_routines.hpp"
#include <functional>
#include <map>
#include <span>
#include <vector>

namespace tecmo98 {
// Maps known source identities to compiled C++ functions, never SH-2 instruction dispatch.
// Source byte checks include caller-supplied literals/dependencies. No cached validation:
// writes through any WorkRam view are observed before the next native invocation.
class CodeRegistry {
    struct Binding {
        std::uint32_t source;
        std::vector<std::uint8_t> expected;
        std::function<void()> native;
    };
    std::map<std::uint32_t,Binding> bindings_;
public:
    void bind(std::uint32_t entry,std::uint32_t source,std::span<const std::uint8_t> expected,
              std::function<void()> native);
    void invoke(std::uint32_t entry,const WorkRam& ram) const;
};
}
