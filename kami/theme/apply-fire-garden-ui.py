#!/usr/bin/env python3
"""Apply the approved Fire Garden visual identity to the actual KAMI UI.

Runs after all KAMI XUI/graphics integrations, not just at login.
The original Fire Garden photograph is reused (not reimagined).
All images are generated at build time; no extra runtime render passes.
"""
from pathlib import Path
from PIL import Image, ImageDraw, ImageEnhance, ImageOps, ImageFilter
import re
import xml.etree.ElementTree as ET

R=Path(__file__).resolve().parents[2]
SKIN=R/"indra/newview/skins/default"
TX=SKIN/"textures"
WIN=TX/"windows"
ART=WIN/"kami_fire_garden.jpg"
assert ART.is_file(), "The approved KAMI Fire Garden art was not staged"
WIN.mkdir(parents=True,exist_ok=True)

# Exact design direction from the APPROVED Fire Garden mockup:
# black/charcoal smoked glass, silver/chrome edges and typography, fire-red
# garden visible behind windows, amber highlights, icy cyan ONLY for selection.
# These are SKIN colors/textures, never scene-lighting or GPU settings.
obsidian=(8,10,12)
smoke=(24,24,27)
blue=(77,187,224)
ember=(215,65,33)
silver=(167,176,183)
gold=(209,113,54)
text=(235,238,239)

def gradient(w,h,first,second,alpha=233):
    img=Image.new("RGBA",(w,h))
    px=img.load()
    for y in range(h):
        t=y/max(1,h-1)
        for x in range(w):
            horizontal=0.10*x/max(1,w-1)
            factor=min(1,t*.84+horizontal)
            px[x,y]=tuple(round(first[i]*(1-factor)+second[i]*factor) for i in range(3))+(alpha,)
    return img

