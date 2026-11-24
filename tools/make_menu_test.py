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
	Item "Copy Test",@200
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

[Action_00]
info = "Copy Test"
Warning = "Copies q:\evox.ini to e:\evoxtest.ini"
Warning = "(a test action: nothing else)"
progress = "Copying..."
LogFile = "e:\action00.log"
copy "\evox.ini" "e:\evoxtest.ini"
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


def make_etm(name, lines, options, title_ids, enabled=0):
    # the .etm layout EvoX reads (include/evox/trainer.h): size +0, option bytes offset +0x0A,
    # string table offset +0x0E, title id list offset +0x12, enabled +0x22
    strings = [name.encode() + b'\0', b''.join(l.encode() + b'\0' for l in lines) + b'\0']
    strings += [o.encode() + b'\0' for o in options]
    data = bytearray(0x40)
    table_at = len(data)
    data += bytes(4 * (len(strings) + 1))
    offsets = []
    for s in strings:
        offsets.append(len(data))
        data += s
    for i, o in enumerate(offsets):
        struct.pack_into('<I', data, table_at + 4 * i, o)
    options_at = len(data)
    data += bytes(len(options))
    ids_at = len(data)
    data += b''.join(struct.pack('<I', t) for t in title_ids) + bytes(4)
    struct.pack_into('<I', data, 0x0A, options_at)
    struct.pack_into('<I', data, 0x0E, table_at)
    struct.pack_into('<I', data, 0x12, ids_at)
    data[0x22] = enabled
    struct.pack_into('<I', data, 0, len(data))
    return bytes(data)


write(os.path.join(out, 'trainers', 'test.etm'),
      make_etm('Test Trainer', ['Trainer for the test game', 'Made by make_menu_test.py'],
               ['Infinite Lives', 'Max Ammo'], [0x4D530004]))
xbe_with_title(game, os.path.join(out, 'games', 'a', 'default.xbe'), 'Alpha Game')
xbe_with_title(game, os.path.join(out, 'games', 'b', 'default.xbe'), 'Bravo Game')
xbe_with_title(game, os.path.join(out, 'games', 'delta', 'default.xbe'), '')
xbe_with_title(game, os.path.join(out, 'games', 'c', 'deep', 'default.xbe'), 'Too Deep')
print('wrote', out)
