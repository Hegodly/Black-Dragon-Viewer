#include "kami/world/shaders.hpp"
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <iostream>

int main() {
    for(const char* entry : {"vertex_main","pixel_main","shadow_vertex_main"}) {
        const char* target=(entry[0]=='v' || entry[0]=='s') ? "vs_5_0" : "ps_5_0";
        Microsoft::WRL::ComPtr<ID3DBlob> binary;
        Microsoft::WRL::ComPtr<ID3DBlob> errors;
        const HRESULT hr=D3DCompile(
            kami::world::kSceneShader,sizeof(kami::world::kSceneShader)-1,
            "kami_original_scene.hlsl",nullptr,nullptr,entry,target,
            D3DCOMPILE_ENABLE_STRICTNESS,0,
            binary.GetAddressOf(),errors.GetAddressOf());
        if(FAILED(hr) || !binary || binary->GetBufferSize()==0) {
            if(errors)
                std::cerr.write(static_cast<const char*>(errors->GetBufferPointer()),
                                static_cast<std::streamsize>(errors->GetBufferSize()));
            return entry[0]=='v'?1:2;
        }
    }
    return 0;
}