def outline(canvas, glow=False, selected=False):
    d=ImageDraw.Draw(canvas,"RGBA")
    w,h=canvas.size
    border=blue if selected or glow else silver
    alpha=208 if selected else 115 if glow else 115
    d.rounded_rectangle((1,1,w-2,h-2),radius=min(5,h//3),
                        outline=(*border,alpha),width=2 if selected else 1)
    if selected:
        d.line((8,h-3,w-8,h-3),fill=(*blue,210),width=2)
    else:
        # Very thin metallic glint, not the stock Black Dragon blue border.
        d.line((8,2,w-8,2),fill=(220,225,230,65),width=1)
    return canvas

def widget(filename, a=obsidian,b=smoke, selected=False, glow=False):
    p=WIN/filename
    if filename in ("kami_fg_window.png","kami_fg_panel.png","kami_fg_tab.png","kami_fg_tab_selected.png"):
        dimensions=(128,64)
    elif filename.startswith("kami_fg_text_field"):
        dimensions=(256,26)
    else:
        dimensions=(128,32)
    canvas=gradient(*dimensions,a,b,232)
    outline(canvas,selected=selected,glow=glow)
    canvas.save(p,optimize=True)
    return p

# Shared *real native controls* across the viewer. Mild, readable accents;
# not the bright neon mockup and not a painted picture of working controls.
assets={
  "Window_Background": "kami_fg_window.png",
  "Panel_Background": "kami_fg_panel.png",
  "PushButton_Off": "kami_fg_button.png",
  "PushButton_On": "kami_fg_button_on.png",
  "PushButton_Press": "kami_fg_button_press.png",
  "PushButton_Selected": "kami_fg_button_selected.png",
  "PushButton_Selected_Press": "kami_fg_button_press.png",
  "PushButton_Default_Off": "kami_fg_button_on.png",
  "Toolbar_Middle_Off": "kami_fg_toolbar.png",
  "Toolbar_Middle_Over": "kami_fg_toolbar_hover.png",
  "Toolbar_Middle_Selected": "kami_fg_toolbar_selected.png",
  "ComboButton_Off": "kami_fg_button.png",
  "ComboButton_On": "kami_fg_button_on.png",
  "SegmentedBtn_Right_Selected": "kami_fg_tab_selected.png",
  "SegmentedBtn_Right_Off": "kami_fg_tab.png",
  "TaskPanel_Tab_Off": "kami_fg_tab.png",
  "TaskPanel_Tab_Selected": "kami_fg_tab_selected.png",
  "TabTop_Left_Off": "kami_fg_tab.png",
  "TabTop_Middle_Off": "kami_fg_tab.png",
  "TabTop_Right_Off": "kami_fg_tab.png",
  "TabTop_Left_Selected": "kami_fg_tab_selected.png",
  "TabTop_Middle_Selected": "kami_fg_tab_selected.png",
  "TabTop_Right_Selected": "kami_fg_tab_selected.png",
  "Toolbar_Left_Off": "kami_fg_toolbar.png",
  "Toolbar_Right_Off": "kami_fg_toolbar.png",
  "Toolbar_Left_Over": "kami_fg_toolbar_hover.png",
  "Toolbar_Right_Over": "kami_fg_toolbar_hover.png",
  "Toolbar_Left_Selected": "kami_fg_toolbar_selected.png",
  "Toolbar_Right_Selected": "kami_fg_toolbar_selected.png",
  "TextField_Off": "kami_fg_text_field.png",
  "TextField_Active": "kami_fg_text_field_active.png",
  "TextField_Disabled": "kami_fg_text_field.png",
  "SliderTrack_Horiz": "kami_fg_slider_track.png",
  "SliderTrack_Horiz_Progress": "kami_fg_slider_track_progress.png",
  "SliderThumb_Off": "kami_fg_slider_thumb.png",
  "SliderThumb_Press": "kami_fg_slider_thumb.png",
  "Checkbox_Off": "kami_fg_checkbox_off.png",
  "Checkbox_On": "kami_fg_checkbox_on.png",
  "Checkbox_On_Press": "kami_fg_checkbox_on.png",
}
widget("kami_fg_window.png",(8,10,12),(22,21,23))
widget("kami_fg_panel.png",(12,12,14),(25,23,23))
widget("kami_fg_button.png",(29,29,31),(15,15,17))
widget("kami_fg_button_on.png",(23,40,48),(15,20,24),selected=True)
widget("kami_fg_button_press.png",(25,51,63),(13,22,29),selected=True)
widget("kami_fg_button_selected.png",(19,40,51),(13,23,29),selected=True)
widget("kami_fg_toolbar.png",(13,14,16),(20,21,23))
widget("kami_fg_toolbar_hover.png",(23,35,40),(14,19,23),glow=True)
widget("kami_fg_toolbar_selected.png",(17,38,48),(12,20,25),selected=True)
widget("kami_fg_tab.png",(22,22,24),(13,14,17))
widget("kami_fg_tab_selected.png",(17,39,50),(12,22,28),selected=True)
widget("kami_fg_text_field.png",(14,15,17),(27,27,29))
widget("kami_fg_text_field_active.png",(16,30,36),(21,23,28),selected=True)

def slider_track(filename,filled):
    img=Image.new("RGBA",(104,16),(0,0,0,0))
    draw=ImageDraw.Draw(img)
    draw.rounded_rectangle((0,5,103,12),radius=4,
        fill=(13,14,16,235),outline=(*silver,170),width=1)
    if filled:
        draw.rounded_rectangle((2,6,101,11),radius=3,fill=(*blue,242))
    img.save(WIN/filename,optimize=True)
slider_track("kami_fg_slider_track.png",False)
slider_track("kami_fg_slider_track_progress.png",True)

thumb=Image.new("RGBA",(32,32),(0,0,0,0))
d=ImageDraw.Draw(thumb)
d.rounded_rectangle((9,6,23,25),radius=5,fill=(169,177,187,249),outline=(233,239,244,255),width=2)
thumb.save(WIN/"kami_fg_slider_thumb.png",optimize=True)

def checkbox(name,checked):
    img=Image.new("RGBA",(24,24),(0,0,0,0))
    draw=ImageDraw.Draw(img)
    draw.rounded_rectangle((2,2,21,21),radius=4,
        fill=(24,44,54,251) if checked else (16,17,20,245),
        outline=(*blue,255) if checked else (*silver,212),width=2)
    if checked:
        draw.line(((6,12),(10,16),(18,7)),fill=(241,244,246,255),width=3,joint="curve")
    img.save(WIN/name,optimize=True)
checkbox("kami_fg_checkbox_off.png",False)
checkbox("kami_fg_checkbox_on.png",True)

# Per-floater preferences artwork. Actual approved Fire Garden, obscured by
# smoked glass; lower left has the brighter fire highlights, right stays
# dark enough to maintain contrast behind every preferences control.
art=Image.open(ART).convert("RGB")
fitted=ImageOps.fit(art,(960,760),method=Image.Resampling.LANCZOS,centering=(0.50,0.50))
fitted=ImageEnhance.Color(fitted).enhance(1.07)
fitted=ImageEnhance.Brightness(fitted).enhance(0.59)
base=Image.new("RGBA",fitted.size)
base.paste(fitted.convert("RGBA"),(0,0))
base=Image.alpha_composite(base,Image.new("RGBA",base.size,(7,8,11,129)))
d=ImageDraw.Draw(base,"RGBA")
d.rectangle((0,0,960,60),fill=(9,10,12,226))
d.line((0,59,959,59),fill=(*silver,157),width=2)
d.rectangle((0,0,245,760),fill=(7,8,10,184))
d.line((245,60,245,760),fill=(*silver,91),width=2)
d.rounded_rectangle((4,4,956,756),radius=7,outline=(*silver,175),width=2)
# Small ember line references the original garden without drowning blue controls.
d.line((18,63,138,63),fill=(*ember,215),width=3)
d.line((820,63,945,63),fill=(*gold,174),width=1)
base.save(WIN/"kami_fg_preferences.png",optimize=True)

# Repoint the actual texture keys used by buttons, nav, tabs and floaters.
tex=TX/"textures.xml"
s=tex.read_text(encoding="utf-8-sig")
for key,filename in assets.items():
    pattern=r'(<texture\b[^>]*\bname="'+re.escape(key)+r'"[^>]*\bfile_name=")[^"]+(")'
    s,n=re.subn(pattern,lambda m:m.group(1)+"windows/"+filename+m.group(2),s,count=1,flags=re.S)
    assert n==1,("Unable to skin texture",key,n)
s=s.replace("</textures>",'  <texture name="KAMI_FireGardenPrefs" file_name="windows/kami_fg_preferences.png" preload="false" />\n</textures>')
tex.write_text(s,encoding="utf-8")
ET.parse(tex)

# Current Black Dragon colors.xml uses reference colors throughout widgets.
# Change only the relevant UI semantic palette, never the world renderer,
# accessibility warning colors, or scene atmosphere/tone mapping.
colors={
 # KAMI mockup's black / charcoal-glass chrome palette.
 "FloaterDefaultBackgroundColor": "0.036 0.038 0.045 0.90",
 "FloaterFocusBackgroundColor": "0.046 0.044 0.048 0.92",
 "FloaterFocusBorderColor": "0.68 0.71 0.74 0.86",
 "FloaterUnfocusBorderColor": "0.40 0.43 0.46 0.64",
 "PanelDefaultBackgroundColor": "0.040 0.040 0.046 0.88",
 "PanelFocusBackgroundColor": "0.055 0.053 0.055 0.91",
 "MenuBarBgColor": "0.034 0.038 0.045 0.97",
 "MenuDefaultBgColor": "0.041 0.042 0.047 0.97",
 "MenuPopupBgColor": "0.049 0.050 0.057 0.97",
 # Cyan is a small interaction highlight, not the entire background.
 "MenuItemHighlightBgColor": "0.11 0.30 0.38 0.70",
 "MenuItemHighlightFgColor": "0.96 0.97 0.98 1",
 "EmphasisColor": "0.30 0.73 0.88 0.54",
 "EmphasisColor_13": "0.30 0.73 0.88 0.13",
 "EmphasisColor_35": "0.30 0.73 0.88 0.35",
 "BeaconColor": "0.30 0.73 0.88 0.95",
 "ComboListBgColor": "0.07 0.07 0.08 0.98",
 "ButtonLabelColor": "0.91 0.92 0.94 1",
 "ButtonLabelSelectedColor": "1 1 1 1",
 "LabelTextColor": "0.92 0.92 0.94 1",
 "ScrollSelectedBGColor": "0.11 0.30 0.39 0.74",
 "ScrollSelectedFGColor": "0.97 0.98 1 1",
 "ScrollHoveredColor": "0.21 0.25 0.28 0.34",
 "ScrollUnselectedColor": "0.78 0.80 0.84 1",
 "InventoryBackgroundColor": "0.046 0.045 0.049 0.92",
 "InventoryMouseOverColor": "0.15 0.24 0.29 0.36",
 "SliderThumbCenterColor": "0.79 0.82 0.85 1",
 "SliderThumbOutlineColor": "0.30 0.73 0.88 1",
 "MultiSliderThumbCenterColor": "0.79 0.82 0.85 1",
 "ScrollbarTrackColor": "0.08 0.08 0.10 0.84",
 "ScrollbarThumbColor": "0.46 0.49 0.52 0.90",
 "DefaultHighlightLight": "0.68 0.72 0.76 0.24",
 "DefaultHighlightDark": "0.08 0.08 0.10 0.65",
 "TitleBarFocusColor": "0.08 0.08 0.10 0.66",
}
col=SKIN/"colors.xml"
ct=ET.parse(col)
root=ct.getroot()
applied=[]
for name,value in colors.items():
    matches=[el for el in root.findall("color") if el.get("name")==name]
    assert len(matches)==1,("Color entry missing or ambiguous",name,len(matches))
    node=matches[0]
    node.attrib.pop("reference",None)
    node.set("value",value)
    applied.append(name)
ET.indent(ct,space=" ")
ct.write(col,encoding="utf-8",xml_declaration=True)
ET.parse(col)

# Give the real Preferences window a dedicated KAMI header and a locally
# available Fire Garden backing. Preserve tab controls, search, button callbacks
# and layout dimensions so preferences continue to work and remain accessible.
prefs=SKIN/"xui/en/floater_preferences.xml"
pt=ET.parse(prefs)
p=pt.getroot()
p.set("title","KAMI  |  Beyond Reality  |  Preferences")
back=[n for n in p.iter() if n.get("name")=="vicon"]
assert len(back)==1
back[0].set("image_name","KAMI_FireGardenPrefs")
back[0].set("color","1 1 1 1")
# Use a compact identity label on the left tab header without shifting controls.
# It must never intercept clicks or obscure the tab selector.
tab=[n for n in p.iter() if n.get("name")=="pref core"]
assert len(tab)==1
tab[0].set("tab_width","140")
ET.indent(pt,space=" ")
pt.write(prefs,encoding="utf-8",xml_declaration=True)
ET.parse(prefs)

# The existing graphics page contains hundreds of functional settings; no
# need to replace its controls or overwrite their listeners to get a new skin.
# The menu and other floaters share the themed colors and standard textures.
for path in [WIN/"kami_fg_preferences.png",WIN/"kami_fg_button.png",WIN/"kami_fg_window.png"]:
    assert path.is_file(),("Missing built Fire Garden UI asset",path)
    with Image.open(path) as icon:
        icon.verify()  # Uniform UI textures compress well; a tiny PNG is valid.
assert all(f'file_name="windows/{name}"' in s for name in assets.values())
print(f"Installed KAMI Fire Garden skin: {len(assets)} real UI texture keys, {len(applied)} native palette colors, approved Fire Garden Preferences artwork.")
