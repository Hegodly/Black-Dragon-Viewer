#include "kami/features/hotfix.hpp"
int main() {
 using namespace kami::hotfix;
 const std::vector<std::uint8_t> bytes={'a','b','c'};
 const auto sha=std::string("ba7816bf8f01cfea414140de5dae2223")+
                      "b00361a396177a9cb410ff61f20015ad";
 if(!verify_sha256(bytes,sha))return 1;
 if(verify_sha256(bytes,std::string(64,'0')))return 2;
 Manifest m{1,2,sha,bytes.size(),false};
 if(validate(m,bytes)!=Result::UntrustedManifest)return 3;
 m.trusted_release_signature=true;
 if(validate(m,bytes)!=Result::ValidatedForStaging)return 4;
 m.package_size=500;
 if(validate(m,bytes)!=Result::SizeMismatch)return 5;
 m.package_size=bytes.size();m.to_version=1;
 if(validate(m,bytes)!=Result::InvalidVersion)return 6;
 return 0;
}
