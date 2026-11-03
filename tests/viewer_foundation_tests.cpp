#include "kami/features/viewer_foundation.hpp"
int main() {
 using namespace kami::viewer;
 if(feature_catalog().size()!=21)return 1;
 if(status(Feature::Authentication)!=Availability::NetworkIntegrationRequired)return 2;
 if(status(Feature::PhotoStudio)!=Availability::OriginalLocalReady)return 3;
 OutfitDraft outfit;
 if(!outfit.add("asset-001",L"Hair")||outfit.add("asset-001",L"Hair"))return 4;
 if(!outfit.remove("asset-001")||outfit.size()!=0)return 5;
 LandmarkLibrary bookmarks;
 if(!bookmarks.add({L"Garden",L"KAMI Test",128,128,40})||
    bookmarks.add({L"Garden",L"KAMI Test",128,128,40}))return 6;
 if(bookmarks.add({L"Invalid",L"Region",300,0,0}))return 7;
 auto budget=estimate_scene_cost(2048,2048,4,100000,48);
 if(budget.texture_bytes!=4ULL*2048*2048*4 ||
    budget.geometry_bytes!=100000ULL*156||!budget.over_budget)return 8;
 return 0;
}
