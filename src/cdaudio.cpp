// The audio CD reader (0x59617F..0x59641D in the original, its _text section); see
// include/evox/cdaudio.h.
#include <xtl.h>
#include "evox/cdaudio.h"

#define IOCTL_CDROM_READ_TOC 0x24000

struct CdTrack {             // TRACK_DATA
    BYTE reserved;
    BYTE control;            // low 4 bits: control (4 = data track); high 4: ADR
    BYTE number;             // 0xAA: the lead-out
    BYTE reserved1;
    BYTE address[4];         // 0, minutes, seconds, frames
};
struct CdToc {               // CDROM_TOC, 0x324 bytes
    BYTE length[2];
    BYTE firstTrack, lastTrack;
    CdTrack tracks[100];
};
static_assert(sizeof(CdToc) == 0x324, "CdToc");

CCdAudio g_CdAudio = {INVALID_HANDLE_VALUE, 0, {}};

static HRESULT LastErrorResult()
{
    DWORD e = GetLastError();
    return (int)e > 0 ? (HRESULT)((e & 0xFFFF) | 0x80070000) : (HRESULT)GetLastError();
}

// 0x5963E2 / 0x596400: minutes:seconds:frames to a sector
static DWORD MsfToSector(const BYTE address[4])
{
    return (address[1] * 60 + address[2]) * 75 + address[3];
}

// 0x596346: a wide device path to ANSI ("file:" dropped, '/' to '\', up to a '?')
static void PathToAnsi(char *out, const WCHAR *in)
{
    if (wcsncmp(in, L"file:", 5) == 0)
        in += 5;
    for (; *in && *in != '?'; in++)
        *out++ = *in == '/' ? '\\' : (char)*in;
    *out = 0;
}

// 0x59617F
HRESULT CCdAudio::Open()
{
    m_handle = INVALID_HANDLE_VALUE;
    m_tracks = 0;
    char path[0x104];
    PathToAnsi(path, L"CDROM0:"); // 0x59620B
    m_handle = CreateFile(path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL,
                          nullptr);
    if (m_handle == INVALID_HANDLE_VALUE)
        return LastErrorResult();
    if (!ReadToc())
        Close();
    return S_OK;
}

// 0x596251: the audio tracks' start sectors, up to the lead-out or a data track (whose start
// less 2:30, the gap before a CD-Extra session, ends the last audio track)
bool CCdAudio::ReadToc()
{
    CdToc toc;
    DWORD returned;
    if (!DeviceIoControl(m_handle, IOCTL_CDROM_READ_TOC, nullptr, 0, &toc, sizeof toc, &returned, nullptr))
        return false; // (the original asks once more only for ERROR_NOT_READY, but its loop stops at 1)
    if (toc.firstTrack > toc.lastTrack)
        return false;
    DWORD i = 0;
    for (; i <= (DWORD)(toc.lastTrack - toc.firstTrack + 1); i++) {
        CdTrack &t = toc.tracks[i];
        m_start[i] = MsfToSector(t.address);
        if (t.number == 0xAA)
            break;
        if (t.control & 4) {
            t.address[2] += 30;
            t.address[1] -= 2;
            if (t.address[2] < 60)
                t.address[1]--;
            else
                t.address[2] -= 60;
            m_start[i] = MsfToSector(t.address);
            break;
        }
    }
    if (i > (DWORD)(toc.lastTrack - toc.firstTrack + 1))
        i--;
    m_tracks = i;
    return (int)i > 0;
}

// 0x59632D
void CCdAudio::Close()
{
    if (m_handle != INVALID_HANDLE_VALUE) {
        CloseHandle(m_handle);
        m_handle = INVALID_HANDLE_VALUE;
    }
    m_tracks = 0;
}
