#include "kami/world/shader_cache.hpp"
#include "kami/world/warmup.hpp"
#include <cstdint>
int main() {
    if(kami::world::ShaderCache::source_version()==0) return 1;
    kami::world::ShaderWarmup worker;
    auto before=worker.snapshot();
    if(before.result!=kami::world::WarmupResult::Idle || before.percent!=0) return 2;
    // This unit test does not require a GPU or start a second GPU compilation
    // on GitHub-hosted Windows runners. The full graphics test is manual.
    return 0;
}
