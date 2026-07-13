#include "domain/ContactSyncReconciliation.h"

#include <algorithm>

namespace {

QString primaryEmail(const Contact& contact)
{
    return contact.emails.isEmpty() ? QString() : contact.emails.first().value;
}

} // namespace

QVector<ContactReconciliationAssignment> ContactSyncReconciliation::reconcile(
    const QVector<Contact>& localPending, const QVector<Contact>& responseChanged)
{
    // Candidate filter: a responseChanged entry is eligible only if it
    // carries a real uid and isn't itself a tombstone.
    QVector<Contact> candidates;
    for (const Contact& contact : responseChanged) {
        if (!contact.uid.isEmpty() && !contact.deleted)
            candidates.append(contact);
    }

    QVector<Contact> unmatchedLocal;
    QVector<ContactReconciliationAssignment> assignments;

    // Pass 1: exact fn + primary-email match.
    for (const Contact& contact : localPending) {
        const QString fn = contact.fn.value_or(QString());
        const QString email = primaryEmail(contact);

        int matchIndex = -1;
        for (int i = 0; i < candidates.size(); ++i) {
            const Contact& candidate = candidates.at(i);
            if (candidate.fn.value_or(QString()) == fn && primaryEmail(candidate) == email) {
                matchIndex = i;
                break;
            }
        }

        if (matchIndex >= 0) {
            assignments.append({ contact.uid, candidates.at(matchIndex).uid });
            candidates.remove(matchIndex);
        } else {
            unmatchedLocal.append(contact);
        }
    }

    // Pass 2: leftover order.
    const int pairCount = std::min(unmatchedLocal.size(), candidates.size());
    for (int i = 0; i < pairCount; ++i)
        assignments.append({ unmatchedLocal.at(i).uid, candidates.at(i).uid });

    return assignments;
}
