#!/usr/bin/env python3
"""Final geometry pass after all graphics integrations; preserve every binding."""
from pathlib import Path
from collections import Counter
import xml.etree.ElementTree as ET
ROOT=Path(__file__).resolve().parents[2]
VIEW=ROOT/'indra/newview'
P=VIEW/'skins/default/xui/en/panel_preferences_graphics1.xml'
def signature(root):
    return Counter((e.tag,e.get('name'),e.get('control_name'),
                    tuple((c.tag,tuple(sorted(c.attrib.items()))) for c in e
                          if 'callback' in c.tag or c.tag.endswith('.item')))
                   for e in root.iter() if e.tag in ('check_box','slider','combo_box','button','spin_ctrl'))
def compact(root):
    before=signature(root)
    stack=next(e for e in root.iter('layout_stack') if e.get('name')=='gfx_stack')
    quick=next(e for e in stack if e.get('name')=='kombatant_display_quick')
    # Controls on the same visual row retain their horizontal relationships.
    # Historical removed controls left hundreds of pixels of empty coordinates.
    rows=[]
    for node in sorted(list(quick),key=lambda e:int(e.get('top','0'))):
        assert 'top' in node.attrib,(node.tag,node.get('name'))
        top=int(node.get('top'))
        if rows and top-rows[-1][0]<=8:
            rows[-1][1].append(node)
        else:rows.append((top,[node]))
    cursor=8
    for original_top,nodes in rows:
        bottom=cursor
        for node in nodes:
            offset=int(node.get('top'))-original_top
            node.set('top',str(cursor+offset))
            node.set('layout','topleft')
            bottom=max(bottom,cursor+offset+int(node.get('height','0')))
        cursor=bottom+6
    quick.set('height',str(cursor+4))
    quick.set('auto_resize','false')
    stack.set('border_size','4');stack.set('animate','false')
    # Consistent readable backing rather than bright artwork behind labels.
    scroll=next(e for e in root.iter('panel') if e.get('name')=='gfx_scroll_panel')
    scroll.set('background_visible','true')
    scroll.set('bg_alpha_color','0.025 0.028 0.035 0.98')
    scroll.set('bg_opaque_color','0.025 0.028 0.035 1')
    total=sum(int(e.get('height','0')) for e in stack if e.tag=='layout_panel')+4*(len(stack)-1)
    stack.set('height',str(total));scroll.set('height',str(total))
    assert before==signature(root),'Graphics binding or callback changed'
    return len(rows),int(quick.get('height'))

def patch_scroll(source):
    start=source.index('void LLFloaterPreference::toggleTabs()\n{')
    end=source.index('//BD - Input/Output resizer',start)
    replacement='''void LLFloaterPreference::toggleTabs()
{
    // KAMI: size the scroll area from every visible layout panel, including
    // the native KAMI controls. The old three-section/66px calculation and
    // translation offset clipped added content and left large blank regions.
    mModifier = 0;
    S32 content_height = 0;
    S32 visible_panels = 0;
    for (LLView* child : *mGFXStack->getChildList())
    {
        LLLayoutPanel* panel = dynamic_cast<LLLayoutPanel*>(child);
        if (panel && panel->getVisible())
        {
            content_height += panel->getRect().getHeight();
            ++visible_panels;
        }
    }
    content_height += llmax(0, visible_panels - 1) * 4;
    content_height = llmax(1, content_height);
    LLPanel* scroll = getChild<LLPanel>("gfx_scroll_panel");
    LLRect rect = scroll->getRect();
    rect.setLeftTopAndSize(rect.mLeft, rect.mTop, rect.getWidth(), content_height);
    scroll->setShape(rect);
    LLRect stack_rect;
    stack_rect.setLeftTopAndSize(0, content_height,
                                mGFXStack->getRect().getWidth(), content_height);
    mGFXStack->setShape(stack_rect);
    mGFXStack->updateLayout();
}

'''
    return source[:start]+replacement+source[end:]

if __name__=='__main__':
    tree=ET.parse(P);rows,height=compact(tree.getroot())
    ET.indent(tree,space=' ');tree.write(P,encoding='utf-8',xml_declaration=True)
    cpp=VIEW/'llfloaterpreference.cpp'
    cpp.write_text(patch_scroll(cpp.read_text(encoding='utf-8-sig')),encoding='utf-8')
    print(f'PASS compact graphics: {rows} rows, {height}px; native scroll includes all visible sections')
