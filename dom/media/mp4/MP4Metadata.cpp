/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "include/MPEG4Extractor.h"
#include "media/stagefright/DataSource.h"
#include "media/stagefright/MediaDefs.h"
#include "media/stagefright/MediaSource.h"
#include "media/stagefright/MetaData.h"
#include "mozilla/Assertions.h"
#include "mozilla/CheckedInt.h"
#include "mozilla/EndianUtils.h"
#include "mozilla/Logging.h"
#include "mozilla/RefPtr.h"
#include "mozilla/Telemetry.h"
#include "mozilla/UniquePtr.h"
#include "VideoUtils.h"
#include "MoofParser.h"
#include "MP4Metadata.h"
#include "ByteStream.h"
#include "MediaPrefs.h"

#include <limits>
#include <stdint.h>
#include <vector>

using namespace stagefright;
using mozilla::media::TimeUnit;

namespace mozilla {
LazyLogModule gMP4MetadataLog("MP4Metadata");

class DataSourceAdapter : public DataSource {
 public:
  explicit DataSourceAdapter(Stream* aSource) : mSource(aSource) {}

  ~DataSourceAdapter() {}

  virtual status_t initCheck() const { return NO_ERROR; }

  virtual ssize_t readAt(off64_t offset, void* data, size_t size) {
    MOZ_ASSERT(((ssize_t)size) >= 0);
    size_t bytesRead;
    if (!mSource->ReadAt(offset, data, size, &bytesRead))
      return ERROR_IO;

    if (bytesRead == 0)
      return ERROR_END_OF_STREAM;

    MOZ_ASSERT(((ssize_t)bytesRead) > 0);
    return bytesRead;
  }

  virtual status_t getSize(off64_t* size) {
    if (!mSource->Length(size))
      return ERROR_UNSUPPORTED;
    return NO_ERROR;
  }

  virtual uint32_t flags() { return kWantsPrefetching | kIsHTTPBasedSource; }

  virtual status_t reconnectAtOffset(off64_t offset) { return NO_ERROR; }

 private:
  RefPtr<Stream> mSource;
};

class MP4MetadataStagefright {
 public:
  explicit MP4MetadataStagefright(Stream* aSource);
  ~MP4MetadataStagefright();

  static MP4Metadata::ResultAndByteBuffer Metadata(Stream* aSource);

  MP4Metadata::ResultAndTrackCount
  GetNumberTracks(mozilla::TrackInfo::TrackType aType) const;
  MP4Metadata::ResultAndTrackInfo GetTrackInfo(
    mozilla::TrackInfo::TrackType aType, size_t aTrackNumber) const;
  bool CanSeek() const;

  MP4Metadata::ResultAndCryptoFile Crypto() const;

  MediaResult ReadTrackIndex(FallibleTArray<Index::Indice>& aDest, mozilla::TrackID aTrackID);

 private:
  int32_t GetTrackNumber(mozilla::TrackID aTrackID);
  void UpdateCrypto(const stagefright::MetaData* aMetaData);
  mozilla::UniquePtr<mozilla::TrackInfo> CheckTrack(const char* aMimeType,
                                                    stagefright::MetaData* aMetaData,
                                                    int32_t aIndex) const;
  CryptoFile mCrypto;
  RefPtr<Stream> mSource;
  sp<MediaExtractor> mMetadataExtractor;
  bool mCanSeek;
};

class IndiceWrapperStagefright : public IndiceWrapper {
 public:
  size_t Length() const override;

  bool GetIndice(size_t aIndex, Index::Indice& aIndice) const override;

  explicit IndiceWrapperStagefright(FallibleTArray<Index::Indice>& aIndice);

