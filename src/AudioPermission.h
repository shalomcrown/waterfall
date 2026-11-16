#pragma once

#include <QString>

#include <functional>

class AudioPermission {
public:
    enum class Status { Granted, Denied, Undetermined };

    static Status status();
    static void request(const std::function<void(Status)>& done);
    static bool canOpenSystemSettings();
    static bool openSystemSettings();
    static QString instructions();
};
