#ifndef DS5_WAV_PCM_H
#define DS5_WAV_PCM_H

#include <cstdint>
#include <vector>

#include <dualsense/dualsense.h>

bool ds5_tool_load_wav_pcm16(const char* path, std::vector<uint8_t>& pcm, ds5_audio_format& format);

#endif
