// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "procreader.h"

#include <QAbstractListModel>
#include <QSortFilterProxyModel>

class ProcessModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Role {
        PidRole = Qt::UserRole + 1,
        NameRole,
        ProcessNameRole,
        IconRole,
        UserRole,
        UidRole,
        CpuRole,
        MemoryRole,
        ReadRole,
        WriteRole,
        IoAvailableRole,
        StartRole
    };

    explicit ProcessModel(QObject *parent = nullptr);
    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    void apply(const QVector<ProcessRecord> &processes);
    const ProcessRecord *find(int pid) const;
    static QVariantMap describe(const ProcessRecord &process);

private:
    QVector<ProcessRecord> m_processes;
};

class ProcessFilter : public QSortFilterProxyModel
{
    Q_OBJECT
    Q_PROPERTY(QString search READ search WRITE setSearch NOTIFY searchChanged)
    Q_PROPERTY(bool allUsers READ allUsers WRITE setAllUsers NOTIFY allUsersChanged)
    Q_PROPERTY(QString sortColumn READ sortColumn NOTIFY sortingChanged)
    Q_PROPERTY(bool ascending READ ascending NOTIFY sortingChanged)
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    explicit ProcessFilter(ProcessModel *source, QObject *parent = nullptr);
    QString search() const;
    void setSearch(const QString &search);
    bool allUsers() const;
    void setAllUsers(bool allUsers);
    QString sortColumn() const;
    bool ascending() const;
    int count() const;
    Q_INVOKABLE void sortBy(const QString &column);
    Q_INVOKABLE QVariantMap get(int row) const;
    Q_INVOKABLE int indexOf(int pid) const;

signals:
    void searchChanged();
    void allUsersChanged();
    void sortingChanged();
    void countChanged();

protected:
    bool filterAcceptsRow(int row, const QModelIndex &parent) const override;
    bool lessThan(const QModelIndex &left, const QModelIndex &right) const override;

private:
    QString m_search;
    bool m_allUsers = false;
    QString m_sortColumn = QStringLiteral("cpu");
    bool m_ascending = false;
    uint m_uid;
};
