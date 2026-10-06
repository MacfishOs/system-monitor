// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QString>

#include <vector>

struct ProcessRecord
{
    int pid = 0;
    int parentPid = 0;
    uint uid = 0;
    QString name;
    QString displayName;
    QString iconName;
    QString user;
    QString command;
    QString executable;
    QString program;
    QString scopeId;
    QString appId;
    QString state;
    int threads = 0;
    int nice = 0;
    quint64 startTicks = 0;
    quint64 cpuTicks = 0;
    quint64 rss = 0;
    // PSS when it could be read, resident memory otherwise.
    quint64 memory = 0;
    quint64 readBytes = 0;
    quint64 writeBytes = 0;
    quint64 pss = 0;
    qint64 pssTime = 0;
    bool pssAvailable = false;
    bool ioAvailable = false;
    bool kernelThread = false;
    bool scopeSettled = false;
    double cpu = 0;
    double readRate = 0;
    double writeRate = 0;
    // Set on application groups only: every process the application owns.
    std::vector<ProcessRecord> members;
};
