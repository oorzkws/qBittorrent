/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2020  Vladimir Golovnev <glassez@yandex.ru>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 * In addition, as a special exception, the copyright holders give permission to
 * link this program with the OpenSSL project's "OpenSSL" library (or with
 * modified versions of it that use the same license as the "OpenSSL" library),
 * and distribute the linked executables. You must obey the GNU General Public
 * License in all respects for all of the code used other than "OpenSSL".  If you
 * modify file(s), you may extend this exception to your version of the file(s),
 * but you are not obligated to do so. If you do not wish to do so, delete this
 * exception statement from your version.
 */

#include "abstractfilestorage.h"

#include <QDir>
#include <QHash>
#include <QVector>

#include "base/exceptions.h"
#include "base/utils/fs.h"

#if defined(Q_OS_WIN)
const Qt::CaseSensitivity CASE_SENSITIVITY {Qt::CaseInsensitive};
#else
const Qt::CaseSensitivity CASE_SENSITIVITY {Qt::CaseSensitive};
#endif

namespace
{
    bool areSameFileNames(QString first, QString second)
    {
        return QString::compare(first, second, CASE_SENSITIVITY) == 0;
    }
}

void BitTorrent::AbstractFileStorage::renameItem(const QString &oldPath, const QString &newPath)
{
    if (!Utils::Fs::isValidFileSystemName(oldPath, true))
        throw RuntimeError(tr("The old path is invalid: '%1'.").arg(oldPath));
    if (!Utils::Fs::isValidFileSystemName(newPath, true))
        throw RuntimeError(tr("The new path is invalid: '%1'.").arg(newPath));

    const QString oldFilePath = QDir::cleanPath(oldPath);
    const QString newFilePath = QDir::cleanPath(newPath);

    if (QDir().isAbsolutePath(newFilePath))
        throw RuntimeError(tr("Absolute path isn't allowed: '%1'.").arg(newFilePath));

    const QString oldFolderPrefix = oldFilePath + QLatin1Char('/');
    const QString newFolderPrefix = newFilePath + QLatin1Char('/');

    QVector<int> renamingFileIndexes;
    renamingFileIndexes.reserve(filesCount());

    bool isFolder = false;

    for (int i = 0; i < filesCount(); ++i)
    {
        const QString path = filePath(i);

        if (renamingFileIndexes.isEmpty()
                && areSameFileNames(path, oldFilePath))
        {
            renamingFileIndexes.append(i);
        }

        if ((renamingFileIndexes.isEmpty() || isFolder)
                && path.startsWith(oldFolderPrefix, CASE_SENSITIVITY))
        {
            renamingFileIndexes.append(i);
            isFolder = true;
        }

        if (areSameFileNames(path, newFilePath))
            throw RuntimeError(tr("The file already exists: '%1'.").arg(newFilePath));

        if (path.startsWith(newFolderPrefix, CASE_SENSITIVITY))
            throw RuntimeError(tr("The folder already exists: '%1'.").arg(newFilePath));
    }

    if (renamingFileIndexes.isEmpty())
        throw RuntimeError(tr("No such file or folder: '%1'.").arg(oldFilePath));

    if (!isFolder)
    {
        renameFile(renamingFileIndexes.first(), newFilePath);
    }
    else
    {
        for (const int index : renamingFileIndexes)
        {
            const QString newFilePath = newFolderPrefix + filePath(index).mid(oldFolderPrefix.size());
            renameFile(index, newFilePath);
        }
    }
}
