# Make a dashboard folder for checking the live information: its evox.ini selects a skin whose
# text labels show every skin variable on the start screen, so the original and the rebuild can
# be compared by screenshot (xemu's -xbe-path serves the folder as Q:). The skin has no logo
# (LogoType 1165389656), so nothing moves but the clock. SMART is on, for the HDD temperature.
# usage: python tools/make_info_test.py <folder> <dashboard xbe>
import os, shutil, sys

out, dash = sys.argv[1], sys.argv[2]
LABELS = [
    'Time <Time>',
    'Name <Name>  Version <Version>  RD <RDName>',
    'IP <IP>  CD <CD>',
    'C: <SpaceC>  E: <SpaceE>',
    'F: <SpaceF>  G: <SpaceG>',
    'X: <SpaceX>  Y: <SpaceY>  Z: <SpaceZ>',
    'BIOS <BIOSVer>  Kernel <KernelVer>',
    'Temp <Temp1> / <Temp2> / <Temp3>',
]
ini = ['[Misc]', 'SkinName = Info', 'AutoLaunchGames = No', 'AutoLaunchDVD = No', 'AutoLaunchAudio = No',
       # the HDD temperature through SMART: xemu's drive has no attribute 194, its temperature is 190
       'EnableSMART = Yes', 'HDD_Temp_ID = 190', '',
       '[Clock]', 'Use24 = No', 'SwapDate = Yes', '',
       '[Skin_Info]', 'LogoType = 1165389656']
for i, text in enumerate(LABELS):
    ini.append('text = 40,%d,1.0,0xffffff,0,"%s"' % (30 + i * 36, text))
os.makedirs(out, exist_ok=True)
shutil.copyfile(dash, os.path.join(out, 'evoxdash.xbe'))
open(os.path.join(out, 'evox.ini'), 'wb').write(('\r\n'.join(ini) + '\r\n').encode())
print('wrote', out)
