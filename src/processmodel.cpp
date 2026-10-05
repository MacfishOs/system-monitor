// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "processmodel.h"

#include <algorithm>
#include <unistd.h>

ProcessModel::ProcessModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int ProcessModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_processes.size();
}

QVariant ProcessModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_processes.size())
        return {};
    const auto &process = m_processes[index.row()];
    switch (role) {
    case PidRole: return process.pid;
    case NameRole: return process.displayName;
    case ProcessNameRole: return process.name;
    case IconRole: return process.iconName;
    case UserRole: return process.user;
    case UidRole: return process.uid;
    case CpuRole: return process.cpu;
    case MemoryRole: return process.memory;
    case ReadRole: return process.readRate;
    case WriteRole: return process.writeRate;
    case IoAvailableRole: return process.ioAvailable;
    case StartRole: return QString::number(process.startTicks);
    default: return {};
    }
}

QHash<int, QByteArray> ProcessModel::roleNames() const
{
    return {
        { PidRole, "pid" },
        { NameRole, "displayName" },
        { ProcessNameRole, "processName" },
        { IconRole, "iconName" },
        { UserRole, "userName" },
        { UidRole, "uid" },
        { CpuRole, "cpu" },
        { MemoryRole, "memory" },
        { ReadRole, "readRate" },
        { WriteRole, "writeRate" },
        { IoAvailableRole, "ioAvailable" },
        { StartRole, "startTicks" }
    };
}

void ProcessModel::apply(const QVector<ProcessRecord> &processes)
{
    QSet<int> present;
    for (const auto &process : processes)
        present.insert(process.pid);

    for (int i = m_processes.size() - 1; i >= 0; --i) {
        if (!present.contains(m_processes[i].pid)) {
            beginRemoveRows({}, i, i);
            m_processes.removeAt(i);
            endRemoveRows();
        }
    }

    int row = 0;
    for (const auto &process : processes) {
        if (row == m_processes.size() || m_processes[row].pid != process.pid) {
            beginInsertRows({}, row, row);
            m_processes.insert(row, process);
            endInsertRows();
        } else {
            m_processes[row] = process;
        }
        ++row;
    }
    if (!m_processes.isEmpty())
        emit dataChanged(index(0), index(m_processes.size() - 1));
}

const ProcessRecord *ProcessModel::find(int pid) const
{
    const auto position = std::lower_bound(m_processes.cbegin(), m_processes.cend(), pid,
                                          [](const ProcessRecord &process, int value) {
        return process.pid < value;
    });
    return position != m_processes.cend() && position->pid == pid ? &*position : nullptr;
}

QVariantMap ProcessModel::describe(const ProcessRecord &process)
{
    return {
        { QStringLiteral("pid"), process.pid },
        { QStringLiteral("parentPid"), process.parentPid },
        { QStringLiteral("name"), process.displayName },
        { QStringLiteral("processName"), process.name },
        { QStringLiteral("icon"), process.iconName },
        { QStringLiteral("user"), process.user },
        { QStringLiteral("uid"), process.uid },
        { QStringLiteral("cpu"), process.cpu },
        { QStringLiteral("memory"), process.memory },
        { QStringLiteral("readRate"), process.readRate },
        { QStringLiteral("writeRate"), process.writeRate },
        { QStringLiteral("ioAvailable"), process.ioAvailable },
        { QStringLiteral("startTicks"), QString::number(process.startTicks) },
        { QStringLiteral("state"), ProcReader::stateName(process.state) },
        { QStringLiteral("threads"), process.threads },
        { QStringLiteral("nice"), process.nice },
        { QStringLiteral("command"), process.command },
        { QStringLiteral("executable"), process.executable }
    };
}

