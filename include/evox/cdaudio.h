// The audio CD reader linked into EvoX's _text section (0x59617F..0x59641D in the original): it
// opens the drive as "CDROM0:" and reads the table of contents. The disc check uses it to tell
// an audio CD: the handle stays open when there are audio tracks.
#pragma once
#include <xtl.h>

class CCdAudio {                     // 0x595977 in the original
public:
    HRESULT Open();                  // 0x59617F INVALID_HANDLE_VALUE unless audio tracks were found
    void Close();                    // 0x59632D

    HANDLE m_handle;                 // +0x00
    DWORD m_tracks;                  // +0x04 audio tracks
    DWORD m_start[100];              // +0x08 each track's first sector, then the end

private:
    bool ReadToc();                  // 0x596251
};

extern CCdAudio g_CdAudio;
