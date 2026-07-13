#pragma once

#include "models/Contact.h"

#include <QString>
#include <QVector>

struct ContactReconciliationAssignment
{
    QString localUid;  // the temp uid used for the local cache row
    QString serverUid; // the matched response entry's real uid

    bool operator==(const ContactReconciliationAssignment&) const = default;
};

// Direct port of Domain/Repositories/ContactSyncReconciliation.swift: there
// is no correlation ID on the wire for a locally-queued create, so matching
// a sync response's "changed" entry back to the local row that produced it
// is done by content first (pass 1: exact fn + primary-email match), then by
// leftover order (pass 2) for whatever content matching couldn't resolve
// (e.g. the server normalized the display name). Each response entry is
// used at most once; unmatched local contacts stay pending for the next
// sync.
class ContactSyncReconciliation
{
public:
    // localPending: queued creates, with .uid set to each one's temp local
    // uid (NOT empty -- unlike the wire copy in change_json, which has
    // uid=="").  responseChanged: the sync response's "changed" array.
    static QVector<ContactReconciliationAssignment> reconcile(
        const QVector<Contact>& localPending, const QVector<Contact>& responseChanged);
};