ProcessFilter::ProcessFilter(ProcessModel *source, QObject *parent)
    : QSortFilterProxyModel(parent)
    , m_uid(getuid())
{
    setSourceModel(source);
    setSortRole(ProcessModel::CpuRole);
    setDynamicSortFilter(true);
    sort(0, Qt::DescendingOrder);
    connect(this, &QAbstractItemModel::rowsInserted, this, &ProcessFilter::countChanged);
    connect(this, &QAbstractItemModel::rowsRemoved, this, &ProcessFilter::countChanged);
    connect(this, &QAbstractItemModel::modelReset, this, &ProcessFilter::countChanged);
}

QString ProcessFilter::search() const { return m_search; }
bool ProcessFilter::allUsers() const { return m_allUsers; }
QString ProcessFilter::sortColumn() const { return m_sortColumn; }
bool ProcessFilter::ascending() const { return m_ascending; }
int ProcessFilter::count() const { return rowCount(); }

void ProcessFilter::setSearch(const QString &search)
{
    if (m_search == search)
        return;
    m_search = search;
    invalidateFilter();
    emit searchChanged();
}

void ProcessFilter::setAllUsers(bool allUsers)
{
    if (m_allUsers == allUsers)
        return;
    m_allUsers = allUsers;
    invalidateFilter();
    emit allUsersChanged();
}

void ProcessFilter::sortBy(const QString &column)
{
    const QHash<QString, int> columns {
        { QStringLiteral("name"), ProcessModel::NameRole },
        { QStringLiteral("cpu"), ProcessModel::CpuRole },
        { QStringLiteral("memory"), ProcessModel::MemoryRole },
        { QStringLiteral("read"), ProcessModel::ReadRole },
        { QStringLiteral("write"), ProcessModel::WriteRole },
        { QStringLiteral("pid"), ProcessModel::PidRole }
    };
    if (!columns.contains(column))
        return;
    m_ascending = m_sortColumn == column ? !m_ascending
                                         : column == QLatin1String("name") || column == QLatin1String("pid");
    m_sortColumn = column;
    setSortRole(columns.value(column));
    sort(0, m_ascending ? Qt::AscendingOrder : Qt::DescendingOrder);
    emit sortingChanged();
}

QVariantMap ProcessFilter::get(int row) const
{
    const auto source = qobject_cast<ProcessModel *>(sourceModel());
    const int pid = data(index(row, 0), ProcessModel::PidRole).toInt();
    const auto process = source->find(pid);
    return process ? ProcessModel::describe(*process) : QVariantMap();
}

int ProcessFilter::indexOf(int pid) const
{
    for (int i = 0; i < rowCount(); ++i) {
        if (data(index(i, 0), ProcessModel::PidRole).toInt() == pid)
            return i;
    }
    return -1;
}

bool ProcessFilter::filterAcceptsRow(int row, const QModelIndex &parent) const
{
    const auto source = sourceModel();
    const auto index = source->index(row, 0, parent);
    if (!m_allUsers && source->data(index, ProcessModel::UidRole).toUInt() != m_uid)
        return false;

    const QString query = m_search.trimmed();
    if (query.isEmpty())
        return true;
    const auto process = qobject_cast<ProcessModel *>(source)->find(source->data(index, ProcessModel::PidRole).toInt());
    return process && (process->name.contains(query, Qt::CaseInsensitive)
                       || process->displayName.contains(query, Qt::CaseInsensitive)
                       || process->command.contains(query, Qt::CaseInsensitive)
                       || process->user.contains(query, Qt::CaseInsensitive)
                       || QString::number(process->pid).contains(query));
}

bool ProcessFilter::lessThan(const QModelIndex &left, const QModelIndex &right) const
{
    const QVariant first = sourceModel()->data(left, sortRole());
    const QVariant second = sourceModel()->data(right, sortRole());
    if (first == second)
        return sourceModel()->data(left, ProcessModel::PidRole).toInt()
                < sourceModel()->data(right, ProcessModel::PidRole).toInt();
    if (sortRole() == ProcessModel::NameRole)
        return QString::localeAwareCompare(first.toString(), second.toString()) < 0;
    return first.toDouble() < second.toDouble();
}
