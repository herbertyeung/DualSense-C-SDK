#include "wav_pcm.h"

#include <cstring>
#include <fstream>
#include <iterator>

bool ds5_tool_load_wav_pcm16(const char* path, std::vector<uint8_t>& pcm, ds5_audio_format& format) {
  std::ifstream file(path, std::ios::binary);
  if (!file) {
    return false;
  }

  std::vector<uint8_t> data((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
  if (data.size() < 44u || std::memcmp(data.data(), "RIFF", 4u) != 0 ||
      std::memcmp(data.data() + 8u, "WAVE", 4u) != 0) {
    return false;
  }

  uint16_t audio_format = 0u;
  uint16_t channels = 0u;
  uint32_t sample_rate = 0u;
  uint16_t bits = 0u;
  size_t cursor = 12u;
  size_t data_offset = 0u;
  uint32_t data_size = 0u;

  // RIFF chunks are word-aligned. Unknown chunks are skipped so diagnostics and
  // the ship demo accept normal PCM16 WAV files with metadata chunks.
  while (cursor + 8u <= data.size()) {
    const char* id = reinterpret_cast<const char*>(data.data() + cursor);
    uint32_t size = 0u;
    std::memcpy(&size, data.data() + cursor + 4u, 4u);
    cursor += 8u;
    if (cursor + size > data.size()) {
      break;
    }

    if (std::memcmp(id, "fmt ", 4u) == 0 && size >= 16u) {
      std::memcpy(&audio_format, data.data() + cursor, 2u);
      std::memcpy(&channels, data.data() + cursor + 2u, 2u);
      std::memcpy(&sample_rate, data.data() + cursor + 4u, 4u);
      std::memcpy(&bits, data.data() + cursor + 14u, 2u);
    } else if (std::memcmp(id, "data", 4u) == 0) {
      data_offset = cursor;
      data_size = size;
    }
    cursor += size + (size & 1u);
  }

  if (audio_format != 1u || channels == 0u || sample_rate == 0u || bits != 16u ||
      data_offset == 0u || data_size == 0u || data_offset + data_size > data.size()) {
    return false;
  }

  pcm.assign(data.begin() + data_offset, data.begin() + data_offset + data_size);
  ds5_audio_format_init(&format, 0u, 0u, 0u);
  format.sample_rate = sample_rate;
  format.channels = channels;
  format.bits_per_sample = bits;
  return true;
}
