#pragma once

#include <QString>

namespace AvatarSource {

QString fromAvatarFileId(const QString& fileId);

QString versioned(const QString& source,
                  int version,
                  const QString& etag = {},
                  const QString& contentHash = {});

QString cleanForIo(const QString& source);

} // namespace AvatarSource
