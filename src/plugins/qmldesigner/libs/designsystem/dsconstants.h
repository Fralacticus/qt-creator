// Copyright (C) 2024 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once
#include "designsystem_global.h"

#include "nodeinstanceglobal.h"

#include <QVariant>

#include <tuple>

namespace QmlDesigner
{

using ThemeId = ushort;
Q_NAMESPACE
enum class GroupType { Colors, Flags, Numbers, Strings };
Q_ENUM_NS(GroupType)

class DESIGNSYSTEM_EXPORT ThemeProperty
{
public:
    bool isValid() const { return !name.trimmed().isEmpty() && value.isValid(); }

    PropertyName name;
    QVariant value;
    bool isBinding = false;
};

constexpr const char *GroupId(const GroupType type) {
    if (type == GroupType::Colors) return "colors";
    if (type == GroupType::Flags) return "flags";
    if (type == GroupType::Numbers) return "numbers";
    if (type == GroupType::Strings) return "strings";

    return "unknown";
}

using DSBindingInfo = std::tuple<PropertyName, ThemeId, GroupType, QStringView>;
}
