# Make a dashboard folder for testing the folder scans and the [Menu] parser: the XBE, an
# evox.ini whose [Menu] uses each keyword, BIOS files, skins, a trainer and game folders (copies
# of the XBE with their certificate titles changed). xemu's -xbe-path serves the folder as the
# DVD drive, which is Q:, so the original and the rebuild can both be run from it.
# usage: python tools/make_menu_test.py <folder> <dashboard xbe> [<xbe for the games>]
import os, shutil, struct, sys

out, dash = sys.argv[1], sys.argv[2]
game = sys.argv[3] if len(sys.argv) > 3 else dash

MENU = r'''[Menu]
# a comment
Section "Root"
{
	Item "Settings",ID_Settings
	Item "Reboot",@5
	Item "Bravo by Item","q:\games\b\default.xbe"
	Item "Missing game","q:\games\missing\default.xbe"
	Item "Region game","q:\games\b\default.xbe",ID_Reg_J,ID_SwapStick
	Item "With trainer","q:\games\b\default.xbe",ID_CMD,"q:\trainers\test.etm"
	AutoAddItem "q:\games\"
	Line "--------",1
	Section "Tools"
	{
		Item "Power Off",ID_Power_Off
		Item "MS Dash",ID_MS_Dash
		Item "Backup",ID_Backup
		SortAll
	}
	Section "Chameleon only" ID_Chameleon_Mode_2
	{
		Item "Hidden",@5
	}
	; another comment
	Item "Last item",@10
}
'''

SKIN = '''SkinName "Blue Skin","Blue"
[Skin_Blue]
BarColor1 = 0x0000ff
'''


def xbe_with_title(src, dst, title):
    d = bytearray(open(src, 'rb').read())
    base, cert = struct.unpack_from('<I', d, 0x104)[0], struct.unpack_from('<I', d, 0x118)[0]
    name = title.encode('utf-16-le')[:0x4E]
    d[cert - base + 0x0C:cert - base + 0x0C + 0x50] = name + bytes(0x50 - len(name))
    os.makedirs(os.path.dirname(dst), exist_ok=True)
    open(dst, 'wb').write(d)


def write(path, data):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    open(path, 'wb').write(data.encode() if isinstance(data, str) else data)


os.makedirs(out, exist_ok=True)
shutil.copyfile(dash, os.path.join(out, 'evoxdash.xbe'))
write(os.path.join(out, 'evox.ini'), MENU.replace('\n', '\r\n'))
write(os.path.join(out, 'bios', 'zulu.bin'), bytes(16))
write(os.path.join(out, 'bios', 'alpha.bin'), bytes(16))
write(os.path.join(out, 'skin', 'Blue', 'skin.ini'), SKIN.replace('\n', '\r\n'))
write(os.path.join(out, 'skin', 'NoName', 'skin.ini'), 'Nothing here\r\n')
# a trainer: the dword at +0x0E is the offset of the dword holding the name's offset
etm = bytearray(0x40)
struct.pack_into('<I', etm, 0x0E, 0x20)
struct.pack_into('<I', etm, 0x20, 0x30)
etm[0x30:0x3D] = b'Test Trainer\0'
write(os.path.join(out, 'trainers', 'test.etm'), bytes(etm))
xbe_with_title(game, os.path.join(out, 'games', 'a', 'default.xbe'), 'Alpha Game')
xbe_with_title(game, os.path.join(out, 'games', 'b', 'default.xbe'), 'Bravo Game')
xbe_with_title(game, os.path.join(out, 'games', 'delta', 'default.xbe'), '')
xbe_with_title(game, os.path.join(out, 'games', 'c', 'deep', 'default.xbe'), 'Too Deep')
print('wrote', out)
