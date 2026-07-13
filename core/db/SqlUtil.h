#pragma once

#include <QString>
#include <QVariant>
#include <optional>

// Shared null-handling helpers for the optional<QString> columns that
// EmailDao (body) and ContactDao (createdAt/updatedAt/fn/...) both bind.
inline QVariant optionalStringToVariant(const std::optional<QString>& value)
{
    return value ? QVariant(*value) : QVariant();
}

inline std::optional<QString> variantToOptionalString(const QVariant& value)
{
    return value.isNull() ? std::nullopt : std::optional<QString>(value.toString());
}
