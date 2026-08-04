#pragma once

#include <stddef.h>
#include <stdint.h>
#include <string.h>

namespace update_policy {

constexpr size_t MAX_MANIFEST_BYTES = 2048U;
constexpr size_t MAX_TAG_LENGTH = 63U;
constexpr size_t MAX_VERSION_LABEL_LENGTH = 31U;
constexpr size_t MAX_BUILD_ID_LENGTH = 95U;
constexpr size_t MAX_ASSET_NAME_LENGTH = 127U;
constexpr size_t MAX_NOTES_LENGTH = 191U;
constexpr uint32_t PACKAGE_HEADER_BYTES = 512U;
constexpr uint32_t MINIMUM_FIRMWARE_BYTES = 64U * 1024U;
constexpr uint32_t MAXIMUM_PACKAGE_BYTES = 8U * 1024U * 1024U;

// Pure, allocation-free compatibility and transport-policy helpers. Keeping
// these independent of Arduino and networking makes the exact rules host-testable.
enum class VersionRelation : uint8_t {
  INVALID = 0,
  OLDER,
  CURRENT,
  NEWER
};

constexpr VersionRelation compareVersion(uint32_t remote, uint32_t installed) {
  return remote == 0
      ? VersionRelation::INVALID
      : (remote < installed
             ? VersionRelation::OLDER
             : (remote == installed ? VersionRelation::CURRENT
                                    : VersionRelation::NEWER));
}

inline bool boundedPrintableAscii(const char* text, size_t maximumLength,
                                  bool allowEmpty) {
  if (!text) return false;
  const size_t length = strnlen(text, maximumLength + 1U);
  if (length > maximumLength || (!allowEmpty && length == 0)) return false;
  for (size_t index = 0; index < length; ++index) {
    const unsigned char value = static_cast<unsigned char>(text[index]);
    if (value < 0x20 || value > 0x7e) return false;
  }
  return true;
}

inline bool lowerHexDigest(const char* digest) {
  if (!digest || strlen(digest) != 64U) return false;
  for (size_t index = 0; index < 64U; ++index) {
    const char value = digest[index];
    if (!((value >= '0' && value <= '9') ||
          (value >= 'a' && value <= 'f'))) {
      return false;
    }
  }
  return true;
}

inline bool identityMatches(uint32_t schema, uint32_t expectedSchema,
                            const char* hardware, const char* expectedHardware,
                            const char* channel, const char* expectedChannel,
                            uint32_t minimumUpdater,
                            uint32_t availableUpdater) {
  return schema == expectedSchema && hardware && expectedHardware && channel &&
         expectedChannel && strcmp(hardware, expectedHardware) == 0 &&
         strcmp(channel, expectedChannel) == 0 &&
         minimumUpdater <= availableUpdater;
}

constexpr bool packageLayoutValid(uint32_t packageBytes,
                                  uint32_t firmwareBytes) {
  return firmwareBytes >= MINIMUM_FIRMWARE_BYTES &&
         packageBytes >= MINIMUM_FIRMWARE_BYTES + PACKAGE_HEADER_BYTES &&
         packageBytes <= MAXIMUM_PACKAGE_BYTES &&
         packageBytes == firmwareBytes + PACKAGE_HEADER_BYTES;
}

inline bool assetNameValid(const char* asset) {
  if (!boundedPrintableAscii(asset, MAX_ASSET_NAME_LENGTH, false)) return false;
  const size_t length = strlen(asset);
  constexpr char suffix[] = ".radarota";
  return length >= sizeof(suffix) - 1U &&
         strcmp(asset + length - (sizeof(suffix) - 1U), suffix) == 0;
}

inline char asciiLower(char value) {
  return value >= 'A' && value <= 'Z'
      ? static_cast<char>(value + ('a' - 'A'))
      : value;
}

inline bool allowedReleaseHost(const char* host) {
  return host &&
         (strcmp(host, "github.com") == 0 ||
          strcmp(host, "objects.githubusercontent.com") == 0 ||
          strcmp(host, "release-assets.githubusercontent.com") == 0);
}

inline bool parseAllowedHttpsUrl(const char* url, char* host,
                                 size_t hostCapacity) {
  constexpr char prefix[] = "https://";
  if (!url || !host || hostCapacity == 0 ||
      strncmp(url, prefix, sizeof(prefix) - 1U) != 0) {
    return false;
  }
  const char* authority = url + sizeof(prefix) - 1U;
  const char* slash = strchr(authority, '/');
  if (!slash || slash == authority) return false;
  const size_t hostLength = static_cast<size_t>(slash - authority);
  if (hostLength >= hostCapacity || memchr(authority, '@', hostLength) ||
      memchr(authority, ':', hostLength)) {
    return false;
  }
  memcpy(host, authority, hostLength);
  host[hostLength] = 0;
  for (size_t index = 0; index < hostLength; ++index) {
    host[index] = asciiLower(host[index]);
  }
  return allowedReleaseHost(host);
}

constexpr bool framingIsUnambiguous(bool contentLengthSeen,
                                    bool transferEncodingSeen,
                                    bool transferEncodingChunkedOnly,
                                    bool clientReportsChunked) {
  return !(contentLengthSeen && transferEncodingSeen) &&
         (!transferEncodingSeen || transferEncodingChunkedOnly) &&
         (contentLengthSeen || clientReportsChunked);
}

constexpr bool enoughSlack(uint32_t now, uint32_t nextPoll,
                           uint32_t minimumSlack) {
  return static_cast<int32_t>(nextPoll - now) >=
         static_cast<int32_t>(minimumSlack);
}

constexpr uint32_t boundedDeadline(uint32_t now, uint32_t nextPoll,
                                   uint32_t totalTimeout,
                                   uint32_t pollGuard) {
  const uint32_t hardDeadline = now + totalTimeout;
  const uint32_t guardedPollDeadline = nextPoll - pollGuard;
  return static_cast<int32_t>(guardedPollDeadline - hardDeadline) < 0
      ? guardedPollDeadline
      : hardDeadline;
}

}  // namespace update_policy
