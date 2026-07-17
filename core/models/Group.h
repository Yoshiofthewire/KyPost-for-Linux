#pragma once

#include <QString>

// A backend contact group -- id/name/rev only, mirroring the source doc's
// GroupEntity scoped down to what this Linux client actually needs (per
// task-2-brief.md: "Groups (scoped down)"). Contact::groupIds (see
// core/models/Contact.h) already carries membership as backend group UUIDs
// via the existing sync payload; this struct exists purely so GroupDao's
// local name-cache can resolve one of those UUIDs to a human-readable label
// for display (a later task's UI) -- it does not model groups as a
// first-class address-book concept, and nothing in this plan materializes
// groups as real native address-book entries (no NativeContactsProvider
// writes anywhere -- see the Global Constraints in task-2-brief.md).
struct Group
{
    QString id;
    QString name;
    qint64 rev = 0;

    bool operator==(const Group&) const = default;
};
