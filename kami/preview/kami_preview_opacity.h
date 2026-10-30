// SPDX-License-Identifier: MIT
#pragma once
#include <vector>
#include <utility>
// Focused controls normally ignore draw-context alpha. Temporarily opt this
// Preferences subtree into it, then restore each exact prior policy after draw.
class KAMIPreviewOpacityScope final
{
    using Entry = std::pair<LLHandle<LLUICtrl>, LLUICtrl::ETypeTransparency>;
    std::vector<Entry> saved;
    void visit(LLView* view)
    {
        if (auto* ctrl = dynamic_cast<LLUICtrl*>(view))
        {
            saved.emplace_back(ctrl->getHandle(), ctrl->getTransparencyType());
            ctrl->setTransparencyType(LLUICtrl::TT_DEFAULT);
        }
        for (auto* child : *view->getChildList()) visit(child);
    }
public:
    KAMIPreviewOpacityScope(LLView* root, bool enabled)
    {
        if (enabled) visit(root);
    }
    ~KAMIPreviewOpacityScope()
    {
        for (auto& entry : saved)
            if (auto* ctrl = entry.first.get()) ctrl->setTransparencyType(entry.second);
    }
};
