/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2019  Vladimir Golovnev <glassez@yandex.ru>
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

#include "sql.h"

Utils::SQL::AbstractQueryBuilder::~AbstractQueryBuilder() {}

Utils::SQL::CreateTableQueryBuilder::CreateTableQueryBuilder(const QString &tableName)
    : m_tableName {tableName}
{
}

Utils::SQL::CreateTableQueryBuilder &
Utils::SQL::CreateTableQueryBuilder::column(const QString &columnName, const QString &columnDef)
{
    m_defs += QString("`%1` %2").arg(columnName, columnDef);
    return *this;
}

Utils::SQL::CreateTableQueryBuilder &
Utils::SQL::CreateTableQueryBuilder::unique(const QStringList &columnNames)
{
    QStringList quotedColumnNames;
    for (const QString &columnName : columnNames)
        quotedColumnNames += "`" + columnName + "`";
    m_defs += QString("UNIQUE(%1)").arg(quotedColumnNames.join(", "));
    return *this;
}

Utils::SQL::CreateTableQueryBuilder &
Utils::SQL::CreateTableQueryBuilder::foreignKey(const QStringList &columnNames, const QString &foreignTableName
                                                , const QStringList &foreignColumnNames, const QString &definition)
{
    QStringList quotedColumnNames;
    for (const QString &columnName : columnNames)
        quotedColumnNames += "`" + columnName + "`";

    QStringList quotedForeignColumnNames;
    for (const QString &columnName : foreignColumnNames)
        quotedForeignColumnNames += "`" + columnName + "`";

    m_defs += QString("FOREIGN KEY(%1) REFERENCES `%2` (%3) %4")
            .arg(quotedColumnNames.join(", "), foreignTableName
                 , quotedForeignColumnNames.join(", "), definition);
    return *this;
}

QString
Utils::SQL::CreateTableQueryBuilder::getQuery() const
{
    return QString("CREATE TABLE `%1` (%2);").arg(m_tableName, m_defs.join(", "));
}

Utils::SQL::CreateTableQueryBuilder Utils::SQL::createTable(const QString &tableName)
{
    return CreateTableQueryBuilder {tableName};
}
