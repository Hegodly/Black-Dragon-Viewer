#!/usr/bin/env python3
"""Clean the static KAMI login layout while preserving login callbacks and credits."""
from pathlib import Path
import xml.etree.ElementTree as E
R=Path(__file__).resolve().parents[2]
p=R/'indra/newview/skins/default/xui/en/panel_login.xml'
tree=E.parse(p);root=tree.getroot()
def named(name):
 matches=[n for n in root.iter() if n.get('name')==name]
 assert len(matches)==1,(name,len(matches))
 return matches[0]
def place(n,left,top,width,height):
 for k in ('left_pad','left_delta','top_pad','top_delta','bottom','right'):n.attrib.pop(k,None)
 n.attrib.update(left=str(left),top=str(top),width=str(width),height=str(height),layout='topleft',follows='left|top')
# Replace the large square artwork on login with a restrained, native-text title.
logo=named('kami_login_branding');index=list(root).index(logo);root.remove(logo)
title=E.Element('text',name='kami_login_title',font='SansSerifHuge',**{'font.style':'BOLD','text_color':'0.85 0.93 1 1','mouse_opaque':'false'})
place(title,40,52,500,40);title.text='K A M I';root.insert(index,title)
subtitle=E.Element('text',name='kami_login_subtitle',font='SansSerifBig',text_color='0.62 0.80 0.96 1',mouse_opaque='false')
place(subtitle,43,96,500,30);subtitle.text='B E Y O N D   R E A L I T Y';root.insert(index+1,subtitle)
# Remove promotion widgets from this login page; About/source credits are retained.
for parent in list(root.iter()):
 for child in list(parent):
  if child.get('name') in ('patreon_supports','show_supporters'):
   parent.remove(child)
footer=named('panel_login2');footer.set('height','330')
stack=next(n for n in footer if n.tag=='layout_stack');stack.set('height','330')
card=named('root_panel');card.attrib.update(width='540',height='320',background_visible='true',bg_alpha_color='0.025 0.04 0.065 0.88',bg_opaque_color='0.025 0.04 0.065 0.88')
positions={
 'app_name_text':(20,12,500,26),
 'pref_btn':(24,48,150,28),'grid_btn':(184,48,150,28),'server_combo':(344,48,172,28),
 'username_combo':(24,90,300,32),'password_edit':(24,132,300,32),
 'remember_name':(340,94,180,24),'remember_password':(340,136,190,24),
 'start_location_combo':(24,174,300,32),'connect_btn':(24,222,492,38),
}
for name,pos in positions.items():
 n=named(name);place(n,*pos);n.set('font','SansSerif')
named('app_name_text').text='KAMI  /  BEYOND REALITY';named('app_name_text').set('font','SansSerifBig')
named('connect_btn').attrib.update(label='ENTER WORLD',**{'font.style':'BOLD','label_color':'0.86 0.95 1 1'})
# Upstream code expects account-action buttons even where its XML omitted them.
for name,label,left in [('forgot_password_text','Forgot password?',24),('create_new_account_text','Create an account',284)]:
 matches=[n for n in root.iter() if n.get('name')==name]
 if matches:
  n=matches[0]
  for parent in root.iter():
   if n in list(parent):parent.remove(n);break
 else:n=E.Element('button',name=name,label=label)
 place(n,left,274,232,28);n.set('font','SansSerif');card.append(n)
# Hide old social-link strip; keep account recovery and quit controls functional.
for strip in list(footer):
 if strip.tag=='layout_stack' and strip is not stack:footer.remove(strip)
for n in list(root):
 if n.tag=='icon' and n.get('name')=='login_bg' and n.get('height')=='135':
  n.set('height','330');n.set('top','-330');n.set('color','0.015 0.025 0.04 0.7')
# Suppress upstream version-identifier clutter without removing callback targets.
named('channel_text').set('label','KAMI Beyond Reality  |  [VERSION]')
named('new_version').set('visible','false')
E.indent(tree,space=' ');tree.write(p,encoding='utf-8',xml_declaration=True)
# APP_NAME supplies login texts and native captions elsewhere.
p=R/'indra/newview/skins/default/xui/en/strings.xml';s=p.read_text(encoding='utf-8-sig');s=s.replace('<string name="APP_NAME">Kombatant Viewer</string>','<string name="APP_NAME">KAMI Beyond Reality</string>');p.write_text(s,encoding='utf-8')
p=R/'indra/newview/llappviewer.cpp';s=p.read_text(encoding='utf-8-sig');assert 'const char* const VIEWER_WINDOW_CLASSNAME = "Kombatant Viewer";' in s;s=s.replace('const char* const VIEWER_WINDOW_CLASSNAME = "Kombatant Viewer";','const char* const VIEWER_WINDOW_CLASSNAME = "KAMI Beyond Reality";');p.write_text(s,encoding='utf-8')
# Ensure every hard lookup required by the existing login controller survives.
for name in ['connect_btn','start_location_combo','username_combo','password_edit','remember_password','remember_name','server_combo','quit_btn','forgot_password_text','create_new_account_text','channel_text','new_version']:named(name)
print('KAMI login cleaned: compact text identity, no promo panels, larger centered sign-in card. Native title requires rebuilt executable.')
