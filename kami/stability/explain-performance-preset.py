#!/usr/bin/env python3
"""Make Performance preset semantics and objective before/after FPS QA visible."""
from pathlib import Path
import xml.etree.ElementTree as ET
R=Path(__file__).resolve().parents[2]
path=R/"indra/newview/skins/default/xui/en/panel_preferences_graphics1.xml"
tree=ET.parse(path)
root=tree.getroot()
def unique(name):
    found=[n for n in root.iter() if n.get("name")==name]
    assert len(found)==1,(name,len(found))
    return found[0]

toggle=unique("kami_performance_toggle")
assert toggle.get("control_name")=="KAMIPerformanceMode"
toggle.set("label","Performance preset — optimize geometry and lighting")
toggle.set("tool_tip","Uncapped, native-pixel performance preset. Limits distant fully rendered avatars to four, particles to 512 and lights to 16; reduces object LOD and draw distance; disables shadows, native AO and SSR. Keeps the user-selected live render resolution, including 4K. Can reduce visible scene detail. Automatic quality modes are suspended while this preset is enabled.")
note=unique("kami_performance_note")
note.text="Keeps native 4K and FPS uncapped; lowers scene detail, not image resolution."
note.set("width","465")
bench=unique("kami_bench_start")
bench.set("label","Benchmark FPS (30s) — run once OFF, then ON")
bench.set("tool_tip","Benchmark in the same location, camera view and display resolution. First run with Performance and all other optimization modes OFF to capture baseline. Then enable Performance and rerun. KAMI reports measured median FPS and percent change. Scene loading, FPS caps and VSync can invalidate a comparison.")
result=unique("kami_bench_result")
result.set("height","86")
result.text="For a measured boost: benchmark once with preset OFF; turn it ON, keep the same camera view, and benchmark again. Displays median FPS and percent change."
ET.indent(tree,space=" ")
tree.write(path,encoding="utf-8",xml_declaration=True)
ET.parse(path)
print("Performance preset instructions, unchanged-4K policy and measured baseline comparison displayed.")