 protected:
  FallibleTArray<Index::Indice> mIndice;
};

IndiceWrapperStagefright::IndiceWrapperStagefright(FallibleTArray<Index::Indice>& aIndice) {
  mIndice.SwapElements(aIndice);
}

size_t IndiceWrapperStagefright::Length() const { return mIndice.Length(); }

bool IndiceWrapperStagefright::GetIndice(size_t aIndex, Index::Indice& aIndice) const {
  if (aIndex >= mIndice.Length()) {
    MOZ_LOG(gMP4MetadataLog, LogLevel::Error, ("Index overflow in indice"));
    return false;
  }

  aIndice = mIndice[aIndex];
  return true;
}

MP4Metadata::MP4Metadata(Stream* aSource)
 : mStagefright(MakeUnique<MP4MetadataStagefright>(aSource)) {}

MP4Metadata::~MP4Metadata() {}

/*static*/ MP4Metadata::ResultAndByteBuffer
MP4Metadata::Metadata(Stream* aSource) {
  return MP4MetadataStagefright::Metadata(aSource);
}

static const char* TrackTypeToString(mozilla::TrackInfo::TrackType aType) {
  switch (aType) {
    case mozilla::TrackInfo::kAudioTrack:
      return "audio";
    case mozilla::TrackInfo::kVideoTrack:
      return "video";
    default:
      return "unknown";
  }
}

MP4Metadata::ResultAndTrackCount
MP4Metadata::GetNumberTracks(mozilla::TrackInfo::TrackType aType) const {
  MP4Metadata::ResultAndTrackCount numTracks =
    mStagefright->GetNumberTracks(aType);

    return numTracks;
}

MP4Metadata::ResultAndTrackInfo
MP4Metadata::GetTrackInfo(mozilla::TrackInfo::TrackType aType,
                          size_t aTrackNumber) const {
  MP4Metadata::ResultAndTrackInfo info =
    mStagefright->GetTrackInfo(aType, aTrackNumber);
  return info;
}

bool MP4Metadata::CanSeek() const {
  return mStagefright->CanSeek();
}

MP4Metadata::ResultAndCryptoFile
MP4Metadata::Crypto() const {
  MP4Metadata::ResultAndCryptoFile crypto = mStagefright->Crypto();

  return crypto;
}

MP4Metadata::ResultAndIndice
MP4Metadata::GetTrackIndice(mozilla::TrackID aTrackID) {
  FallibleTArray<Index::Indice> indiceSF;
    MediaResult rv = mStagefright->ReadTrackIndex(indiceSF, aTrackID);
    if (NS_FAILED(rv)) {
      return {Move(rv), nullptr};
    }

  UniquePtr<IndiceWrapper> indice;
    indice = mozilla::MakeUnique<IndiceWrapperStagefright>(indiceSF);

  return {NS_OK, Move(indice)};
}

static inline MediaResult
ConvertIndex(FallibleTArray<Index::Indice>& aDest,
             const nsTArray<stagefright::MediaSource::Indice>& aIndex,
             int64_t aMediaTime) {
  if (!aDest.SetCapacity(aIndex.Length(), mozilla::fallible)) {
    return MediaResult{NS_ERROR_OUT_OF_MEMORY,
                       RESULT_DETAIL("Could not resize to %zu indices",
                                     aIndex.Length())};
  }
  for (size_t i = 0; i < aIndex.Length(); i++) {
    Index::Indice indice;
    const stagefright::MediaSource::Indice& s_indice = aIndex[i];
    indice.start_offset = s_indice.start_offset;
    indice.end_offset = s_indice.end_offset;
    indice.start_composition = s_indice.start_composition - aMediaTime;
    indice.end_composition = s_indice.end_composition - aMediaTime;
    indice.start_decode = s_indice.start_decode;
    indice.sync = s_indice.sync;
    // FIXME: Make this infallible after bug 968520 is done.
    MOZ_ALWAYS_TRUE(aDest.AppendElement(indice, mozilla::fallible));
    MOZ_LOG(gMP4MetadataLog, LogLevel::Debug,
      ("s_o: %" PRIu64 ", e_o: %" PRIu64 ", s_c: %" PRIu64 ", e_c: %" PRIu64 ", s_d: %" PRIu64 ", sync: %d\n",
      indice.start_offset, indice.end_offset, indice.start_composition,
      indice.end_composition, indice.start_decode, indice.sync));
  }
  return NS_OK;
}

MP4MetadataStagefright::MP4MetadataStagefright(Stream* aSource)
  : mSource(aSource)
  , mMetadataExtractor(new MPEG4Extractor(new DataSourceAdapter(mSource)))
  , mCanSeek(mMetadataExtractor->flags() & MediaExtractor::CAN_SEEK) {
  sp<MetaData> metaData = mMetadataExtractor->getMetaData();

  if (metaData.get()) {
    UpdateCrypto(metaData.get());
  }
}

MP4MetadataStagefright::~MP4MetadataStagefright() {}

MP4Metadata::ResultAndTrackCount
MP4MetadataStagefright::GetNumberTracks(mozilla::TrackInfo::TrackType aType) const {
  size_t tracks = mMetadataExtractor->countTracks();
  uint32_t total = 0;
  for (size_t i = 0; i < tracks; i++) {
    sp<MetaData> metaData = mMetadataExtractor->getTrackMetaData(i);

    const char* mimeType;
    if (metaData == nullptr || !metaData->findCString(kKeyMIMEType, &mimeType)) {
      continue;
    }
    switch (aType) {
      case mozilla::TrackInfo::kAudioTrack:
        if (!strncmp(mimeType, "audio/", 6) &&
            CheckTrack(mimeType, metaData.get(), i)) {
          total++;
        }
        break;
      case mozilla::TrackInfo::kVideoTrack:
        if (!strncmp(mimeType, "video/", 6) &&
            CheckTrack(mimeType, metaData.get(), i)) {
          total++;
        }
        break;
      default:
        break;
    }
  }
  return {NS_OK, total};
}

MP4Metadata::ResultAndTrackInfo
MP4MetadataStagefright::GetTrackInfo(mozilla::TrackInfo::TrackType aType,
                                     size_t aTrackNumber) const {
  size_t tracks = mMetadataExtractor->countTracks();
  if (!tracks) {
    return {MediaResult(NS_ERROR_DOM_MEDIA_METADATA_ERR,
                        RESULT_DETAIL("No %s tracks",
                                      TrackTypeToStr(aType))),
            nullptr};
  }
  int32_t index = -1;
  const char* mimeType;
  sp<MetaData> metaData;

  size_t i = 0;
  while (i < tracks) {
    metaData = mMetadataExtractor->getTrackMetaData(i);

    if (metaData == nullptr || !metaData->findCString(kKeyMIMEType, &mimeType)) {
      continue;
    }
    switch (aType) {
      case mozilla::TrackInfo::kAudioTrack:
        if (!strncmp(mimeType, "audio/", 6) &&
            CheckTrack(mimeType, metaData.get(), i)) {
          index++;
        }
        break;
      case mozilla::TrackInfo::kVideoTrack:
        if (!strncmp(mimeType, "video/", 6) &&
            CheckTrack(mimeType, metaData.get(), i)) {
          index++;
        }
        break;
      default:
        break;
    }
    if (index == aTrackNumber) {
      break;
    }
    i++;
  }
  if (index < 0) {
    return {MediaResult(NS_ERROR_DOM_MEDIA_METADATA_ERR,
                        RESULT_DETAIL("Cannot access %s track #%zu",
                                      TrackTypeToStr(aType),
                                      aTrackNumber)),
            nullptr};
  }

  UniquePtr<mozilla::TrackInfo> e = CheckTrack(mimeType, metaData.get(), index);

  if (e) {
    metaData = mMetadataExtractor->getMetaData();
    int64_t movieDuration;
    if (!e->mDuration.IsPositive() &&
        metaData->findInt64(kKeyMovieDuration, &movieDuration)) {
      // No duration in track, use movie extend header box one.
      e->mDuration = TimeUnit::FromMicroseconds(movieDuration);
    }
  }

  return {NS_OK, Move(e)};
}

mozilla::UniquePtr<mozilla::TrackInfo>
MP4MetadataStagefright::CheckTrack(const char* aMimeType,
                                   stagefright::MetaData* aMetaData,
                                   int32_t aIndex) const {
  sp<MediaSource> track = mMetadataExtractor->getTrack(aIndex);
  if (!track.get()) {
    return nullptr;
  }

  UniquePtr<mozilla::TrackInfo> e;

  if (!strncmp(aMimeType, "audio/", 6)) {
    auto info = mozilla::MakeUnique<MP4AudioInfo>();
    info->Update(aMetaData, aMimeType);
    e = Move(info);
  } else if (!strncmp(aMimeType, "video/", 6)) {
    auto info = mozilla::MakeUnique<MP4VideoInfo>();
    info->Update(aMetaData, aMimeType);
    e = Move(info);
  }

  if (e && e->IsValid()) {
    return e;
  }

  return nullptr;
}

bool MP4MetadataStagefright::CanSeek() const {
  return mCanSeek;
}

MP4Metadata::ResultAndCryptoFile
MP4MetadataStagefright::Crypto() const {
  return {NS_OK, &mCrypto};
}

void MP4MetadataStagefright::UpdateCrypto(const MetaData* aMetaData) {
  const void* data;
  size_t size;
  uint32_t type;

  // There's no point in checking that the type matches anything because it
  // isn't set consistently in the MPEG4Extractor.
  if (!aMetaData->findData(kKeyPssh, &type, &data, &size)) {
    return;
  }
  mCrypto.Update(reinterpret_cast<const uint8_t*>(data), size);
}

MediaResult MP4MetadataStagefright::ReadTrackIndex(FallibleTArray<Index::Indice>& aDest, mozilla::TrackID aTrackID) {
  size_t numTracks = mMetadataExtractor->countTracks();
  int32_t trackNumber = GetTrackNumber(aTrackID);
  if (trackNumber < 0) {
    return MediaResult(NS_ERROR_DOM_MEDIA_METADATA_ERR,
                       RESULT_DETAIL("Cannot find track id %d",
                                     int(aTrackID)));
  }
  sp<MediaSource> track = mMetadataExtractor->getTrack(trackNumber);
  if (!track.get()) {
    return MediaResult(NS_ERROR_DOM_MEDIA_METADATA_ERR,
                       RESULT_DETAIL("Cannot access track id %d",
                                     int(aTrackID)));
  }
  sp<MetaData> metadata = mMetadataExtractor->getTrackMetaData(trackNumber);
  int64_t mediaTime;
  if (!metadata->findInt64(kKeyMediaTime, &mediaTime)) {
    mediaTime = 0;
  }

  return ConvertIndex(aDest, track->exportIndex(), mediaTime);
}

int32_t MP4MetadataStagefright::GetTrackNumber(mozilla::TrackID aTrackID) {
  size_t numTracks = mMetadataExtractor->countTracks();
  for (size_t i = 0; i < numTracks; i++) {
    sp<MetaData> metaData = mMetadataExtractor->getTrackMetaData(i);
    if (!metaData.get()) {
      continue;
    }
    int32_t value;
    if (metaData->findInt32(kKeyTrackID, &value) && value == aTrackID) {
      return i;
    }
  }
  return -1;
}

/*static*/ MP4Metadata::ResultAndByteBuffer
MP4MetadataStagefright::Metadata(Stream* aSource) {
  auto parser = mozilla::MakeUnique<MoofParser>(aSource, 0, false);
  RefPtr<mozilla::MediaByteBuffer> buffer = parser->Metadata();
  if (!buffer) {
    return {MediaResult(NS_ERROR_DOM_MEDIA_METADATA_ERR,
                        RESULT_DETAIL("Cannot parse metadata")),
            nullptr};
  }
  return {NS_OK, Move(buffer)};
}

} // namespace mp4_demuxer
