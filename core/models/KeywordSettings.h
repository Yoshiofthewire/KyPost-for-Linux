#pragma once

#include <QString>

// Per-keyword inbox-tab visibility toggle. No backend/Android source for
// this exact shape exists yet; kept minimal on purpose.
struct KeywordSettings
{
    QString keyword;
    bool visible = false;

    bool operator==(const KeywordSettings&) const = default;
};
