#include "code_registry.hpp"
#include <algorithm>
#include <string>

namespace tecmo98 {
void CodeRegistry::bind(std::uint32_t entry,std::uint32_t source,std::span<const std::uint8_t> expected,
                        std::function<void()> native) {
    if (expected.empty() || !native) throw std::invalid_argument("Native binding needs source bytes and implementation");
    if (source<WorkRam::base || std::uint64_t(source-WorkRam::base)+expected.size()>WorkRam::size)
        throw std::out_of_range("Native binding outside work RAM");
    if (entry<source || std::uint64_t(entry-source)>=expected.size() || (entry&1)!=0)
        throw std::invalid_argument("Native entry outside verified source bytes");
    if (bindings_.contains(entry)) throw std::invalid_argument("Duplicate native entry");
    bindings_.emplace(entry,Binding{source,{expected.begin(),expected.end()},std::move(native)});
}
void CodeRegistry::invoke(std::uint32_t entry,const WorkRam& ram) const {
    const auto found=bindings_.find(entry);
    if (found==bindings_.end()) throw std::runtime_error("Untranslated native entry: "+std::to_string(entry));
    const auto& binding=found->second;
    const auto at=ram.offset(binding.source,binding.expected.size());
    if (!std::equal(binding.expected.begin(),binding.expected.end(),ram.bytes.begin()+at))
        throw std::runtime_error("Native source changed; retranslation required");
    binding.native();
}
}
